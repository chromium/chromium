// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/extensions_permissions_tracker.h"

#include "base/containers/fixed_flat_set.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/manifest_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

namespace {

// Apps/extensions explicitly allowlisted for skipping warnings for MGS (Managed
// guest sessions) users.
constexpr auto kManagedGuestSessionAllowlist = base::MakeFixedFlatSet<
    std::string_view>({
    // Managed guest sessions in general:
    "cbkkbcmdlboombapidmoeolnmdacpkch",  // Chrome RDP
    "inomeogfingihgjfjlpeplalcfajhgai",  // Chrome Remote Desktop
    "djflhoibgkdhkhhcedjiklpkjnoahfmg",  // User Agent Switcher
    "iabmpiboiopbgfabjmgeedhcmjenhbla",  // VNC Viewer
    "haiffjcadagjlijoggckpgfnoeiflnem",  // Citrix Receiver
    "lfnfbcjdepjffcaiagkdmlmiipelnfbb",  // Citrix Receiver (branded)
    "mfaihdlpglflfgpfjcifdjdjcckigekc",  // ARC Runtime
    "ngjnkanfphagcaokhjecbgkboelgfcnf",  // Print button
    "cjanmonomjogheabiocdamfpknlpdehm",  // HP printer driver
    "ioofdkhojeeimmagbjbknkejkgbphdfl",  // RICOH Print for Chrome
    "pmnllmkmjilbojkpgplbdmckghmaocjh",  // Scan app by François Beaufort
    "haeblkpifdemlfnkogkipmghfcbonief",  // Charismathics Smart Card Middleware
    "mpnkhdpphjiihmlmkcamhpogecnnfffa",  // Service NSW Kiosk Utility
    "npilppbicblkkgjfnbmibmhhgjhobpll",  // QwickACCESS
    // TODO(isandrk): Only on the allowlist for the purpose of getting the soft
    // MGS warning.  Remove once dynamic MGS warnings are implemented.
    "ppkfnjlimknmjoaemnpidmdlfchhehel",  // VMware Horizon Client for Chrome

    // Libraries:
    "aclofikceldphonlfmghmimkodjdmhck",  // Ancoris login component
    "eilbnahdgoddoedakcmfkcgfoegeloil",  // Ancoris proxy component
    "ceehlgckkmkaoggdnjhibffkphfnphmg",  // Libdata login
    "fnhgfoccpcjdnjcobejogdnlnidceemb",  // OverDrive

    // Education:
    "cmeclblmdmffdgpdlifgepjddoplmmal",  //  Imagine Learning

    // Retail mode:
    "bjfeaefhaooblkndnoabbkkkenknkemb",  // 500 px demo
    "ehcabepphndocfmgbdkbjibfodelmpbb",  // Angry Birds demo
    "kgimkbnclbekdkabkpjhpakhhalfanda",  // Bejeweled demo
    "joodangkbfjnajiiifokapkpmhfnpleo",  // Calculator
    "fpgfohogebplgnamlafljlcidjedbdeb",  // Calendar demo
    "jkoildpomkimndcphjpffmephmcmkfhn",  // Chromebook Demo App
    "lbhdhapagjhalobandnbdnmblnmocojh",  // Crackle demo
    "ielkookhdphmgbipcfmafkaiagademfp",  // Custom bookmarks
    "kogjlbfgggambihdjcpijgcbmenblimd",  // Custom bookmarks
    "ogbkmlkceflgpilgbmbcfbifckpkfacf",  // Custom bookmarks
    "pbbbjjecobhljkkcenlakfnkmkfkfamd",  // Custom bookmarks
    "jkbfjmnjcdmhlfpephomoiipbhcoiffb",  // Custom bookmarks
    "dgmblbpgafgcgpkoiilhjifindhinmai",  // Custom bookmarks
    "iggnealjakkgfofealilhkkclnbnfnmo",  // Custom bookmarks
    "lplkobnahgbopmpkdapaihnnojkphahc",  // Custom bookmarks
    "lejnflfhjpcannpaghnahbedlabpmhoh",  // Custom bookmarks
    "dhjmfhojkfjmfbnbnpichdmcdghdpccg",  // Cut the Rope demo
    "ebkhfdfghngbimnpgelagnfacdafhaba",  // Deezer demo
    "npnjdccdffhdndcbeappiamcehbhjibf",  // Docs.app demo
    "ekgadegabdkcbkodfbgidncffijbghhl",  // Duolingo demo
    "iddohohhpmajlkbejjjcfednjnhlnenk",  // Evernote demo
    "bjdhhokmhgelphffoafoejjmlfblpdha",  // Gmail demo
    "nldmakcnfaflagmohifhcihkfgcbmhph",  // Gmail offline demo
    "mdhnphfgagkpdhndljccoackjjhghlif",  // Google Drive demo
    "dondgdlndnpianbklfnehgdhkickdjck",  // Google Keep demo
    "amfoiggnkefambnaaphodjdmdooiinna",  // Google Play Movie and TV demo
    "fgjnkhlabjcaajddbaenilcmpcidahll",  // Google+ demo
    "ifpkhncdnjfipfjlhfidljjffdgklanh",  // Google+ Photos demo
    "cgmlfbhkckbedohgdepgbkflommbfkep",  // Hangouts.app demo
    "ndlgnmfmgpdecjgehbcejboifbbmlkhp",  // Hash demo
    "edhhaiphkklkcfcbnlbpbiepchnkgkpn",  // Helper.extension demo
    "jckncghadoodfbbbmbpldacojkooophh",  // Journal demo
    "diehajhcjifpahdplfdkhiboknagmfii",  // Kindle demo
    "idneggepppginmaklfbaniklagjghpio",  // Kingsroad demo
    "nhpmmldpbfjofkipjaieeomhnmcgihfm",  // Menu.app demo
    "kcjbmmhccecjokfmckhddpmghepcnidb",  // Mint demo
    "onbhgdmifjebcabplolilidlpgeknifi",  // Music.app demo
    "kkkbcoabfhgekpnddfkaphobhinociem",  // Netflix demo
    "adlphlfdhhjenpgimjochcpelbijkich",  // New York Times demo
    "cgefhjmlaifaamhhoojmpcnihlbddeki",  // Pandora demo
    "kpjjigggmcjinapdeipapdcnmnjealll",  // Pixlr demo
    "ifnadhpngkodeccijnalokiabanejfgm",  // Pixsta demo
    "klcojgagjmpgmffcildkgbfmfffncpcd",  // Plex demo
    "nnikmgjhdlphciaonjmoppfckbpoinnb",  // Pocket demo
    "khldngaiohpnnoikfmnmfnebecgeobep",  // Polarr Photo demo
    "aleodiobpjillgfjdkblghiiaegggmcm",  // Quickoffice demo
    "nifkmgcdokhkjghdlgflonppnefddien",  // Sheets demo
    "hdmobeajeoanbanmdlabnbnlopepchip",  // Slides demo
    "ikmidginfdcbojdbmejkeakncgdbmonc",  // Soundtrap demo
    "dgohlccohkojjgkkfholmobjjoledflp",  // Spotify demo
    "dhmdaeekeihmajjnmichlhiffffdbpde",  // Store.app demo
    "onklhlmbpfnmgmelakhgehkfdmkpmekd",  // Todoist demo
    "jeabmjjifhfcejonjjhccaeigpnnjaak",  // TweetDeck demo
    "gnckahkflocidcgjbeheneogeflpjien",  // Vine demo
    "pdckcbpciaaicoomipamcabpdadhofgh",  // Weatherbug demo
    "biliocemfcghhioihldfdmkkhnofcgmb",  // Webcam Toy demo
    "bhfoghflalnnjfcfkaelngenjgjjhapk",  // Wevideo demo
    "pjckdjlmdcofkkkocnmhcbehkiapalho",  // Wunderlist demo
    "pbdihpaifchmclcmkfdgffnnpfbobefh",  // YouTube demo

    // New demo mode:
    "lpmakjfjcconjeehbidjclhdlpjmfjjj",  // Highlights app
    "iggildboghmjpbjcpmobahnkmoefkike",  // Highlights app (eve)
    "elhbopodaklenjkeihkdhhfaghalllba",  // Highlights app (nocturne)
    "gjeelkjnolfmhphfhhjokaijbicopfln",  // Highlights app (other)
    "mnoijifedipmbjaoekhadjcijipaijjc",  // Screensaver
    "gdobaoeekhiklaljmhladjfdfkigampc",  // Screensaver (eve)
    "lminefdanffajachfahfpmphfkhahcnj",  // Screensaver (nocturne)
    "fafhbhdboeiciklpkminlncemohljlkj",  // Screensaver (kukui)
    "bnabjkecnachpogjlfilfcnlpcmacglh",  // Screensaver (other)

    // Testing extensions:
    "ongnjlefhnoajpbodoldndkbkdgfomlp",  // Show Managed Storage
    "ilnpadgckeacioehlommkaafedibdeob",  // Enterprise DeviceAttributes
    "oflckobdemeldmjddmlbaiaookhhcngo",  // Citrix Receiver QA version
    "behllobkkfkfnphdnhnkndlbkcpglgmj",  // Autotest

    // Google Apps:
    "mclkkofklkfljcocdinagocijmpgbhab",  // Google input tools
    "gbkeegbaiigmenfmjfclcdgdpimamgkj",  // Office Editing Docs/Sheets/Slides
    "aapbdbdomjkkjkaonfhkkikfgjllcleb",  // Google Translate
    "mgijmajocgfcbeboacabfgobmjgjcoja",  // Google Dictionary
    "mfhehppjhmmnlfbbopchdfldgimhfhfk",  // Google Classroom
    "mkaakpdehdafacodkgkpghoibnmamcme",  // Google Drawings
    "pnhechapfaindjhompbnflcldabbghjo",  // Secure Shell
    "fcgckldmmjdbpdejkclmfnnnehhocbfp",  // Google Finance
    "jhknlonaankphkkbnmjdlpehkinifeeg",  // Google Forms
    "jndclpdbaamdhonoechobihbbiimdgai",  // Chromebook Recovery Utility
    "aohghmighlieiainnegkcijnfilokake",  // Google Docs
    "eemlkeanncmjljgehlbplemhmdmalhdc",  // Chrome Connectivity Diagnostics
    "eoieeedlomnegifmaghhjnghhmcldobl",  // Google Apps Script
    "ndjpildffkeodjdaeebdhnncfhopkajk",  // Network File Share for Chrome OS
    "pfoeakahkgllhkommkfeehmkfcloagkl",  // Fusion Tables
    "aapocclcgogkmnckokdopfmhonfmgoek",  // Google Slides
    "khpfeaanjngmcnplbdlpegiifgpfgdco",  // Smart Card Connector
    "hmjkmjkepdijhoojdojkdfohbdgmmhki",  // Google Keep - notes and lists
    "felcaaldnbdncclmgdcncolpebgiejap",  // Google Sheets
    "khkjfddibboofomnlkndfedpoccieiee",  // Study Kit
    "becloognjehhioodmnimnehjcibkloed",  // Coding with Chrome
    "hfhhnacclhffhdffklopdkcgdhifgngh",  // Camera
    "adokjfanaflbkibffcbhihgihpgijcei",  // Share to Classroom
    "heildphpnddilhkemkielfhnkaagiabh",  // Legacy Browser Support
    "lpcaedmchfhocbbapmcbpinfpgnhiddi",  // Google Keep Chrome Extension
    "ldipcbpaocekfooobnbcddclnhejkcpn",  // Google Scholar Button
    "nnckehldicaciogcbchegobnafnjkcne",  // Google Tone
    "pfmgfdlgomnbgkofeojodiodmgpgmkac",  // Data Saver
    "djcfdncoelnlbldjfhinnjlhdjlikmph",  // High Contrast
    "ipkjmjaledkapilfdigkgfmpekpfnkih",  // Color Enhancer
    "kcnhkahnjcbndmmehfkdnkjomaanaooo",  // Google Voice
    "nlbjncdgjeocebhnmkbbbdekmmmcbfjd",  // RSS Subscription Extension
    "aoggjnmghgmcllfenalipjhmooomfdce",  // SAML SSO for Chrome Apps
    "fhndealchbngfhdoncgcokameljahhog",  // Certificate Enrollment for Chrome OS
    "npeicpdbkakmehahjeeohfdhnlpdklia",  // WebRTC Network Limiter
    "hdkoikmfpncabbdniojdddokkomafcci",  // SSRS Reporting Fix for Chrome
});

}  // namespace

