// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/arc_apps.h"

#include <algorithm>
#include <utility>

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/components/arc/metrics/arc_metrics_service.h"
#include "ash/components/arc/mojom/app_permissions.mojom.h"
#include "ash/components/arc/mojom/compatibility_mode.mojom.h"
#include "ash/components/arc/mojom/file_system.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/apps/app_service/app_icon/dip_px_util.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/file_utils.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps_factory.h"
#include "chrome/browser/apps/app_service/webapk/webapk_manager.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/component_extension_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/arc/common/intent_helper/arc_intent_helper_package.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/permission_utils.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"

// TODO(crbug.com/826982): consider that, per khmel@, "App icon can be
// overwritten (setTaskDescription) or by assigning the icon for the app
// window. In this case some consumers (Shelf for example) switch to
// overwritten icon... IIRC this applies to shelf items and ArcAppWindow icon".

namespace {

void UpdateIconImage(apps::LoadIconCallback callback, apps::IconValuePtr iv) {
  if (iv->icon_type == apps::IconType::kCompressed) {
    ConvertUncompressedIconToCompressedIcon(std::move(iv), std::move(callback));
    return;
  }
  std::move(callback).Run(std::move(iv));
}

void OnArcAppIconCompletelyLoaded(apps::IconType icon_type,
                                  int32_t size_hint_in_dip,
                                  apps::IconEffects icon_effects,
                                  apps::LoadIconCallback callback,
                                  ArcAppIcon* icon) {
  if (!icon) {
    std::move(callback).Run(std::make_unique<apps::IconValue>());
    return;
  }

  auto iv = std::make_unique<apps::IconValue>();
  iv->icon_type = icon_type;
  iv->is_placeholder_icon = false;

  switch (icon_type) {
    case apps::IconType::kCompressed:
      [[fallthrough]];
    case apps::IconType::kUncompressed:
      [[fallthrough]];
    case apps::IconType::kStandard: {
      iv->uncompressed =
          icon->is_adaptive_icon()
              ? apps::CompositeImagesAndApplyMask(icon->foreground_image_skia(),
                                                  icon->background_image_skia())
              : apps::ApplyBackgroundAndMask(icon->image_skia());

      if (icon_effects != apps::IconEffects::kNone) {
        apps::ApplyIconEffects(
            icon_effects, size_hint_in_dip, std::move(iv),
            base::BindOnce(&UpdateIconImage, std::move(callback)));
        return;
      }
      break;
    }
    case apps::IconType::kUnknown:
      NOTREACHED();
      break;
  }

  UpdateIconImage(std::move(callback), std::move(iv));
}

// TODO(crbug.com/1253250): Remove and use GetPermissionType
apps::mojom::PermissionType GetAppServicePermissionType(
    arc::mojom::AppPermission arc_permission_type) {
  switch (arc_permission_type) {
    case arc::mojom::AppPermission::CAMERA:
      return apps::mojom::PermissionType::kCamera;
    case arc::mojom::AppPermission::LOCATION:
      return apps::mojom::PermissionType::kLocation;
    case arc::mojom::AppPermission::MICROPHONE:
      return apps::mojom::PermissionType::kMicrophone;
    case arc::mojom::AppPermission::NOTIFICATIONS:
      return apps::mojom::PermissionType::kNotifications;
    case arc::mojom::AppPermission::CONTACTS:
      return apps::mojom::PermissionType::kContacts;
    case arc::mojom::AppPermission::STORAGE:
      return apps::mojom::PermissionType::kStorage;
  }
}

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

bool GetArcPermissionType(
    apps::mojom::PermissionType app_service_permission_type,
    arc::mojom::AppPermission& arc_permission) {
  switch (app_service_permission_type) {
    case apps::mojom::PermissionType::kCamera:
      arc_permission = arc::mojom::AppPermission::CAMERA;
      return true;
    case apps::mojom::PermissionType::kLocation:
      arc_permission = arc::mojom::AppPermission::LOCATION;
      return true;
    case apps::mojom::PermissionType::kMicrophone:
      arc_permission = arc::mojom::AppPermission::MICROPHONE;
      return true;
    case apps::mojom::PermissionType::kNotifications:
      arc_permission = arc::mojom::AppPermission::NOTIFICATIONS;
      return true;
    case apps::mojom::PermissionType::kContacts:
      arc_permission = arc::mojom::AppPermission::CONTACTS;
      return true;
    case apps::mojom::PermissionType::kStorage:
      arc_permission = arc::mojom::AppPermission::STORAGE;
      return true;
    case apps::mojom::PermissionType::kUnknown:
    case apps::mojom::PermissionType::kPrinting:
      return false;
  }
}

// TODO(crbug.com/1253250): Remove and use CreatePermissions
void UpdateAppPermissions(
    const base::flat_map<arc::mojom::AppPermission,
                         arc::mojom::PermissionStatePtr>& new_permissions,
    std::vector<apps::mojom::PermissionPtr>* permissions) {
  for (const auto& new_permission : new_permissions) {
    auto permission = apps::mojom::Permission::New();
    permission->permission_type =
        GetAppServicePermissionType(new_permission.first);
    permission->value = apps::mojom::PermissionValue::New();
    permission->value->set_bool_value(new_permission.second->granted);
    permission->is_managed = new_permission.second->managed;

    permissions->push_back(std::move(permission));
  }
}

apps::Permissions CreatePermissions(
    const base::flat_map<arc::mojom::AppPermission,
                         arc::mojom::PermissionStatePtr>& new_permissions) {
  apps::Permissions permissions;
  for (const auto& new_permission : new_permissions) {
    permissions.push_back(std::make_unique<apps::Permission>(
        GetPermissionType(new_permission.first),
        std::make_unique<apps::PermissionValue>(new_permission.second->granted),
        new_permission.second->managed));
  }
  return permissions;
}

absl::optional<arc::UserInteractionType> GetUserInterationType(
    apps::mojom::LaunchSource launch_source) {
  auto user_interaction_type = arc::UserInteractionType::NOT_USER_INITIATED;
  switch (launch_source) {
    // kUnknown is not set anywhere, this case is not valid.
    case apps::mojom::LaunchSource::kUnknown:
      return absl::nullopt;
    case apps::mojom::LaunchSource::kFromChromeInternal:
      user_interaction_type = arc::UserInteractionType::NOT_USER_INITIATED;
      break;
    case apps::mojom::LaunchSource::kFromAppListGrid:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER;
      break;
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_CONTEXT_MENU;
      break;
    case apps::mojom::LaunchSource::kFromAppListQuery:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_SEARCH;
      break;
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
      user_interaction_type = arc::UserInteractionType::
          APP_STARTED_FROM_LAUNCHER_SEARCH_CONTEXT_MENU;
      break;
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_SUGGESTED_APP;
      break;
    case apps::mojom::LaunchSource::kFromParentalControls:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_SETTINGS;
      break;
    case apps::mojom::LaunchSource::kFromShelf:
      user_interaction_type = arc::UserInteractionType::APP_STARTED_FROM_SHELF;
      break;
    case apps::mojom::LaunchSource::kFromFileManager:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_FILE_MANAGER;
      break;
    case apps::mojom::LaunchSource::kFromLink:
      user_interaction_type = arc::UserInteractionType::APP_STARTED_FROM_LINK;
      break;
    case apps::mojom::LaunchSource::kFromOmnibox:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_OMNIBOX;
      break;
    case apps::mojom::LaunchSource::kFromSharesheet:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_SHARESHEET;
      break;
    case apps::mojom::LaunchSource::kFromFullRestore:
      user_interaction_type =
          arc::UserInteractionType::APP_STARTED_FROM_FULL_RESTORE;
      break;
    case apps::mojom::LaunchSource::kFromSmartTextContextMenu:
      user_interaction_type = arc::UserInteractionType::
          APP_STARTED_FROM_SMART_TEXT_SELECTION_CONTEXT_MENU;
      break;
    default:
      NOTREACHED();
      return absl::nullopt;
  }
  return user_interaction_type;
}

// Check if this intent filter only contains HTTP and HTTPS schemes.
bool IsHttpOrHttpsIntentFilter(
    const apps::mojom::IntentFilterPtr& intent_filter) {
  for (const auto& condition : intent_filter->conditions) {
    if (condition->condition_type != apps::mojom::ConditionType::kScheme) {
      continue;
    }
    for (const auto& condition_value : condition->condition_values) {
      if (condition_value->value != url::kHttpScheme &&
          condition_value->value != url::kHttpsScheme) {
        return false;
      }
    }
    return true;
  }
  // If there is no scheme |condition_type| found, return false.
  return false;
}

void AddPreferredApp(const std::string& app_id,
                     const apps::mojom::IntentFilterPtr& intent_filter,
                     apps::mojom::IntentPtr intent,
                     arc::ArcServiceManager* arc_service_manager,
                     ArcAppListPrefs* prefs) {
  arc::mojom::IntentHelperInstance* instance = nullptr;
  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(),
        AddPreferredApp);
  }
  if (!instance) {
    return;
  }
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);

  // If |app_info| doesn't exist, we are trying to set preferences for a
  // non-ARC app. Set the preferred app as the ARC intent helper package.
  const std::string& package_name =
      app_info ? app_info->package_name : arc::kArcIntentHelperPackageName;

  instance->AddPreferredApp(
      package_name,
      apps_util::ConvertAppServiceToArcIntentFilter(package_name,
                                                    intent_filter),
      apps_util::ConvertAppServiceToArcIntent(std::move(intent)));
}

