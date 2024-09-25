// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/extension_apps_chromeos.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/containers/contains.h"
#include "base/containers/extend.h"
#include "base/feature_list.h"
#include "base/files/safe_base_name.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/extension_apps_utils.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps_util.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/extension_app_utils.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limit_interface.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/hosted_app_util.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/extensions/gfx_utils.h"
#include "chrome/browser/ash/file_manager/file_browser_handlers.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/chromeos/extensions/web_file_handlers/intent_util.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chrome/browser/web_applications/app_service/publisher_helper.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "content/public/browser/clear_site_data_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/path_util.h"
#include "extensions/browser/ui_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_handlers/app_display_info.h"
#include "extensions/common/manifest_handlers/file_handler_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/manifest_handlers/web_file_handlers_info.h"
#include "net/base/url_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

// Get the LaunchId for a given |app_window|. Set launch_id default value to an
// empty string. If show_in_shelf parameter is true and the window key is not
// empty, its value is appended to the launch_id. Otherwise, if the window key
// is empty, the session_id is used.
std::string GetLaunchId(extensions::AppWindow* app_window) {
  std::string launch_id;
  if (app_window->show_in_shelf()) {
    if (!app_window->window_key().empty()) {
      launch_id = app_window->window_key();
    } else {
      launch_id = base::StringPrintf("%d", app_window->session_id().id());
    }
  }
  return launch_id;
}

std::string GetSourceFromAppListSource(ash::ShelfLaunchSource source) {
  switch (source) {
    case ash::LAUNCH_FROM_APP_LIST:
      return std::string(extension_urls::kLaunchSourceAppList);
    case ash::LAUNCH_FROM_APP_LIST_SEARCH:
      return std::string(extension_urls::kLaunchSourceAppListSearch);
    default:
      return std::string();
  }
}

ash::ShelfLaunchSource ConvertLaunchSource(apps::LaunchSource launch_source) {
  switch (launch_source) {
    case apps::LaunchSource::kUnknown:
    case apps::LaunchSource::kFromParentalControls:
      return ash::LAUNCH_FROM_UNKNOWN;
    case apps::LaunchSource::kFromAppListGrid:
    case apps::LaunchSource::kFromAppListGridContextMenu:
      return ash::LAUNCH_FROM_APP_LIST;
    case apps::LaunchSource::kFromAppListQuery:
    case apps::LaunchSource::kFromAppListQueryContextMenu:
    case apps::LaunchSource::kFromAppListRecommendation:
      return ash::LAUNCH_FROM_APP_LIST_SEARCH;
    case apps::LaunchSource::kFromShelf:
      return ash::LAUNCH_FROM_SHELF;
    case apps::LaunchSource::kFromFileManager:
    case apps::LaunchSource::kFromLink:
    case apps::LaunchSource::kFromOmnibox:
    case apps::LaunchSource::kFromChromeInternal:
    case apps::LaunchSource::kFromKeyboard:
    case apps::LaunchSource::kFromOtherApp:
    case apps::LaunchSource::kFromMenu:
    case apps::LaunchSource::kFromInstalledNotification:
    case apps::LaunchSource::kFromTest:
    case apps::LaunchSource::kFromArc:
    case apps::LaunchSource::kFromSharesheet:
    case apps::LaunchSource::kFromReleaseNotesNotification:
    case apps::LaunchSource::kFromFullRestore:
    case apps::LaunchSource::kFromSmartTextContextMenu:
    case apps::LaunchSource::kFromDiscoverTabNotification:
    case apps::LaunchSource::kFromManagementApi:
    case apps::LaunchSource::kFromKiosk:
    case apps::LaunchSource::kFromCommandLine:
    case apps::LaunchSource::kFromBackgroundMode:
    case apps::LaunchSource::kFromNewTabPage:
    case apps::LaunchSource::kFromIntentUrl:
    case apps::LaunchSource::kFromOsLogin:
    case apps::LaunchSource::kFromProtocolHandler:
    case apps::LaunchSource::kFromUrlHandler:
    case apps::LaunchSource::kFromLockScreen:
    case apps::LaunchSource::kFromAppHomePage:
    case apps::LaunchSource::kFromReparenting:
    case apps::LaunchSource::kFromProfileMenu:
    case apps::LaunchSource::kFromSysTrayCalendar:
    case apps::LaunchSource::kFromInstaller:
    case apps::LaunchSource::kFromFirstRun:
    case apps::LaunchSource::kFromWelcomeTour:
    case apps::LaunchSource::kFromFocusMode:
    case apps::LaunchSource::kFromSparky:
    case apps::LaunchSource::kFromNavigationCapturing:
      return ash::LAUNCH_FROM_UNKNOWN;
  }
}

void MaybeAssociateWebContentsWithArcContext(
    apps::LaunchSource launch_source,
    content::WebContents* web_contents) {
  if (launch_source == apps::LaunchSource::kFromArc && web_contents) {
    // Add a flag to remember this web_contents originated in the ARC context.
    web_contents->SetUserData(
        &arc::ArcWebContentsData::kArcTransitionFlag,
        std::make_unique<arc::ArcWebContentsData>(web_contents));
  }
}
}  // namespace

