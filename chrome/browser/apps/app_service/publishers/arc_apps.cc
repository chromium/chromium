// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/arc_apps.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/components/arc/metrics/arc_metrics_service.h"
#include "ash/components/arc/mojom/app_permissions.mojom.h"
#include "ash/components/arc/mojom/compatibility_mode.mojom.h"
#include "ash/components/arc/mojom/file_system.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "chrome/browser/apps/app_service/app_icon/dip_px_util.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/file_utils.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps_factory.h"
#include "chrome/browser/apps/app_service/webapk/webapk_manager.h"
#include "chrome/browser/ash/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/component_extension_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/arc/common/intent_helper/arc_intent_helper_package.h"
#include "components/arc/intent_helper/intent_constants.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/capability_access.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"

// TODO(crbug.com/40569217): consider that, per khmel@, "App icon can be
// overwritten (setTaskDescription) or by assigning the icon for the app
// window. In this case some consumers (Shelf for example) switch to
// overwritten icon... IIRC this applies to shelf items and ArcAppWindow icon".

namespace {

std::optional<int> g_test_arc_version_;

apps::PermissionType GetPermissionType(
    arc::mojom::AppPermission arc_permission_type) {
  switch (arc_permission_type) {
    case arc::mojom::AppPermission::CAMERA:
      return apps::PermissionType::kCamera;
    case arc::mojom::AppPermission::LOCATION:
      return apps::PermissionType::kLocation;
    case arc::mojom::AppPermission::MICROPHONE:
      return apps::PermissionType::kMicrophone;
    case arc::mojom::AppPermission::NOTIFICATIONS:
      return apps::PermissionType::kNotifications;
    case arc::mojom::AppPermission::CONTACTS:
      return apps::PermissionType::kContacts;
    case arc::mojom::AppPermission::STORAGE:
      return apps::PermissionType::kStorage;
  }
}

bool GetArcPermissionType(apps::PermissionType app_service_permission_type,
                          arc::mojom::AppPermission& arc_permission) {
  switch (app_service_permission_type) {
    case apps::PermissionType::kCamera:
      arc_permission = arc::mojom::AppPermission::CAMERA;
      return true;
    case apps::PermissionType::kLocation:
      arc_permission = arc::mojom::AppPermission::LOCATION;
      return true;
    case apps::PermissionType::kMicrophone:
      arc_permission = arc::mojom::AppPermission::MICROPHONE;
      return true;
    case apps::PermissionType::kNotifications:
      arc_permission = arc::mojom::AppPermission::NOTIFICATIONS;
      return true;
    case apps::PermissionType::kContacts:
      arc_permission = arc::mojom::AppPermission::CONTACTS;
      return true;
    case apps::PermissionType::kStorage:
      arc_permission = arc::mojom::AppPermission::STORAGE;
      return true;
    case apps::PermissionType::kUnknown:
    case apps::PermissionType::kPrinting:
    case apps::PermissionType::kFileHandling:
      return false;
  }
}

apps::Permissions CreatePermissions(
    const base::flat_map<arc::mojom::AppPermission,
                         arc::mojom::PermissionStatePtr>& arc_permissions) {
  apps::Permissions permissions;
  for (const auto& [arc_permission_type, arc_permission_state] :
       arc_permissions) {
    apps::TriState value = arc_permission_state->granted
                               ? apps::TriState::kAllow
                               : apps::TriState::kBlock;
    // Permissions in the one-time state will ask for permission again the next
    // time they are used.
    if (arc_permission_state->one_time) {
      value = apps::TriState::kAsk;
    }

    permissions.push_back(std::make_unique<apps::Permission>(
        GetPermissionType(arc_permission_type), value,
        arc_permission_state->managed, arc_permission_state->details));
  }
  return permissions;
}

std::optional<arc::UserInteractionType> GetUserInterationType(
    apps::LaunchSource launch_source) {
  auto user_interaction_type = arc::UserInteractionType::NOT_USER_INITIATED;
  switch (launch_source) {
    // kUnknown is not set anywhere, this case is not valid.
    case apps::LaunchSource::kUnknown:
      return std::nullopt;
    case apps::LaunchSource::kFromChromeInternal:
      user_interaction_type = arc::UserInteractionType::NOT_USER_INITIATED;
      break;
    case apps::LaunchSource::kFromAppListGrid:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER;
      break;
    case apps::LaunchSource::kFromAppListGridContextMenu:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_CONTEXT_MENU;
      break;
    case apps::LaunchSource::kFromAppListQuery:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_SEARCH;
      break;
    case apps::LaunchSource::kFromAppListQueryContextMenu:
      user_interaction_type = arc::UserInteractionType::
          APP_STARTED_FROM_LAUNCHER_SEARCH_CONTEXT_MENU;
      break;
    case apps::LaunchSource::kFromAppListRecommendation:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_SUGGESTED_APP;
      break;
    case apps::LaunchSource::kFromParentalControls:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_SETTINGS;
      break;
    case apps::LaunchSource::kFromShelf:
      user_interaction_type = arc::UserInteractionType::APP_STARTED_FROM_SHELF;
      break;
    case apps::LaunchSource::kFromFileManager:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_FILE_MANAGER;
      break;
    case apps::LaunchSource::kFromLink:
      user_interaction_type = arc::UserInteractionType::APP_STARTED_FROM_LINK;
      break;
    case apps::LaunchSource::kFromOmnibox:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_OMNIBOX;
      break;
    case apps::LaunchSource::kFromSharesheet:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_SHARESHEET;
      break;
    case apps::LaunchSource::kFromFullRestore:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_FULL_RESTORE;
      break;
    case apps::LaunchSource::kFromSmartTextContextMenu:
      user_interaction_type = arc::UserInteractionType::
          APP_STARTED_FROM_SMART_TEXT_SELECTION_CONTEXT_MENU;
      break;
    case apps::LaunchSource::kFromOtherApp:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_OTHER_APP;
      break;
    case apps::LaunchSource::kFromInstaller:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_INSTALLER;
      break;
    case apps::LaunchSource::kFromKeyboard:
    case apps::LaunchSource::kFromMenu:
    case apps::LaunchSource::kFromInstalledNotification:
    case apps::LaunchSource::kFromTest:
    case apps::LaunchSource::kFromArc:
    case apps::LaunchSource::kFromReleaseNotesNotification:
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
    case apps::LaunchSource::kFromFirstRun:
    case apps::LaunchSource::kFromWelcomeTour:
    case apps::LaunchSource::kFromFocusMode:
    case apps::LaunchSource::kFromSparky:
    case apps::LaunchSource::kFromNavigationCapturing:
      // These LaunchSources do not launch ARC apps. When adding a new
      // LaunchSource, if it is expected to launch ARC apps, add a new
      // UserInteractionType above. Otherwise, add it here.
      NOTREACHED() << "Must define an ARC UserInteractionType for LaunchSource "
                   << static_cast<uint32_t>(launch_source);
  }
  return user_interaction_type;
}

bool ShouldShow(const ArcAppListPrefs::AppInfo& app_info) {
  return app_info.show_in_launcher;
}

void RequestDomainVerificationStatusUpdate(ArcAppListPrefs* prefs) {
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  arc::mojom::IntentHelperInstance* instance = nullptr;

  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(),
        RequestDomainVerificationStatusUpdate);
  }
  if (!instance) {
    return;
  }
  instance->RequestDomainVerificationStatusUpdate();
}

