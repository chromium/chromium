// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/chrome_app_deprecation_controller.h"

#include "ash/public/cpp/system_notification_builder.h"
#include "base/containers/fixed_flat_set.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace apps::chrome_app_deprecation {

namespace {
constexpr auto user_installed_and_kiosk = base::MakeFixedFlatSet<
    std::string_view>(
    {"aakfkoilmhehmmadlkedfbcelkbamdkj", "aepgaekjheajlcifmpjcnpbjcencoefn",
     "afoipjmffplafpbfjopglheidddioiai", "afpnehpifljbjjplppeplamalioanmio",
     "anjihnbmjbbpofafpmklejenkgnjfcdi", "aoijoapjiidlaapoinclpjkmpaeckiff",
     "aphendncpdekdkepekckjkiloclamieb", "baifnloidiaigliddpkifgokjemcbcei",
     "bajigdlccokpmeadnhpfhpehdefbgaen", "bbkieeoaobjflkeakhemifofdbbfhnic",
     "bhfbomkadeplbpgfmiihpglmenahkmao", "bikbageiaongkigeijiahadjbcgindbj",
     "bnkchehofckdmggiknjidlamlpokbodf", "bpmgmelggoioalpijejanjhbjkfeehbg",
     "cahbpjmendhigemdnlifkfmdhnipbdil", "cajomgbhgfomgakdejohnkomlblhhlmo",
     "cdebpoondplobcgjepkgplleeeeojmpa", "cdgdgmknjolkacdiheibdjmidfkooodf",
     "cedlmaejgblmkmnddjikaagkhbfonihp", "cgpnjolncgemfdgbfokgdbmhpondgjmm",
     "coomdpjcngcbdefihidllngfemgnmlhh", "dcfnglblnliiebcjiffpnecdkjnomjbl",
     "demfodeljeofljmbplgpcncaebjmboog", "demlnppodlnndiacjgbijdjnnnoninak",
     "deokbmklnlnlikckmachjjhgnidefhhg", "dgmhhjhnkhlmooconggnbjhlmpkpliij",
     "djkbhkgnbiknnlinckcclejmjkddokhl", "djobiocnmcaeodjcdhbhjgjndhiadgod",
     "eaghkdkaebflfmmhidgnldnncfpknpne", "ealfhldampafeomimeidejkicmipkgkh",
     "eblkmenpohbbmbelfaggegpjfjokihke", "ecgoodkkapeinahfgidbfknincokmhdg",
     "efadkfcohfppfffgblnflcakfhfdjiig", "ejbidlmioeopgmjieecjihnlgacicoie",
     "ejoilaclhpbfooagcjdkkmklhjipgmll", "ekiflcmfallbndjhecchfcipbaajdfhl",
     "ekigfkofdacepchbgkogfedfapdekjgp", "emejfeljcemojhhcmobdeflgjabpafip",
     "emlbfhdjchamibhjgcokeipljabljheo", "enfpdhommpcbfiojillmflopkkjbcjmf",
     "faidilipbonmepcjdkhjfencfaaccgic", "famkiocmnjimafojaajdngnidmgnacme",
     "fecgcoakonfhepcppcbddeefeoekhbah", "fenegagmedfckampfgjbeoflcpcpdppc",
     "ffhbnjlppmbnhahkbkcjgapgfinabjgb", "fhohelmkloeoheiminpldlhkdfcmjbfm",
     "fjdejbdegplidjpkgcblpdibepibfifg", "fmfiolcdkhopmhgjbmlgpfcpfbeneope",
     "fnbgnnegegboidihpleofgakpegcidim", "fooeehkjmkcohfidagefenolegldgmpp",
     "gbfihfamagomeondkhooeamjajjadpio", "geopjmggmojbcnjlkcnfbgdniomaioif",
     "gfajignjkjbleogeegcgjimnkooihmdm", "ggaabodlngcnbdcpkfacegoacchkalmn",
     "ggddmkhlbkollcjopbnkbbhnikncfena", "gjenjmcioeobmpllaeopaoibabhgcohi",
     "glcdffonolecglhbodpaeijkhgdfkbon", "gnddkmpjjjcimefninepfmmddpgaaado",
     "gnogkjfeajjnafijfmffnkgenhnkdnfp", "gpgnoonhefbmngkiafpedbligiiekfcp",
     "haiffjcadagjlijoggckpgfnoeiflnem", "hanegekdenjamflmdgcbjlobfkijeblp",
     "hclmbafbgpncekjmadbbcpekilflmkfg", "hgdemhjioannjiccnfgmllghllhpncpm",
     "hginjgofkfbdfpkjcchdklbkkdbigpna", "hhcgnlnhaapiekdelngjichnccjfkbnc",
     "hkmlofdlheebfpgfcmgbdjddnoniccno", "hmpdelcfcndndcoldocpdmakeabbihgb",
     "hnlanngibjpmdolooednhkedmfbdbmhc", "hpdnjcbgolagabfgcgjpicbknmgefakl",
     "hplnogolijklhfbbfogccgickedplpeo", "iedihkacboebiliakaicmedjmajmjiep",
     "ighapdcohmkppihdjdejlbkolhbgnlfm", "ihlmfpkjommgamcgofmdmojpeolimlfe",
     "iiaffmacblgjekhogmghdjfflchkjmmg", "iilndnicahkogiklibnnibmmeikacnfo",
     "iinmojhiolplpndeijdkfoghkokbfadb", "iiopclfeneoimifgocjnhcjpjgaojhho",
     "ijdoledcajbpfbkiafmmimjhmkmdppjo", "ikgemedabaijdochaempgdpfebllgfcc",
     "iknkgipmikbpldmppngljbedofgmanfm", "inaonhfifmcnldmdnlbnfpikjndebkbj",
     "jfhndkehlkceadabhedbcclclbclhnbh", "jgafcpolgeedpieaadaeeaoanackiina",
     "jglaiblkoeelgfdabnhpcpdnodjonclf", "jjkgijommndbjlekbalbbiiidnigcgfl",
     "jjlhmikmcgmheddmlfeckndcedkmcpng", "jjnejapcbafplbdkbombhmmjnafplkon",
     "jjoncgfekjbknjfejfonaochdpdedbka", "jnnkgopblccifpnkfpfkmdafjebjlhcc",
     "jnojnnofimbdpeihiddafgagckdlnlpe", "jpmngkkdajjfkdknhbifjbglkckbklee",
     "kahkblckpdgogkogmfhfnldpjhdpfiia", "kdbdkbbfhghbggpjmpapmobihghkdmkh",
     "kdndmepchimlohdcdkokdddpbnniijoa", "kenkpdjcfppbccchillfdjkjnejjgand",
     "kflikliicodcopdhibchdfaninnhbalf", "kfllildicglifipmhpnlmpfbkdponghk",
     "khpfeaanjngmcnplbdlpegiifgpfgdco", "khplkoflcklpnlofodhlnjeiodbmejoe",
     "kjceddihhogmglodncbmpembbclhnpda", "kljahdaehfmgddhnibkikcjfppjcjjcn",
     "kmfbmibhlikajdfjbddlolmdkkbiephg", "lbfgjakkeeccemhonnolnmglmfmccaag",
     "lemoeliioheohdcoogohonkamhloahbb", "likeoemlchnioaoaklldmcnilhhpjamo",
     "lknebpkncfibkhjkimejlgppnjgemobn", "lmhpnmjggoibofacnookchiemlihmjdd",
     "lnnghenlbgaeloipgjlafjhlccipbpnm", "maegcedffmoidlccpjahiglkaacbncnn",
     "mclaaifjbcglkbdhdkaamamplpjoabih", "mdmkkicfmmkgmpkmkdikhlbggogpicma",
     "medpmkohocjidlghgmnnkpfigfpddaok", "mhbelemjphdecdagmmengimkkiefmcej",
     "mhfhafklkbgalhbdihiccegaldefdigp", "mhjpnpdhahbahbjedoihlganncneknfo",
     "millmignkmpaolllendlllaibmeehohd", "mkjgggeeejocddadcegdhcchhmemokcn",
     "mndakpenoffnhdmpcpnajekhpbonggeo", "mpjaajdhcmmkeikfdgffdpdjncdnmhmk",
     "ncjnakhgkcldedboafigaailhldnellf", "ndlolfeihajiaklmehdnajjoblphkppd",
     "nenolmmehjhaggnamcglapjjdofcojao", "nghoaommfphpdlipedlebgcnmphedhdb",
     "ngiaihbicdcdflfkhilnaaeobnchggkk", "nhebofpemjfflnkmaneaopjickpliokk",
     "njofdhegeeccijokfiijflbfajgjclch", "oanbapfpojpdpjppgcmdhcjehacnccbm",
     "ocnncjgbkiomppnchhbmmcpblifejpco", "odcalbcbcmnepllckjhdndgmolpnddjo",
     "oefoedhdllfdpfpjhhccdiglflemnfdb", "oflckobdemeldmjddmlbaiaookhhcngo",
     "ofmlpkdeaopippomdfamngkpnbagkdem", "ogmfbebknnapidhhefcdgmoafjeblnjo",
     "okaiidkcbkpimeiebofglgpobdafmmeb", "ondpjadajoodngapikdebdcnjcjkeecc",
     "opalidednimmhdfbcpdmoihhpkahgkak", "pdgbdkbnajhamggjjlhlapedeolflpgm",
     "pdpgalakpabfiiadeiimoolhemoleaeg", "pgolnnkmmlpbnhfcfbephcnkooejbcep",
     "pifpopligmljinioeacaccciabhbbpjo", "plhmjahmpikllpphfaoopdhnkbpffccm",
     "pnclfbefcgmenbbbpljbhbdacgkgkjlh", "ppkfnjlimknmjoaemnpidmdlfchhehel"});

// The std::unordered_set<std::string_view> type has complex constructors and
// for static variables it would require an exit-time destructor. For these
// cases go/totw/110 suggests using NoDestructor to prevent the destructor from
// running and avoid multi-thread race conditions. We do not risk memory leaks
// because the allowlist are always valid while Chrome is running.
static base::NoDestructor<std::unordered_set<std::string_view>>
    test_allowlisted_apps;

bool IsAllowlisted(std::string_view app_id) {
  if (test_allowlisted_apps->contains(app_id)) {
    return true;
  }

  return user_installed_and_kiosk.contains(app_id);
}

void ShowNotification(const std::string& app_name, Profile* profile) {
  message_center::Notification notification =
      ash::SystemNotificationBuilder()
          .SetId("Chrome Apps Deprecation")
          .SetCatalogName(ash::NotificationCatalogName::kChromeAppDeprecation)
          .SetTitle(base::ASCIIToUTF16(app_name))
          .SetMessage(l10n_util::GetStringUTF16(
              IDS_USER_INSTALLED_CHROME_APP_DEPRECATION_NOTIFICATION_MESSAGE))
          .SetWarningLevel(
              message_center::SystemNotificationWarningLevel::WARNING)
          .Build(/*keep_timestamp=*/false);

  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      NotificationHandler::Type::ANNOUNCEMENT, notification,
      /*metadata=*/nullptr);
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
}  // namespace

bool IsLaunchAllowed(std::string_view app_id, Profile* profile) {
  const extensions::Extension* app =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id.data());

  if (!app || !app->is_app()) {
    return true;
  }

  if (IsUserInstalled(app_id, profile)) {
    if (IsAllowlisted(app_id)) {
      return true;
    }

    ShowNotification(app->name(), profile);
  }

  return true;
}

void AddAppToAllowlistForTesting(std::string_view app_id) {
  test_allowlisted_apps->emplace(app_id);
}

}  // namespace apps::chrome_app_deprecation
