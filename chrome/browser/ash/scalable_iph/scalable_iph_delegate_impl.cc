// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scalable_iph/scalable_iph_delegate_impl.h"

#include <memory>

#include "apps/launcher.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/scalable_iph/wallpaper_ash_notification_view.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/message_center/message_view_factory.h"
#include "ash/webui/grit/ash_print_management_resources.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/buildflag.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "chrome/browser/ash/crosapi/files_app_launcher.h"
#include "chrome/browser/ash/crosapi/url_handler_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/scalable_iph/buildflags.h"
#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "chromeos/ash/grit/ash_resources.h"
#include "chromeos/crosapi/cpp/gurl_os_handler_utils.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_CROS_SCALABLE_IPH)
#include "chrome/grit/preinstalled_web_apps_resources.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_CROS_SCALABLE_IPH)

namespace ash {

namespace {

using ::chromeos::network_config::mojom::ConnectionStateType;
using ::chromeos::network_config::mojom::FilterType;
using ::chromeos::network_config::mojom::kNoLimit;
using ::chromeos::network_config::mojom::NetworkFilter;
using ::chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using ::chromeos::network_config::mojom::NetworkType;
using DelegateObserver = ::scalable_iph::ScalableIphDelegate::Observer;
using Action = ::scalable_iph::ScalableIphDelegate::Action;
using NotificationParams =
    ::scalable_iph::ScalableIphDelegate::NotificationParams;
using NotificationImageType =
    ::scalable_iph::ScalableIphDelegate::NotificationImageType;
using BubbleIcon = ::scalable_iph::ScalableIphDelegate::BubbleIcon;
using scalable_iph::ActionType;

constexpr char kNotificationSourceName[] = "ChromeOS";
constexpr char kWallpaperNotificationType[] = "wallpaper_notification_type";
constexpr char kNotifierId[] = "scalable_iph";
constexpr char kButtonIndex = 0;
constexpr gfx::Size kBubbleIconSizeDip = gfx::Size(64, 64);

const base::flat_map<ActionType, std::string>& GetActionTypeURLs() {
  static const base::NoDestructor<base::flat_map<ActionType, std::string>>
      action_type_urls(
          {{ActionType::kOpenChrome, "chrome://new-tab-page/"},
           {ActionType::kOpenPersonalizationApp, "chrome://personalization/"},
           {ActionType::kOpenPlayStore,
            "https://play.google.com/store/games?device=chromebook"},
           {ActionType::kOpenGoogleDocs,
            "https://docs.google.com/document/?usp=installed_webapp/"},
           {ActionType::kOpenGooglePhotos, "https://photos.google.com/"},
           {ActionType::kOpenYouTube, "https://www.youtube.com/"}});
  return *action_type_urls;
}

bool HasOnlineNetwork(const std::vector<NetworkStatePropertiesPtr>& networks) {
  for (const NetworkStatePropertiesPtr& network : networks) {
    if (network->connection_state == ConnectionStateType::kOnline) {
      return true;
    }
  }
  return false;
}

// Adds the given `notification` to the message center after it removes any
// existing notification that has the same ID.
void AddOrReplaceNotification(
    std::unique_ptr<message_center::Notification> notification) {
  auto* message_center = message_center::MessageCenter::Get();
  message_center->RemoveNotification(notification->id(),
                                     /*by_user=*/false);
  message_center->AddNotification(std::move(notification));
}

message_center::NotifierId GetNotifierId() {
  return message_center::NotifierId(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId,
      NotificationCatalogName::kScalableIphNotification);
}

bool IsWallpaperNotification(const NotificationParams& params) {
  return params.image_type == NotificationImageType::kWallpaper;
}

message_center::NotificationType GetNotificationType(
    const NotificationParams& params) {
  switch (params.image_type) {
    case NotificationImageType::kWallpaper:
      return message_center::NOTIFICATION_TYPE_CUSTOM;
    case NotificationImageType::kNoImage:
      return message_center::NOTIFICATION_TYPE_SIMPLE;
  }
  NOTREACHED_NORETURN();
}

bool IsAppValidForProfile(Profile* profile, const std::string& app_id) {
  if (app_id == arc::kPlayStoreAppId) {
    return arc::IsArcPlayStoreEnabledForProfile(profile);
  }

  return arc::IsArcAllowedForProfile(profile);
}

void OpenUrlForProfile(Profile* profile, const GURL& url) {
  if (crosapi::browser_util::IsLacrosPrimaryBrowser()) {
    const GURL sanitized_url =
        crosapi::gurl_os_handler_utils::SanitizeAshURL(url);
    // Handle settings-related urls to open in their respective windows
    // rather than a browser window.
    if (ChromeWebUIControllerFactory::GetInstance()->CanHandleUrl(
            sanitized_url)) {
      crosapi::UrlHandlerAsh().OpenUrl(sanitized_url);
      return;
    }

    // TODO(b/291771298): Opening personalization hub links doesn't work in
    // the lacros browser so we need to handle it separately.
    if (url.spec() ==
        GetActionTypeURLs().at(ActionType::kOpenPersonalizationApp)) {
      NavigateParams navigate_params(
          profile, url,
          ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                    ui::PAGE_TRANSITION_FROM_API));
      navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
      navigate_params.window_action = NavigateParams::SHOW_WINDOW;
      Navigate(&navigate_params);
      return;
    }
  }

  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewWindow);
}