void ResetVerifiedLinks(
    const apps::mojom::IntentFilterPtr& intent_filter,
    const apps::mojom::ReplacedAppPreferencesPtr& replaced_app_preferences,
    arc::ArcServiceManager* arc_service_manager,
    ArcAppListPrefs* prefs) {
  if (!arc_service_manager) {
    return;
  }
  std::vector<std::string> package_names;

  // Find the apps that needs to reset verified link domain status in ARC.
  for (auto& entry : replaced_app_preferences->replaced_preference) {
    auto app_info = prefs->GetApp(entry.first);
    if (!app_info) {
      continue;
    }
    for (auto& intent_filter : entry.second) {
      if (IsHttpOrHttpsIntentFilter(intent_filter)) {
        package_names.push_back(app_info->package_name);
        break;
      }
    }
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(),
      SetVerifiedLinks);
  if (instance) {
    instance->SetVerifiedLinks(package_names, /*always_open=*/false);
  }
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
  } else {
    return arc::mojom::ActionType::VIEW;
  }
}

// Constructs an OpenUrlsRequest to be passed to
// FileSystemInstance.DEPRECATED_OpenUrlsWithPermission.
arc::mojom::OpenUrlsRequestPtr ConstructOpenUrlsRequest(
    const apps::mojom::IntentPtr& intent,
    const arc::mojom::ActivityNamePtr& activity,
    const std::vector<GURL>& content_urls) {
  arc::mojom::OpenUrlsRequestPtr request = arc::mojom::OpenUrlsRequest::New();
  request->action_type = GetArcActionType(intent->action);
  request->activity_name = activity.Clone();
  for (const auto& content_url : content_urls) {
    arc::mojom::ContentUrlWithMimeTypePtr url_with_type =
        arc::mojom::ContentUrlWithMimeType::New();
    url_with_type->content_url = content_url;
    url_with_type->mime_type = intent->mime_type.value();
    request->urls.push_back(std::move(url_with_type));
  }
  if (intent->share_text.has_value() || intent->share_title.has_value()) {
    request->extras = apps_util::CreateArcIntentExtras(intent);
  }
  return request;
}

