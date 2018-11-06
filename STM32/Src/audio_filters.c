#include "audio_filters.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "arm_math.h"
#include "wm8731.h"

arm_fir_instance_f32    FIR_RX_Hilbert_I;
arm_fir_instance_f32    FIR_RX_Hilbert_Q;
arm_fir_instance_f32    FIR_TX_Hilbert_I;
arm_fir_instance_f32    FIR_TX_Hilbert_Q;
arm_fir_instance_f32    FIR_RX_LPF;

//states
float32_t   Fir_Rx_Hilbert_State_I[FIR_RX_HILBERT_STATE_SIZE];
float32_t   Fir_Rx_Hilbert_State_Q[FIR_RX_HILBERT_STATE_SIZE];
float32_t   Fir_Tx_Hilbert_State_I[FIR_TX_HILBERT_STATE_SIZE];
float32_t   Fir_Tx_Hilbert_State_Q[FIR_TX_HILBERT_STATE_SIZE];
float32_t		Fir_Rx_LPF_State[FPGA_AUDIO_BUFFER_SIZE + FIR_LPF_Taps];
float32_t		iir_rx_state[FIR_RX_LPF_STATE_SIZE];
float32_t		IIR_aa_state[FIR_RX_AA_STATE_SIZE];

// +0 degrees Hilbert narrow 3.6k
const float32_t i_rx_3k6_coeffs[IQ_RX_HILBERT_TAPS] = { -0.000181752008817777,-0.00019812444299558,-0.000173915580313638,-0.000106975730010085,-0.0000174463735378,0.000049667546759971,0.000032561004630231,-0.000127681493947272,-0.000459708207577463,-0.000935771201527816,-0.00145865712043841,-0.00187227346724911,-0.00200036401889901,-0.00170780689629204,-0.000967958368014592,0.000088289293095689,0.001166623215145986,0.001862531622450524,0.001771946721352388,0.000635741675713185,-0.00152451719575799,-0.00433057882661493,-0.00707443210838268,-0.00886614749018614,-0.00888315684570901,-0.00666207782636225,-0.0023460199801576,0.003204723474564544,0.008506335530971061,0.011765956147501436,0.011367103182967093,0.006413424630698947,-0.00282047953175066,-0.0146812039524101,-0.0262751422843746,-0.0339730971952901,-0.0341994964523126,-0.0243347165617545,-0.00350726050818481,0.026942155121425213,0.063476622132557281,0.100898465799951861,0.133312097666693830,0.155307720769003010,0.163093615613067344,0.155307720769004343,0.133312097666696161,0.100898465799954873,0.063476622132560459,0.026942155121428071,-0.00350726050818265,-0.0243347165617533,-0.0341994964523123,-0.0339730971952905,-0.0262751422843754,-0.0146812039524111,-0.00282047953175152,0.006413424630698421,0.011367103182966964,0.011765956147501661,0.008506335530971517,0.003204723474565072,-0.00234601998015715,-0.00666207782636198,-0.00888315684570896,-0.00886614749018627,-0.00707443210838294,-0.00433057882661522,-0.00152451719575822,0.000635741675713052,0.001771946721352369,0.001862531622450603,0.001166623215146122,0.000088289293095838,-0.00096795836801447,-0.00170780689629197,-0.002000364018899,-0.00187227346724914,-0.00145865712043846,-0.000935771201527874,-0.000459708207577509,-0.000127681493947298,0.000032561004630225,0.000049667546759980,-0.000017446373537785,-0.00010697573001007,-0.000173915580313629,-0.000198124442995578,-0.000181752 };
// +90 degrees Hilbert narrow 3.6k
const float32_t q_rx_3k6_coeffs[IQ_RX_HILBERT_TAPS] = { -0.000294806871446761,-0.000430038513938049,-0.00057577635651954,-0.000701452460068679,-0.000773483568490036,-0.000770724776536896,-0.000701666832263429,-0.000616113554803381,-0.000603644067469941,-0.00077404001429163,-0.00122090559459946,-0.00197736129120186,-0.00297920572331694,-0.00405321865749724,-0.00494407320037525,-0.00538239440020133,-0.00518124403535075,-0.00433349474961163,-0.00307403519977391,-0.00187345276484406,-0.00134566807456456,-0.00207838789258401,-0.00442514292801338,-0.00832115293523963,-0.0131925136715957,-0.0180131059057811,-0.0215266256625295,-0.0226003081681251,-0.0206265730133377,-0.0158556828219807,-0.00954049290906044,-0.00380949445236171,-0.00125158785236759,-0.00427935828129978,-0.0144137579244869,-0.0316779221618311,-0.0542839354303643,-0.0787391229290012,-0.100398818306822,-0.114375118099481,-0.116607805332378,-0.104844680649768,-0.0792835060106304,-0.0426990455969243,-1.802423033856850E-15,0.042699045596920997,0.079283506010627697,0.104844680649766375,0.116607805332377912,0.114375118099481379,0.100398818306823789,0.078739122929003105,0.054283935430366234,0.031677922161832718,0.014413757924488018,0.004279358281300266,0.001251587852367549,0.003809494452361318,0.009540492909059896,0.015855682821980240,0.020626573013337410,0.022600308168125034,0.021526625662529674,0.018013105905781469,0.013192513671596058,0.008321152935239943,0.004425142928013570,0.002078387892584060,0.001345668074564481,0.001873452764843904,0.003074035199773733,0.004333494749611476,0.005181244035350659,0.005382394400201308,0.004944073200375284,0.004053218657497318,0.002979205723317022,0.001977361291201922,0.001220905594599500,0.000774040014291644,0.000603644067469934,0.000616113554803364,0.000701666832263413,0.000770724776536887,0.000773483568490036,0.000701452460068687,0.000575776356519553,0.000430038513938061,0.000294806871446770 };