int GetResourceId(BubbleIcon icon) {
#if BUILDFLAG(ENABLE_CROS_SCALABLE_IPH)
  switch (icon) {
    case BubbleIcon::kChromeIcon:
      return IDR_PRODUCT_LOGO_128;
    case BubbleIcon::kGoogleDocsIcon:
      return IDR_PREINSTALLED_WEB_APPS_GOOGLE_DOCS_ICON_192_PNG;
    case BubbleIcon::kPrintJobsIcon:
      return IDR_ASH_PRINT_MANAGEMENT_PRINT_MANAGEMENT_192_PNG;
    case BubbleIcon::kYouTubeIcon:
      return IDR_PREINSTALLED_WEB_APPS_YOUTUBE_ICON_192_PNG;
    case BubbleIcon::kPlayStoreIcon:
      return IDR_SCALABLE_IPH_GOOGLE_PLAY_ICON_128_PNG;
    case BubbleIcon::kGooglePhotosIcon:
      return IDR_SCALABLE_IPH_GOOGLE_PHOTOS_ICON_128_PNG;
    case BubbleIcon::kNoIcon:
      NOTREACHED_NORETURN();
  }
#else
  return IDR_PRODUCT_LOGO_128;
#endif  // BUILDFLAG(ENABLE_CROS_SCALABLE_IPH)
}

class ScalableIphNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  ScalableIphNotificationDelegate(
      std::unique_ptr<scalable_iph::IphSession> iph_session,
      std::string notification_id,
      Action action)
      : iph_session_(std::move(iph_session)),
        notification_id_(notification_id),
        action_(action) {}

  // message_center::NotificationDelegate:
  void Click(const absl::optional<int>& button_index,
             const absl::optional<std::u16string>& reply) override {
    if (!button_index.has_value() || button_index.value() != kButtonIndex) {
      return;
    }

    iph_session_->PerformAction(action_.action_type, action_.iph_event_name);
    message_center::MessageCenter::Get()->RemoveNotification(notification_id_,
                                                             /*by_user=*/false);
  }

 private:
  ~ScalableIphNotificationDelegate() override = default;

  std::unique_ptr<scalable_iph::IphSession> iph_session_;
  std::string notification_id_;
  Action action_;
};

}  // namespace