void OnContentUrlResolved(const base::FilePath& file_path,
                          const std::string& app_id,
                          int32_t event_flags,
                          apps::mojom::IntentPtr intent,
                          arc::mojom::ActivityNamePtr activity,
                          apps::mojom::WindowInfoPtr window_info,
                          const std::vector<GURL>& content_urls) {
  for (const auto& content_url : content_urls) {
    if (!content_url.is_valid()) {
      LOG(ERROR) << "Share files failed, file urls are not valid";
      return;
    }
  }

  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    return;
  }

  DCHECK(window_info);
  int32_t session_id = window_info->window_id;
  int64_t display_id = window_info->display_id;

  arc::mojom::FileSystemInstance* arc_file_system = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->file_system(),
      OpenUrlsWithPermissionAndWindowInfo);
  if (arc_file_system) {
    arc_file_system->OpenUrlsWithPermissionAndWindowInfo(
        ConstructOpenUrlsRequest(intent, activity, content_urls),
        apps::MakeArcWindowInfo(std::move(window_info)), base::DoNothing());
  } else {
    arc_file_system = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->file_system(),
        DEPRECATED_OpenUrlsWithPermission);
    if (!arc_file_system) {
      return;
    }

    arc_file_system->DEPRECATED_OpenUrlsWithPermission(
        ConstructOpenUrlsRequest(intent, activity, content_urls),
        base::DoNothing());
  }

  ::full_restore::SaveAppLaunchInfo(
      file_path,
      std::make_unique<app_restore::AppLaunchInfo>(
          app_id, event_flags, std::move(intent), session_id, display_id));
}

// Sets the session id for |window_info|. If the full restore feature is
// disabled, or the session id has been set, returns |window_info|. Otherwise,
// fetches a new ARC session id, and sets to window_id for |window_info|.
apps::mojom::WindowInfoPtr SetSessionId(
    apps::mojom::WindowInfoPtr window_info) {
  if (!window_info) {
    window_info = apps::mojom::WindowInfo::New();
    window_info->display_id = display::kInvalidDisplayId;
  }

  if (window_info->window_id != -1) {
    return window_info;
  }

  window_info->window_id =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  return window_info;
}

absl::optional<bool> GetResizeLocked(ArcAppListPrefs* prefs,
                                     const std::string& app_id) {
  // Set null to resize lock state until the Mojo connection to ARC++ has been
  // established. This prevents Chrome and ARC++ from having inconsistent
  // states.
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    return absl::nullopt;
  }

  // If we don't have the connection (e.g. for non-supported Android versions),
  // returns null.
  auto* compatibility_mode =
      arc_service_manager->arc_bridge_service()->compatibility_mode();
  if (!compatibility_mode->IsConnected()) {
    return absl::nullopt;
  }

  // Check if |SetResizeLockState| is available to see if Android is ready to
  // be synchronized. Otherwise we need to hide the corresponding setting by
  // returning null.
  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(compatibility_mode, SetResizeLockState);
  if (!instance) {
    return absl::nullopt;
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
      return absl::nullopt;
  }
}

// TODO(crbug.com/1253250): Remove and use GetResizeLocked.
apps::mojom::OptionalBool IsResizeLocked(ArcAppListPrefs* prefs,
                                         const std::string& app_id) {
  auto resize_locked = GetResizeLocked(prefs, app_id);
  if (!resize_locked.has_value()) {
    return apps::mojom::OptionalBool::kUnknown;
  }

  return resize_locked.value() ? apps::mojom::OptionalBool::kTrue
                               : apps::mojom::OptionalBool::kFalse;
}

bool IsWebAppShellPackage(Profile* profile,
                          const ArcAppListPrefs::AppInfo& app_info) {
  ash::ApkWebAppService* apk_web_app_service =
      ash::ApkWebAppService::Get(profile);
  return apk_web_app_service &&
         apk_web_app_service->IsWebAppShellPackage(app_info.package_name);
}

}  // namespace

namespace apps {

// static
ArcApps* ArcApps::Get(Profile* profile) {
  return ArcAppsFactory::GetForProfile(profile);
}

ArcApps::ArcApps(AppServiceProxy* proxy)
    : AppPublisher(proxy),
      profile_(proxy->profile()),
      arc_icon_once_loader_(profile_) {}

ArcApps::~ArcApps() = default;

void ArcApps::Initialize() {
  if (!arc::IsArcAllowedForProfile(profile_) ||
      (arc::ArcServiceManager::Get() == nullptr)) {
    return;
  }

  mojo::Remote<apps::mojom::AppService>& app_service = proxy()->AppService();
  if (!app_service.is_bound()) {
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

  auto* instance_registry = &proxy()->InstanceRegistry();
  if (instance_registry) {
    instance_registry_observation_.Observe(instance_registry);
  }

  if (web_app::AreWebAppsEnabled(profile_)) {
    web_apk_manager_ = std::make_unique<apps::WebApkManager>(profile_);
  }

  PublisherBase::Initialize(app_service, apps::mojom::AppType::kArc);

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
}

void ArcApps::Shutdown() {
  // Disconnect the observee-observer connections that we made during the
  // constructor.
  //
  // This isn't entirely correct. The object returned by
  // ArcAppListPrefs::Get(some_profile) can vary over the lifetime of that
  // profile. If it changed, we'll try to disconnect from different
  // ArcAppListPrefs-related objects than the ones we connected to, at the time
  // of this object's construction.
  //
  // Even so, this is probably harmless, assuming that calling
  // foo->RemoveObserver(bar) is a no-op (and e.g. does not crash) if bar
  // wasn't observing foo in the first place, and assuming that the dangling
  // observee-observer connection on the old foo's are never followed again.
  //
  // To fix this properly, we would probably need to add something like an
  // OnArcAppListPrefsWillBeDestroyed method to ArcAppListPrefs::Observer, and
  // in this class's implementation of that method, disconnect. Furthermore,
  // when the new ArcAppListPrefs object is created, we'll have to somehow be
  // notified so we can re-connect this object as an observer.
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (prefs) {
    prefs->RemoveObserver(this);
  }
  arc_icon_once_loader_.StopObserving(prefs);

  auto* intent_helper_bridge =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (intent_helper_bridge) {
    intent_helper_bridge->SetAdaptiveIconDelegate(nullptr);
  }

  arc_intent_helper_observation_.Reset();
}

void ArcApps::LoadIcon(const std::string& app_id,
                       const IconKey& icon_key,
                       IconType icon_type,
                       int32_t size_hint_in_dip,
                       bool allow_placeholder_icon,
                       apps::LoadIconCallback callback) {
  if (icon_type == IconType::kUnknown) {
    std::move(callback).Run(std::make_unique<IconValue>());
    return;
  }
  IconEffects icon_effects = static_cast<IconEffects>(icon_key.icon_effects);

  // Treat the Play Store as a special case, loading an icon defined by a
  // resource instead of asking the Android VM (or the cache of previous
  // responses from the Android VM). Presumably this is for bootstrapping:
  // the Play Store icon (the UI for enabling and installing Android apps)
  // should be showable even before the user has installed their first
  // Android app and before bringing up an Android VM for the first time.
  if (app_id == arc::kPlayStoreAppId) {
    LoadPlayStoreIcon(icon_type, size_hint_in_dip, icon_effects,
                      std::move(callback));
  } else {
    const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile_);
    DCHECK(arc_prefs);

    // If the app has been removed, immediately terminate the icon request since
    // it can't possibly succeed.
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        arc_prefs->GetApp(app_id);
    if (!app_info) {
      std::move(callback).Run(std::make_unique<IconValue>());
      return;
    }

    arc_icon_once_loader_.LoadIcon(
        app_id, size_hint_in_dip, icon_type,
        base::BindOnce(&OnArcAppIconCompletelyLoaded, icon_type,
                       size_hint_in_dip, icon_effects, std::move(callback)));
  }
}

void ArcApps::LaunchAppWithParams(AppLaunchParams&& params,
                                  LaunchCallback callback) {
  auto event_flags = apps::GetEventFlags(params.container, params.disposition,
                                         /*prefer_container=*/false);
  auto window_info = apps::MakeWindowInfo(params.display_id);
  if (params.intent) {
    LaunchAppWithIntent(params.app_id, event_flags, std::move(params.intent),
                        params.launch_source, std::move(window_info),
                        base::DoNothing());
  } else {
    Launch(params.app_id, event_flags, params.launch_source,
           std::move(window_info));
  }
  // TODO(crbug.com/1244506): Add launch return value.
  std::move(callback).Run(LaunchResult());
}

void ArcApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (prefs) {
    for (const auto& app_id : prefs->GetAppIds()) {
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
          prefs->GetApp(app_id);
      if (app_info) {
        apps.push_back(Convert(prefs, app_id, *app_info));
      }
    }
  }
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps), apps::mojom::AppType::kArc,
                     true /* should_notify_initialized */);
  subscribers_.Add(std::move(subscriber));
}