namespace apps {

ExtensionAppsChromeOs::ExtensionAppsChromeOs(AppServiceProxy* proxy,
                                             AppType app_type)
    : ExtensionAppsBase(proxy, app_type),
      instance_registry_(&proxy->InstanceRegistry()),
      web_file_handlers_permission_handler_(
          std::make_unique<extensions::WebFileHandlersPermissionHandler>(
              profile())) {
  DCHECK(instance_registry_);
}

ExtensionAppsChromeOs::~ExtensionAppsChromeOs() {
  app_window_registry_.Reset();

  // In unit tests, AppServiceProxy might be ReinitializeForTesting, so
  // ExtensionApps might be destroyed without calling Shutdown, so arc_prefs_
  // needs to be removed from observer in the destructor function.
  if (arc_prefs_) {
    arc_prefs_->RemoveObserver(this);
    arc_prefs_ = nullptr;
  }
}

void ExtensionAppsChromeOs::Shutdown() {
  if (arc_prefs_) {
    arc_prefs_->RemoveObserver(this);
    arc_prefs_ = nullptr;
  }
}

void ExtensionAppsChromeOs::ObserveArc() {
  // Observe the ARC apps to set the badge on the equivalent Chrome app's icon.
  if (arc_prefs_) {
    arc_prefs_->RemoveObserver(this);
  }

  arc_prefs_ = ArcAppListPrefs::Get(profile());
  if (arc_prefs_) {
    arc_prefs_->AddObserver(this);
  }
}

void ExtensionAppsChromeOs::Initialize() {
  ExtensionAppsBase::Initialize();

  app_window_registry_.Observe(extensions::AppWindowRegistry::Get(profile()));

  if (app_type() == AppType::kExtension) {
    return;
  }

  media_dispatcher_.Observe(MediaCaptureDevicesDispatcher::GetInstance()
                                ->GetMediaStreamCaptureIndicator()
                                .get());

  // NotificationDisplayService could be null in some tests.
  if (auto* notification_display_service =
          NotificationDisplayServiceFactory::GetForProfile(profile())) {
    notification_display_service_.Observe(notification_display_service);
  }

  profile_pref_change_registrar_.Init(profile()->GetPrefs());
  profile_pref_change_registrar_.Add(
      policy::policy_prefs::kHideWebStoreIcon,
      base::BindRepeating(&ExtensionAppsBase::OnHideWebStoreIconPrefChanged,
                          GetWeakPtr()));

  auto* local_state = g_browser_process->local_state();
  if (local_state) {
    local_state_pref_change_registrar_.Init(local_state);
    local_state_pref_change_registrar_.Add(
        policy::policy_prefs::kSystemFeaturesDisableList,
        base::BindRepeating(&ExtensionAppsBase::OnSystemFeaturesPrefChanged,
                            GetWeakPtr()));
    local_state_pref_change_registrar_.Add(
        policy::policy_prefs::kSystemFeaturesDisableMode,
        base::BindRepeating(&ExtensionAppsBase::OnSystemFeaturesPrefChanged,
                            GetWeakPtr()));
    OnSystemFeaturesPrefChanged();
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ExtensionAppsChromeOs::GetCompressedIconData(
    const std::string& app_id,
    int32_t size_in_dip,
    ui::ResourceScaleFactor scale_factor,
    LoadIconCallback callback) {
  apps::GetChromeAppCompressedIconData(profile(), app_id, size_in_dip,
                                       scale_factor, std::move(callback));
}
#endif

void ExtensionAppsChromeOs::LaunchAppWithParamsImpl(AppLaunchParams&& params,
                                                    LaunchCallback callback) {
  const auto* extension = MaybeGetExtension(params.app_id);

  if (params.launch_files.empty() && !params.intent) {
    LaunchImpl(std::move(params));
    return;
  }

  bool is_quickoffice = extension_misc::IsQuickOfficeExtension(extension->id());
  if (extension->is_app() || is_quickoffice) {
    auto launch_source = params.launch_source;
    content::WebContents* web_contents = LaunchImpl(std::move(params));
    MaybeAssociateWebContentsWithArcContext(launch_source, web_contents);
  } else {
    DCHECK(extension->is_extension());
    // TODO(petermarshall): Set Arc flag as above?
    auto event_flags = apps::GetEventFlags(params.disposition,
                                           /*prefer_container=*/false);
    LaunchExtension(params.app_id, event_flags, std::move(params.intent),
                    params.launch_source,
                    std::make_unique<WindowInfo>(params.display_id),
                    base::DoNothing());
  }
}

void ExtensionAppsChromeOs::LaunchAppWithArgumentsCallback(
    LaunchSource launch_source,
    const std::string& app_id,
    int32_t event_flags,
    IntentPtr intent,
    WindowInfoPtr window_info,
    LaunchCallback callback,
    bool should_open) {
  // Exit early, while notifying, in case `Don't open` was chosen.
  if (!should_open) {
    std::move(callback).Run(LaunchResult(State::kFailed));
    return;
  }

  content::WebContents* web_contents = LaunchAppWithIntentImpl(
      app_id, event_flags, std::move(intent), launch_source,
      std::move(window_info), std::move(callback));
  MaybeAssociateWebContentsWithArcContext(launch_source, web_contents);
}

void ExtensionAppsChromeOs::LaunchAppWithIntent(const std::string& app_id,
                                                int32_t event_flags,
                                                IntentPtr intent,
                                                LaunchSource launch_source,
                                                WindowInfoPtr window_info,
                                                LaunchCallback callback) {
  // `extension` is required.
  const auto* extension = MaybeGetExtension(app_id);
  if (!extension) {
    std::move(callback).Run(LaunchResult(State::kFailed));
    return;
  }

  // Launch Web File Handlers if they're supported by the extension.
  if (extensions::WebFileHandlers::SupportsWebFileHandlers(*extension)) {
    std::vector<base::SafeBaseName> base_names =
        extensions::GetBaseNamesForIntent(*intent);

    // This vector cannot be empty because this is reached after explicitly
    // opening one or more files.
    if (base_names.empty()) {
      std::move(callback).Run(LaunchResult(State::kFailed));
      return;
    }

    // Confirm that the extension can open the file and then call the callback.
    web_file_handlers_permission_handler_->Confirm(
        *extension, base_names,
        base::BindOnce(&ExtensionAppsChromeOs::LaunchAppWithArgumentsCallback,
                       weak_factory_.GetWeakPtr(), launch_source, app_id,
                       event_flags, std::move(intent), std::move(window_info),
                       std::move(callback)));

    return;
  }

  bool is_quickoffice = extension_misc::IsQuickOfficeExtension(extension->id());

  // Launch legacy app.
  if (extension->is_app() || is_quickoffice) {
    content::WebContents* web_contents = LaunchAppWithIntentImpl(
        app_id, event_flags, std::move(intent), launch_source,
        std::move(window_info), std::move(callback));

    MaybeAssociateWebContentsWithArcContext(launch_source, web_contents);
    return;
  }

  // Launch extension.
  DCHECK(extension->is_extension());
  // TODO(petermarshall): Set Arc flag as above?
  LaunchExtension(app_id, event_flags, std::move(intent), launch_source,
                  std::move(window_info), std::move(callback));
}

void ExtensionAppsChromeOs::GetMenuModel(
    const std::string& app_id,
    MenuType menu_type,
    int64_t display_id,
    base::OnceCallback<void(MenuItems)> callback) {
  MenuItems menu_items;
  const auto* extension = MaybeGetExtension(app_id);
  if (!extension) {
    std::move(callback).Run(std::move(menu_items));
    return;
  }

  if (app_id == app_constants::kChromeAppId) {
    std::move(callback).Run(CreateBrowserMenuItems(profile()));
    return;
  }

  bool is_platform_app = extension->is_platform_app();
  if (!is_platform_app) {
    CreateOpenNewSubmenu(
        extensions::GetLaunchType(extensions::ExtensionPrefs::Get(profile()),
                                  extension) ==
                extensions::LaunchType::LAUNCH_TYPE_WINDOW
            ? IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW
            : IDS_APP_LIST_CONTEXT_MENU_NEW_TAB,
        menu_items);
  }

  if (!is_platform_app && menu_type == MenuType::kAppList &&
      extensions::util::IsAppLaunchableWithoutEnabling(app_id, profile()) &&
      extensions::OptionsPageInfo::HasOptionsPage(extension)) {
    AddCommandItem(ash::OPTIONS, IDS_NEW_TAB_APP_OPTIONS, menu_items);
  }

  if (menu_type == MenuType::kShelf &&
      instance_registry_->ContainsAppId(app_id)) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, menu_items);
  }

  const extensions::ManagementPolicy* policy =
      extensions::ExtensionSystem::Get(profile())->management_policy();
  DCHECK(policy);
  if (policy->UserMayModifySettings(extension, nullptr) &&
      !policy->MustRemainInstalled(extension, nullptr)) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM, menu_items);
  }

  if (extensions::AppDisplayInfo::ShouldDisplayInAppLauncher(*extension)) {
    AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                   menu_items);
  }

  std::move(callback).Run(std::move(menu_items));
}

