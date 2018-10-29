// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/sys_info.h"
#include "base/task/post_task.h"
#include "chrome/browser/apps/platform_apps/app_load_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/image_loader_client.h"
#include "chromeos/settings/install_attributes.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "net/base/network_change_notifier.h"

namespace chromeos {

namespace {

// Global DemoSession instance.
DemoSession* g_demo_session = nullptr;

// Type of demo config forced on for tests.
base::Optional<DemoSession::DemoModeConfig> g_force_demo_config;

// Path relative to the path at which offline demo resources are loaded that
// contains image with demo Android apps.
constexpr base::FilePath::CharType kDemoAppsPath[] =
    FILE_PATH_LITERAL("android_demo_apps.squash");

constexpr base::FilePath::CharType kExternalExtensionsPrefsPath[] =
    FILE_PATH_LITERAL("demo_extensions.json");

// Path relative to the path at which offline demo resources are loaded that
// contains the highlights app.
constexpr char kHighlightsAppPath[] = "chrome_apps/highlights";

// Path relative to the path at which offline demo resources are loaded that
// contains sample photos.
constexpr char kPhotosPath[] = "media/photos";

bool IsDemoModeOfflineEnrolled() {
  DCHECK(DemoSession::IsDeviceInDemoMode());
  return DemoSession::GetDemoConfig() == DemoSession::DemoModeConfig::kOffline;
}

// Returns the list of apps normally pinned by Demo Mode policy that shouldn't
// be pinned if the device is offline.
std::vector<std::string> GetIgnorePinPolicyApps() {
  return {
      // Popular third-party game preinstalled in Demo Mode that is
      // online-only, so shouldn't be featured in the shelf when offline.
      "com.pixonic.wwr.chbkdemo",
      // TODO(michaelpg): YouTube is also pinned as a *default* app.
      extension_misc::kYoutubeAppId,
  };
}

// Copies photos into the Downloads directory.
// TODO(michaelpg): Test this behavior (requires overriding the Downloads
// directory).
void InstallDemoMedia(base::FilePath offline_resources_path) {
  if (offline_resources_path.empty()) {
    LOG(ERROR) << "Offline resources not loaded - no media available.";
    return;
  }

  base::FilePath src_path = offline_resources_path.Append(kPhotosPath);
  base::FilePath dest_path = file_manager::util::GetDownloadsFolderForProfile(
      ProfileManager::GetActiveUserProfile());

  if (!base::CopyDirectory(src_path, dest_path, false /* recursive */))
    LOG(ERROR) << "Failed to install demo mode media.";
}

std::string GetBoardName() {
  const std::vector<std::string> board =
      base::SplitString(base::SysInfo::GetLsbReleaseBoard(), "-",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return board[0];
}

std::string GetHighlightsAppId() {
  if (GetBoardName() == "eve")
    return extension_misc::kHighlightsAlt1AppId;
  if (GetBoardName() == "nocturne")
    return extension_misc::kHighlightsAlt2AppId;
  return extension_misc::kHighlightsAppId;
}

// If the current locale is not the default one, ensure it is reverted to the
// default when demo session restarts (i.e. user-selected locale is only allowed
// to be used for a single session).
void RestoreDefaultLocaleForNextSession() {
  auto* user = user_manager::UserManager::Get()->GetActiveUser();
  // Tests may not have an active user.
  if (!user)
    return;
  if (!user->is_profile_created()) {
    user->AddProfileCreatedObserver(
        base::BindOnce(&RestoreDefaultLocaleForNextSession));
    return;
  }
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  const std::string current_locale =
      profile->GetPrefs()->GetString(language::prefs::kApplicationLocale);
  if (current_locale.empty()) {
    LOG(WARNING) << "Current locale read from kApplicationLocale is empty!";
    return;
  }
  const std::string default_locale =
      g_browser_process->local_state()->GetString(
          prefs::kDemoModeDefaultLocale);
  if (default_locale.empty()) {
    // If the default locale is uninitialized, consider the current locale to be
    // the default. This is safe because users are not allowed to change the
    // locale prior to introduction of this code.
    g_browser_process->local_state()->SetString(prefs::kDemoModeDefaultLocale,
                                                current_locale);
    return;
  }
  if (current_locale != default_locale) {
    // If the user has changed the locale, request to change it back (which will
    // take effect when the session restarts).
    profile->ChangeAppLocale(default_locale,
                             Profile::APP_LOCALE_CHANGED_VIA_DEMO_SESSION);
  }
}

}  // namespace

// static
const char DemoSession::kDemoModeResourcesComponentName[] =
    "demo-mode-resources";

// static
base::FilePath DemoSession::GetPreInstalledDemoResourcesPath() {
  base::FilePath preinstalled_components_root;
  base::PathService::Get(DIR_PREINSTALLED_COMPONENTS,
                         &preinstalled_components_root);
  return preinstalled_components_root.AppendASCII("cros-components")
      .AppendASCII(kDemoModeResourcesComponentName);
}

// static
std::string DemoSession::DemoConfigToString(
    DemoSession::DemoModeConfig config) {
  switch (config) {
    case DemoSession::DemoModeConfig::kNone:
      return "none";
    case DemoSession::DemoModeConfig::kOnline:
      return "online";
    case DemoSession::DemoModeConfig::kOffline:
      return "offline";
  }
  NOTREACHED() << "Unknown demo mode configuration";
  return std::string();
}

// static
bool DemoSession::IsDeviceInDemoMode() {
  return GetDemoConfig() != DemoModeConfig::kNone;
}

// static
DemoSession::DemoModeConfig DemoSession::GetDemoConfig() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (g_force_demo_config.has_value())
    return *g_force_demo_config;