void ArcApps::LoadIcon(const std::string& app_id,
                       apps::mojom::IconKeyPtr icon_key,
                       apps::mojom::IconType mojom_icon_type,
                       int32_t size_hint_in_dip,
                       bool allow_placeholder_icon,
                       LoadIconCallback callback) {
  if (!icon_key || mojom_icon_type == apps::mojom::IconType::kUnknown) {
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }
  IconEffects icon_effects = static_cast<IconEffects>(icon_key->icon_effects);

  apps::IconType icon_type = ConvertMojomIconTypeToIconType(mojom_icon_type);

  // Treat the Play Store as a special case, loading an icon defined by a
  // resource instead of asking the Android VM (or the cache of previous
  // responses from the Android VM). Presumably this is for bootstrapping:
  // the Play Store icon (the UI for enabling and installing Android apps)
  // should be showable even before the user has installed their first
  // Android app and before bringing up an Android VM for the first time.
  if (app_id == arc::kPlayStoreAppId) {
    LoadPlayStoreIcon(icon_type, size_hint_in_dip, icon_effects,
                      IconValueToMojomIconValueCallback(std::move(callback)));
  } else {
    const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile_);
    DCHECK(arc_prefs);

    // If the app has been removed, immediately terminate the icon request since
    // it can't possibly succeed.
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        arc_prefs->GetApp(app_id);
    if (!app_info) {
      std::move(callback).Run(apps::mojom::IconValue::New());
      return;
    }

    arc_icon_once_loader_.LoadIcon(
        app_id, size_hint_in_dip, icon_type,
        base::BindOnce(&OnArcAppIconCompletelyLoaded, icon_type,
                       size_hint_in_dip, icon_effects,
                       IconValueToMojomIconValueCallback(std::move(callback))));
  }
}

