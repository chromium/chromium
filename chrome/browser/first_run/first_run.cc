// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/one_shot_event.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/importer/importer_uma.h"
#include "chrome/browser/importer/profile_writer.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

using base::UserMetricsAction;

namespace {

// A bitfield formed from values in AutoImportState to record the state of
// AutoImport. This is used in testing to verify import startup actions that
// occur before an observer can be registered in the test.
uint16_t g_auto_import_state = first_run::AUTO_IMPORT_NONE;

// Flags for functions of similar name.
bool g_should_show_welcome_page = false;
bool g_should_do_autofill_personal_data_manager_first_run = false;

// Indicates whether this is first run. Populated when IsChromeFirstRun
// is invoked, then used as a cache on subsequent calls.
first_run::internal::FirstRunState g_first_run =
    first_run::internal::FIRST_RUN_UNKNOWN;

// Cached first run sentinel creation time.
// Used to avoid excess file operations.
base::Time g_cached_sentinel_creation_time;

// This class acts as an observer for the ImporterProgressObserver::ImportEnded
// callback. When the import process is started, certain errors may cause
// ImportEnded() to be called synchronously, but the typical case is that
// ImportEnded() is called asynchronously. Thus we have to handle both cases.
class ImportEndedObserver : public importer::ImporterProgressObserver {
 public:
  ImportEndedObserver() : ended_(false) {}
  ~ImportEndedObserver() override {}

  // importer::ImporterProgressObserver:
  void ImportStarted() override {}
  void ImportItemStarted(importer::ImportItem item) override {}
  void ImportItemEnded(importer::ImportItem item) override {}
  void ImportEnded() override {
    ended_ = true;
    if (!callback_for_import_end_.is_null())
      callback_for_import_end_.Run();
  }

  void set_callback_for_import_end(const base::Closure& callback) {
    callback_for_import_end_ = callback;
  }

  bool ended() const {
    return ended_;
  }

 private:
  // Set if the import has ended.
  bool ended_;

  base::Closure callback_for_import_end_;