void ExtensionAppsChromeOs::LaunchExtension(const std::string& app_id,
                                            int32_t event_flags,
                                            IntentPtr intent,
                                            LaunchSource launch_source,
                                            WindowInfoPtr window_info,
                                            LaunchCallback callback) {
  const auto* extension = MaybeGetExtension(app_id);
  DCHECK(extension);

  std::vector<storage::FileSystemURL> file_urls;
  if (!intent->files.empty()) {
    storage::FileSystemContext* file_system_context =
        file_manager::util::GetFileSystemContextForSourceURL(profile(),
                                                             extension->url());
    for (const IntentFilePtr& file : intent->files) {
      file_urls.push_back(
          file_system_context->CrackURLInFirstPartyContext(file->url));
    }
  }

  DCHECK(intent->activity_name);
  std::string action_id = intent->activity_name.value_or("");

  file_manager::file_browser_handlers::ExecuteFileBrowserHandler(
      profile(), extension, action_id, file_urls,
      base::BindOnce(
          [](LaunchCallback callback,
             extensions::api::file_manager_private::TaskResult result,
             std::string error) {
            bool success =
                result !=
                extensions::api::file_manager_private::TaskResult::kFailed;
            std::move(callback).Run(ConvertBoolToLaunchResult(success));
          },
          std::move(callback)));
}

