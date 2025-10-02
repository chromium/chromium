// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/chrome_app_deprecation/chrome_app_deprecation.h"

#include <cstddef>
#include <unordered_set>

#include "ash/public/cpp/system_notification_builder.h"
#include "ash/style/system_dialog_delegate_view.h"
#include "base/containers/fixed_flat_set.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/component_updater/chrome_apps_deprecation_allowlist_component_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace apps::chrome_app_deprecation {

BASE_FEATURE(kAllowUserInstalledChromeApps, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAllowChromeAppsInKioskSessions,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChromeAppsDeprecationComponentUpdater,
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {
constexpr auto kCommonAllowlist = base::MakeFixedFlatSet<std::string_view>({
    // go/keep-sorted start
    "aakfkoilmhehmmadlkedfbcelkbamdkj",
    "acdafoiapclbpdkhnighhilgampkglpc",
    "aepgaekjheajlcifmpjcnpbjcencoefn",
    "afoipjmffplafpbfjopglheidddioiai",
    "afpnehpifljbjjplppeplamalioanmio",
    "ahpbemfdnadmigmdjhebofmeaonbpfmc",
    "anjihnbmjbbpofafpmklejenkgnjfcdi",
    "aoijoapjiidlaapoinclpjkmpaeckiff",
    "aphendncpdekdkepekckjkiloclamieb",
    "baifnloidiaigliddpkifgokjemcbcei",
    "bajigdlccokpmeadnhpfhpehdefbgaen",
    "bbkieeoaobjflkeakhemifofdbbfhnic",
    "bhfbomkadeplbpgfmiihpglmenahkmao",
    "bikbageiaongkigeijiahadjbcgindbj",
    "bnkchehofckdmggiknjidlamlpokbodf",
    "bpmgmelggoioalpijejanjhbjkfeehbg",
    "cahbpjmendhigemdnlifkfmdhnipbdil",
    "cajomgbhgfomgakdejohnkomlblhhlmo",
    "cdebpoondplobcgjepkgplleeeeojmpa",
    "cdgdgmknjolkacdiheibdjmidfkooodf",
    "cedlmaejgblmkmnddjikaagkhbfonihp",
    "cgpnjolncgemfdgbfokgdbmhpondgjmm",
    "coomdpjcngcbdefihidllngfemgnmlhh",
    "dcfnglblnliiebcjiffpnecdkjnomjbl",
    "demfodeljeofljmbplgpcncaebjmboog",
    "demlnppodlnndiacjgbijdjnnnoninak",
    "denipklgekfpcdmbahmbpnmokgajnhma",
    "deokbmklnlnlikckmachjjhgnidefhhg",
    "dgmhhjhnkhlmooconggnbjhlmpkpliij",
    "djkbhkgnbiknnlinckcclejmjkddokhl",
    "djobiocnmcaeodjcdhbhjgjndhiadgod",
    "eaghkdkaebflfmmhidgnldnncfpknpne",
    "ealfhldampafeomimeidejkicmipkgkh",
    "eblkmenpohbbmbelfaggegpjfjokihke",
    "ecgoodkkapeinahfgidbfknincokmhdg",
    "efadkfcohfppfffgblnflcakfhfdjiig",
    "ejbidlmioeopgmjieecjihnlgacicoie",
    "ejoilaclhpbfooagcjdkkmklhjipgmll",
    "ekiflcmfallbndjhecchfcipbaajdfhl",
    "ekigfkofdacepchbgkogfedfapdekjgp",
    "emejfeljcemojhhcmobdeflgjabpafip",
    "emlbfhdjchamibhjgcokeipljabljheo",
    "enfpdhommpcbfiojillmflopkkjbcjmf",
    "faidilipbonmepcjdkhjfencfaaccgic",
    "famkiocmnjimafojaajdngnidmgnacme",
    "fecgcoakonfhepcppcbddeefeoekhbah",
    "fenegagmedfckampfgjbeoflcpcpdppc",
    "ffhbnjlppmbnhahkbkcjgapgfinabjgb",
    "fhohelmkloeoheiminpldlhkdfcmjbfm",
    "fjdejbdegplidjpkgcblpdibepibfifg",
    "fmfiolcdkhopmhgjbmlgpfcpfbeneope",
    "fnbgnnegegboidihpleofgakpegcidim",
    "fooeehkjmkcohfidagefenolegldgmpp",
    "gbfihfamagomeondkhooeamjajjadpio",
    "geopjmggmojbcnjlkcnfbgdniomaioif",
    "gfajignjkjbleogeegcgjimnkooihmdm",
    "ggaabodlngcnbdcpkfacegoacchkalmn",
    "ggddmkhlbkollcjopbnkbbhnikncfena",
    "gjenjmcioeobmpllaeopaoibabhgcohi",
    "glcdffonolecglhbodpaeijkhgdfkbon",
    "gnddkmpjjjcimefninepfmmddpgaaado",
    "gngadipbljmmcgcjjflidckpbgebnhod",
    "gnogkjfeajjnafijfmffnkgenhnkdnfp",
    "gpgnoonhefbmngkiafpedbligiiekfcp",
    "haeblkpifdemlfnkogkipmghfcbonief",
    "haiffjcadagjlijoggckpgfnoeiflnem",
    "hanegekdenjamflmdgcbjlobfkijeblp",
    "hclmbafbgpncekjmadbbcpekilflmkfg",
    "hgdemhjioannjiccnfgmllghllhpncpm",
    "hginjgofkfbdfpkjcchdklbkkdbigpna",
    "hhcgnlnhaapiekdelngjichnccjfkbnc",
    "hkamnlhnogggfddmjomgbdokdkgfelgg",
    "hkmlofdlheebfpgfcmgbdjddnoniccno",
    "hmpdelcfcndndcoldocpdmakeabbihgb",
    "hnlanngibjpmdolooednhkedmfbdbmhc",
    "hpdnjcbgolagabfgcgjpicbknmgefakl",
    "hplnogolijklhfbbfogccgickedplpeo",
    "iedihkacboebiliakaicmedjmajmjiep",
    "ighapdcohmkppihdjdejlbkolhbgnlfm",
    "ihlmfpkjommgamcgofmdmojpeolimlfe",
    "iiaffmacblgjekhogmghdjfflchkjmmg",
    "iilndnicahkogiklibnnibmmeikacnfo",
    "iinmojhiolplpndeijdkfoghkokbfadb",
    "iiopclfeneoimifgocjnhcjpjgaojhho",
    "ijdoledcajbpfbkiafmmimjhmkmdppjo",
    "ikfcpmgefdpheiiomgmhlmmkihchmdlj",
    "ikgemedabaijdochaempgdpfebllgfcc",
    "iknkgipmikbpldmppngljbedofgmanfm",
    "inaonhfifmcnldmdnlbnfpikjndebkbj",
    "jfhndkehlkceadabhedbcclclbclhnbh",
    "jgafcpolgeedpieaadaeeaoanackiina",
    "jglaiblkoeelgfdabnhpcpdnodjonclf",
    "jjkgijommndbjlekbalbbiiidnigcgfl",
    "jjlhmikmcgmheddmlfeckndcedkmcpng",
    "jjnejapcbafplbdkbombhmmjnafplkon",
    "jjoncgfekjbknjfejfonaochdpdedbka",
    "jlgegmdnodfhciolbdjciihnlaljdbjo",
    "jnnkgopblccifpnkfpfkmdafjebjlhcc",
    "jnojnnofimbdpeihiddafgagckdlnlpe",
    "jpmngkkdajjfkdknhbifjbglkckbklee",
    "kahkblckpdgogkogmfhfnldpjhdpfiia",
    "kdbdkbbfhghbggpjmpapmobihghkdmkh",
    "kdndmepchimlohdcdkokdddpbnniijoa",
    "kenkpdjcfppbccchillfdjkjnejjgand",
    "kflikliicodcopdhibchdfaninnhbalf",
    "kfllildicglifipmhpnlmpfbkdponghk",
    "khpfeaanjngmcnplbdlpegiifgpfgdco",
    "khplkoflcklpnlofodhlnjeiodbmejoe",
    "kjceddihhogmglodncbmpembbclhnpda",
    "kjfhgcncjdebkoofmbjoiemiboifnpbo",
    "kljahdaehfmgddhnibkikcjfppjcjjcn",
    "kmfbmibhlikajdfjbddlolmdkkbiephg",
    "lbfgjakkeeccemhonnolnmglmfmccaag",
    "ldmpofkllgeicjiihkimgeccbhghhmfj",
    "lemoeliioheohdcoogohonkamhloahbb",
    "likeoemlchnioaoaklldmcnilhhpjamo",
    "lkbhffjfgpmpeppncnimiiikojibkhnm",
    "lknebpkncfibkhjkimejlgppnjgemobn",
    "lmhpnmjggoibofacnookchiemlihmjdd",
    "lnnghenlbgaeloipgjlafjhlccipbpnm",
    "maegcedffmoidlccpjahiglkaacbncnn",
    "mclaaifjbcglkbdhdkaamamplpjoabih",
    "mdmkkicfmmkgmpkmkdikhlbggogpicma",
    "medpmkohocjidlghgmnnkpfigfpddaok",
    "mhbelemjphdecdagmmengimkkiefmcej",
    "mhfhafklkbgalhbdihiccegaldefdigp",
    "mhjpnpdhahbahbjedoihlganncneknfo",
    "millmignkmpaolllendlllaibmeehohd",
    "mkjgggeeejocddadcegdhcchhmemokcn",
    "mndakpenoffnhdmpcpnajekhpbonggeo",
    "moklfjoegmpoolceggbebbmgbddlhdgp",
    "mpjaajdhcmmkeikfdgffdpdjncdnmhmk",
    "ncjnakhgkcldedboafigaailhldnellf",
    "ndlolfeihajiaklmehdnajjoblphkppd",
    "nenolmmehjhaggnamcglapjjdofcojao",
    "nghoaommfphpdlipedlebgcnmphedhdb",
    "ngiaihbicdcdflfkhilnaaeobnchggkk",
    "nhebofpemjfflnkmaneaopjickpliokk",
    "njofdhegeeccijokfiijflbfajgjclch",
    "oanbapfpojpdpjppgcmdhcjehacnccbm",
    "ocnncjgbkiomppnchhbmmcpblifejpco",
    "odcalbcbcmnepllckjhdndgmolpnddjo",
    "oefoedhdllfdpfpjhhccdiglflemnfdb",
    "oflckobdemeldmjddmlbaiaookhhcngo",
    "ofmlpkdeaopippomdfamngkpnbagkdem",
    "ogmfbebknnapidhhefcdgmoafjeblnjo",
    "okaiidkcbkpimeiebofglgpobdafmmeb",
    "ondpjadajoodngapikdebdcnjcjkeecc",
    "opalidednimmhdfbcpdmoihhpkahgkak",
    "pdgbdkbnajhamggjjlhlapedeolflpgm",
    "pdpgalakpabfiiadeiimoolhemoleaeg",
    "pgolnnkmmlpbnhfcfbephcnkooejbcep",
    "pifpopligmljinioeacaccciabhbbpjo",
    "plhmjahmpikllpphfaoopdhnkbpffccm",
    "pnclfbefcgmenbbbpljbhbdacgkgkjlh",
    "ppkfnjlimknmjoaemnpidmdlfchhehel"
    // go/keep-sorted end
});

constexpr auto kUserInstalledAllowlist = base::flat_set<std::string_view>();

constexpr auto kKioskSessionAllowlist =
    base::MakeFixedFlatSet<std::string_view>({
        // go/keep-sorted start
        "adbijfidmjidmkkpiglnfkflcoblkfmn",
        "adpfhflbokfdhnfakijgjkpkjegncbpl",
        "agkggapglfgffelalcfgbjmhkaljnbmn",
        "alaoimaeafbgfglpffgcidfgbjnekifp",
        "alhlkpgheiefedomljbenmkpconkffhk",
        "amdpebpoiccejfcnocgebkidfmkcdfei",
        "aoebmljacknghkklaholjkflllbghhnj",
        "bgldcjbajnkfkephalfogfgklkgjnjeo",
        "bhcnmihmgdljpnnoobnbdmdjhmfgcpio",
        "bloholppicibpgbagaebcaagiikicjbn",
        "cafpcfibibiomlehdnmabchhekeifbgb",
        "cdomppfkcljjopjijjdchhjfioljaeph",
        "cgihdamofndnjjlglmcaabdafhmoconf",
        "ckmkndfplnldgohnnkhmeokbmedpdbjl",
        "clbgknjcblogheibmcbbdlpkollmgofh",
        "cmhiajbopgbagidplpiaclnpglmhbhka",
        "cpbpbhkfonocjjamhjeabdihibkoajlc",
        "dakemaookmhkdfgcgebakflmhgdhille",
        "dakmgckkclepfbfeldlgenikiobflcne",
        "ddhhodggehedggajomidnmgchfnbeold",
        "dfjigmapgofdlgieniibjdcddlaafick",
        "dinalfjmfmjkdnkgbbjncgchmghijpgl",
        "ealpglkmnpenllgjjgdojoemohidefdm",
        "edhlcbaemfhpoblalbdgeegmaddjdcae",
        "edpaojhfdnnebhmmhdlpnpomoaopfjod",
        "efdahhfldoeikfglgolhibmdidbnpneo",
        "emlbcjpcbepfnhpkiidenlnfdjbghmpg",
        "fammfnbkkollpklfkachppebochgakjg",
        "fcichhfeoaikaoldkncmggipmpcbgffg",
        "fdlpibjfnlhnmeckjjhfiejfdghkmkdm",
        "gbecpjnejcnafnkgfciepngjcndodann",
        "gbgncgdjjnelalecmmkimnlgfpmbihog",
        "gcefeoeohcoeoofmehgjfipjiepodlhg",
        "gdehbmmmjkddbonbmknngoigkleicpec",
        "genfdmkliekafjhadcpnhefgicceohhd",
        "gmdgbdlpbnhiogedlhmdiceocbgcbpgi",
        "gobhocmdcdpfebockbogdfhnebgmemnf",
        "hadonmdpeimgfpmmmeldbmjiknnbfdhk",
        "hbcogfhdhehbfnedbbboiiddpkkjjnio",
        "hbfbekdejbpmnpilhdnfokjehnianfeb",
        "hblfbmjdaalalhifaajnnodlkiloengc",
        "hchdcamjekgapahefjapegmaapggeafe",
        "hebfpdlglfmneladiogocbflmbjneeoh",
        "hgkaljnpgngpcgnaonmbdgaolefknaaj",
        "hhbmmipodfklmbmiaegcbmbfmmfbngnf",
        "hjbkdjhfdcinjcljfbealemkioalnfao",
        "ibboejlnnenbhpjfpgoglholgpdjjeff",
        "icfpencnfmadodjpbbdipkkkljmamine",
        "iflkfmkmpafjfdkkokpkjpjmiogkdjjl",
        "igknghlgndjihblholjbbhjbcfilkilb",
        "ilehifjdadbblbcnciiggmcbmobkikcb",
        "jamdkebjilnlfjndffcnekbipcfkhmem",
        "jcgamccimilnfjpbkbadommjcaplmfod",
        "jefdfinffojbalcgpkigjjijghmllgil",
        "jiecdjmgkgmgmbonhifblhfaaecnomcj",
        "jifdnnnegbhoagepoobbmajnpkmcbjig",
        "jjlmjgfhdijljijikefhmgmhbchnkmnm",
        "jmiabaaccndlngedakcjbpbgokhgcpfd",
        "jnlegeoomaehdodfmpmlflpjapebjjjl",
        "jnlhnplbndpohngdfjhmdinlpofclhdp",
        "kacodfanpfkedlelnagnbgfbaabjfddn",
        "kbkcdgjhbdlplagmlcpafgamnapneoba",
        "kcdfcljkllboedjeoaicmmabopnnaoaa",
        "kdffphekpginklcnoefcelkjclbjnbmi",
        "kedeaijhpgoggdafoabafeldkoolemig",
        "kgoklcfigmpofpbkdglgbhfgpjdjgppl",
        "kjbdapadhmcgplddmcggjkhacdnpjmod",
        "kpjcmnnhdgonbhjnfhebgapnkicknmpp",
        "lfemdemifjedlccfbhpocnicmjlcgmce",
        "lgpjgoglfmjggeggfelogaboagbcaklg",
        "lmdoekjmofbfghllkonahbfdcckmgjlf",
        "lnokaenamkoojjbhehhpggplknlbejmi",
        "mbkamiddebohpehiafofidepfffpffln",
        "mfejnceblfpkdodajfohmjimcbipnhhh",
        "mfgkakkfpnhfmnipnbehiglkjijancnk",
        "mhboapffkffmmcggindghkakhdhmjcje",
        "mhdohnfjdghnpjmhnlodibcnjlaeinap",
        "mkgbgfehlfaioaejpaedngdohcpdpbpd",
        "nanoidlkencgghkphophigbmnohnbbcb",
        "nclhjadnjgfjocbnfmlcfnagnieialof",
        "nddaogoljagaikdogplnajkdggkfmgei",
        "ngpbnegpinocjhpnppjeppllflpgafkk",
        "nhlaojpmboioihghmmdbhgcbjgmcicdk",
        "nickmpjdfebcopckkfjmflblnmijbiom",
        "nloplhgjobaomjdppnbcdjfgbefifbdo",
        "obgbgecgadcagmhnanalmklenjajimld",
        "oblnbnkmblikfegpcngkcbppphcenhjj",
        "ocljbfllcpgnlnnaommbmaphaagjmkmj",
        "odjaaghiehpobimgdjjfofmablbaleem",
        "ofaokfiblaffkgcapcilcehdhlidehcd",
        "olaaocfpicpjiocmoklnbfpdlbglbadp",
        "omkghcboodpimaoimdkmigofhjcpmpeb",
        "omlplbdgdcpaaknjnkodikcklbkhefoh",
        "oopdabjckchhklpldcdjllmedcdnbdio",
        "pjdhfcpflabeafmgdpgdfdejbhkdcgja",
        "pjicdfmcmiihceiefbmioikgkcicochj",
        "plebdlehcdhfkmidnmfpolcifjngmdck",
        "pmcgpdpmlgkeociebbpdbppimbeheoli"
        // go/keep-sorted end
    });

// Add only allowlisted test app ids.
constexpr auto kTestAllowlist = {
    // go/keep-sorted start
    "aajgmlihcokkalfjbangebcffdoanjfo",
    "epeagdmdgnhlibpbnhalblaohdhhkpne",
    "fimgekdokgldflggeacgijngdienfdml",
    "kjecmldfmbflidigcdfdnegjgkgggoih"
    // go/keep-sorted end
};

struct DeprecationState {
  std::unordered_set<std::string> common_allowlist_from_component_updater;
  std::unordered_set<std::string>
      user_installed_allowlist_from_component_updater;
  std::unordered_set<std::string>
      kiosk_session_allowlist_from_component_updater;
  std::unordered_set<std::string> test_allowlisted_apps;

  base::Version last_allowlist_component_version;

  static DeprecationState* GetInstance() {
    static base::NoDestructor<DeprecationState> instance;
    return instance.get();
  }

  DeprecationState()
      : test_allowlisted_apps(
            std::unordered_set<std::string>(kTestAllowlist.begin(),
                                            kTestAllowlist.end())),
        last_allowlist_component_version("0.0.0") {}
  ~DeprecationState() = default;
};

// This enum lists the possible outcomes of the deprecation checks performed
// during the launch of a ChromeApp.
//
// These values are persisted to logs and the values match the entries of
// `enum ChromeAppDeprecationLaunchOutcome` in
// `tools/metrics/histograms/metadata/apps/enums.xml`.
// Entries should not be renumbered and numeric values should never be reused.
// LINT.IfChange(ChromeAppDeprecationLaunchOutcome)
enum class DeprecationCheckOutcome {
  kUserInstalledAllowedByFlag = 0,
  kUserInstalledAllowedByAllowlist = 1,
  kUserInstalledBlocked = 2,
  kKioskModeAllowedByFlag = 3,
  kKioskModeAllowedByAllowlist = 4,
  kKioskModeAllowedByAdminPolicy = 5,
  kKioskModeBlocked = 6,
  kManagedAllowedByFlag = 7,
  kManagedAllowedByAllowlist = 8,
  kManagedAllowedByAdminPolicy = 9,
  kManagedBlocked = 10,
  kAllowedNotChromeApp = 11,
  kAllowedDefault = 12,
  kBlockedDefault = 13,
  kMaxValue = kBlockedDefault
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/apps/enums.xml:ChromeAppDeprecationLaunchOutcome)

void ReportMetric(DeprecationCheckOutcome outcome) {
  base::UmaHistogramEnumeration("Apps.AppLaunch.ChromeAppsDeprecationCheck",
                                outcome);
}

bool g_fake_kiosk_session_for_testing = false;
bool g_skip_system_dialog_for_testing = false;

enum class AllowlistContext { UserInstalled, KioskSession };

bool IsAllowlisted(std::string_view app_id, AllowlistContext context) {
  auto* state = DeprecationState::GetInstance();
  switch (context) {
    case AllowlistContext::UserInstalled:
      return kCommonAllowlist.contains(app_id) ||
             state->common_allowlist_from_component_updater.contains(
                 app_id.data()) ||
             kUserInstalledAllowlist.contains(app_id) ||
             state->user_installed_allowlist_from_component_updater.contains(
                 app_id.data()) ||
             state->test_allowlisted_apps.contains(app_id.data());
    case AllowlistContext::KioskSession:
      return kCommonAllowlist.contains(app_id) ||
             state->common_allowlist_from_component_updater.contains(
                 app_id.data()) ||
             kKioskSessionAllowlist.contains(app_id) ||
             state->kiosk_session_allowlist_from_component_updater.contains(
                 app_id.data()) ||
             state->test_allowlisted_apps.contains(app_id.data());
  }
}

void ShowNotification(const extensions::Extension& app, Profile* profile) {
  message_center::Notification notification =
      ash::SystemNotificationBuilder()
          .SetId(app.id() + "-deprecation-notification")
          .SetCatalogName(ash::NotificationCatalogName::kChromeAppDeprecation)
          .SetTitle(base::ASCIIToUTF16(app.name()))
          .SetMessage(l10n_util::GetStringUTF16(
              IDS_USER_INSTALLED_CHROME_APP_DEPRECATION_NOTIFICATION_MESSAGE))
          .SetWarningLevel(
              message_center::SystemNotificationWarningLevel::WARNING)
          .Build(/*keep_timestamp=*/false);

  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      NotificationHandler::Type::ANNOUNCEMENT, notification,
      /*metadata=*/nullptr);
}

views::Widget* g_dialog_widget_;

void OnDialogClosed() {
  if (g_dialog_widget_) {
    delete g_dialog_widget_;
  }
  g_dialog_widget_ = nullptr;
}

void ShowLaunchBlockedDialog(const std::string& app_name) {
  if (g_dialog_widget_ != nullptr) {
    return;
  }

  auto dialog =
      views::Builder<ash::SystemDialogDelegateView>()
          .SetTitleText(l10n_util::GetStringFUTF16(
              IDS_USER_INSTALLED_CHROME_APP_DEPRECATION_BLOCKED_LAUNCH_DIALOG_TITLE,
              base::UTF8ToUTF16(app_name)))
          .SetDescription(l10n_util::GetStringUTF16(
              IDS_USER_INSTALLED_CHROME_APP_DEPRECATION_BLOCKED_LAUNCH_DIALOG_MESSAGE))
          .SetModalType(ui::mojom::ModalType::kSystem)
          .SetAcceptButtonText(l10n_util::GetStringUTF16(
              IDS_USER_INSTALLED_CHROME_APP_DEPRECATION_BLOCKED_LAUNCH_DIALOG_CLOSE_BUTTON))
          .SetModalType(ui::mojom::ModalType::kSystem)
          .SetCloseCallback(base::BindOnce(&OnDialogClosed))
          .Build();
  dialog->SetCancelButtonVisible(false);

  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.delegate = dialog.release();
  params.name = "ChrAppDeprecation-LaunchBlocked";
  params.activatable = views::Widget::InitParams::Activatable::kYes;

  g_dialog_widget_ = new views::Widget(std::move(params));
  g_dialog_widget_->Show();
}

bool IsUserInstalled(std::string_view app_id, Profile* profile) {
  auto* prefs = extensions::ExtensionPrefs::Get(profile);
  if (!prefs) {
    return false;
  }

  std::optional<const extensions::ExtensionInfo> extension_info =
      prefs->GetInstalledExtensionInfo(app_id.data());
  if (!extension_info) {
    return false;
  }

  auto location = extension_info->extension_location;

  return location == extensions::mojom::ManifestLocation::kInternal ||
         location == extensions::mojom::ManifestLocation::kUnpacked;
}

DeprecationStatus HandleUserInstalledApp(const extensions::Extension& app,
                                         Profile* profile) {
  if (IsAllowlisted(app.id(), AllowlistContext::UserInstalled)) {
    ReportMetric(DeprecationCheckOutcome::kUserInstalledAllowedByAllowlist);
    return DeprecationStatus::kLaunchAllowed;
  }

  if (base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps)) {
    ShowNotification(app, profile);
    ReportMetric(DeprecationCheckOutcome::kUserInstalledAllowedByFlag);
    return DeprecationStatus::kLaunchAllowed;
  }

  if (!g_skip_system_dialog_for_testing) {
    ShowLaunchBlockedDialog(app.name());
  }
  ReportMetric(DeprecationCheckOutcome::kUserInstalledBlocked);
  return DeprecationStatus::kLaunchBlocked;
}