//with +/-45 degrees phase added, 48000 sampling frequency Fc=1.50kHz, BW=2.70kHz Kaiser, Beta = 3.650, Raised Cosine 0.910
const float32_t i_tx_coeffs[IQ_TX_HILBERT_TAPS] = { -0.000015911433738947,-0.000019959728028938,-0.000023891471599754,-0.000027119763124069,-0.000029087039705292,-0.000029431397715954,-0.000028148219961633,-0.000025702247123746,-0.000023048146401552,-0.000021532467002145,-0.000022675017711946,-0.000027858477628481,-0.000037984418906156,-0.000053173935947263,-0.000072594827015540,-0.000094480871554435,-0.000116372847662635,-0.000135561336982947,-0.000149658359810178,-0.000157181343994879,-0.000158011427902595,-0.000153597634188139,-0.000146821594676398,-0.000141508706977174,-0.000141657313079918,-0.000150538332814799,-0.000169872539761493,-0.000199303342326905,-0.000236339821701096,-0.000276850226155669,-0.000316055860965538,-0.000349836665758744,-0.000376046667777827,-0.000395483070998372,-0.000412180921252682,-0.000432823344456314,-0.000465251321301123,-0.000516291531622259,-0.000589344418113810,-0.000682329334357383,-0.000786618128311585,-0.000887471604155052,-0.000966224936321975,-0.001004084647738534,-0.000986971617519555,-0.000910465431454767,-0.000783674244635495,-0.000630853460997997,-0.000489869695624849,-0.000407142410379079,-0.000429421385599579,-0.000593545407736026,-0.000916011846980452,-0.001384598358663629,-0.001954277926711752,-0.002549184202610570,-0.003071435226054831,-0.003416332297185353,-0.003492031457300130,-0.003240513223388775,-0.002655839887826624,-0.001795531271501309,-0.000781549566761797,0.000211143419483594,0.000978377875720550,0.001320403087257693,0.001080515967568864,0.000182121345956079,-0.001343074868529297,-0.003342478753804935,-0.005550446581697207,-0.007618265957299014,-0.009164142003551838,-0.009837061742258062,-0.009385008993095094,-0.007715887072085315,-0.004939218878809190,-0.001378502462245723,0.002452076537338900,0.005907488065309155,0.008302997095508243,0.009025375745234239,0.007648260210940124,0.004033210841998318,-0.001601654938770521,-0.008660246045580289,-0.016203010335164171,-0.023038057426319945,-0.027860750884303231,-0.029425395934213904,-0.026726377512887117,-0.019162929429234910,-0.006662334542242190,0.010259113968032065,0.030510126124987710,0.052515877605683242,0.074378297568183149,0.094086304242110563,0.109749301278724468,0.119823145137080742,0.123298109645664233,0.119823145137071957,0.109749301278707773,0.094086304242087637,0.074378297568156199,0.052515877605654772,0.030510126124960246,0.010259113968007870,-0.006662334542261386,-0.019162929429248053,-0.026726377512893910,-0.029425395934214775,-0.027860750884299197,-0.023038057426312451,-0.016203010335154873,-0.008660246045570802,-0.001601654938762230,0.004033210842004407,0.007648260210943465,0.009025375745234765,0.008302997095506299,0.005907488065305405,0.002452076537334174,-0.001378502462250567,-0.004939218878813399,-0.007715887072088339,-0.009385008993096640,-0.009837061742258109,-0.009164142003550596,-0.007618265957296863,-0.005550446581694617,-0.003342478753802375,-0.001343074868527162,0.000182121345957515,0.001080515967569481,0.001320403087257519,0.000978377875719736,0.000211143419482372,-0.000781549566763161,-0.001795531271502565,-0.002655839887827574,-0.003240513223389301,-0.003492031457300196,-0.003416332297185003,-0.003071435226054171,-0.002549184202609736,-0.001954277926710889,-0.001384598358662863,-0.000916011846979873,-0.000593545407735683,-0.000429421385599475,-0.000407142410379183,-0.000489869695625105,-0.000630853460998336,-0.000783674244635851,-0.000910465431455086,-0.000986971617519803,-0.001004084647738694,-0.000966224936322048,-0.000887471604155053,-0.000786618128311534,-0.000682329334357303,-0.000589344418113718,-0.000516291531622168,-0.000465251321301040,-0.000432823344456239,-0.000412180921252613,-0.000395483070998306,-0.000376046667777760,-0.000349836665758677,-0.000316055860965474,-0.000276850226155611,-0.000236339821701050,-0.000199303342326874,-0.000169872539761481,-0.000150538332814806,-0.000141657313079940,-0.000141508706977206,-0.000146821594676434,-0.000153597634188172,-0.000158011427902620,-0.000157181343994892,-0.000149658359810178,-0.000135561336982934,-0.000116372847662614,-0.000094480871554409,-0.000072594827015514,-0.000053173935947240,-0.000037984418906137,-0.000027858477628469,-0.000022675017711940,-0.000021532467002142,-0.000023048146401550,-0.000025702247123743,-0.000028148219961627,-0.000029431397715943,-0.000029087039705276,-0.000027119763124050,-0.000023891471599733,-0.000019959728028917,-0.000015911433738929 };
const float32_t q_tx_coeffs[IQ_TX_HILBERT_TAPS] = { -0.000055876484573473,-0.000064501669527415,-0.000074738701316321,-0.000086507654429771,-0.000099410855699246,-0.000112742030021933,-0.000125581860328626,-0.000136976384505566,-0.000146171471747199,-0.000152855683517751,-0.000157351204077214,-0.000160693204323070,-0.000164554344882989,-0.000171001789764558,-0.000182113756522494,-0.000199522659221598,-0.000223982104007250,-0.000255065964662709,-0.000291093444701605,-0.000329333837166658,-0.000366484543520879,-0.000399347771170467,-0.000425571116339335,-0.000444281412941937,-0.000456442686257327,-0.000464813293654387,-0.000473459903203533,-0.000486891982756433,-0.000508986765583088,-0.000541954254458334,-0.000585620149319337,-0.000637266013271176,-0.000692159225019129,-0.000744745974185545,-0.000790300256973267,-0.000826662663506291,-0.000855608799816551,-0.000883393930312482,-0.000920144557182625,-0.000977999833391699,-0.001068208413135301,-0.001197698143396605,-0.001365882230232392,-0.001562573853813371,-0.001767799237590146,-0.001954010129822268,-0.002090729412517879,-0.002151093704598354,-0.002119197166650733,-0.001996721724352054,-0.001807182145055243,-0.001596303598977302,-0.001427605606562385,-0.001373133139886880,-0.001500318939184498,-0.001856985786393334,-0.002457280182694243,-0.003271661399515204,-0.004223805794000835,-0.005196381265502662,-0.006046183103054665,-0.006627310796927153,-0.006819219662039622,-0.006554969302166284,-0.005844167767895242,-0.004785243876864911,-0.003562886684106115,-0.002428691218878755,-0.001665958674417137,-0.001542756544570849,-0.002260180366086176,-0.003904692125323062,-0.006413957060137492,-0.009564477743523059,-0.012986526898586098,-0.016207713154723136,-0.018721568815726210,-0.020072626005615220,-0.019945430948836697,-0.018242650197742859,-0.015137438929630096,-0.011087811480348275,-0.006805694582534307,-0.003180041098991087,-0.001160864894184417,-0.001618136646873710,-0.005194938168183790,-0.012177070502025733,-0.022400779394307693,-0.035216238922248289,-0.049517325350430505,-0.063838956132037980,-0.076513206133968448,-0.085866090480225241,-0.090429824178616405,-0.089141744773038101,-0.081501626274434108,-0.067663896323307105,-0.048449702280469985,-0.025274673539499328,16.41584600088100E-15,0.025274673539530806,0.048449702280497546,0.067663896323328629,0.081501626274448152,0.089141744773044040,0.090429824178614490,0.085866090480216414,0.076513206133954376,0.063838956132020688,0.049517325350412138,0.035216238922230865,0.022400779394292886,0.012177070502014685,0.005194938168177043,0.001618136646871193,0.001160864894185542,0.003180041098994882,0.006805694582539598,0.011087811480353871,0.015137438929634959,0.018242650197746225,0.019945430948838137,0.020072626005614679,0.018721568815723948,0.016207713154719635,0.012986526898581976,0.009564477743518948,0.006413957060133944,0.003904692125320466,0.002260180366084725,0.001542756544570538,0.001665958674417785,0.002428691218880064,0.003562886684107731,0.004785243876866489,0.005844167767896496,0.006554969302167027,0.006819219662039779,0.006627310796926756,0.006046183103053836,0.005196381265501569,0.004223805793999667,0.003271661399514136,0.002457280182693402,0.001856985786392793,0.001500318939184273,0.001373133139886934,0.001427605606562642,0.001596303598977673,0.001807182145055637,0.001996721724352394,0.002119197166650973,0.002151093704598469,0.002090729412517876,0.001954010129822170,0.001767799237589989,0.001562573853813192,0.001365882230232221,0.001197698143396462,0.001068208413135197,0.000977999833391630,0.000920144557182582,0.000883393930312452,0.000855608799816521,0.000826662663506252,0.000790300256973217,0.000744745974185486,0.000692159225019068,0.000637266013271121,0.000585620149319296,0.000541954254458312,0.000508986765583088,0.000486891982756452,0.000473459903203566,0.000464813293654426,0.000456442686257364,0.000444281412941965,0.000425571116339349,0.000399347771170465,0.000366484543520863,0.000329333837166631,0.000291093444701573,0.000255065964662676,0.000223982104007221,0.000199522659221575,0.000182113756522479,0.000171001789764551,0.000164554344882987,0.000160693204323071,0.000157351204077214,0.000152855683517748,0.000146171471747192,0.000136976384505555,0.000125581860328611,0.000112742030021918,0.000099410855699231,0.000086507654429759,0.000074738701316315,0.000064501669527414,0.000055876484573478 };