arc::mojom::ActionType GetArcActionType(const std::string& action) {
  if (action == apps_util::kIntentActionView) {
    return arc::mojom::ActionType::VIEW;
  } else if (action == apps_util::kIntentActionSend) {
    return arc::mojom::ActionType::SEND;
  } else if (action == apps_util::kIntentActionSendMultiple) {
    return arc::mojom::ActionType::SEND_MULTIPLE;
  } else if (action == apps_util::kIntentActionEdit) {
    return arc::mojom::ActionType::EDIT;
  } else {
    return arc::mojom::ActionType::VIEW;
  }
}

// Constructs an OpenUrlsRequest to be passed to
// FileSystemInstance.OpenUrlsWithPermissionAndWindowInfo.
arc::mojom::OpenUrlsRequestPtr ConstructOpenUrlsRequest(
    const apps::IntentPtr& intent,
    const arc::mojom::ActivityNamePtr& activity,
    const std::vector<GURL>& content_urls) {
  arc::mojom::OpenUrlsRequestPtr request = arc::mojom::OpenUrlsRequest::New();
  request->action_type = GetArcActionType(intent->action);
  request->activity_name = activity.Clone();
  DCHECK_EQ(content_urls.size(), intent->files.size());
  for (size_t i = 0; i < content_urls.size(); i++) {
    auto content_url = content_urls[i];
    arc::mojom::ContentUrlWithMimeTypePtr url_with_type =
        arc::mojom::ContentUrlWithMimeType::New();
    url_with_type->content_url = content_url;
    DCHECK(intent->files[i]->mime_type.has_value() ||
           intent->mime_type.has_value());
    // Save the file's original mimetype to the URL if it exists. Otherwise, use
    // the common intent mime type instead.
    url_with_type->mime_type = intent->files[i]->mime_type.has_value()
                                   ? intent->files[i]->mime_type.value()
                                   : intent->mime_type.value();
    request->urls.push_back(std::move(url_with_type));
  }
  if (intent->share_text.has_value() || intent->share_title.has_value() ||
      !intent->extras.empty()) {
    request->extras = apps_util::CreateArcIntentExtras(intent);
  }
  return request;
}

void OnContentUrlResolved(const base::FilePath& file_path,
                          const std::string& app_id,
                          int32_t event_flags,
                          apps::IntentPtr intent,
                          arc::mojom::ActivityNamePtr activity,
                          apps::WindowInfoPtr window_info,
                          base::OnceCallback<void(bool)> callback,
                          const std::vector<GURL>& content_urls) {
  for (const auto& content_url : content_urls) {
    if (!content_url.is_valid()) {
      LOG(ERROR) << "Share files failed, file urls are not valid";
      std::move(callback).Run(/*success=*/false);
      return;
    }
  }

  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  DCHECK(window_info);
  int32_t session_id = window_info->window_id;
  int64_t display_id = window_info->display_id;

  arc::mojom::FileSystemInstance* arc_file_system = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->file_system(),
      OpenUrlsWithPermissionAndWindowInfo);
  if (!arc_file_system) {
    LOG(ERROR) << "Failed to open urls, ARC File System not found";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  arc_file_system->OpenUrlsWithPermissionAndWindowInfo(
      ConstructOpenUrlsRequest(intent, activity, content_urls),
      apps::MakeArcWindowInfo(std::move(window_info)), base::DoNothing());

  ::full_restore::SaveAppLaunchInfo(
      file_path,
      std::make_unique<app_restore::AppLaunchInfo>(
          app_id, event_flags, std::move(intent), session_id, display_id));

  std::move(callback).Run(/*success=*/true);
}

// Sets the session id for |window_info|. If the full restore feature is
// disabled, or the session id has been set, returns |window_info|. Otherwise,
// fetches a new ARC session id, and sets to window_id for |window_info|.
apps::WindowInfoPtr SetSessionId(apps::WindowInfoPtr window_info) {
  if (!window_info) {
    window_info =
        std::make_unique<apps::WindowInfo>(display::kInvalidDisplayId);
  }

  if (window_info->window_id != -1) {
    return window_info;
  }

  window_info->window_id =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  return window_info;
}

std::optional<bool> GetResizeLocked(ArcAppListPrefs* prefs,
                                    const std::string& app_id) {
  // Set null to resize lock state until the Mojo connection to ARC++ has been
  // established. This prevents Chrome and ARC++ from having inconsistent
  // states.
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    return std::nullopt;
  }

  // If we don't have the connection (e.g. for non-supported Android versions),
  // returns null.
  auto* compatibility_mode =
      arc_service_manager->arc_bridge_service()->compatibility_mode();
  if (!compatibility_mode->IsConnected()) {
    return std::nullopt;
  }

  // Check if |SetResizeLockState| is available to see if Android is ready to
  // be synchronized. Otherwise we need to hide the corresponding setting by
  // returning null.
  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(compatibility_mode, SetResizeLockState);
  if (!instance) {
    return std::nullopt;
  }

  auto resize_lock_state = prefs->GetResizeLockState(app_id);
  switch (resize_lock_state) {
    case arc::mojom::ArcResizeLockState::ON:
      return true;
    case arc::mojom::ArcResizeLockState::OFF:
      return false;
    case arc::mojom::ArcResizeLockState::UNDEFINED:
    case arc::mojom::ArcResizeLockState::READY:
    // FULLY_LOCKED means the resize-lock-related features are not available
    // including the resizability toggle in the app management page.
    case arc::mojom::ArcResizeLockState::FULLY_LOCKED:
      return std::nullopt;
  }
}

bool IsWebAppShellPackage(Profile* profile,
                          const ArcAppListPrefs::AppInfo& app_info) {
  ash::ApkWebAppService* apk_web_app_service =
      ash::ApkWebAppService::Get(profile);
  return apk_web_app_service &&
         apk_web_app_service->IsWebAppShellPackage(app_info.package_name);
}

bool IntentHasFilesAndMimeTypes(const apps::IntentPtr& intent) {
  if (intent->files.empty()) {
    return false;
  }
  bool all_files_have_mime_type = base::ranges::all_of(
      intent->files,
      [](apps::IntentFilePtr& file) { return file->mime_type.has_value(); });
  return all_files_have_mime_type || intent->mime_type.has_value();
}

// Returns true if the app with the given |app_id| should open supported links
// inside the app by default.
bool AppShouldDefaultHandleLinksInApp(const std::string& app_id) {
  // Play Store provides core system functionality and should handle links
  // inside the app rather than in the browser.
  return app_id == arc::kPlayStoreAppId;
}

// Returns true if the package with the given |package_name| should open
// supported links inside the browser by default, on managed devices.
bool PackageShouldDefaultHandleLinksInBrowser(const std::string& package_name) {
  constexpr auto allowlist = base::MakeFixedFlatSet<std::string_view>({
      "com.google.android.apps.docs",                 // Google Drive
      "com.google.android.apps.docs.editors.docs",    // Google Docs
      "com.google.android.apps.docs.editors.sheets",  // Google Sheets
      "com.google.android.apps.docs.editors.slides",  // Google Slides
  });

  return allowlist.contains(package_name);
}

// Returns the value of the policy ArcOpenLinksInBrowserByDefault.
// For managed users it is false by default, for consumer accounts it is always
// true.
bool IsArcOpenLinksInBrowserByDefault(Profile* profile) {
  return !profile->GetProfilePolicyConnector()->IsManaged() ||
         profile->GetPrefs()->GetBoolean(
             arc::prefs::kArcOpenLinksInBrowserByDefault);
}

