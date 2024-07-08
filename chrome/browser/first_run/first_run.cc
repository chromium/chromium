// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/one_shot_event.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/headless/headless_mode_util.h"
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
#include "chrome/installer/util/initial_preferences.h"
#include "chrome/installer/util/initial_preferences_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
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

  ImportEndedObserver(const ImportEndedObserver&) = delete;
  ImportEndedObserver& operator=(const ImportEndedObserver&) = delete;

  ~ImportEndedObserver() override {}

  // importer::ImporterProgressObserver:
  void ImportStarted() override {}
  void ImportItemStarted(importer::ImportItem item) override {}
  void ImportItemEnded(importer::ImportItem item) override {}
  void ImportEnded() override {
    ended_ = true;
    if (callback_for_import_end_)
      std::move(callback_for_import_end_).Run();
  }

  void set_callback_for_import_end(base::OnceClosure callback) {
    callback_for_import_end_ = std::move(callback);
  }

  bool ended() const {
    return ended_;
  }

 private:
  // Set if the import has ended.
  bool ended_;

  base::OnceClosure callback_for_import_end_;
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
  }
}

// Imports bookmarks from an html file whose path is provided by
// |import_bookmarks_path|.
void ImportFromFile(Profile* profile,
                    const std::string& import_bookmarks_path) {
  importer::SourceProfile source_profile;
  source_profile.importer_type = importer::TYPE_BOOKMARKS_FILE;

  const base::FilePath::StringType& import_bookmarks_path_str =
#if BUILDFLAG(IS_WIN)
      base::UTF8ToWide(import_bookmarks_path);
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
  base::ranges::transform(src, ret->begin(), &UrlFromString);
}

base::FilePath& GetInitialPrefsPathForTesting() {
  static base::NoDestructor<base::FilePath> s;
  return *s;
}

// Makes chrome the user's default browser according to policy or
// |make_chrome_default_for_user| if no policy is set.
void ProcessDefaultBrowserPolicy(bool make_chrome_default_for_user) {
  // Only proceed if chrome can be made default unattended. In other cases, this
  // is handled by the first run default browser prompt (on Windows 8+).
  if (shell_integration::GetDefaultBrowserSetPermission() ==
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

// Get the file path of the first run sentinel; returns false on failure.
bool GetFirstRunSentinelFilePath(base::FilePath* path) {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return false;
  *path = user_data_dir.Append(chrome::kFirstRunSentinel);
  return true;
}

// Create the first run sentinel file; returns the status of the operation.
startup_metric_utils::FirstRunSentinelCreationResult CreateSentinel() {
  base::FilePath first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel)) {
    return startup_metric_utils::FirstRunSentinelCreationResult::
        kFailedToGetPath;
  }

  if (base::PathExists(first_run_sentinel)) {
    return startup_metric_utils::FirstRunSentinelCreationResult::
        kFilePathExists;
  }

  if (!base::WriteFile(first_run_sentinel, "")) {
    return startup_metric_utils::FirstRunSentinelCreationResult::
        kFileSystemError;
  }

  return startup_metric_utils::FirstRunSentinelCreationResult::kSuccess;
}

// Reads the creation time of the first run sentinel file. If the first run
// sentinel file does not exist, it will return base::Time().
base::Time ReadFirstRunSentinelCreationTime() {
  base::Time first_run_sentinel_creation_time = base::Time();
  base::FilePath first_run_sentinel;
  if (GetFirstRunSentinelFilePath(&first_run_sentinel)) {
    base::File::Info info;
    if (base::GetFileInfo(first_run_sentinel, &info))
      first_run_sentinel_creation_time = info.creation_time;
  }
  return first_run_sentinel_creation_time;
}

// Returns true if the sentinel file exists (or the path cannot be obtained).
bool IsFirstRunSentinelPresent() {
  base::FilePath sentinel;
  return !GetFirstRunSentinelFilePath(&sentinel) || base::PathExists(sentinel);
}

}  // namespace