//FIR LPF 2.7k
const float32_t FIR_2k7_LPF[FIR_LPF_Taps] = { -132.4349258372167810E-6,-219.6565051849731840E-6,-221.0954878572501340E-6,-112.7825639678642350E-6, 102.4575856101722450E-6, 389.2853249984518700E-6, 680.7047072996329010E-6, 886.5176124126478500E-6, 908.9190303575152260E-6, 663.1798212029935940E-6, 99.94394333471568360E-6,-775.2593942400565080E-6,-0.001889866979367635,-0.003103365535555945,-0.004219896473177527,-0.005014402752420468,-0.005269727863388856,-0.004819569738167059,-0.003590272808030926,-0.001633541409887721, 857.4275646318113790E-6, 0.003554075767444719, 0.006023490745282516, 0.007782972853164249, 0.008372642756768314, 0.007436788590800983, 0.004801979871354186, 538.9791894714875300E-6,-0.005003503223187542,-0.011202116188924751,-0.017203361876765948,-0.022007518376887823,-0.024584085267108431,-0.024005391839791387,-0.019581149012259533,-0.010975173068247874, 0.001713350295482858, 0.017917873275534617, 0.036629508851894865, 0.056479298752105583, 0.075870050881509124, 0.093142932024622954, 0.106758708615444731, 0.115470911994313030, 0.118468712970506387, 0.115470911994313030, 0.106758708615444731, 0.093142932024622954, 0.075870050881509124, 0.056479298752105583, 0.036629508851894865, 0.017917873275534617, 0.001713350295482858,-0.010975173068247874,-0.019581149012259533,-0.024005391839791387,-0.024584085267108431,-0.022007518376887823,-0.017203361876765948,-0.011202116188924751,-0.005003503223187542, 538.9791894714875300E-6, 0.004801979871354186, 0.007436788590800983, 0.008372642756768314, 0.007782972853164249, 0.006023490745282516, 0.003554075767444719, 857.4275646318113790E-6,-0.001633541409887721,-0.003590272808030926,-0.004819569738167059,-0.005269727863388856,-0.005014402752420468,-0.004219896473177527,-0.003103365535555945,-0.001889866979367635,-775.2593942400565080E-6, 99.94394333471568360E-6, 663.1798212029935940E-6, 908.9190303575152260E-6, 886.5176124126478500E-6, 680.7047072996329010E-6, 389.2853249984518700E-6, 102.4575856101722450E-6,-112.7825639678642350E-6,-221.0954878572501340E-6,-219.6565051849731840E-6,-132.4349258372167810E-6 };

