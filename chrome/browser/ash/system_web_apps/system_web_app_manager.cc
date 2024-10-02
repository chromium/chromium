// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"

#include <iterator>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/camera_app_ui/url_constants.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/dcheck_is_on.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/one_shot_event.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "chrome/browser/ash/system_web_apps/apps/boca_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/camera_app/camera_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/connectivity_diagnostics_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/crosh_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/demo_mode_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/diagnostics_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/eche_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/file_manager_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/firmware_update_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/graduation_app_delegate.h"
#include "chrome/browser/ash/system_web_apps/apps/help_app/help_app_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/mall_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/media_app/media_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/os_feedback_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/os_flags_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/os_settings_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_system_app_delegate.h"
#include "chrome/browser/ash/system_web_apps/apps/print_management_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/print_preview_cros_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/projector_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/recorder_app/recorder_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/sanitize_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/scanning_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/shimless_rma_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/shortcut_customization_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/terminal_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/vc_background_ui/vc_background_ui_system_app_delegate.h"
#include "chrome/browser/ash/system_web_apps/color_helpers.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_background_task.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_icon_checker.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager_factory.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_background_task_info.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_system_web_app_delegate_map_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"
#if !defined(OFFICIAL_BUILD)
#include "chrome/browser/ash/system_web_apps/apps/sample_system_web_app_info.h"
#endif  // !defined(OFFICIAL_BUILD)

namespace ash {

const constexpr char kIconHealthMetricName[] =
    "Webapp.SystemApps.IconsAreHealthyInSession";
const constexpr char kIconsFixedOnReinstallMetricName[] =
    "Webapp.SystemApps.IconsFixedOnReinstall";

namespace {

SystemWebAppDelegateMap CreateSystemWebApps(Profile* profile) {
  std::vector<std::unique_ptr<SystemWebAppDelegate>> info_vec;
  // TODO(crbug.com/40118385): Currently unused, will be hooked up
  // post-migration. We're making delegates for everything, and will then use
  // them in place of SystemAppInfos.
  info_vec.push_back(std::make_unique<CameraSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<DemoModeSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<DiagnosticsSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<OSSettingsSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<CroshSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<TerminalSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<HelpAppSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<MediaSystemAppDelegate>(profile));
  info_vec.push_back(
      std::make_unique<PrintManagementSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<ScanningSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<ShimlessRMASystemAppDelegate>(profile));
  info_vec.push_back(
      std::make_unique<ConnectivityDiagnosticsSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<EcheSystemAppDelegate>(profile));
  info_vec.push_back(
      std::make_unique<PersonalizationSystemAppDelegate>(profile));
  info_vec.push_back(
      std::make_unique<ShortcutCustomizationSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<OSFeedbackAppDelegate>(profile));
  info_vec.push_back(std::make_unique<FileManagerSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<ProjectorSystemWebAppDelegate>(profile));
  info_vec.push_back(
      std::make_unique<FirmwareUpdateSystemAppDelegate>(profile));
  info_vec.push_back(std::make_unique<OsFlagsSystemWebAppDelegate>(profile));
  info_vec.push_back(
      std::make_unique<vc_background_ui::VcBackgroundUISystemAppDelegate>(
          profile));
  info_vec.push_back(std::make_unique<PrintPreviewCrosDelegate>(profile));
  info_vec.push_back(std::make_unique<RecorderSystemAppDelegate>(profile));
  if (ash::boca_util::IsEnabled()) {
    info_vec.push_back(std::make_unique<BocaSystemAppDelegate>(profile));
  }
  info_vec.push_back(std::make_unique<MallSystemAppDelegate>(profile));
  if (base::FeatureList::IsEnabled(ash::features::kSanitize)) {
    info_vec.push_back(std::make_unique<SanitizeSystemAppDelegate>(profile));
  }

  if (base::FeatureList::IsEnabled(ash::features::kSanitize)) {
    info_vec.push_back(std::make_unique<SanitizeSystemAppDelegate>(profile));
  }

  if (features::IsGraduationEnabled()) {
    info_vec.push_back(
        std::make_unique<graduation::GraduationAppDelegate>(profile));
  }

#if !defined(OFFICIAL_BUILD)
  info_vec.push_back(std::make_unique<SampleSystemAppDelegate>(profile));
#endif  // !defined(OFFICIAL_BUILD)

  SystemWebAppDelegateMap delegate_map;
  for (auto& info : info_vec) {
    if (info->IsAppEnabled() ||
        base::FeatureList::IsEnabled(features::kEnableAllSystemWebApps)) {
      // Gets `type` before std::move().
      SystemWebAppType type = info->GetType();
      delegate_map.emplace(type, std::move(info));
    }
  }
  return delegate_map;
}

bool HasSystemWebAppScheme(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) ||
         url.SchemeIs(content::kChromeUIUntrustedScheme);
}

web_app::ExternalInstallOptions CreateInstallOptionsForSystemApp(
    const SystemWebAppType type,
    const SystemWebAppDelegate& delegate,
    bool force_update,
    bool is_disabled) {
  DCHECK(delegate.GetInstallUrl().scheme() == content::kChromeUIScheme ||
         delegate.GetInstallUrl().scheme() ==
             content::kChromeUIUntrustedScheme);

  web_app::ExternalInstallOptions install_options(
      delegate.GetInstallUrl(), web_app::mojom::UserDisplayMode::kStandalone,
      web_app::ExternalInstallSource::kSystemInstalled);
  install_options.only_use_app_info_factory = true;
  // This can be Unretained because it's referring to the delegate owning this
  // factory method. The lifetime of that is the same as the
  // SystemWebAppManager.
  install_options.app_info_factory = base::BindRepeating(
      &SystemWebAppDelegate::GetWebAppInfo, base::Unretained(&delegate));
  install_options.add_to_applications_menu = delegate.ShouldShowInLauncher();
  install_options.add_to_desktop = false;
  install_options.add_to_quick_launch_bar = false;
  install_options.add_to_search = delegate.ShouldShowInSearchAndShelf();
  install_options.add_to_management = false;
  install_options.is_disabled = is_disabled;
  install_options.force_reinstall = force_update;
  install_options.uninstall_and_replace =
      delegate.GetAppIdsToUninstallAndReplace();
  install_options.system_app_type = type;
  install_options.handles_file_open_intents =
      delegate.ShouldHandleFileOpenIntents();

  const auto& search_terms = delegate.GetAdditionalSearchTerms();
  base::ranges::transform(
      search_terms, std::back_inserter(install_options.additional_search_terms),
      &l10n_util::GetStringUTF8);
  return install_options;
}

}  // namespace