  const policy::BrowserPolicyConnectorChromeOS* const connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  bool is_demo_device_mode = connector->GetInstallAttributes()->GetMode() ==
                             policy::DeviceMode::DEVICE_MODE_DEMO;
  bool is_demo_device_domain = connector->GetInstallAttributes()->GetDomain() ==
                               DemoSetupController::kDemoModeDomain;

  // TODO(agawronska): We check device mode and domain to allow for dev/test
  // setup that is done by manual enrollment into demo domain. Device mode is
  // not set to DeviceMode::DEVICE_MODE_DEMO then. This extra condition
  // can be removed when all following conditions are fulfilled:
  // * DMServer is returning DeviceMode::DEVICE_MODE_DEMO for demo devices
  // * Offline policies specify DeviceMode::DEVICE_MODE_DEMO
  // * Demo mode setup flow is available to external developers
  bool is_demo_mode = is_demo_device_mode || is_demo_device_domain;

  const PrefService* prefs = g_browser_process->local_state();

  // The testing browser process might not have local state.
  if (!prefs)
    return DemoModeConfig::kNone;

  // Demo mode config preference is set at the end of the demo setup after
  // device is enrolled.
  auto demo_config = DemoModeConfig::kNone;
  int demo_config_pref = prefs->GetInteger(prefs::kDemoModeConfig);
  if (demo_config_pref >= static_cast<int>(DemoModeConfig::kNone) &&
      demo_config_pref <= static_cast<int>(DemoModeConfig::kLast)) {
    demo_config = static_cast<DemoModeConfig>(demo_config_pref);
  }

  if (is_demo_mode && demo_config == DemoModeConfig::kNone) {
    LOG(WARNING) << "Device mode is demo, but no demo mode config set";
  } else if (!is_demo_mode && demo_config != DemoModeConfig::kNone) {
    LOG(WARNING) << "Device mode is not demo, but demo mode config is set";
  }