//LPF 2.7k SSB	
arm_iir_lattice_instance_f32 IIR_2k7_LPF =
{
	.numStages = 14,
	.pkCoeffs = (float*)(const float[]) { 0.0001352030231, -0.00130182039, 0.005726013798, -0.01496474817,  0.02503616177, -0.02589187399,  0.01126107108,  0.01126107108, -0.02589187399,  0.02503616177, -0.01496474817, 0.005726013798, -0.00130182039,0.0001352030231 },
	.pvCoeffs = (float*)(const float[]) { 1,   -12.19707775,    69.14491272,   -241.1901703,    577.4949341, -1002.180908,     1296.73999,   -1266.595215,    933.8630371,   -513.3150635, 204.4580383,   -55.88367844,    9.394873619,  -0.7337206602 }
};
//IIR BPF 120 - 2700Hz bandpass, 1410Hz center frequency
arm_iir_lattice_instance_f32 IIR_2k7_BPF =
{
	.numStages = 11,
	.pkCoeffs = (float*)(const float[]) { 9.876072227e-07,-2.875157406e-06,4.513433396e-06,-5.487821454e-06,4.838313544e-06, -3.148503765e-06,1.617615567e-06,-5.811978667e-07,1.591739931e-07,-2.494947715e-08, 1.486472057e-09 },
	.pvCoeffs = (float*)(const float[]) { 1,   -9.626163483,    41.94167328,     -108.92173,    186.7095947, -220.7347412,    182.2707825,   -103.8026886,    39.01845932,   -8.741547585, 0.8863812685 }
};

//IIR 5k Anti-aliasing Filter
arm_iir_lattice_instance_f32 IIR_aa_5k =
{
	.numStages = IIR_aa_5k_numStages,
	.pkCoeffs = (float*)(const float[])
	{
		0.547564303565301,
		-0.874437851307620,
		0.929381964274064,
		-0.931756218288255,
		0.967791921156360,
		-0.855303237881406
	},

	.pvCoeffs = (float*)(const float[])
	{
		0.00321398613474079,
		0.0103775071609725,
		0.0204793631675465,
		0.0207411754628459,
		0.0142582715360116,
		0.00416771325172439,
		0.000586794199906653
	}
};

//IIR Biquad Filter (a notch, peak, and lowshelf filter)
arm_biquad_casd_df1_inst_f32 IIR_biquad =
{
	.numStages = 4,
	.pCoeffs = (float32_t *)(const float32_t[])
	{
		1,0,0,0,0,  1,0,0,0,0,  1,0,0,0,0,  1,0,0,0,0
	}, // 4 x 5 = 20 coefficients
	.pState = (float32_t *)(const float32_t[])
	{
		0,0,0,0,   0,0,0,0,   0,0,0,0,   0,0,0,0
	} // 4 x 4 = 16 state variables
};