DeprecationStatus HandleKioskSessionApp(const extensions::Extension& app,
                                        Profile* profile) {
  // TODO(crbug.com/379262711): Block the execution in M151.
  if (IsAllowlisted(app.id(), AllowlistContext::KioskSession)) {
    ReportMetric(DeprecationCheckOutcome::kKioskModeAllowedByAllowlist);
    return DeprecationStatus::kLaunchAllowed;
  }

  if (profile->GetPrefs()->GetBoolean(prefs::kKioskChromeAppsForceAllowed)) {
    ReportMetric(DeprecationCheckOutcome::kKioskModeAllowedByAdminPolicy);
    return DeprecationStatus::kLaunchAllowed;
  }

  if (base::FeatureList::IsEnabled(kAllowChromeAppsInKioskSessions)) {
    ReportMetric(DeprecationCheckOutcome::kKioskModeAllowedByFlag);
    return DeprecationStatus::kLaunchAllowed;
  }

  ReportMetric(DeprecationCheckOutcome::kKioskModeBlocked);
  return DeprecationStatus::kLaunchBlocked;
}

std::optional<const ChromeAppDeprecation::DynamicAllowlists>
ReadAllowlistsFromFile(const base::FilePath& file_path) {
  std::string allowlists_data;
  if (!base::ReadFileToString(file_path, &allowlists_data)) {
    return std::nullopt;
  }

  ChromeAppDeprecation::DynamicAllowlists allowlists;
  if (!allowlists.ParseFromString(allowlists_data)) {
    return std::nullopt;
  }

  return allowlists;
}