void ArcApps::Launch(const std::string& app_id,
                     int32_t event_flags,
                     apps::mojom::LaunchSource launch_source,
                     apps::mojom::WindowInfoPtr window_info) {
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
                                  apps::mojom::IntentPtr intent,
                                  apps::mojom::LaunchSource launch_source,
                                  apps::mojom::WindowInfoPtr window_info,
                                  LaunchAppWithIntentCallback callback) {
  auto user_interaction_type = GetUserInterationType(launch_source);
  if (!user_interaction_type.has_value()) {
    std::move(callback).Run(/*success=*/false);
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
    std::move(callback).Run(/*success=*/false);
    return;
  }
  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(app_id);
  if (!app_info) {
    LOG(ERROR) << "Launch App failed, could not find app with id " << app_id;
    std::move(callback).Run(/*success=*/false);
    return;
  }

  if (app_info->ready) {
    arc::mojom::ActivityNamePtr activity = arc::mojom::ActivityName::New();
    activity->package_name = app_info->package_name;
    if (intent->activity_name.has_value() &&
        !intent->activity_name.value().empty()) {
      activity->activity_name = intent->activity_name.value();
    }

    auto new_window_info = SetSessionId(std::move(window_info));
    int32_t session_id = new_window_info->window_id;
    int64_t display_id = new_window_info->display_id;

    if (intent->mime_type.has_value() && intent->files.has_value()) {
      std::vector<GURL> file_urls;
      for (const auto& file : intent->files.value()) {
        file_urls.push_back(file->url);
      }
      arc::ConvertToContentUrlsAndShare(
          profile_, apps::GetFileSystemURL(profile_, file_urls),
          base::BindOnce(&OnContentUrlResolved, profile_->GetPath(), app_id,
                         event_flags, std::move(intent), std::move(activity),
                         std::move(new_window_info)));
      std::move(callback).Run(/*success=*/true);
      return;
    }

    auto intent_for_full_restore = intent.Clone();

    std::string intent_str =
        apps_util::CreateLaunchIntent(app_info->package_name, intent);
    if (!intent_str.empty()) {
      // If |intent| can be converted to a string, call the Launch interface.
      if (!arc::LaunchAppWithIntent(
              profile_, app_id, intent_str, event_flags,
              user_interaction_type.value(),
              MakeArcWindowInfo(std::move(new_window_info)))) {
        VLOG(2) << "Failed to launch app: " + app_id + ".";
        std::move(callback).Run(/*success=*/false);
        return;
      }
    } else {
      // If |intent| can't be converted to a string, call the HandleIntent
      // interface.
      auto arc_intent =
          apps_util::ConvertAppServiceToArcIntent(std::move(intent));

      if (!arc_intent) {
        LOG(ERROR) << "Launch App failed, launch intent is not valid";
        std::move(callback).Run(/*success=*/false);
        return;
      }

      auto* arc_service_manager = arc::ArcServiceManager::Get();
      if (!arc_service_manager) {
        std::move(callback).Run(/*success=*/false);
        return;
      }

      arc::mojom::IntentHelperInstance* instance = ARC_GET_INSTANCE_FOR_METHOD(
          arc_service_manager->arc_bridge_service()->intent_helper(),
          HandleIntentWithWindowInfo);
      if (instance) {
        instance->HandleIntentWithWindowInfo(
            std::move(arc_intent), std::move(activity),
            MakeArcWindowInfo(std::move(new_window_info)));
      } else {
        instance = ARC_GET_INSTANCE_FOR_METHOD(
            arc_service_manager->arc_bridge_service()->intent_helper(),
            HandleIntent);
        if (!instance) {
          std::move(callback).Run(/*success=*/false);
          return;
        }

        instance->HandleIntent(std::move(arc_intent), std::move(activity));
      }
    }

    prefs->SetLastLaunchTime(app_id);

    full_restore::SaveAppLaunchInfo(
        profile_->GetPath(),
        std::make_unique<app_restore::AppLaunchInfo>(
            app_id, event_flags, std::move(intent_for_full_restore), session_id,
            display_id));
    std::move(callback).Run(/*success=*/true);
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
        std::move(callback).Run(/*success=*/true);
        return;
      }
    }
  } else {
    if (prefs->IsDefault(app_id)) {
      // The setting can fail if the preference is managed.  However, the
      // caller is responsible to not call this function in such case.  DCHECK
      // is here to prevent possible mistake.
      if (!arc::SetArcPlayStoreEnabledForProfile(profile_, true)) {
        std::move(callback).Run(/*success=*/false);
        return;
      }
      DCHECK(arc::IsArcPlayStoreEnabledForProfile(profile_));

      // PlayStore item has special handling for shelf controllers. In order
      // to avoid unwanted initial animation for PlayStore item do not create
      // deferred launch request when PlayStore item enables Google Play
      // Store.
      if (app_id == arc::kPlayStoreAppId) {
        prefs->SetLastLaunchTime(app_id);
        std::move(callback).Run(/*success=*/false);
        return;
      }
    } else {
      // Only reachable when ARC always starts.
      DCHECK(arc::ShouldArcAlwaysStart());
    }
  }
  std::move(callback).Run(/*success=*/true);
}

void ArcApps::SetPermission(const std::string& app_id,
                            apps::mojom::PermissionPtr permission) {
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

  // TODO(crbug.com/1198390): Add unknown type for arc permissions enum.
  arc::mojom::AppPermission permission_type = arc::mojom::AppPermission::CAMERA;

  if (!GetArcPermissionType(permission->permission_type, permission_type)) {
    LOG(ERROR) << "SetPermission failed, permission type not supported by ARC.";
    return;
  }

  if (apps_util::IsPermissionEnabled(permission->value)) {
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

void ArcApps::SetResizeLocked(const std::string& app_id,
                              apps::mojom::OptionalBool locked) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  if (locked == apps::mojom::OptionalBool::kUnknown) {
    return;
  }
  prefs->SetResizeLockState(app_id, locked == apps::mojom::OptionalBool::kTrue
                                        ? arc::mojom::ArcResizeLockState::ON
                                        : arc::mojom::ArcResizeLockState::OFF);
}

void ArcApps::Uninstall(const std::string& app_id,
                        apps::mojom::UninstallSource uninstall_source,
                        bool clear_site_data,
                        bool report_abuse) {
  arc::UninstallArcApp(app_id, profile_);
}

void ArcApps::PauseApp(const std::string& app_id) {
  if (paused_apps_.MaybeAddApp(app_id)) {
    SetIconEffect(app_id);
  }

  constexpr bool kPaused = true;
  PublisherBase::Publish(paused_apps_.GetAppWithPauseStatus(
                             apps::mojom::AppType::kArc, app_id, kPaused),
                         subscribers_);
  AppPublisher::Publish(paused_apps_.CreateAppWithPauseStatus(
      AppType::kArc, app_id, /*paused=*/true));
  CloseTasks(app_id);
}

void ArcApps::UnpauseApp(const std::string& app_id) {
  if (paused_apps_.MaybeRemoveApp(app_id)) {
    SetIconEffect(app_id);
  }

  constexpr bool kPaused = false;
  PublisherBase::Publish(paused_apps_.GetAppWithPauseStatus(
                             apps::mojom::AppType::kArc, app_id, kPaused),
                         subscribers_);
  AppPublisher::Publish(paused_apps_.CreateAppWithPauseStatus(
      AppType::kArc, app_id, /*paused=*/false));
}

void ArcApps::StopApp(const std::string& app_id) {
  CloseTasks(app_id);
}

void ArcApps::GetMenuModel(const std::string& app_id,
                           apps::mojom::MenuType menu_type,
                           int64_t display_id,
                           GetMenuModelCallback callback) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    std::move(callback).Run(apps::mojom::MenuItems::New());
    return;
  }
  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(app_id);
  if (!app_info) {
    std::move(callback).Run(apps::mojom::MenuItems::New());
    return;
  }

  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();

  // Add Open item if the app is not opened and not suspended.
  if (!base::Contains(app_id_to_task_ids_, app_id) && !app_info->suspended) {
    AddCommandItem((menu_type == apps::mojom::MenuType::kAppList)
                       ? ash::LAUNCH_NEW
                       : ash::MENU_OPEN_NEW,
                   IDS_APP_CONTEXT_MENU_ACTIVATE_ARC, &menu_items);
  }

  if (app_info->shortcut) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_REMOVE_SHORTCUT, &menu_items);
  } else if (app_info->ready && !app_info->sticky) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM, &menu_items);
  }

  // App Info item.
  if (app_info->ready && ShouldShow(*app_info)) {
    AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                   &menu_items);
  }

  if (menu_type == apps::mojom::MenuType::kShelf &&
      base::Contains(app_id_to_task_ids_, app_id)) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, &menu_items);
  }

  BuildMenuForShortcut(app_info->package_name, std::move(menu_items),
                       std::move(callback));
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
  arc::ShowPackageInfo(app_info->package_name,
                       arc::mojom::ShowPackageInfoPage::MAIN,
                       display::Screen::GetScreen()->GetPrimaryDisplay().id());
}