  return is_demo_mode ? demo_config : DemoModeConfig::kNone;
}

// static
void DemoSession::SetDemoConfigForTesting(DemoModeConfig demo_config) {
  g_force_demo_config = demo_config;
}

// static
void DemoSession::ResetDemoConfigForTesting() {
  g_force_demo_config = base::nullopt;
}

// static
void DemoSession::PreloadOfflineResourcesIfInDemoMode() {
  if (!IsDeviceInDemoMode())
    return;

  if (!g_demo_session)
    g_demo_session = new DemoSession();
  g_demo_session->EnsureOfflineResourcesLoaded(base::OnceClosure());
}

// static
DemoSession* DemoSession::StartIfInDemoMode() {
  if (!IsDeviceInDemoMode())
    return nullptr;

  if (g_demo_session && g_demo_session->started())
    return g_demo_session;

  if (!g_demo_session)
    g_demo_session = new DemoSession();

  g_demo_session->started_ = true;
  g_demo_session->EnsureOfflineResourcesLoaded(base::OnceClosure());
  return g_demo_session;
}

// static
void DemoSession::ShutDownIfInitialized() {
  if (!g_demo_session)
    return;

  DemoSession* demo_session = g_demo_session;
  g_demo_session = nullptr;
  delete demo_session;
}

// static
DemoSession* DemoSession::Get() {
  return g_demo_session;
}

// static
std::string DemoSession::GetScreensaverAppId() {
  if (GetBoardName() == "eve")
    return extension_misc::kScreensaverAlt1AppId;
  if (GetBoardName() == "nocturne")
    return extension_misc::kScreensaverAlt2AppId;
  return extension_misc::kScreensaverAppId;
}

// static
bool DemoSession::ShouldDisplayInAppLauncher(const std::string& app_id) {
  if (!IsDeviceInDemoMode())
    return true;
  return app_id != GetScreensaverAppId() &&
         app_id != extensions::kWebStoreAppId &&
         app_id != extension_misc::kGeniusAppId;
}

// static
void DemoSession::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDemoModeDefaultLocale, std::string());
}