void AssignComponentUpdaterAllowlists(
    const base::Version& component_version,
    std::optional<const ChromeAppDeprecation::DynamicAllowlists>
        component_data) {
  auto* state = DeprecationState::GetInstance();

  // We do not need to verify the version number here, as it has been already
  // checked in LoadComponentUpdaterAllowlists.
  if (!component_version.IsValid() || !component_data) {
    return;
  }

  state->common_allowlist_from_component_updater =
      std::unordered_set<std::string>(
          component_data->common_allowlist().begin(),
          component_data->common_allowlist().end());

  state->user_installed_allowlist_from_component_updater =
      std::unordered_set<std::string>(
          component_data->user_installed_allowlist().begin(),
          component_data->user_installed_allowlist().end());

  state->kiosk_session_allowlist_from_component_updater =
      std::unordered_set<std::string>(
          component_data->kiosk_session_allowlist().begin(),
          component_data->kiosk_session_allowlist().end());

  state->last_allowlist_component_version = component_version;
  g_load_component_updater_allowlists_complete_for_testing = true;
}

void LoadComponentUpdaterAllowlists(const base::Version& component_version,
                                    const base::FilePath& file_path) {
  auto* state = DeprecationState::GetInstance();
  if (!component_version.IsValid() ||
      !(component_version > state->last_allowlist_component_version)) {
    g_load_component_updater_allowlists_complete_for_testing = true;
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadAllowlistsFromFile, file_path),
      base::BindOnce(&AssignComponentUpdaterAllowlists, component_version));
}
}  // namespace