void ArcApps::OnPreferredAppSet(
    const std::string& app_id,
    apps::mojom::IntentFilterPtr intent_filter,
    apps::mojom::IntentPtr intent,
    apps::mojom::ReplacedAppPreferencesPtr replaced_app_preferences) {
  auto* arc_service_manager = arc::ArcServiceManager::Get();

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  AddPreferredApp(app_id, intent_filter, std::move(intent), arc_service_manager,
                  prefs);
  ResetVerifiedLinks(intent_filter, replaced_app_preferences,
                     arc_service_manager, prefs);
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
    PublisherBase::Publish(Convert(prefs, app_id, app_info), subscribers_);
    AppPublisher::Publish(CreateApp(prefs, app_id, app_info));
  }
}

void ArcApps::OnAppStatesChanged(const std::string& app_id,
                                 const ArcAppListPrefs::AppInfo& app_info) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs || IsWebAppShellPackage(profile_, app_info)) {
    return;
  }

  PublisherBase::Publish(Convert(prefs, app_id, app_info), subscribers_);
  AppPublisher::Publish(CreateApp(prefs, app_id, app_info));
}

void ArcApps::OnAppRemoved(const std::string& app_id) {
  app_notifications_.RemoveNotificationsForApp(app_id);
  paused_apps_.MaybeRemoveApp(app_id);

  if (base::Contains(app_id_to_task_ids_, app_id)) {
    for (int task_id : app_id_to_task_ids_[app_id]) {
      task_id_to_app_id_.erase(task_id);
    }
    app_id_to_task_ids_.erase(app_id);
  }

  apps::mojom::AppPtr mojom_app = apps::mojom::App::New();
  mojom_app->app_type = apps::mojom::AppType::kArc;
  mojom_app->app_id = app_id;
  mojom_app->readiness = apps::mojom::Readiness::kUninstalledByUser;
  PublisherBase::Publish(std::move(mojom_app), subscribers_);

  auto app = std::make_unique<App>(AppType::kArc, app_id);
  app->readiness = Readiness::kUninstalledByUser;
  AppPublisher::Publish(std::move(app));
}

void ArcApps::OnAppIconUpdated(const std::string& app_id,
                               const ArcAppIconDescriptor& descriptor) {
  SetIconEffect(app_id);
}

void ArcApps::OnAppNameUpdated(const std::string& app_id,
                               const std::string& name) {
  apps::mojom::AppPtr mojom_app = apps::mojom::App::New();
  mojom_app->app_type = apps::mojom::AppType::kArc;
  mojom_app->app_id = app_id;
  mojom_app->name = name;
  PublisherBase::Publish(std::move(mojom_app), subscribers_);

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
  apps::mojom::AppPtr mojom_app = apps::mojom::App::New();
  mojom_app->app_type = apps::mojom::AppType::kArc;
  mojom_app->app_id = app_id;
  mojom_app->last_launch_time = app_info->last_launch_time;
  PublisherBase::Publish(std::move(mojom_app), subscribers_);

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
      PublisherBase::Publish(Convert(prefs, app_id, *app_info, update_icon),
                             subscribers_);
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
    const absl::optional<std::string>& package_name) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }

  auto GetAppInfoAndPublish = [prefs, this](std::string app_id) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (app_info) {
      PublisherBase::Publish(
          Convert(prefs, app_id, *app_info, false /* update_icon */),
          subscribers_);
      AppPublisher::Publish(
          CreateApp(prefs, app_id, *app_info, false /* update_icon */));
    }
  };

  // If there is no specific package_name, update all apps, otherwise update
  // apps for the package.

  // Note: Cannot combine the two for-loops because the return type of
  // GetAppIds() is std::vector<std::string> and the return type of
  // GetAppsForPackage() is std::unordered_set<std::string>.
  if (package_name == absl::nullopt) {
    for (const auto& app_id : prefs->GetAppIds()) {
      GetAppInfoAndPublish(app_id);
    }
  } else {
    for (const auto& app_id : prefs->GetAppsForPackage(package_name.value())) {
      GetAppInfoAndPublish(app_id);
    }
  }
}