  DISALLOW_COPY_AND_ASSIGN(ImportEndedObserver);
};

// Launches the import, via |importer_host|, from |source_profile| into
// |target_profile| for the items specified in the |items_to_import| bitfield.
// This may be done in a separate process depending on the platform, but it will
// always block until done.
void ImportFromSourceProfile(const importer::SourceProfile& source_profile,
                             Profile* target_profile,
                             uint16_t items_to_import) {
  // Deletes itself.
  ExternalProcessImporterHost* importer_host =
      new ExternalProcessImporterHost;
  // Don't show the warning dialog if import fails.
  importer_host->set_headless();

  ImportEndedObserver observer;
  importer_host->set_observer(&observer);
  importer_host->StartImportSettings(source_profile,
                                     target_profile,
                                     items_to_import,
                                     new ProfileWriter(target_profile));
  // If the import process has not errored out, block on it.
  if (!observer.ended()) {
    base::RunLoop loop;
    observer.set_callback_for_import_end(loop.QuitClosure());
    loop.Run();
    observer.set_callback_for_import_end(base::Closure());
  }
}

// Imports bookmarks from an html file whose path is provided by
// |import_bookmarks_path|.
void ImportFromFile(Profile* profile,
                    const std::string& import_bookmarks_path) {
  importer::SourceProfile source_profile;
  source_profile.importer_type = importer::TYPE_BOOKMARKS_FILE;

  const base::FilePath::StringType& import_bookmarks_path_str =
#if defined(OS_WIN)
      base::UTF8ToUTF16(import_bookmarks_path);
#else
      import_bookmarks_path;
#endif
  source_profile.source_path = base::FilePath(import_bookmarks_path_str);

  ImportFromSourceProfile(source_profile, profile, importer::FAVORITES);
  g_auto_import_state |= first_run::AUTO_IMPORT_BOOKMARKS_FILE_IMPORTED;
}

// Imports settings from the first profile in |importer_list|.
void ImportSettings(Profile* profile,
                    std::unique_ptr<ImporterList> importer_list,
                    uint16_t items_to_import) {
  DCHECK(items_to_import);
  const importer::SourceProfile& source_profile =
      importer_list->GetSourceProfileAt(0);

  // Ensure that importers aren't requested to import items that they do not
  // support. If there is no overlap, skip.
  items_to_import &= source_profile.services_supported;
  if (items_to_import)
    ImportFromSourceProfile(source_profile, profile, items_to_import);

  g_auto_import_state |= first_run::AUTO_IMPORT_PROFILE_IMPORTED;
}

GURL UrlFromString(const std::string& in) {
  return GURL(in);
}

void ConvertStringVectorToGURLVector(
    const std::vector<std::string>& src,
    std::vector<GURL>* ret) {
  ret->resize(src.size());
  std::transform(src.begin(), src.end(), ret->begin(), &UrlFromString);
}

base::FilePath& GetMasterPrefsPathForTesting() {
  static base::NoDestructor<base::FilePath> s;
  return *s;
}

// Makes chrome the user's default browser according to policy or
// |make_chrome_default_for_user| if no policy is set.
void ProcessDefaultBrowserPolicy(bool make_chrome_default_for_user) {
  // Only proceed if chrome can be made default unattended. In other cases, this
  // is handled by the first run default browser prompt (on Windows 8+).
  if (shell_integration::GetDefaultWebClientSetPermission() ==
      shell_integration::SET_DEFAULT_UNATTENDED) {
    // The policy has precedence over the user's choice.
    if (g_browser_process->local_state()->IsManagedPreference(
            prefs::kDefaultBrowserSettingEnabled)) {
      if (g_browser_process->local_state()->GetBoolean(
          prefs::kDefaultBrowserSettingEnabled)) {
        shell_integration::SetAsDefaultBrowser();
      }
    } else if (make_chrome_default_for_user) {
      shell_integration::SetAsDefaultBrowser();
    }
  }
}

// Reads the creation time of the first run sentinel file. If the first run
// sentinel file does not exist, it will return base::Time().
base::Time ReadFirstRunSentinelCreationTime() {
  base::Time first_run_sentinel_creation_time = base::Time();
  base::FilePath first_run_sentinel;
  if (first_run::internal::GetFirstRunSentinelFilePath(&first_run_sentinel)) {
    base::File::Info info;
    if (base::GetFileInfo(first_run_sentinel, &info))
      first_run_sentinel_creation_time = info.creation_time;
  }
  return first_run_sentinel_creation_time;
}

}  // namespace

namespace first_run {
namespace internal {

void SetupMasterPrefsFromInstallPrefs(
    const installer::MasterPreferences& install_prefs,
    MasterPrefs* out_prefs) {
  ConvertStringVectorToGURLVector(
      install_prefs.GetFirstRunTabs(), &out_prefs->new_tabs);

  bool value = false;
  if (install_prefs.GetBool(
          installer::master_preferences::kMakeChromeDefaultForUser,
          &value) && value) {
    out_prefs->make_chrome_default_for_user = true;
  }

  install_prefs.GetString(
      installer::master_preferences::kDistroImportBookmarksFromFilePref,
      &out_prefs->import_bookmarks_path);

  install_prefs.GetString(
      installer::master_preferences::kDistroSuppressDefaultBrowserPromptPref,
      &out_prefs->suppress_default_browser_prompt_for_version);
}

bool GetFirstRunSentinelFilePath(base::FilePath* path) {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return false;
  *path = user_data_dir.Append(chrome::kFirstRunSentinel);
  return true;
}

bool CreateSentinel() {
  base::FilePath first_run_sentinel;
  return GetFirstRunSentinelFilePath(&first_run_sentinel) &&
      base::WriteFile(first_run_sentinel, "", 0) != -1;
}

// -- Platform-specific functions --

#if !defined(OS_LINUX) && !defined(OS_BSD)
bool IsOrganicFirstRun() {
  std::string brand;
  google_brand::GetBrand(&brand);
  return google_brand::IsOrganicFirstRun(brand);
}
#endif

FirstRunState DetermineFirstRunState(bool has_sentinel,
                                     bool force_first_run,
                                     bool no_first_run) {
  return (force_first_run || (!has_sentinel && !no_first_run))
             ? FIRST_RUN_TRUE
             : FIRST_RUN_FALSE;
}

}  // namespace internal

MasterPrefs::MasterPrefs() = default;

MasterPrefs::~MasterPrefs() = default;

void RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kImportAutofillFormData, false);
  registry->RegisterBooleanPref(prefs::kImportBookmarks, false);
  registry->RegisterBooleanPref(prefs::kImportHistory, false);
  registry->RegisterBooleanPref(prefs::kImportHomepage, false);
  registry->RegisterBooleanPref(prefs::kImportSavedPasswords, false);
  registry->RegisterBooleanPref(prefs::kImportSearchEngine, false);
}