// Returns the hard-coded Play Store intent filters. This is a stop-gap solution
// to handle Play Store URLs before ARC gets ready.
// TODO(b/259205050): Remove this once intent filters are properly cached.
std::vector<apps::IntentFilterPtr> GetHardcodedPlayStoreIntentFilters() {
  const std::vector<std::string> actions = {arc::kIntentActionView};
  const std::vector<std::string> schemes = {"http", "https"};
  const std::vector<std::string> mime_types;

  std::vector<arc::IntentFilter::AuthorityEntry> authorities;
  authorities.emplace_back("play.google.com", -1);

  std::vector<arc::IntentFilter::PatternMatcher> paths;
  paths.emplace_back("", arc::mojom::PatternType::PATTERN_LITERAL);
  paths.emplace_back("/", arc::mojom::PatternType::PATTERN_LITERAL);
  paths.emplace_back("/store", arc::mojom::PatternType::PATTERN_PREFIX);
  paths.emplace_back("/redeem", arc::mojom::PatternType::PATTERN_PREFIX);
  paths.emplace_back("/wishlist", arc::mojom::PatternType::PATTERN_PREFIX);
  paths.emplace_back("/apps/test/", arc::mojom::PatternType::PATTERN_PREFIX);
  paths.emplace_back("/apps", arc::mojom::PatternType::PATTERN_LITERAL);
  paths.emplace_back("/apps/launch", arc::mojom::PatternType::PATTERN_LITERAL);
  paths.emplace_back("/protect/home", arc::mojom::PatternType::PATTERN_PREFIX);

  std::vector<apps::IntentFilterPtr> intent_filters;
  apps::IntentFilterPtr filter = apps_util::CreateIntentFilterForArc(
      arc::IntentFilter(arc::kPlayStorePackage, actions, std::move(authorities),
                        std::move(paths), schemes, mime_types));
  if (filter) {
    intent_filters.push_back(std::move(filter));
  }
  return intent_filters;
}

apps::InstallReason GetInstallReason(const ArcAppListPrefs* prefs,
                                     const std::string& app_id,
                                     const ArcAppListPrefs::AppInfo& app_info) {
  if (prefs->IsControlledByPolicy(app_info.package_name)) {
    return apps::InstallReason::kPolicy;
  }

  // Sticky represents apps that cannot be uninstalled and are installed by the
  // system. Policy installed apps are also considered sticky, so kPolicy must
  // be first.
  if (app_info.sticky) {
    return apps::InstallReason::kSystem;
  }

  if (prefs->IsOem(app_id)) {
    return apps::InstallReason::kOem;
  }

  if (prefs->IsDefault(app_id)) {
    return apps::InstallReason::kDefault;
  }

  return apps::InstallReason::kUser;
}

bool ArcVersionEligibleForPromiseIcons() {
  return g_test_arc_version_.value_or(arc::GetArcAndroidSdkVersionAsInt()) >=
         arc::kArcVersionR;
}

}  // namespace

namespace apps {

void ArcApps::SetArcVersionForTesting(int version) {
  g_test_arc_version_ = version;
}

// static
ArcApps* ArcApps::Get(Profile* profile) {
  return ArcAppsFactory::GetForProfile(profile);
}

ArcApps::ArcApps(AppServiceProxy* proxy)
    : AppPublisher(proxy), profile_(proxy->profile()) {}

ArcApps::~ArcApps() {
  proxy()->UnregisterPublisher(AppType::kArc);
}

void ArcApps::Initialize() {
  if (!arc::IsArcAllowedForProfile(profile_) ||
      (arc::ArcServiceManager::Get() == nullptr)) {
    return;
  }

  // Make some observee-observer connections.
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  prefs->AddObserver(this);
  proxy()->SetArcIsRegistered();

  auto* intent_helper_bridge =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (intent_helper_bridge) {
    intent_helper_bridge->SetAdaptiveIconDelegate(
        &arc_activity_adaptive_icon_impl_);
    arc_intent_helper_observation_.Observe(intent_helper_bridge);
  }

  // There is no MessageCenterController for unit tests, so observe when the
  // MessageCenterController is created in production code.
  if (ash::ArcNotificationsHostInitializer::Get()) {
    notification_initializer_observation_.Observe(
        ash::ArcNotificationsHostInitializer::Get());
  }

  auto* arc_bridge_service =
      arc::ArcPrivacyItemsBridge::GetForBrowserContext(profile_);
  if (arc_bridge_service) {
    arc_privacy_items_bridge_observation_.Observe(arc_bridge_service);
  }

  auto* instance_registry = &proxy()->InstanceRegistry();
  if (instance_registry) {
    instance_registry_observation_.Observe(instance_registry);
  }

  if (web_app::AreWebAppsEnabled(profile_)) {
    web_apk_manager_ = std::make_unique<apps::WebApkManager>(profile_);
  }

  RegisterPublisher(AppType::kArc);

  std::vector<AppPtr> apps;
  for (const auto& app_id : prefs->GetAppIds()) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (app_info) {
      apps.push_back(CreateApp(prefs, app_id, *app_info));
    }
  }
  AppPublisher::Publish(std::move(apps), AppType::kArc,
                        /*should_notify_initialized=*/true);

  ObserveDisabledSystemFeaturesPolicy();
}

void ArcApps::Shutdown() {
  // Disconnect the observee-observer connections that we made during the
  // constructor.
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (prefs) {
    prefs->RemoveObserver(this);
  }

  auto* intent_helper_bridge =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (intent_helper_bridge) {
    intent_helper_bridge->SetAdaptiveIconDelegate(nullptr);
  }

  arc_intent_helper_observation_.Reset();
  arc_privacy_items_bridge_observation_.Reset();
}

void ArcApps::GetCompressedIconData(const std::string& app_id,
                                    int32_t size_in_dip,
                                    ui::ResourceScaleFactor scale_factor,
                                    LoadIconCallback callback) {
  GetArcAppCompressedIconData(profile_, app_id, size_in_dip, scale_factor,
                              std::move(callback));
}

void ArcApps::Launch(const std::string& app_id,
                     int32_t event_flags,
                     LaunchSource launch_source,
                     WindowInfoPtr window_info) {
  auto user_interaction_type = GetUserInterationType(launch_source);
  if (!user_interaction_type.has_value()) {
    return;
  }

  if (app_id == arc::kPlayStoreAppId &&
      apps_util::IsHumanLaunch(launch_source)) {
    arc::RecordPlayStoreLaunchWithinAWeek(profile_->GetPrefs(),
                                          /*launched=*/true);
  }

  auto new_window_info = SetSessionId(std::move(window_info));
  int32_t session_id = new_window_info->window_id;
  int64_t display_id = new_window_info->display_id;

  arc::LaunchApp(profile_, app_id, event_flags, user_interaction_type.value(),
                 MakeArcWindowInfo(std::move(new_window_info)));

  full_restore::SaveAppLaunchInfo(
      profile_->GetPath(), std::make_unique<app_restore::AppLaunchInfo>(
                               app_id, event_flags, session_id, display_id));
}