void ExtensionAppsChromeOs::PauseApp(const std::string& app_id) {
  if (paused_apps_.MaybeAddApp(app_id)) {
    SetIconEffect(app_id);
  }
  AppPublisher::Publish(paused_apps_.CreateAppWithPauseStatus(
      app_type(), app_id, /*paused=*/true));

  if (!instance_registry_->ContainsAppId(app_id)) {
    return;
  }

  ash::app_time::AppTimeLimitInterface* app_limit =
      ash::app_time::AppTimeLimitInterface::Get(profile());
  DCHECK(app_limit);
  app_limit->PauseWebActivity(app_id);
}

void ExtensionAppsChromeOs::UnpauseApp(const std::string& app_id) {
  if (paused_apps_.MaybeRemoveApp(app_id)) {
    SetIconEffect(app_id);
  }

  AppPublisher::Publish(paused_apps_.CreateAppWithPauseStatus(
      app_type(), app_id, /*paused=*/false));

  ash::app_time::AppTimeLimitInterface* app_time =
      ash::app_time::AppTimeLimitInterface::Get(profile());
  DCHECK(app_time);
  app_time->ResumeWebActivity(app_id);
}

void ExtensionAppsChromeOs::UpdateAppSize(const std::string& app_id) {
  if (app_type() != AppType::kChromeApp) {
    return;
  }

  const extensions::Extension* extension = MaybeGetExtension(app_id);
  if (!extension) {
    return;
  }

  extensions::path_util::CalculateExtensionDirectorySize(
      extension->path(),
      base::BindOnce(&ExtensionAppsChromeOs::OnSizeCalculated,
                     weak_factory_.GetWeakPtr(), extension->id()));
}

void ExtensionAppsChromeOs::OnAppWindowAdded(
    extensions::AppWindow* app_window) {
  if (!ShouldRecordAppWindowActivity(app_window)) {
    return;
  }

  auto* window = app_window->GetNativeWindow();
  DCHECK(!instance_registry_->Exists(window));

  app_window_to_aura_window_[app_window] = window;

  // Attach window to multi-user manager now to let it manage visibility state
  // of the window correctly.
  if (SessionControllerClientImpl::IsMultiProfileAvailable()) {
    auto* multi_user_window_manager =
        MultiUserWindowManagerHelper::GetWindowManager();
    if (multi_user_window_manager) {
      multi_user_window_manager->SetWindowOwner(
          window, multi_user_util::GetAccountIdFromProfile(profile()));
    }
  }
  RegisterInstance(app_window, InstanceState::kStarted);
}

void ExtensionAppsChromeOs::OnAppWindowShown(extensions::AppWindow* app_window,
                                             bool was_hidden) {
  if (!ShouldRecordAppWindowActivity(app_window)) {
    return;
  }

  InstanceState state =
      instance_registry_->GetState(app_window->GetNativeWindow());
  // If the window is shown, it should be started, running and not hidden.
  state = static_cast<apps::InstanceState>(
      state | apps::InstanceState::kStarted | apps::InstanceState::kRunning);
  state =
      static_cast<apps::InstanceState>(state & ~apps::InstanceState::kHidden);
  RegisterInstance(app_window, state);
}

void ExtensionAppsChromeOs::OnAppWindowHidden(
    extensions::AppWindow* app_window) {
  if (!ShouldRecordAppWindowActivity(app_window)) {
    return;
  }

  // For hidden |app_window|, the other state bit, started, running, active, and
  // visible should be cleared.
  RegisterInstance(app_window, InstanceState::kHidden);
}

void ExtensionAppsChromeOs::OnAppWindowRemoved(
    extensions::AppWindow* app_window) {
  if (!ShouldRecordAppWindowActivity(app_window)) {
    return;
  }

  RegisterInstance(app_window, InstanceState::kDestroyed);
  app_window_to_aura_window_.erase(app_window);
}

void ExtensionAppsChromeOs::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  if (!Accepts(extension)) {
    return;
  }

  app_notifications_.RemoveNotificationsForApp(extension->id());
  paused_apps_.MaybeRemoveApp(extension->id());

  auto result = media_requests_.RemoveRequests(extension->id());

  apps::AppPublisher::ModifyCapabilityAccess(extension->id(), result.camera,
                                             result.microphone);

  ExtensionAppsBase::OnExtensionUninstalled(browser_context, extension, reason);
}