// static
const char SystemWebAppManager::kInstallResultHistogramName[];
const char SystemWebAppManager::kInstallDurationHistogramName[];

SystemWebAppManager::SystemWebAppManager(Profile* profile)
    : profile_(profile),
      provider_(web_app::WebAppProvider::GetForLocalAppsUnchecked(profile_)),
      on_apps_synchronized_(new base::OneShotEvent()),
      on_tasks_started_(new base::OneShotEvent()),
      on_icon_check_completed_(new base::OneShotEvent()),
      install_result_per_profile_histogram_name_(
          std::string(kInstallResultHistogramName) + ".Profiles." +
          web_app::GetProfileCategoryForLogging(profile)),
      pref_service_(profile_->GetPrefs()),
      icon_checker_(SystemWebAppIconChecker::Create(profile_)) {
  DCHECK(provider_);
  // Always create delegates because many System Web App WebUIs are disabled
  // when the delegate is not present and we need them in tests. Tests can
  // override the list of delegates with SetSystemAppsForTesting().
  // Tests can override the list of delegates with `SetSystemAppsForTesting`.
  system_app_delegates_ = CreateSystemWebApps(profile_);

#if defined(OFFICIAL_BUILD)
  const bool is_official = true;
#else
  const bool is_official = false;
#endif
  const bool is_test =
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType);

  if (is_test || !is_official) {
    // Tests and non-official builds should always update.
    update_policy_ = UpdatePolicy::kAlwaysUpdate;
  } else {
    // Official builds should trigger updates whenever the version number
    // changes.
    update_policy_ = UpdatePolicy::kOnVersionChange;
  }

  // Ensure that system web apps have chrome://theme and
  // chrome-untrusted://theme available.
  content::URLDataSource::Add(profile, GetThemeSource(profile));
  content::URLDataSource::Add(profile,
                              GetThemeSource(profile, /*untrusted=*/true));
}

SystemWebAppManager::~SystemWebAppManager() {
  // SystemWebAppManager lifetime matches WebAppProvider lifetime (see
  // BrowserContextDependencyManager) but we reset pointers to
  // system_app_delegates_ for integrity with DCHECKs.
  if (provider_->is_registry_ready()) {
    ConnectProviderToSystemWebAppDelegateMap(nullptr);
  }
}