void ArcApps::LaunchAppWithIntent(const std::string& app_id,
                                  int32_t event_flags,
                                  IntentPtr intent,
                                  LaunchSource launch_source,
                                  WindowInfoPtr window_info,
                                  LaunchCallback callback) {
  auto user_interaction_type = GetUserInterationType(launch_source);
  if (!user_interaction_type.has_value()) {
    std::move(callback).Run(LaunchResult(State::kFailed));
    return;
  }

  if (app_id == arc::kPlayStoreAppId &&
      apps_util::IsHumanLaunch(launch_source)) {
    arc::RecordPlayStoreLaunchWithinAWeek(profile_->GetPrefs(),
                                          /*launched=*/true);
  }

  arc::ArcMetricsService::RecordArcUserInteraction(
      profile_, user_interaction_type.value());

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    std::move(callback).Run(LaunchResult(State::kFailed));
    return;
  }
  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(app_id);
  if (!app_info) {
    LOG(ERROR) << "Launch App failed, could not find app with id " << app_id;
    std::move(callback).Run(LaunchResult(State::kFailed));
    return;
  }

  arc::mojom::ActivityNamePtr activity = arc::mojom::ActivityName::New();
  activity->package_name = app_info->package_name;
  if (intent->activity_name.has_value() &&
      !intent->activity_name.value().empty()) {
    activity->activity_name = intent->activity_name.value();
  }

  auto new_window_info = SetSessionId(std::move(window_info));
  int32_t session_id = new_window_info->window_id;
  int64_t display_id = new_window_info->display_id;

  // Check if the intent has files, and whether the intent has a mime type or
  // all the individual files have mime types.
  if (IntentHasFilesAndMimeTypes(intent)) {
    if (app_info->ready) {
      std::vector<GURL> file_urls;
      for (const auto& file : intent->files) {
        file_urls.push_back(file->url);
      }
      arc::ConvertToContentUrlsAndShare(
          profile_, apps::GetFileSystemURL(profile_, file_urls),
          base::BindOnce(&OnContentUrlResolved, profile_->GetPath(), app_id,
                         event_flags, std::move(intent), std::move(activity),
                         std::move(new_window_info),
                         base::BindOnce(
                             [](LaunchCallback callback, bool success) {
                               std::move(callback).Run(
                                   ConvertBoolToLaunchResult(success));
                             },
                             std::move(callback))));
      return;
    }
  } else {
    auto intent_for_full_restore = intent->Clone();

    if (!arc::LaunchAppWithIntent(
            profile_, app_id, std::move(intent), event_flags,
            user_interaction_type.value(),
            MakeArcWindowInfo(std::move(new_window_info)))) {
      VLOG(2) << "Failed to launch app: " + app_id + ".";
      std::move(callback).Run(LaunchResult(State::kFailed));
      return;
    }

    full_restore::SaveAppLaunchInfo(
        profile_->GetPath(),
        std::make_unique<app_restore::AppLaunchInfo>(
            app_id, event_flags, std::move(intent_for_full_restore), session_id,
            display_id));
    std::move(callback).Run(LaunchResult(State::kSuccess));
    return;
  }

  if (arc::IsArcPlayStoreEnabledForProfile(profile_)) {
    // Handle the case when default app tries to re-activate OptIn flow.
    if (arc::IsArcPlayStoreEnabledPreferenceManagedForProfile(profile_) &&
        !arc::ArcSessionManager::Get()->enable_requested() &&
        prefs->IsDefault(app_id)) {
      arc::SetArcPlayStoreEnabledForProfile(profile_, true);
      // PlayStore item has special handling for shelf controllers. In order
      // to avoid unwanted initial animation for PlayStore item do not create
      // deferred launch request when PlayStore item enables Google Play
      // Store.
      if (app_id == arc::kPlayStoreAppId) {
        prefs->SetLastLaunchTime(app_id);
        std::move(callback).Run(LaunchResult(State::kSuccess));
        return;
      }
    }
  } else {
    if (prefs->IsDefault(app_id)) {
      // The setting can fail if the preference is managed.  However, the
      // caller is responsible to not call this function in such case.  DCHECK
      // is here to prevent possible mistake.
      if (!arc::SetArcPlayStoreEnabledForProfile(profile_, true)) {
        std::move(callback).Run(LaunchResult(State::kFailed));
        return;
      }
      DCHECK(arc::IsArcPlayStoreEnabledForProfile(profile_));

      // PlayStore item has special handling for shelf controllers. In order
      // to avoid unwanted initial animation for PlayStore item do not create
      // deferred launch request when PlayStore item enables Google Play
      // Store.
      if (app_id == arc::kPlayStoreAppId) {
        prefs->SetLastLaunchTime(app_id);
        std::move(callback).Run(LaunchResult(State::kFailed));
        return;
      }
    } else {
      // Only reachable when ARC always starts.
      DCHECK(arc::ShouldArcAlwaysStart());
    }
  }
  std::move(callback).Run(LaunchResult(State::kSuccess));
}

void ArcApps::LaunchAppWithParams(AppLaunchParams&& params,
                                  LaunchCallback callback) {
  auto event_flags = apps::GetEventFlags(params.disposition,
                                         /*prefer_container=*/false);
  if (params.intent) {
    LaunchAppWithIntent(params.app_id, event_flags, std::move(params.intent),
                        params.launch_source,
                        std::make_unique<WindowInfo>(params.display_id),
                        std::move(callback));
  } else {
    Launch(params.app_id, event_flags, params.launch_source,
           std::make_unique<WindowInfo>(params.display_id));
    // TODO(crbug.com/40787924): Add launch return value.
    std::move(callback).Run(LaunchResult());
  }
}

void ArcApps::LaunchShortcut(const std::string& app_id,
                             const std::string& shortcut_id,
                             int64_t display_id) {
  arc::ExecuteArcShortcutCommand(profile_, app_id, shortcut_id, display_id);
}

void ArcApps::SetPermission(const std::string& app_id,
                            PermissionPtr permission) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(app_id);
  if (!app_info) {
    LOG(ERROR) << "SetPermission failed, could not find app with id " << app_id;
    return;
  }

  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    LOG(WARNING) << "SetPermission failed, ArcServiceManager not available.";
    return;
  }

  // TODO(crbug.com/40760689): Add unknown type for arc permissions enum.
  arc::mojom::AppPermission permission_type = arc::mojom::AppPermission::CAMERA;

  if (!GetArcPermissionType(permission->permission_type, permission_type)) {
    LOG(ERROR) << "SetPermission failed, permission type not supported by ARC.";
    return;
  }

  if (permission->IsPermissionEnabled()) {
    auto* permissions_instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->app_permissions(),
        GrantPermission);
    if (permissions_instance) {
      permissions_instance->GrantPermission(app_info->package_name,
                                            permission_type);
    }
  } else {
    auto* permissions_instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->app_permissions(),
        RevokePermission);
    if (permissions_instance) {
      permissions_instance->RevokePermission(app_info->package_name,
                                             permission_type);
    }
  }
}

void ArcApps::Uninstall(const std::string& app_id,
                        UninstallSource uninstall_source,
                        bool clear_site_data,
                        bool report_abuse) {
  arc::UninstallArcApp(app_id, profile_);
}

void ArcApps::GetMenuModel(const std::string& app_id,
                           MenuType menu_type,
                           int64_t display_id,
                           base::OnceCallback<void(MenuItems)> callback) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    std::move(callback).Run(MenuItems());
    return;
  }
  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(app_id);
  if (!app_info) {
    std::move(callback).Run(MenuItems());
    return;
  }

  MenuItems menu_items;

  // Add Open item if the app is not opened and not suspended.
  if (!base::Contains(app_id_to_task_ids_, app_id) && !app_info->suspended) {
    AddCommandItem(ash::LAUNCH_NEW, IDS_APP_CONTEXT_MENU_ACTIVATE_ARC,
                   menu_items);
  }

  if (app_info->shortcut) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_REMOVE_SHORTCUT, menu_items);
  } else if (app_info->ready && !app_info->sticky) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM, menu_items);
  }

  // App Info item.
  if (app_info->ready && ShouldShow(*app_info)) {
    AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                   menu_items);
  }

  if (menu_type == MenuType::kShelf &&
      base::Contains(app_id_to_task_ids_, app_id)) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, menu_items);
  }

  BuildMenuForShortcut(app_info->package_name, std::move(menu_items),
                       std::move(callback));
}