bool IsChromeFirstRun() {
  if (g_first_run == internal::FIRST_RUN_UNKNOWN) {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    g_first_run = internal::DetermineFirstRunState(
        internal::IsFirstRunSentinelPresent(),
        command_line->HasSwitch(switches::kForceFirstRun),
        command_line->HasSwitch(switches::kNoFirstRun));
  }
  return g_first_run == internal::FIRST_RUN_TRUE;
}

#if defined(OS_MACOSX)
bool IsFirstRunSuppressed(const base::CommandLine& command_line) {
  return command_line.HasSwitch(switches::kNoFirstRun);
}
#endif

bool IsMetricsReportingOptIn() {
  // Metrics reporting is opt-out by default for all platforms and channels.
  // However, user will have chance to modify metrics reporting state during
  // first run.
  return false;
}

void CreateSentinelIfNeeded() {
  if (IsChromeFirstRun())
    internal::CreateSentinel();

  // Causes the first run sentinel creation time to be read and cached, while
  // I/O is still allowed.
  ignore_result(GetFirstRunSentinelCreationTime());
}

base::Time GetFirstRunSentinelCreationTime() {
  if (g_cached_sentinel_creation_time.is_null())
    g_cached_sentinel_creation_time = ReadFirstRunSentinelCreationTime();
  return g_cached_sentinel_creation_time;
}

void ResetCachedSentinelDataForTesting() {
  g_cached_sentinel_creation_time = base::Time();
  g_first_run = first_run::internal::FIRST_RUN_UNKNOWN;
}

void SetShouldShowWelcomePage() {
  g_should_show_welcome_page = true;
}

bool ShouldShowWelcomePage() {
  bool retval = g_should_show_welcome_page;
  g_should_show_welcome_page = false;
  return retval;
}

bool IsOnWelcomePage(content::WebContents* contents) {
  return contents->GetURL().GetWithEmptyPath() ==
         GURL(chrome::kChromeUIWelcomeURL);
}

void SetShouldDoPersonalDataManagerFirstRun() {
  g_should_do_autofill_personal_data_manager_first_run = true;
}

bool ShouldDoPersonalDataManagerFirstRun() {
  bool retval = g_should_do_autofill_personal_data_manager_first_run;
  g_should_do_autofill_personal_data_manager_first_run = false;
  return retval;
}

void SetMasterPrefsPathForTesting(const base::FilePath& master_prefs) {
  GetMasterPrefsPathForTesting() = master_prefs;
}

std::unique_ptr<installer::MasterPreferences> LoadMasterPrefs() {
  base::FilePath master_prefs_path;
  if (!GetMasterPrefsPathForTesting().empty())
    master_prefs_path = GetMasterPrefsPathForTesting();
  else
    master_prefs_path = base::FilePath(first_run::internal::MasterPrefsPath());

  if (master_prefs_path.empty())
    return nullptr;
  auto install_prefs =
      std::make_unique<installer::MasterPreferences>(master_prefs_path);
  if (!install_prefs->read_from_file())
    return nullptr;
  return install_prefs;
}