// static
SystemWebAppManager* SystemWebAppManager::Get(Profile* profile) {
  return SystemWebAppManagerFactory::GetForProfile(profile);
}

// static
web_app::WebAppProvider* SystemWebAppManager::GetWebAppProvider(
    Profile* profile) {
  return web_app::WebAppProvider::GetForLocalAppsUnchecked(profile);
}

// static
SystemWebAppManager* SystemWebAppManager::GetForTest(Profile* profile) {
  // Running a nested base::RunLoop outside of tests causes a deadlock. Crash
  // immediately instead of deadlocking for easier debugging (especially for
  // TAST tests which use prod binaries).
  CHECK_IS_TEST();

  web_app::WebAppProvider* provider =
      SystemWebAppManager::GetWebAppProvider(profile);
  if (!provider) {
    return nullptr;
  }

  SystemWebAppManager* swa_manager = Get(profile);
  DCHECK(swa_manager);

  if (provider->on_registry_ready().is_signaled()) {
    return swa_manager;
  }

  base::RunLoop run_loop;
  provider->on_registry_ready().Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  return swa_manager;
}

void SystemWebAppManager::StopBackgroundTasks() {
  for (auto& task : tasks_) {
    task->StopTask();
  }
}

void SystemWebAppManager::StopBackgroundTasksForTesting() {
  StopBackgroundTasks();
}

bool SystemWebAppManager::IsAppEnabled(SystemWebAppType type) const {
  if (base::FeatureList::IsEnabled(features::kEnableAllSystemWebApps)) {
    return true;
  }

  const SystemWebAppDelegate* delegate =
      GetSystemWebApp(system_app_delegates_, type);
  if (!delegate) {
    return false;
  }

  return delegate->IsAppEnabled();
}

void SystemWebAppManager::ScheduleStart() {
  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&SystemWebAppManager::Start,
                                weak_ptr_factory_.GetWeakPtr()));
}

void SystemWebAppManager::Start() {
  TRACE_EVENT0("ui", "SystemWebAppManager::Start");
  DCHECK(provider_->is_registry_ready());

  // `Start` can be called multiple times in tests.
  ui_manager_observation_.Reset();
  ui_manager_observation_.Observe(&provider_->ui_manager());

  ConnectProviderToSystemWebAppDelegateMap(&system_app_delegates_);

  const base::TimeTicks install_start_time = base::TimeTicks::Now();

#if DCHECK_IS_ON()
  // Check Origin Trials are defined correctly.
  for (const auto& type_and_app_info : system_app_delegates_) {
    for (const auto& origin_to_trial_names :
         type_and_app_info.second->GetEnabledOriginTrials()) {
      // Only allow force enabled origin trials on chrome:// and
      // chrome-untrusted:// URLs.
      const auto& scheme = origin_to_trial_names.first.scheme();
      DCHECK(scheme == content::kChromeUIScheme ||
             scheme == content::kChromeUIUntrustedScheme);
      // TODO(crbug.com/40115403): Find some ways to validate supplied
      // origin trial names. Ideally, construct them from some static const
      // char*.
    }
  }
#endif  // DCHECK_IS_ON()

  previous_session_had_broken_icons_ =
      pref_service_->GetBoolean(kSystemWebAppSessionHasBrokenIconsPrefName);

  std::vector<web_app::ExternalInstallOptions> install_options_list;
  const bool should_force_install_apps = ShouldForceInstallApps();
  if (should_force_install_apps) {
    UpdateLastAttemptedInfo();
  }

  const auto& disabled_system_apps =
      provider_->policy_manager().GetDisabledSystemWebApps();

  for (const auto& app : system_app_delegates_) {
    bool is_disabled = base::Contains(disabled_system_apps, app.first);
    install_options_list.push_back(CreateInstallOptionsForSystemApp(
        app.first, *app.second, should_force_install_apps, is_disabled));
  }

  const bool exceeded_retries = CheckAndIncrementRetryAttempts();
  if (exceeded_retries) {
    LOG(ERROR)
        << "Exceeded SWA install retry attempts.  Skipping installation, will "
           "retry on next OS update or when locale changes.";
    SCOPED_CRASH_KEY_BOOL("SystemWebAppManager", "broken_icons",
                          PreviousSessionHadBrokenIcons());
    base::debug::DumpWithoutCrashing();
    return;
  }

  // In tests, only install System Web Apps if `InstallSystemAppsForTesting()`
  // or `SetSystemAppsForTesting()` has been called.
  if (skip_app_installation_in_test_ &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    install_options_list.clear();
  }

  provider_->externally_managed_app_manager().SynchronizeInstalledApps(
      std::move(install_options_list),
      web_app::ExternalInstallSource::kSystemInstalled,
      base::BindOnce(&SystemWebAppManager::OnAppsSynchronized,
                     weak_ptr_factory_.GetWeakPtr(), should_force_install_apps,
                     install_start_time));
}