void ExtensionAppsChromeOs::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  ApplyChromeBadge(package_info.package_name);
}

void ExtensionAppsChromeOs::OnPackageRemoved(const std::string& package_name,
                                             bool uninstalled) {
  ApplyChromeBadge(package_name);
}

void ExtensionAppsChromeOs::OnPackageListInitialRefreshed() {
  if (!arc_prefs_) {
    return;
  }
  for (const auto& app_name : arc_prefs_->GetPackagesFromPrefs()) {
    ApplyChromeBadge(app_name);
  }
}

void ExtensionAppsChromeOs::OnArcAppListPrefsDestroyed() {
  arc_prefs_ = nullptr;
}

void ExtensionAppsChromeOs::OnIsCapturingVideoChanged(
    content::WebContents* web_contents,
    bool is_capturing_video) {
  const webapps::AppId* web_app_id =
      web_app::WebAppTabHelper::GetAppId(web_contents);
  if (web_app_id) {
    if (web_app::WebAppProvider::GetForWebApps(profile()) &&
        !web_app::IsAppServiceShortcut(
            *web_app_id, *web_app::WebAppProvider::GetForWebApps(profile()))) {
      // This media access is coming from a web app.
      return;
    }
  }

  std::string app_id = app_constants::kChromeAppId;
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  DCHECK(registry);
  const extensions::ExtensionSet& extensions = registry->enabled_extensions();
  const extensions::Extension* extension =
      extensions.GetAppByURL(web_contents->GetVisibleURL());
  if (extension && Accepts(extension)) {
    app_id = extension->id();
  }

  auto result = media_requests_.UpdateCameraState(app_id, web_contents,
                                                  is_capturing_video);
  apps::AppPublisher::ModifyCapabilityAccess(app_id, result.camera,
                                             result.microphone);
}

void ExtensionAppsChromeOs::OnIsCapturingAudioChanged(
    content::WebContents* web_contents,
    bool is_capturing_audio) {
  const webapps::AppId* web_app_id =
      web_app::WebAppTabHelper::GetAppId(web_contents);
  if (web_app_id) {
    if (web_app::WebAppProvider::GetForWebApps(profile()) &&
        !web_app::IsAppServiceShortcut(
            *web_app_id, *web_app::WebAppProvider::GetForWebApps(profile()))) {
      // This media access is coming from a web app.
      return;
    }
  }

  std::string app_id = app_constants::kChromeAppId;
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  DCHECK(registry);
  const extensions::ExtensionSet& extensions = registry->enabled_extensions();
  const extensions::Extension* extension =
      extensions.GetAppByURL(web_contents->GetVisibleURL());
  if (extension && Accepts(extension)) {
    app_id = extension->id();
  }

  auto result = media_requests_.UpdateMicrophoneState(app_id, web_contents,
                                                      is_capturing_audio);
  apps::AppPublisher::ModifyCapabilityAccess(app_id, result.camera,
                                             result.microphone);
}

void ExtensionAppsChromeOs::OnNotificationDisplayed(
    const message_center::Notification& notification,
    const NotificationCommon::Metadata* const metadata) {
  switch (notification.notifier_id().type) {
    case message_center::NotifierType::APPLICATION:
      MaybeAddNotification(notification.notifier_id().id, notification.id());
      return;
    case message_center::NotifierType::WEB_PAGE:
      MaybeAddWebPageNotifications(notification, metadata);
      return;
    default:
      return;
  }
}

void ExtensionAppsChromeOs::OnNotificationClosed(
    const std::string& notification_id) {
  const auto app_ids =
      app_notifications_.GetAppIdsForNotification(notification_id);
  if (app_ids.empty()) {
    return;
  }

  app_notifications_.RemoveNotification(notification_id);

  for (const auto& app_id : app_ids) {
    AppPublisher::Publish(
        app_notifications_.CreateAppWithHasBadgeStatus(app_type(), app_id));
  }
}

void ExtensionAppsChromeOs::OnNotificationDisplayServiceDestroyed(
    NotificationDisplayService* service) {
  DCHECK(notification_display_service_.IsObservingSource(service));
  notification_display_service_.Reset();
}

bool ExtensionAppsChromeOs::MaybeAddNotification(
    const std::string& app_id,
    const std::string& notification_id) {
  if (MaybeGetExtension(app_id) == nullptr) {
    return false;
  }

  app_notifications_.AddNotification(app_id, notification_id);
  AppPublisher::Publish(
      app_notifications_.CreateAppWithHasBadgeStatus(app_type(), app_id));
  return true;
}