// This method calls App Service directly with a list of intent filters received
// from ARC, rather than going through AppServiceProxy's
// SetSupportedlinksPreference method. This is because recent changes to app
// intent filters (such as after an install or update) may not have propagated
// to AppServiceProxy's AppRegistryCache yet.
// TODO(crbug.com/1265315): Use AppServiceProxy to set the preference once app
// changes are synchronous within Ash.
void ArcApps::OnArcSupportedLinksChanged(
    const std::vector<arc::mojom::SupportedLinksPtr>& added,
    const std::vector<arc::mojom::SupportedLinksPtr>& removed,
    arc::mojom::SupportedLinkChangeSource source) {
  mojo::Remote<apps::mojom::AppService>& app_service = proxy()->AppService();
  if (!app_service.is_bound()) {
    return;
  }

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }

  for (const auto& supported_link : added) {
    std::string app_id =
        prefs->GetAppIdByPackageName(supported_link->package_name);
    if (app_id.empty() || !supported_link->filters.has_value()) {
      continue;
    }

    // When kDefaultLinkCapturingInBrowser is enabled, ignore any requests from
    // ARC to set an app as handling supported links by default. We allow
    // requests if they were initiated by user action, or if the app already
    // has a non-default setting on the browser side.
    bool should_ignore_update =
        base::FeatureList::GetInstance()->IsEnabled(
            features::kDefaultLinkCapturingInBrowser) &&
        source == arc::mojom::SupportedLinkChangeSource::kArcSystem &&
        !proxy()->PreferredApps().IsPreferredAppForSupportedLinks(app_id);

    if (should_ignore_update) {
      continue;
    }

    std::vector<apps::mojom::IntentFilterPtr> app_service_filters;
    for (const auto& arc_filter : supported_link->filters.value()) {
      auto converted =
          apps_util::ConvertArcToAppServiceIntentFilter(arc_filter);
      if (apps_util::IsSupportedLinkForApp(app_id, converted)) {
        app_service_filters.push_back(std::move(converted));
      }
    }

    app_service->SetSupportedLinksPreference(apps::mojom::AppType::kArc, app_id,
                                             std::move(app_service_filters));
  }

  for (const auto& supported_link : removed) {
    std::string app_id =
        prefs->GetAppIdByPackageName(supported_link->package_name);
    if (app_id.empty()) {
      continue;
    }
    app_service->RemoveSupportedLinksPreference(apps::mojom::AppType::kArc,
                                                app_id);
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
  PublisherBase::Publish(app_notifications_.GetAppWithHasBadgeStatus(
                             apps::mojom::AppType::kArc, app_id),
                         subscribers_);
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
    PublisherBase::Publish(app_notifications_.GetAppWithHasBadgeStatus(
                               apps::mojom::AppType::kArc, app_id),
                           subscribers_);
    AppPublisher::Publish(
        app_notifications_.CreateAppWithHasBadgeStatus(AppType::kArc, app_id));
  }
}