ScalableIphDelegateImpl::ScalableIphDelegateImpl(Profile* profile)
    : profile_(profile) {
  GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  remote_cros_network_config_->AddObserver(
      receiver_cros_network_config_observer_.BindNewPipeAndPassRemote());

  QueryOnlineNetworkState();

  shell_observer_.Observe(Shell::Get());

  auto* session_controller = Shell::Get()->session_controller();
  CHECK(session_controller);
  session_observer_.Observe(session_controller);

  auto* power_manager_client = chromeos::PowerManagerClient::Get();
  CHECK(power_manager_client);
  power_manager_client_observer_.Observe(power_manager_client);

  AppListController* app_list_controller = AppListController::Get();
  CHECK(app_list_controller);
  app_list_controller_observer_.Observe(app_list_controller);

  MessageViewFactory::SetCustomNotificationViewFactory(
      kWallpaperNotificationType,
      base::BindRepeating(&WallpaperAshNotificationView::CreateWithPreview));
}

// Remember NOT to interact with `iph_session` from the destructor. See the
// comment of `ScalableIphDelegate::ShowBubble` for details.
ScalableIphDelegateImpl::~ScalableIphDelegateImpl() {
  // Remove the custom notification view factories.
  MessageViewFactory::ClearCustomNotificationViewFactory(
      kWallpaperNotificationType);
}

void ScalableIphDelegateImpl::ShowBubble(
    const scalable_iph::ScalableIphDelegate::BubbleParams& params,
    std::unique_ptr<scalable_iph::IphSession> iph_session) {
  // It will be no-op if the `bubble_id_` is an empty string when the first time
  // to show a bubble.
  ash::AnchoredNudgeManager::Get()->Cancel(bubble_id_);
  bubble_id_ = params.bubble_id;
  bubble_iph_session_ = std::move(iph_session);

  ash::AnchoredNudgeData nudge_data(
      params.bubble_id, NudgeCatalogName::kScalableIphBubble,
      base::UTF8ToUTF16(params.text), /*anchor_view=*/nullptr);

  if (!params.button.text.empty()) {
    nudge_data.first_button_text = base::UTF8ToUTF16(params.button.text);
    nudge_data.first_button_callback = base::BindRepeating(
        &ScalableIphDelegateImpl::OnNudgeButtonClicked,
        weak_ptr_factory_.GetWeakPtr(), params.bubble_id, params.button.action);
  }

  nudge_data.dismiss_callback =
      base::BindRepeating(&ScalableIphDelegateImpl::OnNudgeDismissed,
                          weak_ptr_factory_.GetWeakPtr(), params.bubble_id);

  if (params.icon != BubbleIcon::kNoIcon) {
    gfx::ImageSkia* image =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            GetResourceId(params.icon));
    gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
        *image, skia::ImageOperations::RESIZE_BEST, kBubbleIconSizeDip);
    resized_image.EnsureRepsForSupportedScales();
    nudge_data.image_model = ui::ImageModel::FromImageSkia(resized_image);
  }
  ash::AnchoredNudgeManager::Get()->Show(nudge_data);
}

void ScalableIphDelegateImpl::ShowNotification(
    const NotificationParams& params,
    std::unique_ptr<scalable_iph::IphSession> iph_session) {
  // TODO(b/284158831): Add implementation.
  std::string notification_source_name = kNotificationSourceName;
  std::string notification_title = params.title;
  std::string notification_text = params.text;

  message_center::RichNotificationData rich_notification_data;
  CHECK(!params.button.text.empty())
      << "Scalable IPH notification should have a button";
  std::string button_text = params.button.text;
  message_center::ButtonInfo button_info;
  button_info.title = base::UTF8ToUTF16(button_text);
  rich_notification_data.buttons.push_back(button_info);

#if BUILDFLAG(ENABLE_CROS_SCALABLE_IPH)
  if (IsWallpaperNotification(params)) {
    rich_notification_data.image =
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            IDR_SCALABLE_IPH_NOTIFICATION_WALLPAPER_1_PNG);
  }