void ExtensionAppsChromeOs::MaybeAddWebPageNotifications(
    const message_center::Notification& notification,
    const NotificationCommon::Metadata* const metadata) {
  const PersistentNotificationMetadata* persistent_metadata =
      PersistentNotificationMetadata::From(metadata);

  const NonPersistentNotificationMetadata* non_persistent_metadata =
      NonPersistentNotificationMetadata::From(metadata);

  GURL url = notification.origin_url();

  if (persistent_metadata) {
    url = persistent_metadata->service_worker_scope;
  } else if (non_persistent_metadata &&
             !non_persistent_metadata->document_url.is_empty()) {
    url = non_persistent_metadata->document_url;
  }

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  DCHECK(registry);
  const extensions::ExtensionSet& extensions = registry->enabled_extensions();
  const extensions::Extension* extension = extensions.GetAppByURL(url);
  if (extension) {
    MaybeAddNotification(extension->id(), notification.id());
  }
}

// static
bool ExtensionAppsChromeOs::IsBlocklisted(const std::string& app_id) {
  // We blocklist (meaning we don't publish the app, in the App Service sense)
  // some apps that are already published by other app publishers.
  //
  // This sense of "blocklist" is separate from the extension registry's
  // kDisabledByBlocklist concept, which is when SafeBrowsing will send out a
  // blocklist of malicious extensions to disable.

  // The Play Store is conceptually provided by the ARC++ publisher, but
  // because it (the Play Store icon) is also the UI for enabling Android apps,
  // we also want to show the app icon even before ARC++ is enabled. Prior to
  // the App Service, as a historical implementation quirk, the Play Store both
  // has an "ARC++ app" component and an "Extension app" component, and both
  // share the same App ID.
  //
  // In the App Service world, there should be a unique app publisher for any
  // given app. In this case, the ArcApps publisher publishes the Play Store
  // app, and the ExtensionApps publisher does not.
  if (app_id == arc::kPlayStoreAppId) {
    return true;
  }

  // If lacros chrome apps is enabled, a small list of extension apps or
  // extensions on ash extension keeplist is allowed to run in both ash and
  // lacros, don't publish such app or extension if it is blocked for app
  // service in ash.
  if (crosapi::browser_util::IsLacrosChromeAppsEnabled()) {
    if (extensions::ExtensionAppRunsInBothOSAndStandaloneBrowser(app_id) &&
        extensions::ExtensionAppBlockListedForAppServiceInOS(app_id)) {
      return true;
    }

    if (extensions::ExtensionRunsInBothOSAndStandaloneBrowser(app_id) &&
        extensions::ExtensionBlockListedForAppServiceInOS(app_id)) {
      return true;
    }
  }

  return false;
}

void ExtensionAppsChromeOs::UpdateShowInFields(const std::string& app_id) {
  const auto* extension = MaybeGetExtension(app_id);
  if (!extension) {
    return;
  }

  auto app = std::make_unique<App>(app_type(), app_id);
  SetShowInFields(extension, *app);
  AppPublisher::Publish(std::move(app));
}

void ExtensionAppsChromeOs::OnHideWebStoreIconPrefChanged() {
  UpdateShowInFields(extensions::kWebStoreAppId);
}

void ExtensionAppsChromeOs::OnSystemFeaturesPrefChanged() {
  PrefService* const local_state = g_browser_process->local_state();
  if (!local_state || !local_state->FindPreference(
                          policy::policy_prefs::kSystemFeaturesDisableList)) {
    return;
  }

  const base::Value::List& disabled_system_features_pref =
      local_state->GetList(policy::policy_prefs::kSystemFeaturesDisableList);

  const bool is_pref_disabled_mode_hidden =
      local_state->GetString(
          policy::policy_prefs::kSystemFeaturesDisableMode) ==
      policy::kHiddenDisableMode;
  const bool is_disabled_mode_changed =
      (is_pref_disabled_mode_hidden != is_disabled_apps_mode_hidden_);
  is_disabled_apps_mode_hidden_ = is_pref_disabled_mode_hidden;

  UpdateAppDisabledState(disabled_system_features_pref,
                         static_cast<int>(policy::SystemFeature::kWebStore),
                         extensions::kWebStoreAppId, is_disabled_mode_changed);
}