void ArcApps::SetResizeLocked(const std::string& app_id, bool locked) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  prefs->SetResizeLockState(app_id, locked
                                        ? arc::mojom::ArcResizeLockState::ON
                                        : arc::mojom::ArcResizeLockState::OFF);
}

void ArcApps::SetAppLocale(const std::string& app_id,
                           const std::string& locale_tag) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!profile_->GetPrefs() || !prefs) {
    return;
  }
  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(app_id);
  if (!app_info) {
    LOG(ERROR) << "SetAppLocale failed, could not find app with id " << app_id;
    return;
  }
  if (app_info->package_name.empty()) {
    LOG(ERROR) << "SetAppLocale failed, package name is empty with app_id "
               << app_id;
    return;
  }
  // Set app locale and update last-set app locale.
  arc::mojom::AppInstance* app_instance =
      (arc::ArcServiceManager::Get()
           ? ARC_GET_INSTANCE_FOR_METHOD(
                 arc::ArcServiceManager::Get()->arc_bridge_service()->app(),
                 SetAppLocale)
           : nullptr);
  if (app_instance) {
    app_instance->SetAppLocale(app_info->package_name, locale_tag);
  } else {
    // If AppInstance is not ready, we still want to update the prefs to ensure
    // good UX. To ensure eventual-correctness between ARC settings and Chrome
    // settings, on ARC boot, ARC will always sends its latest-set locale to
    // Chrome. If there's a mismatch, Chrome will then send back its latest-set
    // locale to ARC, both settings are still synchronized.
    prefs->SetAppLocale(app_info->package_name, locale_tag);
  }
  // Update the last-set locale, unless the locale tag is the system language.
  if (!locale_tag.empty()) {
    profile_->GetPrefs()->SetString(arc::prefs::kArcLastSetAppLocale,
                                    locale_tag);
  }
}

void ArcApps::PauseApp(const std::string& app_id) {
  if (paused_apps_.MaybeAddApp(app_id)) {
    SetIconEffect(app_id);
  }

  AppPublisher::Publish(paused_apps_.CreateAppWithPauseStatus(
      AppType::kArc, app_id, /*paused=*/true));
  CloseTasks(app_id);
}

void ArcApps::UnpauseApp(const std::string& app_id) {
  if (paused_apps_.MaybeRemoveApp(app_id)) {
    SetIconEffect(app_id);
  }

  AppPublisher::Publish(paused_apps_.CreateAppWithPauseStatus(
      AppType::kArc, app_id, /*paused=*/false));
}

void ArcApps::BlockApp(const std::string& app_id) {
  if (base::Contains(blocked_app_ids_, app_id)) {
    return;
  }

  blocked_app_ids_.insert(app_id);

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  CHECK(prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (!app_info) {
    return;
  }

  auto app = std::make_unique<App>(AppType::kArc, app_id);
  app->readiness = GetReadiness(app_id, *app_info);
  app->icon_key = IconKey(GetIconEffects(app_id, *app_info));
  AppPublisher::Publish(std::move(app));

  CloseTasks(app_id);
}

void ArcApps::UnblockApp(const std::string& app_id) {
  if (!base::Contains(blocked_app_ids_, app_id)) {
    return;
  }

  blocked_app_ids_.erase(app_id);

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  CHECK(prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (!app_info) {
    return;
  }

  auto app = std::make_unique<App>(AppType::kArc, app_id);
  app->readiness = GetReadiness(app_id, *app_info);
  app->icon_key = IconKey(GetIconEffects(app_id, *app_info));
  AppPublisher::Publish(std::move(app));
}

void ArcApps::StopApp(const std::string& app_id) {
  CloseTasks(app_id);
}

void ArcApps::UpdateAppSize(const std::string& app_id) {
  arc::mojom::AppInstance* app_instance =
      (arc::ArcServiceManager::Get()
           ? ARC_GET_INSTANCE_FOR_METHOD(
                 arc::ArcServiceManager::Get()->arc_bridge_service()->app(),
                 UpdateAppDetails)
           : nullptr);
  if (!app_instance) {
    return;
  }
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(app_id);
  if (!app_info) {
    return;
  }
  if (app_info->package_name.empty()) {
    return;
  }

  // A request is made to simultaneously update all of the app's details,
  // inclusive of the app size, for simplicity
  app_instance->UpdateAppDetails(app_info->package_name);
}

void ArcApps::ExecuteContextMenuCommand(const std::string& app_id,
                                        int command_id,
                                        const std::string& shortcut_id,
                                        int64_t display_id) {
  arc::ExecuteArcShortcutCommand(profile_, app_id, shortcut_id, display_id);
}

void ArcApps::OpenNativeSettings(const std::string& app_id) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(app_id);
  if (!app_info) {
    LOG(ERROR) << "Cannot open native settings for " << app_id
               << ". App is not found.";
    return;
  }
  if (app_info->package_name.empty()) {
    LOG(ERROR) << "Cannot open native settings for " << app_id
               << ". Package name is empty.";
    return;
  }
  const auto page = arc::IsReadOnlyPermissionsEnabled()
                        ? arc::mojom::ShowPackageInfoPage::MANAGE_PERMISSIONS
                        : arc::mojom::ShowPackageInfoPage::MAIN;
  arc::ShowPackageInfo(app_info->package_name, page,
                       display::Screen::GetScreen()->GetPrimaryDisplay().id());
}

void ArcApps::OnSupportedLinksPreferenceChanged(const std::string& app_id,
                                                bool open_in_app) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (!app_info) {
    return;
  }

  arc::mojom::IntentHelperInstance* instance = nullptr;
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(),
        SetVerifiedLinks);
  }
  if (!instance) {
    return;
  }

  instance->SetVerifiedLinks({app_info->package_name}, open_in_app);
}

void ArcApps::OnAppRegistered(const std::string& app_id,
                              const ArcAppListPrefs::AppInfo& app_info) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (prefs && !IsWebAppShellPackage(profile_, app_info)) {
    AppPublisher::Publish(CreateApp(prefs, app_id, app_info));
  }
}

void ArcApps::OnAppStatesChanged(const std::string& app_id,
                                 const ArcAppListPrefs::AppInfo& app_info) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs || IsWebAppShellPackage(profile_, app_info)) {
    return;
  }

  AppPublisher::Publish(CreateApp(prefs, app_id, app_info));
}

void ArcApps::OnAppRemoved(const std::string& app_id) {
  app_notifications_.RemoveNotificationsForApp(app_id);
  paused_apps_.MaybeRemoveApp(app_id);
  blocked_app_ids_.erase(app_id);

  if (base::Contains(app_id_to_task_ids_, app_id)) {
    for (int task_id : app_id_to_task_ids_[app_id]) {
      task_id_to_app_id_.erase(task_id);
    }
    app_id_to_task_ids_.erase(app_id);
  }

  auto app = std::make_unique<App>(AppType::kArc, app_id);
  app->readiness = Readiness::kUninstalledByUser;
  AppPublisher::Publish(std::move(app));
}

void ArcApps::OnAppNameUpdated(const std::string& app_id,
                               const std::string& name) {
  auto app = std::make_unique<App>(AppType::kArc, app_id);
  app->name = name;
  AppPublisher::Publish(std::move(app));
}