ScopedSkipSystemDialogForTesting::ScopedSkipSystemDialogForTesting() {
  g_skip_system_dialog_for_testing = true;
}

ScopedSkipSystemDialogForTesting::~ScopedSkipSystemDialogForTesting() {
  g_skip_system_dialog_for_testing = false;
}

ScopedAddAppToAllowlistForTesting::ScopedAddAppToAllowlistForTesting(
    std::string app_id)
    : app_id_(std::move(app_id)) {
  auto* state = DeprecationState::GetInstance();

  CHECK(!state->test_allowlisted_apps.contains(app_id_));
  state->test_allowlisted_apps.emplace(app_id_);
}

ScopedAddAppToAllowlistForTesting::~ScopedAddAppToAllowlistForTesting() {
  auto* state = DeprecationState::GetInstance();
  state->test_allowlisted_apps.erase(app_id_);
  CHECK(!state->test_allowlisted_apps.contains(app_id_));
}

DeprecationStatus HandleDeprecation(std::string_view app_id, Profile* profile) {
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id.data());

  if (!app || !app->is_app()) {
    ReportMetric(DeprecationCheckOutcome::kAllowedNotChromeApp);
    return DeprecationStatus::kLaunchAllowed;
  }

  if (chromeos::IsKioskSession() || g_fake_kiosk_session_for_testing) {
    return HandleKioskSessionApp(*app, profile);
  }

  if (IsUserInstalled(app_id, profile)) {
    return HandleUserInstalledApp(*app, profile);
  }

  ReportMetric(DeprecationCheckOutcome::kAllowedDefault);
  return DeprecationStatus::kLaunchAllowed;
}