bool ExtensionAppsChromeOs::Accepts(const extensions::Extension* extension) {
  CHECK(extension);

  if (app_type() == AppType::kExtension) {
    if (!extension->is_extension() || IsBlocklisted(extension->id())) {
      return false;
    }

    // QuickOffice has file_handlers which we need to register.
    if (extension_misc::IsQuickOfficeExtension(extension->id())) {
      // Don't publish quickoffice in ash if 1st party ash extension keep list
      // is enforced, since quickoffice extension is published in Lacros.
      return !crosapi::browser_util::ShouldEnforceAshExtensionKeepList();
    }

    // Do not publish extensions in Ash if it should run in Lacros instead.
    if (crosapi::browser_util::ShouldEnforceAshExtensionKeepList()) {
      return false;
    }

    // Allow MV3 file handlers.
    if (extensions::WebFileHandlers::SupportsWebFileHandlers(*extension) &&
        extensions::WebFileHandlers::HasFileHandlers(*extension)) {
      return true;
    }

    // Only accept extensions with file_browser_handlers.
    FileBrowserHandler::List* handler_list =
        FileBrowserHandler::GetHandlers(extension);
    if (!handler_list) {
      return false;
    }
    return true;
  }

  if (!extension->is_app() || IsBlocklisted(extension->id())) {
    return false;
  }

  // Do not publish legacy packaged apps in Ash if Lacros is user's primary
  // browser. Legacy packaged apps are deprecated and not supported by Lacros.
  if (extension->is_legacy_packaged_app() &&
      crosapi::browser_util::IsLacrosEnabled()) {
    return false;
  }

  //  Do not publish hosted apps in Ash if hosted apps should run in
  //  Lacros.
  if (extension->is_hosted_app() &&
      extension->id() != app_constants::kChromeAppId &&
      crosapi::IsStandaloneBrowserHostedAppsEnabled()) {
    return false;
  }

  // Do not publish platform apps in Ash if it should run in Lacros instead.
  if (extension->is_platform_app() &&
      crosapi::browser_util::IsLacrosChromeAppsEnabled()) {
    return extensions::ExtensionAppRunsInOS(extension->id());
  }

  return true;
}

AppLaunchParams ExtensionAppsChromeOs::ModifyAppLaunchParams(
    const std::string& app_id,
    LaunchSource launch_source,
    AppLaunchParams params) {
  ash::ShelfLaunchSource source = ConvertLaunchSource(launch_source);
  if ((source == ash::LAUNCH_FROM_APP_LIST ||
       source == ash::LAUNCH_FROM_APP_LIST_SEARCH) &&
      app_id == extensions::kWebStoreAppId) {
    // Get the corresponding source string.
    std::string source_value = GetSourceFromAppListSource(source);

    // Set an override URL to include the source.
    const auto* extension = MaybeGetExtension(app_id);
    DCHECK(extension);
    GURL extension_url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);
    params.override_url = net::AppendQueryParameter(
        extension_url, extension_urls::kWebstoreSourceField, source_value);
  }
  return params;
}

void ExtensionAppsChromeOs::SetShowInFields(
    const extensions::Extension* extension,
    App& app) {
  ExtensionAppsBase::SetShowInFields(extension, app);

  // Explicitly mark QuickOffice as being able to handle intents even though it
  // is otherwise hidden from the user. Otherwise, extensions are only published
  // if they have file_browser_handlers, which means they need to handle
  // intents.
  if (extension_misc::IsQuickOfficeExtension(extension->id()) ||
      extension->is_extension()) {
    app.handles_intents = true;
  }
}

bool ExtensionAppsChromeOs::ShouldShownInLauncher(
    const extensions::Extension* extension) {
  return app_list::ShouldShowInLauncher(extension, profile());
}

AppPtr ExtensionAppsChromeOs::CreateApp(const extensions::Extension* extension,
                                        Readiness readiness) {
  CHECK(extension);
  // When Lacros is enabled, extensions not on the ash keep list should not be
  // published to the app service at all. Thus this method should not be called.
  DCHECK(!(extension->is_platform_app() &&
           crosapi::browser_util::IsLacrosChromeAppsEnabled() &&
           !extensions::ExtensionAppRunsInOS(extension->id())));
  const bool is_app_disabled = base::Contains(disabled_apps_, extension->id());

  auto app = CreateAppImpl(
      extension, is_app_disabled ? Readiness::kDisabledByPolicy : readiness);
  bool paused = paused_apps_.IsPaused(extension->id());
  app->icon_key = IconKey(GetIconEffects(extension, paused));

  if (is_app_disabled && is_disabled_apps_mode_hidden_) {
    app->show_in_launcher = false;
    app->show_in_search = false;
    app->show_in_shelf = false;
    app->handles_intents = false;
  }

  app->has_badge = app_notifications_.HasNotification(extension->id());
  app->paused = paused;

  if (extension->is_app() ||
      extensions::IsLegacyQuickOfficeExtension(*extension)) {
    app->intent_filters = apps_util::CreateIntentFiltersForChromeApp(extension);
  } else if (extension->is_extension()) {
    app->intent_filters = apps_util::CreateIntentFiltersForExtension(extension);
  }
  return app;
}

void ExtensionAppsChromeOs::OnSizeCalculated(const std::string& app_id,
                                             int64_t size) {
  std::vector<AppPtr> apps;
  auto app = std::make_unique<apps::App>(app_type(), app_id);
  app->app_size_in_bytes = size;
  apps.push_back(std::move(app));
  AppPublisher::Publish(std::move(apps), app_type(),
                        /*should_notify_initialized=*/false);
}