void ArcApps::OnArcNotificationManagerDestroyed(
    ash::ArcNotificationManagerBase* notification_manager) {
  DCHECK(notification_observation_.IsObservingSource(notification_manager));
  notification_observation_.Reset();
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

void ArcApps::LoadPlayStoreIcon(apps::IconType icon_type,
                                int32_t size_hint_in_dip,
                                IconEffects icon_effects,
                                apps::LoadIconCallback callback) {
  // Use overloaded Chrome icon for Play Store that is adapted to Chrome style.
  constexpr bool quantize_to_supported_scale_factor = true;
  int size_hint_in_px = apps_util::ConvertDipToPx(
      size_hint_in_dip, quantize_to_supported_scale_factor);
  int resource_id = (size_hint_in_px <= 32) ? IDR_ARC_SUPPORT_ICON_32
                                            : IDR_ARC_SUPPORT_ICON_192;
  constexpr bool is_placeholder_icon = false;
  LoadIconFromResource(icon_type, size_hint_in_dip, resource_id,
                       is_placeholder_icon, icon_effects, std::move(callback));
}

apps::mojom::InstallReason GetInstallReason(
    const ArcAppListPrefs* prefs,
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  // Sticky represents apps that cannot be uninstalled and are installed by the
  // system.
  if (app_info.sticky) {
    return apps::mojom::InstallReason::kSystem;
  }

  if (prefs->IsOem(app_id)) {
    return apps::mojom::InstallReason::kOem;
  }

  if (prefs->IsDefault(app_id)) {
    return apps::mojom::InstallReason::kDefault;
  }

  if (prefs->IsControlledByPolicy(app_info.package_name)) {
    return apps::mojom::InstallReason::kPolicy;
  }

  return apps::mojom::InstallReason::kUser;
}

AppPtr ArcApps::CreateApp(ArcAppListPrefs* prefs,
                          const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info,
                          bool update_icon) {
  auto install_reason = ConvertMojomInstallReasonToInstallReason(
      GetInstallReason(prefs, app_id, app_info));
  auto app = AppPublisher::MakeApp(
      AppType::kArc, app_id,
      app_info.suspended ? Readiness::kDisabledByPolicy : Readiness::kReady,
      app_info.name, install_reason,
      install_reason == InstallReason::kSystem ? InstallSource::kSystem
                                               : InstallSource::kPlayStore);

  app->publisher_id = app_info.package_name;

  if (update_icon) {
    app->icon_key = std::move(
        *icon_key_factory_.CreateIconKey(GetIconEffects(app_id, app_info)));
  }

  app->last_launch_time = app_info.last_launch_time;
  app->install_time = app_info.install_time;

  std::unique_ptr<ArcAppListPrefs::PackageInfo> package =
      prefs->GetPackage(app_info.package_name);
  if (package) {
    app->permissions = CreatePermissions(package->permissions);
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

  app->handles_intents = show;

  app->allow_uninstall = app_info.ready && !app_info.sticky;

  app->has_badge = app_notifications_.HasNotification(app_id);
  app->paused = paused_apps_.IsPaused(app_id);

  auto* intent_helper_bridge =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (intent_helper_bridge &&
      app_info.package_name != arc::kArcIntentHelperPackageName) {
    app->intent_filters = apps_util::CreateIntentFiltersFromArcBridge(
        app_info.package_name, intent_helper_bridge);
  }

  app->resize_locked = GetResizeLocked(prefs, app_id);

  // TODO(crbug.com/1253250): Add other fields for the App struct.
  return app;
}

apps::mojom::AppPtr ArcApps::Convert(ArcAppListPrefs* prefs,
                                     const std::string& app_id,
                                     const ArcAppListPrefs::AppInfo& app_info,
                                     bool update_icon) {
  auto install_reason = GetInstallReason(prefs, app_id, app_info);
  apps::mojom::AppPtr app = PublisherBase::MakeApp(
      apps::mojom::AppType::kArc, app_id,
      app_info.suspended ? apps::mojom::Readiness::kDisabledByPolicy
                         : apps::mojom::Readiness::kReady,
      app_info.name, install_reason);

  app->publisher_id = app_info.package_name;

  auto paused = paused_apps_.IsPaused(app_id)
                    ? apps::mojom::OptionalBool::kTrue
                    : apps::mojom::OptionalBool::kFalse;

  if (update_icon) {
    app->icon_key =
        icon_key_factory_.MakeIconKey(GetIconEffects(app_id, app_info));
  }

  app->last_launch_time = app_info.last_launch_time;
  app->install_time = app_info.install_time;
  app->install_source = install_reason == apps::mojom::InstallReason::kSystem
                            ? apps::mojom::InstallSource::kSystem
                            : apps::mojom::InstallSource::kPlayStore;

  auto show = ShouldShow(app_info) ? apps::mojom::OptionalBool::kTrue
                                   : apps::mojom::OptionalBool::kFalse;
  // All published ARC apps are launchable. All launchable apps should be
  // permitted to be shown on the shelf, and have their pins on the shelf
  // persisted.
  app->show_in_shelf = apps::mojom::OptionalBool::kTrue;
  app->show_in_launcher = show;
  app->handles_intents = show;

  if (app_id == arc::kPlayGamesAppId &&
      show == apps::mojom::OptionalBool::kFalse) {
    // Play Games should only be hidden in the launcher.
    app->show_in_search = apps::mojom::OptionalBool::kTrue;
    app->show_in_management = apps::mojom::OptionalBool::kTrue;
  } else {
    app->show_in_search = show;
    app->show_in_management = show;
  }

  app->allow_uninstall = (app_info.ready && !app_info.sticky)
                             ? apps::mojom::OptionalBool::kTrue
                             : apps::mojom::OptionalBool::kFalse;

  app->has_badge = app_notifications_.HasNotification(app_id)
                       ? apps::mojom::OptionalBool::kTrue
                       : apps::mojom::OptionalBool::kFalse;
  app->paused = paused;
  app->resize_locked = IsResizeLocked(prefs, app_id);

  std::unique_ptr<ArcAppListPrefs::PackageInfo> package =
      prefs->GetPackage(app_info.package_name);
  if (package) {
    UpdateAppPermissions(package->permissions, &app->permissions);
  }

  auto* intent_helper_bridge =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (intent_helper_bridge &&
      app_info.package_name != arc::kArcIntentHelperPackageName) {
    UpdateAppIntentFilters(app_info.package_name, intent_helper_bridge,
                           &app->intent_filters);
  }

  return app;
}

void ArcApps::ConvertAndPublishPackageApps(
    const arc::mojom::ArcPackageInfo& package_info,
    bool update_icon) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (prefs) {
    for (const auto& app_id :
         prefs->GetAppsForPackage(package_info.package_name)) {
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
          prefs->GetApp(app_id);
      if (app_info && !IsWebAppShellPackage(profile_, *app_info)) {
        PublisherBase::Publish(Convert(prefs, app_id, *app_info, update_icon),
                               subscribers_);
        AppPublisher::Publish(CreateApp(prefs, app_id, *app_info, update_icon));
      }
    }
  }
}

IconEffects ArcApps::GetIconEffects(const std::string& app_id,
                                    const ArcAppListPrefs::AppInfo& app_info) {
  IconEffects icon_effects = IconEffects::kNone;
  if (app_info.suspended) {
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

  apps::mojom::AppPtr mojom_app = apps::mojom::App::New();
  mojom_app->app_type = apps::mojom::AppType::kArc;
  mojom_app->app_id = app_id;
  mojom_app->icon_key =
      icon_key_factory_.MakeIconKey(GetIconEffects(app_id, *app_info));
  PublisherBase::Publish(std::move(mojom_app), subscribers_);

  auto app = std::make_unique<App>(AppType::kArc, app_id);
  app->icon_key = std::move(
      *icon_key_factory_.CreateIconKey(GetIconEffects(app_id, *app_info)));
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

void ArcApps::UpdateAppIntentFilters(
    std::string package_name,
    arc::ArcIntentHelperBridge* intent_helper_bridge,
    std::vector<apps::mojom::IntentFilterPtr>* intent_filters) {
  const std::vector<arc::IntentFilter>& arc_intent_filters =
      intent_helper_bridge->GetIntentFilterForPackage(package_name);
  for (auto& arc_intent_filter : arc_intent_filters) {
    intent_filters->push_back(
        apps_util::ConvertArcToAppServiceIntentFilter(arc_intent_filter));
  }
}

void ArcApps::BuildMenuForShortcut(const std::string& package_name,
                                   apps::mojom::MenuItemsPtr menu_items,
                                   GetMenuModelCallback callback) {
  // The previous request is cancelled, and start a new request if the callback
  // of the previous request is not called.
  arc_app_shortcuts_request_ =
      std::make_unique<arc::ArcAppShortcutsRequest>(base::BindOnce(
          &ArcApps::OnGetAppShortcutItems, weak_ptr_factory_.GetWeakPtr(),
          base::TimeTicks::Now(), std::move(menu_items), std::move(callback)));
  arc_app_shortcuts_request_->StartForPackage(package_name);
}

void ArcApps::OnGetAppShortcutItems(
    const base::TimeTicks start_time,
    apps::mojom::MenuItemsPtr menu_items,
    GetMenuModelCallback callback,
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

  AddSeparator(ui::DOUBLE_SEPARATOR, &menu_items);
  int command_id = ash::LAUNCH_APP_SHORTCUT_FIRST;
  for (const auto& item : items) {
    if (command_id != ash::LAUNCH_APP_SHORTCUT_FIRST) {
      AddSeparator(ui::PADDED_SEPARATOR, &menu_items);
    }
    AddShortcutCommandItem(command_id++, item.shortcut_id, item.short_label,
                           item.icon, &menu_items);
  }
  std::move(callback).Run(std::move(menu_items));
  arc_app_shortcuts_request_.reset();

  UMA_HISTOGRAM_TIMES("Arc.AppShortcuts.BuildMenuTime",
                      base::TimeTicks::Now() - start_time);
}

}  // namespace apps