void RegisterAllowlistComponentUpdater(
    component_updater::ComponentUpdateService* cus) {
  if (!base::FeatureList::IsEnabled(kChromeAppsDeprecationComponentUpdater)) {
    return;
  }

  base::MakeRefCounted<component_updater::ComponentInstaller>(
      std::make_unique<
          component_updater::
              ChromeAppsDeprecationAllowlistComponentInstallerPolicy>(
          base::BindRepeating(&LoadComponentUpdaterAllowlists)),
      /*action_handler=*/nullptr, base::TaskPriority::BEST_EFFORT)
      ->Register(cus, base::DoNothing());
}

void SetKioskSessionForTesting(bool value) {
  g_fake_kiosk_session_for_testing = value;
}

void AssignComponentUpdaterAllowlistsForTesting(
    const base::Version& component_version,
    std::optional<const ChromeAppDeprecation::DynamicAllowlists>
        component_data) {
  AssignComponentUpdaterAllowlists(component_version,
                                   component_data);  // IN-TEST
}

void LoadComponentUpdaterAllowlistsForTesting(
    const base::Version& component_version,
    const base::FilePath& file_path) {
  LoadComponentUpdaterAllowlists(component_version, file_path);  // IN-TEST
}

bool g_load_component_updater_allowlists_complete_for_testing;

base::Version GetLastAllowlistComponentVersionForTesting() {
  auto* state = DeprecationState::GetInstance();
  return state->last_allowlist_component_version;
}

}  // namespace apps::chrome_app_deprecation