void DemoSession::EnsureOfflineResourcesLoaded(
    base::OnceClosure load_callback) {
  if (offline_resources_loaded_) {
    if (load_callback)
      std::move(load_callback).Run();
    return;
  }

  if (load_callback)
    offline_resources_load_callbacks_.emplace_back(std::move(load_callback));

  if (offline_resources_load_requested_)
    return;
  offline_resources_load_requested_ = true;

  if (offline_enrolled_) {
    LoadPreinstalledOfflineResources();
    return;
  }

  component_updater::CrOSComponentManager* cros_component_manager =
      g_browser_process->platform_part()->cros_component_manager();
  if (cros_component_manager) {
    g_browser_process->platform_part()->cros_component_manager()->Load(
        kDemoModeResourcesComponentName,
        component_updater::CrOSComponentManager::MountPolicy::kMount,
        component_updater::CrOSComponentManager::UpdatePolicy::kSkip,
        base::BindOnce(&DemoSession::InstalledComponentLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Cros component manager may be unset in tests - if that is the case,
    // report component install failure, so DemoSession attempts loading the
    // component directly from the pre-installed component path.
    // TODO(michaelpg): Rework tests to require the online component to load in
    // online-enrolled demo mode.
    InstalledComponentLoaded(
        component_updater::CrOSComponentManager::Error::INSTALL_FAILURE,
        base::FilePath());
  }
}

void DemoSession::SetOfflineResourcesLoadedForTesting(
    const base::FilePath& path) {
  OnOfflineResourcesLoaded(path);
}

base::FilePath DemoSession::GetDemoAppsPath() const {
  if (offline_resources_path_.empty())
    return base::FilePath();
  return offline_resources_path_.Append(kDemoAppsPath);
}

base::FilePath DemoSession::GetExternalExtensionsPrefsPath() const {
  if (offline_resources_path_.empty())
    return base::FilePath();
  return offline_resources_path_.Append(kExternalExtensionsPrefsPath);
}

base::FilePath DemoSession::GetOfflineResourceAbsolutePath(
    const base::FilePath& relative_path) const {
  if (offline_resources_path_.empty())
    return base::FilePath();
  if (relative_path.ReferencesParent())
    return base::FilePath();
  return offline_resources_path_.Append(relative_path);
}

bool DemoSession::ShouldIgnorePinPolicy(const std::string& app_id_or_package) {
  if (!g_demo_session || !g_demo_session->started())
    return false;

  // TODO(michaelpg): Update shelf when network status changes.
  // TODO(michaelpg): Also check for captive portal.
  if (!net::NetworkChangeNotifier::IsOffline())
    return false;

  return base::ContainsValue(ignore_pin_policy_offline_apps_,
                             app_id_or_package);
}

void DemoSession::SetExtensionsExternalLoader(
    scoped_refptr<DemoExtensionsExternalLoader> extensions_external_loader) {
  extensions_external_loader_ = extensions_external_loader;
  InstallAppFromUpdateUrl(GetScreensaverAppId());
}

void DemoSession::OverrideIgnorePinPolicyAppsForTesting(
    std::vector<std::string> apps) {
  ignore_pin_policy_offline_apps_ = std::move(apps);
}

DemoSession::DemoSession()
    : offline_enrolled_(IsDemoModeOfflineEnrolled()),
      ignore_pin_policy_offline_apps_(GetIgnorePinPolicyApps()),
      session_manager_observer_(this),
      extension_registry_observer_(this),
      weak_ptr_factory_(this) {
  session_manager_observer_.Add(session_manager::SessionManager::Get());
  OnSessionStateChanged();
}

DemoSession::~DemoSession() = default;

void DemoSession::InstalledComponentLoaded(
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& path) {
  if (error == component_updater::CrOSComponentManager::Error::NONE) {
    OnOfflineResourcesLoaded(base::make_optional(path));
    return;
  }

  LoadPreinstalledOfflineResources();
}

void DemoSession::LoadPreinstalledOfflineResources() {
  chromeos::DBusThreadManager::Get()
      ->GetImageLoaderClient()
      ->LoadComponentAtPath(
          kDemoModeResourcesComponentName, GetPreInstalledDemoResourcesPath(),
          base::BindOnce(&DemoSession::OnOfflineResourcesLoaded,
                         weak_ptr_factory_.GetWeakPtr()));
}

void DemoSession::OnOfflineResourcesLoaded(
    base::Optional<base::FilePath> mounted_path) {
  offline_resources_loaded_ = true;

  if (mounted_path.has_value())
    offline_resources_path_ = mounted_path.value();

  std::list<base::OnceClosure> load_callbacks;
  load_callbacks.swap(offline_resources_load_callbacks_);
  for (auto& callback : load_callbacks)
    std::move(callback).Run();
}

void DemoSession::InstallDemoResources() {
  DCHECK(offline_resources_loaded_);
  if (offline_enrolled_)
    LoadAndLaunchHighlightsApp();
  base::PostTaskWithTraits(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&InstallDemoMedia, offline_resources_path_));
}

void DemoSession::LoadAndLaunchHighlightsApp() {
  DCHECK(offline_resources_loaded_);
  if (offline_resources_path_.empty()) {
    LOG(ERROR) << "Offline resources not loaded - no highlights app available.";
    InstallAppFromUpdateUrl(GetHighlightsAppId());
    return;
  }
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  const base::FilePath resources_path =
      offline_resources_path_.Append(kHighlightsAppPath);
  if (!apps::AppLoadService::Get(profile)->LoadAndLaunch(
          resources_path, base::CommandLine(base::CommandLine::NO_PROGRAM),
          base::FilePath() /* cur_dir */)) {
    LOG(WARNING) << "Failed to launch highlights app from offline resources.";
    InstallAppFromUpdateUrl(GetHighlightsAppId());
  }
}

void DemoSession::InstallAppFromUpdateUrl(const std::string& id) {
  if (!extensions_external_loader_)
    return;
  auto* user = user_manager::UserManager::Get()->GetActiveUser();
  if (!user->is_profile_created()) {
    user->AddProfileCreatedObserver(
        base::BindOnce(&DemoSession::InstallAppFromUpdateUrl,
                       weak_ptr_factory_.GetWeakPtr(), id));
    return;
  }
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  extension_registry_observer_.Add(extensions::ExtensionRegistry::Get(profile));
  extensions_external_loader_->LoadApp(id);
}

void DemoSession::OnSessionStateChanged() {
  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::ACTIVE) {
    return;
  }
  RestoreDefaultLocaleForNextSession();

  if (!offline_enrolled_)
    InstallAppFromUpdateUrl(GetHighlightsAppId());

  EnsureOfflineResourcesLoaded(base::BindOnce(
      &DemoSession::InstallDemoResources, weak_ptr_factory_.GetWeakPtr()));
}

void DemoSession::OnExtensionInstalled(content::BrowserContext* browser_context,
                                       const extensions::Extension* extension,
                                       bool is_update) {
  if (extension->id() != GetHighlightsAppId())
    return;
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  OpenApplication(AppLaunchParams(
      profile, extension, extensions::LAUNCH_CONTAINER_WINDOW,
      WindowOpenDisposition::NEW_WINDOW, extensions::SOURCE_CHROME_INTERNAL));
}

}  // namespace chromeos