bool IsAllowlistedForManagedGuestSession(const std::string& extension_id) {
  return kManagedGuestSessionAllowlist.contains(extension_id);
}

ExtensionsPermissionsTracker::ExtensionsPermissionsTracker(
    ExtensionRegistry* registry,
    content::BrowserContext* browser_context)
    : registry_(registry),
      pref_service_(Profile::FromBrowserContext(browser_context)->GetPrefs()) {
  observation_.Observe(registry_.get());
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      pref_names::kInstallForceList,
      base::BindRepeating(
          &ExtensionsPermissionsTracker::OnForcedExtensionsPrefChanged,
          base::Unretained(
              this)));  // Safe as ExtensionsPermissionsTracker
                        // owns pref_change_registrar_ & outlives it
  // Try to load list now.
  OnForcedExtensionsPrefChanged();
}

ExtensionsPermissionsTracker::~ExtensionsPermissionsTracker() = default;

void ExtensionsPermissionsTracker::OnForcedExtensionsPrefChanged() {
  // TODO(crbug.com/40103683): handle pref_names::kExtensionManagement with
  // installation_mode: forced.
  const base::Value& value =
      pref_service_->GetValue(pref_names::kInstallForceList);
  if (!value.is_dict()) {
    return;
  }

  extension_safety_ratings_.clear();
  pending_forced_extensions_.clear();

  for (const auto entry : value.GetDict()) {
    const ExtensionId& extension_id = entry.first;
    // By default the extension permissions are assumed to trigger full warning
    // (false). When the extension is loaded, if all of its permissions is safe,
    // it'll be marked safe (true)
    extension_safety_ratings_.insert(make_pair(extension_id, false));
    const Extension* extension =
        registry_->enabled_extensions().GetByID(extension_id);
    if (extension)
      ParseExtensionPermissions(extension);
    else
      pending_forced_extensions_.insert(extension_id);
  }
  if (pending_forced_extensions_.empty())
    UpdateLocalState();
}