ProcessMasterPreferencesResult ProcessMasterPreferences(
    const base::FilePath& user_data_dir,
    std::unique_ptr<installer::MasterPreferences> install_prefs,
    MasterPrefs* out_prefs) {
  DCHECK(!user_data_dir.empty());

  if (install_prefs.get()) {
    if (!internal::ShowPostInstallEULAIfNeeded(install_prefs.get()))
      return EULA_EXIT_NOW;

    std::unique_ptr<base::DictionaryValue> master_dictionary =
        install_prefs->master_dictionary().CreateDeepCopy();
    // The distribution dictionary (and any prefs below it) are never registered
    // for use in Chrome's PrefService. Strip them from the master dictionary
    // before mapping it to prefs.
    master_dictionary->RemoveWithoutPathExpansion(
        installer::master_preferences::kDistroDict, nullptr);

    if (!chrome_prefs::InitializePrefsFromMasterPrefs(
            profiles::GetDefaultProfileDir(user_data_dir),
            std::move(master_dictionary))) {
      DLOG(ERROR) << "Failed to initialize from master_preferences.";
    }

    base::DictionaryValue* extensions = 0;
    if (install_prefs->GetExtensionsBlock(&extensions)) {
      DVLOG(1) << "Extensions block found in master preferences";
      extensions::ExtensionUpdater::UpdateImmediatelyForFirstRun();
    }

    internal::SetupMasterPrefsFromInstallPrefs(*install_prefs, out_prefs);
  }

  return FIRST_RUN_PROCEED;
}

void AutoImport(
    Profile* profile,
    const std::string& import_bookmarks_path) {
  g_auto_import_state |= AUTO_IMPORT_CALLED;

  // Use |profile|'s PrefService to determine what to import. It will reflect in
  // order:
  //  1) Policies.
  //  2) Master preferences (used to initialize user prefs in
  //     ProcessMasterPreferences()).
  //  3) Recommended policies.
  //  4) Registered default.
  PrefService* prefs = profile->GetPrefs();
  uint16_t items_to_import = 0;
  static constexpr struct {
    const char* pref_path;
    importer::ImportItem bit;
  } kImportItems[] = {
      {prefs::kImportAutofillFormData, importer::AUTOFILL_FORM_DATA},
      {prefs::kImportBookmarks, importer::FAVORITES},
      {prefs::kImportHistory, importer::HISTORY},
      {prefs::kImportHomepage, importer::HOME_PAGE},
      {prefs::kImportSavedPasswords, importer::PASSWORDS},
      {prefs::kImportSearchEngine, importer::SEARCH_ENGINES},
  };

  for (const auto& import_item : kImportItems) {
    if (prefs->GetBoolean(import_item.pref_path))
      items_to_import |= import_item.bit;
  }

  if (items_to_import) {
    // It may be possible to do the if block below asynchronously. In which
    // case, get rid of this RunLoop. http://crbug.com/366116.
    base::RunLoop run_loop;
    auto importer_list = std::make_unique<ImporterList>();
    importer_list->DetectSourceProfiles(
        g_browser_process->GetApplicationLocale(),
        false,  // include_interactive_profiles?
        run_loop.QuitClosure());
    run_loop.Run();

    if (importer_list->count() > 0) {
      importer::LogImporterUseToMetrics(
          "AutoImport", importer_list->GetSourceProfileAt(0).importer_type);

      ImportSettings(profile, std::move(importer_list), items_to_import);
    }
  }

  if (!import_bookmarks_path.empty())
    ImportFromFile(profile, import_bookmarks_path);
}

void DoPostImportTasks(Profile* profile, bool make_chrome_default_for_user) {
  // Only set default browser after import as auto import relies on the current
  // default browser to know what to import from.
  ProcessDefaultBrowserPolicy(make_chrome_default_for_user);

  SetShouldShowWelcomePage();
  SetShouldDoPersonalDataManagerFirstRun();

  internal::DoPostImportPlatformSpecificTasks(profile);
}

uint16_t auto_import_state() {
  return g_auto_import_state;
}

}  // namespace first_run