void SystemWebAppManager::Shutdown() {
  shutting_down_ = true;
  StopBackgroundTasks();

  // Icon check might be in progress, destroying the icon_checker_ ensures that
  // any pending callbacks are dropped.
  icon_checker_.reset();
}

void SystemWebAppManager::InstallSystemAppsForTesting() {
  {
    // If system web app manager is still starting (explained below), we need
    // to wait until it finishes before we can reset the states. This is to
    // ensure the app synchronization request to web app system is complete
    // before issuing another one in Start().
    //
    // In browser tests, SystemWebAppManager starts immediately after
    // WebAppProvider is ready and calls SynchronizeInstalledApps.
    //
    // SynchronizeInstalledApps asynchronously handles the install request
    // (awaits on web app system lock). If this operation isn't complete before
    // test body starts, we'll issue a second SynchronizeInstalledApps request
    // and violates the expectation (in web app system) that there can be only
    // one in-flight synchronize request for each app install source.
    //
    // TODO(crbug.com/40250473): Ensure browsertests sets up system apps
    // before SystemWebAppManager::Start(). Then, remove this method (or
    // restrict its use to system web app feature test that needs to simulate
    // system restart).
    base::RunLoop run_loop;
    on_apps_synchronized().Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  ResetForTesting();  // IN-TEST

  skip_app_installation_in_test_ = false;
  Start();

  // Wait for the System Web Apps to install.
  base::RunLoop run_loop;
  on_apps_synchronized().Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

std::optional<webapps::AppId> SystemWebAppManager::GetAppIdForSystemApp(
    SystemWebAppType type) const {
  if (!provider_->is_registry_ready()) {
    return std::nullopt;
  }
  return web_app::GetAppIdForSystemApp(provider_->registrar_unsafe(),
                                       system_app_delegates_, type);
}

std::optional<SystemWebAppType> SystemWebAppManager::GetSystemAppTypeForAppId(
    const webapps::AppId& app_id) const {
  if (!provider_->is_registry_ready()) {
    return std::nullopt;
  }
  return web_app::GetSystemAppTypeForAppId(provider_->registrar_unsafe(),
                                           system_app_delegates_, app_id);
}

const SystemWebAppDelegate* SystemWebAppManager::GetSystemApp(
    SystemWebAppType type) const {
  return GetSystemWebApp(system_app_delegates_, type);
}

std::vector<webapps::AppId> SystemWebAppManager::GetAppIds() const {
  std::vector<webapps::AppId> app_ids;
  for (const auto& app_type_to_app_info : system_app_delegates_) {
    std::optional<webapps::AppId> app_id =
        GetAppIdForSystemApp(app_type_to_app_info.first);
    if (app_id.has_value()) {
      app_ids.push_back(app_id.value());
    }
  }
  return app_ids;
}

bool SystemWebAppManager::IsSystemWebApp(const webapps::AppId& app_id) const {
  DCHECK(provider_->is_registry_ready());
  return web_app::IsSystemWebApp(provider_->registrar_unsafe(),
                                 system_app_delegates_, app_id);
}

const std::vector<std::string>* SystemWebAppManager::GetEnabledOriginTrials(
    const SystemWebAppDelegate* system_app,
    const GURL& url) const {
  DCHECK(system_app);
  const auto& origin_to_origin_trials = system_app->GetEnabledOriginTrials();
  auto iter_trials = origin_to_origin_trials.find(url::Origin::Create(url));

  if (iter_trials == origin_to_origin_trials.end()) {
    return nullptr;
  }

  return &iter_trials->second;
}

void SystemWebAppManager::OnReadyToCommitNavigation(
    const webapps::AppId& app_id,
    content::NavigationHandle* navigation_handle) {
  // Perform tab-specific setup when a navigation in a System Web App is about
  // to be committed.
  if (!IsSystemWebApp(app_id)) {
    return;
  }

  // No need to setup origin trials for intra-document navigation.
  if (navigation_handle->IsSameDocument()) {
    return;
  }

  const std::optional<SystemWebAppType> type = GetSystemAppTypeForAppId(app_id);
  // This function should only be called when an navigation happens inside a
  // System App. So the |app_id| should always have a valid associated System
  // App type.
  DCHECK(type.has_value());
  auto* system_app = GetSystemApp(type.value());
  DCHECK(system_app);

  const std::vector<std::string>* trials =
      GetEnabledOriginTrials(system_app, navigation_handle->GetURL());
  if (trials) {
    navigation_handle->ForceEnableOriginTrials(*trials);
  }
}

std::optional<SystemWebAppType> SystemWebAppManager::GetSystemAppForURL(
    const GURL& url) const {
  if (!HasSystemWebAppScheme(url)) {
    return std::nullopt;
  }

  if (!provider_->is_registry_ready()) {
    return std::nullopt;
  }

  std::optional<webapps::AppId> app_id =
      provider_->registrar_unsafe().FindAppWithUrlInScope(url);
  if (!app_id.has_value()) {
    return std::nullopt;
  }

  std::optional<SystemWebAppType> type =
      GetSystemAppTypeForAppId(app_id.value());
  if (!type.has_value()) {
    return std::nullopt;
  }

  const SystemWebAppDelegate* delegate =
      GetSystemWebApp(system_app_delegates_, type.value());
  if (!delegate) {
    return std::nullopt;
  }

  return type;
}

std::optional<SystemWebAppType>
SystemWebAppManager::GetCapturingSystemAppForURL(const GURL& url) const {
  std::optional<SystemWebAppType> type = GetSystemAppForURL(url);
  if (!type.has_value()) {
    return std::nullopt;
  }

  const SystemWebAppDelegate* delegate =
      GetSystemWebApp(system_app_delegates_, type.value());
  if (!delegate->ShouldCaptureNavigations()) {
    return std::nullopt;
  }

  // TODO(crbug://1051229): Expand ShouldCaptureNavigation to take a GURL, and
  // move this into the camera one.
  if (type == SystemWebAppType::CAMERA) {
    GURL::Replacements replacements;
    replacements.ClearQuery();
    replacements.ClearRef();
    if (url.ReplaceComponents(replacements).spec() !=
        kChromeUICameraAppMainURL) {
      return std::nullopt;
    }
  }

  return type;
}

void SystemWebAppManager::OnWebAppUiManagerDestroyed() {
  ui_manager_observation_.Reset();
}

void SystemWebAppManager::SetSystemAppsForTesting(
    SystemWebAppDelegateMap system_apps) {
  skip_app_installation_in_test_ = false;
  system_app_delegates_ = std::move(system_apps);
}

const std::vector<std::unique_ptr<SystemWebAppBackgroundTask>>&
SystemWebAppManager::GetBackgroundTasksForTesting() {
  return tasks_;
}

void SystemWebAppManager::SetUpdatePolicyForTesting(UpdatePolicy policy) {
  update_policy_ = policy;
}

void SystemWebAppManager::ResetForTesting() {
  CHECK_IS_TEST();
  StopBackgroundTasks();
  tasks_.clear();
  icon_checker_ = SystemWebAppIconChecker::Create(profile_);
  on_apps_synchronized_ = std::make_unique<base::OneShotEvent>();
  on_icon_check_completed_ = std::make_unique<base::OneShotEvent>();
  on_tasks_started_ = std::make_unique<base::OneShotEvent>();
}

const base::Version& SystemWebAppManager::CurrentVersion() const {
  return version_info::GetVersion();
}

const std::string& SystemWebAppManager::CurrentLocale() const {
  return g_browser_process->GetApplicationLocale();
}

bool SystemWebAppManager::PreviousSessionHadBrokenIcons() const {
  return previous_session_had_broken_icons_;
}
void SystemWebAppManager::RecordSystemWebAppInstallDuration(
    const base::TimeDelta& install_duration) const {
  // Install duration should be non-negative. A low resolution clock could
  // result in a |install_duration| of 0.
  DCHECK_GE(install_duration.InMilliseconds(), 0);

  if (!shutting_down_) {
    base::UmaHistogramMediumTimes(kInstallDurationHistogramName,
                                  install_duration);
  }
}

void SystemWebAppManager::RecordSystemWebAppInstallResults(
    const std::map<GURL, web_app::ExternallyManagedAppManager::InstallResult>&
        install_results) const {
  // Report install result codes. Exclude kSuccessAlreadyInstalled from metrics.
  // This result means the installation pipeline is a no-op (which happens every
  // time user logs in, and if there hasn't been a version upgrade). This skews
  // the install success rate.
  std::map<GURL, web_app::ExternallyManagedAppManager::InstallResult>
      results_to_report;
  base::ranges::copy_if(
      install_results,
      std::inserter(results_to_report, results_to_report.end()),
      [](const auto& url_and_result) {
        return url_and_result.second.code !=
               webapps::InstallResultCode::kSuccessAlreadyInstalled;
      });

  for (const auto& url_and_result : results_to_report) {
    // Record aggregate result.
    base::UmaHistogramEnumeration(
        kInstallResultHistogramName,
        shutting_down_
            ? webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown
            : url_and_result.second.code);

    // Record per-profile result.
    base::UmaHistogramEnumeration(
        install_result_per_profile_histogram_name_,
        shutting_down_
            ? webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown
            : url_and_result.second.code);
  }

  // Record per-app result.
  for (const auto& type_and_app_info : system_app_delegates_) {
    const GURL& install_url = type_and_app_info.second->GetInstallUrl();
    const auto url_and_result = results_to_report.find(install_url);
    if (url_and_result != results_to_report.cend()) {
      const std::string app_histogram_name =
          std::string(kInstallResultHistogramName) + ".Apps." +
          type_and_app_info.second->GetInternalName();
      base::UmaHistogramEnumeration(
          app_histogram_name, shutting_down_
                                  ? webapps::InstallResultCode::
                                        kCancelledOnWebAppProviderShuttingDown
                                  : url_and_result->second.code);
    }
  }
}

void SystemWebAppManager::OnAppsSynchronized(
    bool did_force_install_apps,
    const base::TimeTicks& install_start_time,
    std::map<GURL, web_app::ExternallyManagedAppManager::InstallResult>
        install_results,
    std::map<GURL, webapps::UninstallResultCode> uninstall_results) {
  const base::TimeDelta install_duration =
      base::TimeTicks::Now() - install_start_time;

  // TODO(qjw): Figure out where install_results come from, decide if
  // installation failures need to be handled
  pref_service_->SetString(prefs::kSystemWebAppLastUpdateVersion,
                           CurrentVersion().GetString());
  pref_service_->SetString(prefs::kSystemWebAppLastInstalledLocale,
                           CurrentLocale());

  // Report install duration only if the install pipeline actually installs
  // all the apps (e.g. on version upgrade).
  if (did_force_install_apps) {
    RecordSystemWebAppInstallDuration(install_duration);
  }

  RecordSystemWebAppInstallResults(install_results);

  for (const auto& it : system_app_delegates_) {
    std::optional<SystemWebAppBackgroundTaskInfo> background_info =
        it.second->GetTimerInfo();
    if (background_info && it.second->IsAppEnabled()) {
      tasks_.push_back(std::make_unique<SystemWebAppBackgroundTask>(
          profile_, background_info.value()));
    }
  }

  // May be called more than once in tests.
  if (!on_apps_synchronized_->is_signaled()) {
    on_apps_synchronized_->Signal();
    DCHECK(provider_->is_registry_ready());
    provider_->policy_manager().OnDisableListPolicyChanged();
    // TODO(http://crbug/1173187): Don't create SWA background tasks that are
    // associated with a disabled SWA.
  }

  if (!shutting_down_) {
    // Start an icon health check.
    icon_checker_->StartCheck(
        GetAppIds(), base::BindOnce(&SystemWebAppManager::OnIconCheckResult,
                                    weak_ptr_factory_.GetWeakPtr()));
  }

  // Start the tasks async to give any code running in an on_app_synchronized
  // context a chance to finish first.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SystemWebAppManager::StartBackgroundTasks,
                                weak_ptr_factory_.GetWeakPtr()));
}