#endif  // BUILDFLAG(ENABLE_CROS_SCALABLE_IPH)

  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotificationPtr(
          GetNotificationType(params), params.notification_id,
          base::UTF8ToUTF16(notification_title),
          base::UTF8ToUTF16(notification_text),
          base::UTF8ToUTF16(notification_source_name), GURL(), GetNotifierId(),
          rich_notification_data,
          base::MakeRefCounted<ScalableIphNotificationDelegate>(
              std::move(iph_session), params.notification_id,
              params.button.action),
          gfx::kNoneIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  if (IsWallpaperNotification(params)) {
    notification->set_custom_view_type(kWallpaperNotificationType);
  }
  AddOrReplaceNotification(std::move(notification));
}

void ScalableIphDelegateImpl::AddObserver(DelegateObserver* observer) {
  observers_.AddObserver(observer);
}

void ScalableIphDelegateImpl::RemoveObserver(DelegateObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool ScalableIphDelegateImpl::IsOnline() {
  return has_online_network_;
}

int ScalableIphDelegateImpl::ClientAgeInDays() {
  const base::Time& creation_time = profile_->GetCreationTime();
  const base::TimeDelta& delta = base::Time::Now() - creation_time;
  return delta.InDaysFloored();
}

void ScalableIphDelegateImpl::PerformActionForScalableIph(
    ActionType action_type) {
  switch (action_type) {
    case ActionType::kOpenChrome: {
      OpenUrlForProfile(profile_,
                        GURL(GetActionTypeURLs().at(ActionType::kOpenChrome)));
      break;
    }
    case ActionType::kOpenPersonalizationApp: {
      OpenUrlForProfile(
          profile_,
          GURL(GetActionTypeURLs().at(ActionType::kOpenPersonalizationApp)));
      break;
    }
    case ActionType::kOpenPlayStore: {
      bool app_launched = false;
      if (IsAppValidForProfile(profile_, arc::kPlayStoreAppId)) {
        app_launched = arc::LaunchApp(
            profile_, arc::kPlayStoreAppId, ui::EF_NONE,
            arc::UserInteractionType::APP_STARTED_FROM_OTHER_APP);
      }
      if (!app_launched) {
        OpenUrlForProfile(
            profile_, GURL(GetActionTypeURLs().at(ActionType::kOpenPlayStore)));
      }
      break;
    }
    case ActionType::kOpenGoogleDocs: {
      OpenUrlForProfile(
          profile_, GURL(GetActionTypeURLs().at(ActionType::kOpenGoogleDocs)));
      break;
    }
    case ActionType::kOpenGooglePhotos: {
      bool app_launched = false;
      if (IsAppValidForProfile(profile_, arc::kGooglePhotosAppId)) {
        app_launched = arc::LaunchApp(
            profile_, arc::kGooglePhotosAppId, ui::EF_NONE,
            arc::UserInteractionType::APP_STARTED_FROM_OTHER_APP);
      }
      if (!app_launched) {
        OpenUrlForProfile(
            profile_,
            GURL(GetActionTypeURLs().at(ActionType::kOpenGooglePhotos)));
      }
      break;
    }
    case ActionType::kOpenSettingsPrinter: {
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          profile_, chromeos::settings::mojom::kPrintingDetailsSubpagePath);
      break;
    }
    case ActionType::kOpenPhoneHub: {
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          profile_, chromeos::settings::mojom::kMultiDeviceSectionPath);
      break;
    }
    case ActionType::kOpenYouTube: {
      if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
              profile_) &&
          IsAppValidForProfile(profile_, extension_misc::kYoutubePwaAppId)) {
        auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
        proxy->LaunchAppWithUrl(
            extension_misc::kYoutubePwaAppId,
            apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                                /*prefer_container=*/true),
            GURL(GetActionTypeURLs().at(ActionType::kOpenYouTube)),
            apps::LaunchSource::kFromOtherApp,
            std::make_unique<apps::WindowInfo>(display::kDefaultDisplayId));
      } else {
        OpenUrlForProfile(
            profile_, GURL(GetActionTypeURLs().at(ActionType::kOpenYouTube)));
      }
      break;
    }
    case ActionType::kOpenFileManager: {
      std::string user_id_hash =
          ash::BrowserContextHelper::GetUserIdHashFromBrowserContext(profile_);
      std::unique_ptr<crosapi::FilesAppLauncher> files_app_launcher =
          std::make_unique<crosapi::FilesAppLauncher>(
              apps::AppServiceProxyFactory::GetForProfile(profile_));
      files_app_launcher->Launch(base::BindOnce(
          crosapi::browser_util::ClearGotoFilesClicked,
          g_browser_process->local_state(), std::move(user_id_hash)));
      break;
    }
    case ActionType::kOpenLauncher:
    case ActionType::kInvalid: {
      DLOG(WARNING)
          << "Action type does not have an implemented call-to-action.";
      return;
    }
  }
}