void ArcApps::OnAppLastLaunchTimeUpdated(const std::string& app_id) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (!app_info || IsWebAppShellPackage(profile_, *app_info)) {
    return;
  }

  auto app = std::make_unique<App>(AppType::kArc, app_id);
  app->last_launch_time = app_info->last_launch_time;
  AppPublisher::Publish(std::move(app));
}

void ArcApps::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  ConvertAndPublishPackageApps(package_info);
}

void ArcApps::OnPackageModified(
    const arc::mojom::ArcPackageInfo& package_info) {
  static constexpr bool update_icon = false;
  ConvertAndPublishPackageApps(package_info, update_icon);
}

void ArcApps::OnPackageListInitialRefreshed() {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  // This method is called when ARC++ finishes booting. Do not update the icon;
  // it should be impossible for the icon to have changed since ARC++ has not
  // been running.
  static constexpr bool update_icon = false;
  for (const auto& app_id : prefs->GetAppIds()) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (app_info && !IsWebAppShellPackage(profile_, *app_info)) {
      AppPublisher::Publish(CreateApp(prefs, app_id, *app_info, update_icon));
    }
  }
}

void ArcApps::OnTaskCreated(int32_t task_id,
                            const std::string& package_name,
                            const std::string& activity,
                            const std::string& intent,
                            int32_t session_id) {
  const std::string app_id = ArcAppListPrefs::GetAppId(package_name, activity);
  app_id_to_task_ids_[app_id].insert(task_id);
  task_id_to_app_id_[task_id] = app_id;
}

void ArcApps::OnTaskDestroyed(int32_t task_id) {
  auto it = task_id_to_app_id_.find(task_id);
  if (it == task_id_to_app_id_.end()) {
    return;
  }

  const std::string app_id = it->second;
  task_id_to_app_id_.erase(it);
  DCHECK(base::Contains(app_id_to_task_ids_, app_id));
  app_id_to_task_ids_[app_id].erase(task_id);
  if (app_id_to_task_ids_[app_id].empty()) {
    app_id_to_task_ids_.erase(app_id);
  }
}

void ArcApps::OnIntentFiltersUpdated(
    const std::optional<std::string>& package_name) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }

  auto GetAppInfoAndPublish = [prefs, this](std::string app_id) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (app_info) {
      AppPublisher::Publish(
          CreateApp(prefs, app_id, *app_info, false /* update_icon */));
    }
  };

  // If there is no specific package_name, update all apps, otherwise update
  // apps for the package.

  // Note: Cannot combine the two for-loops because the return type of
  // GetAppIds() is std::vector<std::string> and the return type of
  // GetAppsForPackage() is std::unordered_set<std::string>.
  if (package_name == std::nullopt) {
    for (const auto& app_id : prefs->GetAppIds()) {
      GetAppInfoAndPublish(app_id);
    }
  } else {
    for (const auto& app_id : prefs->GetAppsForPackage(package_name.value())) {
      GetAppInfoAndPublish(app_id);
    }
  }
}

void ArcApps::OnArcSupportedLinksChanged(
    const std::vector<arc::mojom::SupportedLinksPackagePtr>& added,
    const std::vector<arc::mojom::SupportedLinksPackagePtr>& removed,
    arc::mojom::SupportedLinkChangeSource source) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }

  for (const auto& supported_link : added) {
    std::string app_id =
        prefs->GetAppIdByPackageName(supported_link->package_name);
    if (app_id.empty()) {
      continue;
    }

    // ARC apps may handle links by default on the ARC side, but do not handle
    // links by default on the Ash side. This means that the default setting may
    // be different between Ash and ARC. Any user action to change the setting
    // will make it the same between both sides.
    //
    // To make this work, we need to ignore request from the ARC system to
    // update the supported links setting. We allow updates in the following
    // cases:
    bool allow_update =
        // When the user explicitly changes the setting in Android Settings.
        source == arc::mojom::SupportedLinkChangeSource::kUserPreference ||
        // If the app is already marked as preferred on the Ash side.
        proxy()->PreferredAppsList().IsPreferredAppForSupportedLinks(app_id) ||
        // If the app is specifically allowed to handle links by default.
        AppShouldDefaultHandleLinksInApp(app_id);

    // Managed users apply updates from the ARC side by default, except for an
    // allowlist of apps which handle links in the browser to improve the user
    // experience. This policy that can be changed by enterprise admin.
    if (!IsArcOpenLinksInBrowserByDefault(profile_) &&
        !PackageShouldDefaultHandleLinksInBrowser(
            supported_link->package_name)) {
      allow_update = true;
    }

    if (!allow_update) {
      continue;
    }

    proxy()->SetSupportedLinksPreference(app_id);
  }

  for (const auto& supported_link : removed) {
    std::string app_id =
        prefs->GetAppIdByPackageName(supported_link->package_name);
    if (app_id.empty()) {
      continue;
    }
    proxy()->RemoveSupportedLinksPreference(app_id);
  }
}

void ArcApps::OnSetArcNotificationsInstance(
    ash::ArcNotificationManagerBase* arc_notification_manager) {
  DCHECK(arc_notification_manager);
  notification_observation_.Observe(arc_notification_manager);
}

void ArcApps::OnArcNotificationInitializerDestroyed(
    ash::ArcNotificationsHostInitializer* initializer) {
  DCHECK(notification_initializer_observation_.IsObservingSource(initializer));
  notification_initializer_observation_.Reset();
}

void ArcApps::OnNotificationUpdated(const std::string& notification_id,
                                    const std::string& app_id) {
  if (app_id.empty()) {
    return;
  }

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }

  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(app_id);
  if (!app_info) {
    return;
  }

  app_notifications_.AddNotification(app_id, notification_id);
  AppPublisher::Publish(
      app_notifications_.CreateAppWithHasBadgeStatus(AppType::kArc, app_id));
}

void ArcApps::OnNotificationRemoved(const std::string& notification_id) {
  const auto app_ids =
      app_notifications_.GetAppIdsForNotification(notification_id);
  if (app_ids.empty()) {
    return;
  }

  app_notifications_.RemoveNotification(notification_id);

  for (const auto& app_id : app_ids) {
    AppPublisher::Publish(
        app_notifications_.CreateAppWithHasBadgeStatus(AppType::kArc, app_id));
  }
}

void ArcApps::OnArcNotificationManagerDestroyed(
    ash::ArcNotificationManagerBase* notification_manager) {
  DCHECK(notification_observation_.IsObservingSource(notification_manager));
  notification_observation_.Reset();
}

void ArcApps::OnPrivacyItemsChanged(
    const std::vector<arc::mojom::PrivacyItemPtr>& privacy_items) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }

  // Get the existing accessing app ids from `accessing_apps_`, and set all of
  // them as false to explicitly update `AppCapabilityAccessCache` to ensure the
  // access is stopped when they are not list in `privacy_items`. If they are
  // still accessing, they will exist in `privacy_items`, and be set as true in
  // the next loop for `privacy_items`.
  base::flat_map<std::string, CapabilityAccessPtr> capability_accesses;
  for (const auto& app_id : accessing_apps_) {
    auto access = std::make_unique<CapabilityAccess>(app_id);
    access->app_id = app_id;
    access->camera = false;
    access->microphone = false;
    capability_accesses[app_id] = std::move(access);
  }
  accessing_apps_.clear();

  // Check the new items in `privacy_items`, and update `capability_accesses` to
  // set the access item as true, if the camera or the microphone is still in
  // use.
  for (const auto& item : privacy_items) {
    arc::mojom::AppPermissionGroup permission = item->permission_group;
    if (permission != arc::mojom::AppPermissionGroup::CAMERA &&
        permission != arc::mojom::AppPermissionGroup::MICROPHONE) {
      continue;
    }

    auto package_name = item->privacy_application->package_name;
    for (const auto& app_id : prefs->GetAppsForPackage(package_name)) {
      accessing_apps_.insert(app_id);
      auto [it, inserted] = capability_accesses.try_emplace(
          app_id, std::make_unique<CapabilityAccess>(app_id));
      if (permission == arc::mojom::AppPermissionGroup::CAMERA) {
        it->second->camera = true;
      }
      if (permission == arc::mojom::AppPermissionGroup::MICROPHONE) {
        it->second->microphone = true;
      }
    }
  }

  // Write the record to `AppCapabilityAccessCache`.
  std::vector<CapabilityAccessPtr> accesses;
  for (auto& item : capability_accesses) {
    accesses.push_back(std::move(item.second));
  }
  proxy()->OnCapabilityAccesses(std::move(accesses));
}