namespace first_run {
namespace internal {

void SetupInitialPrefsFromInstallPrefs(
    const installer::InitialPreferences& install_prefs,
    MasterPrefs* out_prefs) {
  ConvertStringVectorToGURLVector(
      install_prefs.GetFirstRunTabs(), &out_prefs->new_tabs);

  bool value = false;
  if (install_prefs.GetBool(
          installer::initial_preferences::kMakeChromeDefaultForUser, &value) &&
      value) {
    out_prefs->make_chrome_default_for_user = true;
  }

  install_prefs.GetString(
      installer::initial_preferences::kDistroImportBookmarksFromFilePref,
      &out_prefs->import_bookmarks_path);

  install_prefs.GetString(
      installer::initial_preferences::kDistroSuppressDefaultBrowserPromptPref,
      &out_prefs->suppress_default_browser_prompt_for_version);

#if BUILDFLAG(IS_MAC)
  if (install_prefs.GetBool(prefs::kConfirmToQuitEnabled, &value) && value)
    out_prefs->confirm_to_quit = true;
#endif  // BUILDFLAG(IS_MAC)
}

// -- Platform-specific functions --

#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_BSD)
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
        IsFirstRunSentinelPresent(),
        command_line->HasSwitch(switches::kForceFirstRun),
        command_line->HasSwitch(switches::kNoFirstRun));
  }
  return g_first_run == internal::FIRST_RUN_TRUE;
}

#if BUILDFLAG(IS_MAC)
bool IsFirstRunSuppressed(const base::CommandLine& command_line) {
  return command_line.HasSwitch(switches::kNoFirstRun);
}
#endif

void CreateSentinelIfNeeded() {
  if (IsChromeFirstRun()) {
    auto sentinel_creation_result = CreateSentinel();
    startup_metric_utils::GetBrowser().RecordFirstRunSentinelCreation(
        sentinel_creation_result);
  }

  // Causes the first run sentinel creation time to be read and cached, while
  // I/O is still allowed.
  std::ignore = GetFirstRunSentinelCreationTime();
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

void SetInitialPrefsPathForTesting(const base::FilePath& initial_prefs) {
  GetInitialPrefsPathForTesting() = initial_prefs;
}

std::unique_ptr<installer::InitialPreferences> LoadInitialPrefs() {
  base::FilePath initial_prefs_path;
  if (!GetInitialPrefsPathForTesting().empty()) {
    initial_prefs_path = GetInitialPrefsPathForTesting();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  } else if (const base::CommandLine* command_line =
                 base::CommandLine::ForCurrentProcess();
             command_line->HasSwitch(switches::kInitialPreferencesFile)) {
    initial_prefs_path =
        command_line->GetSwitchValuePath(switches::kInitialPreferencesFile);
#endif
  } else {
    initial_prefs_path =
        base::FilePath(first_run::internal::InitialPrefsPath());
  }

  if (initial_prefs_path.empty())
    return nullptr;
  auto initial_prefs =
      std::make_unique<installer::InitialPreferences>(initial_prefs_path);
  if (!initial_prefs->read_from_file())
    return nullptr;
  return initial_prefs;
}

ProcessInitialPreferencesResult ProcessInitialPreferences(
    const base::FilePath& user_data_dir,
    std::unique_ptr<installer::InitialPreferences> initial_prefs,
    MasterPrefs* out_prefs) {
  DCHECK(!user_data_dir.empty());

  if (initial_prefs.get()) {
    // Don't show EULA when running in headless mode since this would
    // effectively block the UI because there is no one to accept it.
    if (!headless::IsHeadlessMode() &&
        !internal::ShowPostInstallEULAIfNeeded(initial_prefs.get())) {
      return EULA_EXIT_NOW;
    }

    base::Value::Dict initial_dictionary =
        initial_prefs->initial_dictionary().Clone();
    // The distribution dictionary (and any prefs below it) are never registered
    // for use in Chrome's PrefService. Strip them from the initial dictionary
    // before mapping it to prefs.
    initial_dictionary.Remove(installer::initial_preferences::kDistroDict);

    if (!chrome_prefs::InitializePrefsFromMasterPrefs(
            profiles::GetDefaultProfileDir(user_data_dir),
            std::move(initial_dictionary))) {
      DLOG(ERROR) << "Failed to initialize from initial preferences.";
    }

    const base::Value::Dict* extensions = nullptr;
    if (initial_prefs->GetExtensionsBlock(extensions)) {
      DVLOG(1) << "Extensions block found in initial preferences";
      extensions::ExtensionUpdater::UpdateImmediatelyForFirstRun();
    }

    internal::SetupInitialPrefsFromInstallPrefs(*initial_prefs, out_prefs);
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
  //  2) Initial preferences (used to initialize user prefs in
  //     ProcessInitialPreferences()).
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

void DoPostImportTasks(bool make_chrome_default_for_user) {
  // Only set default browser after import as auto import relies on the current
  // default browser to know what to import from.
  ProcessDefaultBrowserPolicy(make_chrome_default_for_user);

  internal::DoPostImportPlatformSpecificTasks();
}

uint16_t auto_import_state() {
  return g_auto_import_state;
}

}  // namespace first_run