void ScalableIphDelegateImpl::OnActiveNetworksChanged(
    std::vector<NetworkStatePropertiesPtr> networks) {
  SetHasOnlineNetwork(HasOnlineNetwork(networks));
}

void ScalableIphDelegateImpl::OnShellDestroying() {
  app_list_controller_observer_.Reset();
  power_manager_client_observer_.Reset();
  session_observer_.Reset();
  shell_observer_.Reset();
}

void ScalableIphDelegateImpl::OnLockStateChanged(bool locked) {
  NotifyLockStateChanged(locked);
}

void ScalableIphDelegateImpl::SuspendDone(base::TimeDelta sleep_duration) {
  // Do not record event when the lock screen is enabled.
  if (ash::LockScreen::HasInstance()) {
    return;
  }
  NotifySuspendDoneWithoutLockScreen();
}

void ScalableIphDelegateImpl::OnAppListVisibilityChanged(bool shown,
                                                         int64_t display_id) {
  for (DelegateObserver& observer : observers_) {
    observer.OnAppListVisibilityChanged(shown);
  }
}

void ScalableIphDelegateImpl::SetHasOnlineNetwork(bool has_online_network) {
  if (has_online_network_ == has_online_network) {
    return;
  }

  has_online_network_ = has_online_network;

  for (DelegateObserver& observer : observers_) {
    observer.OnConnectionChanged(has_online_network_);
  }
}

void ScalableIphDelegateImpl::QueryOnlineNetworkState() {
  remote_cros_network_config_->GetNetworkStateList(
      NetworkFilter::New(FilterType::kActive, NetworkType::kAll, kNoLimit),
      base::BindOnce(&ScalableIphDelegateImpl::OnNetworkStateList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScalableIphDelegateImpl::OnNetworkStateList(
    std::vector<NetworkStatePropertiesPtr> networks) {
  SetHasOnlineNetwork(HasOnlineNetwork(networks));
}

void ScalableIphDelegateImpl::NotifyLockStateChanged(bool locked) {
  for (DelegateObserver& observer : observers_) {
    observer.OnLockStateChanged(locked);
  }
}

void ScalableIphDelegateImpl::NotifySuspendDoneWithoutLockScreen() {
  for (DelegateObserver& observer : observers_) {
    observer.OnSuspendDoneWithoutLockScreen();
  }
}

void ScalableIphDelegateImpl::OnNudgeButtonClicked(const std::string& bubble_id,
                                                   Action action) {
  if (bubble_id_ != bubble_id) {
    DCHECK(false) << "Callback for an obsolete bubble id gets called "
                  << bubble_id;
    return;
  }
  bubble_iph_session_->PerformAction(action.action_type, action.iph_event_name);
}

void ScalableIphDelegateImpl::OnNudgeDismissed(const std::string& bubble_id) {
  if (bubble_id_ != bubble_id) {
    DCHECK(false) << "Callback for an obsolete bubble id gets called "
                  << bubble_id;
    return;
  }
  bubble_iph_session_.reset();
  bubble_id_ = "";
}

}  // namespace ash