void SystemWebAppManager::StartBackgroundTasks() const {
  for (const auto& task : tasks_) {
    task->StartTask();
  }
  // This happens as part of synchronize, and can also be called multiple times
  // in testing.
  if (!on_tasks_started_->is_signaled()) {
    on_tasks_started_->Signal();
  }
}

void SystemWebAppManager::OnIconCheckResult(
    SystemWebAppIconChecker::IconState result) {
  switch (result) {
    case SystemWebAppIconChecker::IconState::kNoAppInstalled:
      break;
    case SystemWebAppIconChecker::IconState::kBroken:
      base::UmaHistogramBoolean(kIconHealthMetricName, false);
      if (PreviousSessionHadBrokenIcons()) {
        base::UmaHistogramBoolean(kIconsFixedOnReinstallMetricName, false);
      }
      pref_service_->SetBoolean(kSystemWebAppSessionHasBrokenIconsPrefName,
                                true);
      break;
    case SystemWebAppIconChecker::IconState::kOk:
      base::UmaHistogramBoolean(kIconHealthMetricName, true);
      if (PreviousSessionHadBrokenIcons()) {
        base::UmaHistogramBoolean(kIconsFixedOnReinstallMetricName, true);
      }
      pref_service_->ClearPref(kSystemWebAppSessionHasBrokenIconsPrefName);
      pref_service_->ClearPref(prefs::kSystemWebAppInstallFailureCount);
      break;
  }

  // Might get signaled multiple times in tests.
  if (!on_icon_check_completed_->is_signaled()) {
    on_icon_check_completed_->Signal();
  }
}