void ArcApps::OnInstanceUpdate(const apps::InstanceUpdate& update) {
  if (!update.StateChanged()) {
    return;
  }
  if (update.AppId() != arc::kSettingsAppId) {
    return;
  }
  if (update.State() & apps::InstanceState::kActive) {
    settings_app_is_active_ = true;
  } else if (settings_app_is_active_) {
    settings_app_is_active_ = false;
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
    if (!prefs) {
      return;
    }
    RequestDomainVerificationStatusUpdate(prefs);
  }
}

void ArcApps::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* instance_registry) {
  DCHECK(instance_registry_observation_.IsObservingSource(instance_registry));
  instance_registry_observation_.Reset();
}

AppPtr ArcApps::CreateApp(ArcAppListPrefs* prefs,
                          const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info,
                          bool update_icon,
                          bool raw_icon_updated) {
  auto install_reason = GetInstallReason(prefs, app_id, app_info);
  auto app = AppPublisher::MakeApp(
      AppType::kArc, app_id, GetReadiness(app_id, app_info), app_info.name,
      install_reason,
      install_reason == InstallReason::kSystem ? InstallSource::kSystem
                                               : InstallSource::kPlayStore);

  app->publisher_id = app_info.package_name;
  app->installer_package_id =
      PackageId(PackageType::kArc, app_info.package_name);
  app->policy_ids = {app_info.package_name};

  if (update_icon) {
    app->icon_key = IconKey(raw_icon_updated, GetIconEffects(app_id, app_info));
  }

  app->version = app_info.version_name;

  app->last_launch_time = app_info.last_launch_time;
  app->install_time = app_info.install_time;

  std::unique_ptr<ArcAppListPrefs::PackageInfo> package =
      prefs->GetPackage(app_info.package_name);
  if (package) {
    app->permissions = CreatePermissions(package->permissions);
    if (package->locale_info) {
      app->supported_locales = package->locale_info->supported_locales;
      app->selected_locale = package->locale_info->selected_locale;
    }
  }

  auto show = ShouldShow(app_info);

  // All published ARC apps are launchable. All launchable apps should be
  // permitted to be shown on the shelf, and have their pins on the shelf
  // persisted.
  app->show_in_shelf = true;
  app->show_in_launcher = show;

  if (app_id == arc::kPlayGamesAppId && !show) {
    // Play Games should only be hidden in the launcher.
    app->show_in_search = true;
    app->show_in_management = true;
  } else {
    app->show_in_search = show;
    app->show_in_management = show;
  }

  // Package Installer is hidden from the launcher, search and management but
  // should still handle intents.
  if (app_id == arc::kPackageInstallerAppId) {
    app->handles_intents = true;
  } else {
    app->handles_intents = show;
  }

  app->allow_uninstall = app_info.ready && !app_info.sticky;
  app->allow_close = true;

  app->has_badge = app_notifications_.HasNotification(app_id);
  app->paused = paused_apps_.IsPaused(app_id);

  auto* intent_helper_bridge =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (intent_helper_bridge &&
      app_info.package_name != arc::kArcIntentHelperPackageName) {
    app->intent_filters = apps_util::CreateIntentFiltersFromArcBridge(
        app_info.package_name, intent_helper_bridge);
  }

  // Set hard-coded Play Store intent filters if not set. This is a stop-gap
  // solution to handle Play Store URLs before ARC gets ready.
  // TODO(b/259205050): Remove this once intent filters are properly cached.
  if (app->intent_filters.empty() && app_id == arc::kPlayStoreAppId) {
    app->intent_filters = GetHardcodedPlayStoreIntentFilters();
  }

  app->resize_locked = GetResizeLocked(prefs, app_id);

  app->app_size_in_bytes = app_info.app_size_in_bytes;
  app->data_size_in_bytes = app_info.data_size_in_bytes;

  return app;
}

void ArcApps::ConvertAndPublishPackageApps(
    const arc::mojom::ArcPackageInfo& package_info,
    bool update_icon) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }

  for (const auto& app_id :
       prefs->GetAppsForPackage(package_info.package_name)) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (app_info && !IsWebAppShellPackage(profile_, *app_info)) {
      // If the package is added or modified, the app icon files might be
      // modified, so set `update_icon` and `raw_icon_updated` as true to update
      // icon files in the icon folders.
      AppPublisher::Publish(CreateApp(prefs, app_id, *app_info,
                                      /*update_icon=*/true,
                                      /*raw_icon_updated=*/true));
    }
  }
}

IconEffects ArcApps::GetIconEffects(const std::string& app_id,
                                    const ArcAppListPrefs::AppInfo& app_info) {
  IconEffects icon_effects = IconEffects::kNone;
  if (GetReadiness(app_id, app_info) != Readiness::kReady) {
    icon_effects =
        static_cast<IconEffects>(icon_effects | IconEffects::kBlocked);
  }
  if (paused_apps_.IsPaused(app_id)) {
    icon_effects =
        static_cast<IconEffects>(icon_effects | IconEffects::kPaused);
  }
  return icon_effects;
}

void ArcApps::SetIconEffect(const std::string& app_id) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (!app_info || IsWebAppShellPackage(profile_, *app_info)) {
    return;
  }

  auto app = std::make_unique<App>(AppType::kArc, app_id);
  app->icon_key = IconKey(GetIconEffects(app_id, *app_info));
  AppPublisher::Publish(std::move(app));
}

void ArcApps::CloseTasks(const std::string& app_id) {
  if (!base::Contains(app_id_to_task_ids_, app_id)) {
    return;
  }

  for (int task_id : app_id_to_task_ids_[app_id]) {
    arc::CloseTask(task_id);
    task_id_to_app_id_.erase(task_id);
  }
  app_id_to_task_ids_.erase(app_id);
}

void ArcApps::BuildMenuForShortcut(
    const std::string& package_name,
    MenuItems menu_items,
    base::OnceCallback<void(MenuItems)> callback) {
  // The previous request is cancelled, and start a new request if the callback
  // of the previous request is not called.
  arc_app_shortcuts_request_ =
      std::make_unique<arc::ArcAppShortcutsRequest>(base::BindOnce(
          &ArcApps::OnGetAppShortcutItems, weak_ptr_factory_.GetWeakPtr(),
          std::move(menu_items), std::move(callback)));
  arc_app_shortcuts_request_->StartForPackage(package_name);
}