IconEffects ExtensionAppsChromeOs::GetIconEffects(
    const extensions::Extension* extension,
    bool paused) {
  IconEffects icon_effects = IconEffects::kNone;
  icon_effects =
      static_cast<IconEffects>(icon_effects | IconEffects::kCrOsStandardIcon);

  if (extensions::util::ShouldApplyChromeBadge(profile(), extension->id())) {
    icon_effects =
        static_cast<IconEffects>(icon_effects | IconEffects::kChromeBadge);
  }
  icon_effects = static_cast<IconEffects>(
      icon_effects | ExtensionAppsBase::GetIconEffects(extension));
  if (paused) {
    icon_effects =
        static_cast<IconEffects>(icon_effects | IconEffects::kPaused);
  }
  if (base::Contains(disabled_apps_, extension->id())) {
    icon_effects =
        static_cast<IconEffects>(icon_effects | IconEffects::kBlocked);
  }
  return icon_effects;
}

void ExtensionAppsChromeOs::ApplyChromeBadge(const std::string& package_name) {
  const std::vector<std::string> extension_ids =
      extensions::util::GetEquivalentInstalledExtensions(profile(),
                                                         package_name);

  for (auto& app_id : extension_ids) {
    SetIconEffect(app_id);
  }
}

void ExtensionAppsChromeOs::SetIconEffect(const std::string& app_id) {
  const auto* extension = MaybeGetExtension(app_id);
  if (!extension) {
    return;
  }

  auto app = std::make_unique<App>(app_type(), app_id);
  app->icon_key =
      IconKey(GetIconEffects(extension, paused_apps_.IsPaused(app_id)));
  AppPublisher::Publish(std::move(app));
}

bool ExtensionAppsChromeOs::ShouldRecordAppWindowActivity(
    extensions::AppWindow* app_window) {
  DCHECK(app_window);

  const extensions::Extension* extension = app_window->GetExtension();
  if (!extension) {
    return false;
  }

  // ARC Play Store is not published by this publisher, but the window for Play
  // Store should be able to be added to InstanceRegistry by the Chrome app
  // publisher.
  if (extension->id() == arc::kPlayStoreAppId &&
      app_type() != AppType::kExtension) {
    return true;
  }

  if (!Accepts(extension)) {
    return false;
  }

  return true;
}

void ExtensionAppsChromeOs::RegisterInstance(extensions::AppWindow* app_window,
                                             InstanceState new_state) {
  aura::Window* window = app_window->GetNativeWindow();

  // If the current state has been marked as |new_state|, we don't need to
  // update.
  if (instance_registry_->GetState(window) == new_state) {
    return;
  }

  if (new_state == InstanceState::kDestroyed) {
    DCHECK(base::Contains(app_window_to_aura_window_, app_window));
    window = app_window_to_aura_window_[app_window];
  }
  InstanceParams params(app_window->extension_id(), window);
  params.launch_id = GetLaunchId(app_window);
  params.state = std::make_pair(new_state, base::Time::Now());
  params.browser_context = app_window->browser_context();
  instance_registry_->CreateOrUpdateInstance(std::move(params));
}

content::WebContents* ExtensionAppsChromeOs::LaunchImpl(
    AppLaunchParams&& params) {
  AppLaunchParams params_for_restore(
      params.app_id, params.container, params.disposition, params.launch_source,
      params.display_id, params.launch_files, params.intent);

  auto* web_contents = ExtensionAppsBase::LaunchImpl(std::move(params));

  std::unique_ptr<app_restore::AppLaunchInfo> launch_info;
  int session_id = GetSessionIdForRestoreFromWebContents(web_contents);
  if (!SessionID::IsValidValue(session_id)) {
    // Save all launch information for platform apps, which can launch via
    // event, e.g. file app.
    launch_info = std::make_unique<app_restore::AppLaunchInfo>(
        params_for_restore.app_id, params_for_restore.container,
        params_for_restore.disposition, params_for_restore.display_id,
        std::move(params_for_restore.launch_files),
        std::move(params_for_restore.intent));
    full_restore::SaveAppLaunchInfo(profile()->GetPath(),
                                    std::move(launch_info));
  }

  return web_contents;
}

void ExtensionAppsChromeOs::UpdateAppDisabledState(
    const base::Value::List& disabled_system_features_pref,
    int feature,
    const std::string& app_id,
    bool is_disabled_mode_changed) {
  const bool is_disabled =
      base::Contains(disabled_system_features_pref, base::Value(feature));
  // Sometimes the policy is updated before the app is installed, so this way
  // the disabled_apps_ is updated regardless the Publish should happen or not
  // and the app will be published with the correct readiness upon its
  // installation.
  const bool should_publish =
      (base::Contains(disabled_apps_, app_id) != is_disabled) ||
      is_disabled_mode_changed;

  if (is_disabled) {
    disabled_apps_.insert(app_id);
  } else {
    disabled_apps_.erase(app_id);
  }

  if (!should_publish) {
    return;
  }

  const auto* extension = MaybeGetExtension(app_id);
  if (!extension) {
    return;
  }

  AppPublisher::Publish(CreateApp(extension, is_disabled
                                                 ? Readiness::kDisabledByPolicy
                                                 : Readiness::kReady));
}

}  // namespace apps