bool SystemWebAppManager::ShouldForceInstallApps() const {
  if (base::FeatureList::IsEnabled(features::kAlwaysReinstallSystemWebApps)) {
    return true;
  }

  if (update_policy_ == UpdatePolicy::kAlwaysUpdate) {
    return true;
  }

  if (PreviousSessionHadBrokenIcons()) {
    return true;
  }

  base::Version current_installed_version(
      pref_service_->GetString(prefs::kSystemWebAppLastUpdateVersion));

  const std::string& current_installed_locale(
      pref_service_->GetString(prefs::kSystemWebAppLastInstalledLocale));

  // If Chrome version rolls back for some reason, ensure System Web Apps are
  // always in sync with Chrome version.
  const bool versionIsDifferent = !current_installed_version.IsValid() ||
                                  current_installed_version != CurrentVersion();

  // If system language changes, ensure System Web Apps launcher localization
  // are in sync with current language.
  const bool localeIsDifferent = current_installed_locale != CurrentLocale();

  return versionIsDifferent || localeIsDifferent;
}

void SystemWebAppManager::UpdateLastAttemptedInfo() {
  base::Version last_attempted_version(
      pref_service_->GetString(prefs::kSystemWebAppLastAttemptedVersion));

  const std::string& last_attempted_locale(
      pref_service_->GetString(prefs::kSystemWebAppLastAttemptedLocale));

  const bool is_retry = last_attempted_version.IsValid() &&
                        last_attempted_version == CurrentVersion() &&
                        last_attempted_locale == CurrentLocale();

  if (!is_retry) {
    pref_service_->SetInteger(prefs::kSystemWebAppInstallFailureCount, 0);
  }

  pref_service_->SetString(prefs::kSystemWebAppLastAttemptedVersion,
                           CurrentVersion().GetString());
  pref_service_->SetString(prefs::kSystemWebAppLastAttemptedLocale,
                           CurrentLocale());
  pref_service_->CommitPendingWrite();
}

bool SystemWebAppManager::CheckAndIncrementRetryAttempts() {
  int installation_failures =
      pref_service_->GetInteger(prefs::kSystemWebAppInstallFailureCount);
  bool reached_retry_limit = installation_failures > kInstallFailureAttempts;

  if (!reached_retry_limit) {
    pref_service_->SetInteger(prefs::kSystemWebAppInstallFailureCount,
                              installation_failures + 1);
    pref_service_->CommitPendingWrite();
    return false;
  }
  return true;
}

void SystemWebAppManager::ConnectProviderToSystemWebAppDelegateMap(
    const SystemWebAppDelegateMap* system_web_apps_delegate_map) const {
  // TODO(crbug.com/40243506): Consider DCHECKing that provider_ is ready.
  provider_->manifest_update_manager().SetSystemWebAppDelegateMap(
      system_web_apps_delegate_map);
  provider_->policy_manager().SetSystemWebAppDelegateMap(
      system_web_apps_delegate_map);
}

}  // namespace ash