void ArcApps::OnGetAppShortcutItems(
    MenuItems menu_items,
    base::OnceCallback<void(MenuItems)> callback,
    std::unique_ptr<apps::AppShortcutItems> app_shortcut_items) {
  if (!app_shortcut_items || app_shortcut_items->empty()) {
    // No need log time for empty requests.
    std::move(callback).Run(std::move(menu_items));
    arc_app_shortcuts_request_.reset();
    return;
  }

  apps::AppShortcutItems& items = *app_shortcut_items;
  // Sort the shortcuts based on two rules: (1) Static (declared in manifest)
  // shortcuts and then dynamic shortcuts; (2) Within each shortcut type
  // (static and dynamic), shortcuts are sorted in order of increasing rank.
  std::sort(items.begin(), items.end(),
            [](const apps::AppShortcutItem& item1,
               const apps::AppShortcutItem& item2) {
              return std::tie(item1.type, item1.rank) <
                     std::tie(item2.type, item2.rank);
            });

  AddSeparator(ui::DOUBLE_SEPARATOR, menu_items);
  int command_id = ash::LAUNCH_APP_SHORTCUT_FIRST;
  for (const auto& item : items) {
    if (command_id != ash::LAUNCH_APP_SHORTCUT_FIRST) {
      AddSeparator(ui::PADDED_SEPARATOR, menu_items);
    }
    AddShortcutCommandItem(command_id++, item.shortcut_id, item.short_label,
                           item.icon, menu_items);
  }
  std::move(callback).Run(std::move(menu_items));
  arc_app_shortcuts_request_.reset();
}

void ArcApps::OnInstallationStarted(const std::string& package_name) {
  if (ash::features::ArePromiseIconsEnabled() &&
      ArcVersionEligibleForPromiseIcons()) {
    PromiseAppPtr promise_app = AppPublisher::MakePromiseApp(
        PackageId(PackageType::kArc, package_name));

    // All ARC installations start as "Pending".
    promise_app->status = PromiseStatus::kPending;
    AppPublisher::PublishPromiseApp(std::move(promise_app));
  }
}

void ArcApps::OnInstallationProgressChanged(const std::string& package_name,
                                            float progress) {
  if (ash::features::ArePromiseIconsEnabled()) {
    PackageId package_id = PackageId(PackageType::kArc, package_name);
    const PromiseApp* existing_promise_app =
        proxy()->PromiseAppRegistryCache()->GetPromiseApp(package_id);
    if (!existing_promise_app) {
      LOG(ERROR) << "Cannot update installation progress value for "
                 << package_name
                 << ", as there is no promise app registered for this package.";
      return;
    }
    PromiseAppPtr promise_app = AppPublisher::MakePromiseApp(package_id);
    promise_app->progress = progress;

    // Update the status to reflect that the app is actively downloading/
    // installing. We update the status here on the first progress update
    // instead of in OnInstallationActiveChanged, due to some conflicts with
    // what the ARC active status indicates and what we need.
    if (existing_promise_app->status == PromiseStatus::kPending) {
      promise_app->status = PromiseStatus::kInstalling;
    }
    AppPublisher::PublishPromiseApp(std::move(promise_app));
  }
}

void ArcApps::OnInstallationActiveChanged(const std::string& package_name,
                                          bool active) {
  if (ash::features::ArePromiseIconsEnabled()) {
    PackageId package_id(PackageType::kArc, package_name);
    if (!proxy()->PromiseAppRegistryCache()->HasPromiseApp(package_id)) {
      LOG(ERROR) << "Cannot update installation active status for "
                 << package_name
                 << ", as there is no promise app registered for this package.";
      return;
    }
    // TODO(b/261907409): Set PromiseStatus to kPending if the installation is
    // no longer active, i.e. if active=false after there has been at least one
    // progress change.
  }
}

void ArcApps::OnInstallationFinished(const std::string& package_name,
                                     bool success,
                                     bool is_launchable_app) {
  if (ash::features::ArePromiseIconsEnabled() &&
      ArcVersionEligibleForPromiseIcons()) {
    if (success && is_launchable_app) {
      return;
    }
    // Remove the promise app of any failed installation or non-launchable
    // package.
    PackageId package_id(PackageType::kArc, package_name);
    if (!proxy()->PromiseAppRegistryCache()->HasPromiseApp(package_id)) {
      return;
    }
    PromiseAppPtr promise_app = AppPublisher::MakePromiseApp(package_id);
    promise_app->status = PromiseStatus::kCancelled;
    AppPublisher::PublishPromiseApp(std::move(promise_app));
  }
}

void ArcApps::OnAppConnectionClosed() {
  std::vector<PromiseAppPtr> promise_apps =
      proxy()->PromiseAppRegistryCache()->GetAllPromiseApps();

  for (auto& promise_app : promise_apps) {
    if (promise_app->package_id.package_type() != PackageType::kArc) {
      continue;
    }
    promise_app->status = PromiseStatus::kCancelled;
    AppPublisher::PublishPromiseApp(std::move(promise_app));
  }
}

void ArcApps::ObserveDisabledSystemFeaturesPolicy() {
  PrefService* const local_state = g_browser_process->local_state();
  if (!local_state) {  // Sometimes it's not available in tests.
    return;
  }

  local_state_pref_change_registrar_.Init(local_state);
  local_state_pref_change_registrar_.Add(
      policy::policy_prefs::kSystemFeaturesDisableList,
      base::BindRepeating(&ArcApps::OnDisableListPolicyChanged,
                          base::Unretained(this)));
}

void ArcApps::OnDisableListPolicyChanged() {
  PrefService* const local_state = g_browser_process->local_state();
  if (!local_state) {
    return;
  }

  const base::Value::List& disabled_system_features_pref =
      local_state->GetList(policy::policy_prefs::kSystemFeaturesDisableList);
  bool disable_arc_settings = false;
  for (const auto& entry : disabled_system_features_pref) {
    if (static_cast<policy::SystemFeature>(entry.GetInt()) ==
        policy::SystemFeature::kOsSettings) {
      disable_arc_settings = true;
      break;
    }
  }

  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile_);
  if (!arc_prefs) {
    return;
  }

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(arc::kSettingsAppId);
  if (!app_info) {
    return;
  }

  bool is_disabled = false;
  bool found = proxy()->AppRegistryCache().ForOneApp(
      arc::kSettingsAppId, [&is_disabled](const apps::AppUpdate& update) {
        is_disabled = apps_util::IsDisabled(update.Readiness());
      });
  if (!found) {
    return;
  }

  if (disable_arc_settings == is_disabled) {
    return;
  }

  auto app = std::make_unique<App>(AppType::kArc, arc::kSettingsAppId);
  if (disable_arc_settings) {
    settings_app_is_disabled_ = true;
    app->icon_key = IconKey(/*raw_icon_updated=*/false, IconEffects::kBlocked);
  } else {
    settings_app_is_disabled_ = false;
    app->icon_key = IconKey(GetIconEffects(arc::kSettingsAppId, *app_info));
  }
  app->readiness = GetReadiness(arc::kSettingsAppId, *app_info);

  AppPublisher::Publish(std::move(app));
}

bool ArcApps::IsAppSuspended(const std::string& app_id,
                             const ArcAppListPrefs::AppInfo& app_info) {
  if (app_id == arc::kSettingsAppId && settings_app_is_disabled_) {
    return true;
  }

  return app_info.suspended;
}

Readiness ArcApps::GetReadiness(const std::string& app_id,
                                const ArcAppListPrefs::AppInfo& app_info) {
  if (IsAppSuspended(app_id, app_info)) {
    return Readiness::kDisabledByPolicy;
  }

  if (base::Contains(blocked_app_ids_, app_id)) {
    return Readiness::kDisabledByLocalSettings;
  }

  return Readiness::kReady;
}

}  // namespace apps