bool ExtensionsPermissionsTracker::IsSafePerms(
    const PermissionsData* perms_data) const {
  const PermissionSet& active_permissions = perms_data->active_permissions();
  const APIPermissionSet& api_permissions = active_permissions.apis();
  for (auto* permission : api_permissions) {
    if (permission->info()->requires_managed_session_full_login_warning()) {
      return false;
    }
  }
  const ManifestPermissionSet& manifest_permissions =
      active_permissions.manifest_permissions();
  for (const auto* permission : manifest_permissions) {
    if (permission->RequiresManagedSessionFullLoginWarning()) {
      return false;
    }
  }
  if (active_permissions.ShouldWarnAllHosts() ||
      !active_permissions.effective_hosts().is_empty()) {
    return false;
  }

  return true;
}

void ExtensionsPermissionsTracker::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  auto itr = extension_safety_ratings_.find(extension->id());
  if (itr == extension_safety_ratings_.end())
    return;
  pending_forced_extensions_.erase(extension->id());

  ParseExtensionPermissions(extension);

  // If the extension isn't safe or all extensions are loaded, update the local
  // state.
  if (!itr->second || pending_forced_extensions_.empty())
    UpdateLocalState();
}

void ExtensionsPermissionsTracker::UpdateLocalState() {
  bool any_unsafe = base::ranges::any_of(
      extension_safety_ratings_,
      [](const auto& key_value) { return !key_value.second; });

  DCHECK(pending_forced_extensions_.empty() || any_unsafe);

  g_browser_process->local_state()->SetBoolean(
      prefs::kManagedSessionUseFullLoginWarning, any_unsafe);
}

// static
void ExtensionsPermissionsTracker::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kManagedSessionUseFullLoginWarning,
                                true);
}

void ExtensionsPermissionsTracker::ParseExtensionPermissions(
    const Extension* extension) {
  extension_safety_ratings_[extension->id()] =
      IsAllowlistedForManagedGuestSession(extension->id()) ||
      IsSafePerms(extension->permissions_data());
}

}  // namespace extensions
