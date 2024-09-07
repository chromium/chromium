// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scalable_iph/scalable_iph_delegate_impl.h"

#include <memory>
#include <string_view>

#include "apps/launcher.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/root_window_controller.h"
#include "ash/scalable_iph/scalable_iph_ash_notification_view.h"
#include "ash/scalable_iph/wallpaper_ash_notification_view.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/notification_center/message_view_factory.h"
#include "ash/webui/grit/ash_print_management_resources.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
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
#include "chrome/browser/ash/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/ash/printing/synced_printers_manager.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chromeos/ash/components/phonehub/feature_status_provider.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/components/scalable_iph/buildflags.h"
#include "chromeos/ash/components/scalable_iph/iph_session.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "chromeos/ash/grit/ash_resources.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/variations/service/variations_service.h"
#include "net/base/url_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "url/gurl.h"
#include "url/url_canon_stdstring.h"

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
using DelegateSessionState = ::scalable_iph::ScalableIphDelegate::SessionState;
using Action = ::scalable_iph::ScalableIphDelegate::Action;
using NotificationParams =
    ::scalable_iph::ScalableIphDelegate::NotificationParams;
using NotificationImageType =
    ::scalable_iph::ScalableIphDelegate::NotificationImageType;
using BubbleIcon = ::scalable_iph::ScalableIphDelegate::BubbleIcon;
using scalable_iph::ActionType;

constexpr char kScalableIphNotificationType[] =
    "scalable_iph_notification_type";
constexpr char kWallpaperNotificationType[] = "wallpaper_notification_type";
constexpr char kNotifierId[] = "scalable_iph";
constexpr char kButtonIndex = 0;
constexpr gfx::Size kBubbleIconSizeDip = gfx::Size(60, 60);
constexpr char kHelpAppPerksUrl[] = "chrome://help-app/offers";

constexpr std::string_view kChromebookPerksUrl =
    "https://www.google.com/chromebook/perks/";
constexpr std::string_view kChromebookPerksUrlQueryNameId = "id";

constexpr auto kIdsOfPerksMinecraftRealms2023 =
    base::MakeFixedFlatMap<std::string_view, std::string_view>(
        {{"us", "minecraft.realms.2023"},
         {"gb", "minecraft.uk.2023"},
         {"ca", "minecraft.realms.ca.2023"},
         {"au", "minecraft.realms.au.2023"}});

std::string GetCountryCode() {
  return g_browser_process->variations_service()->GetStoredPermanentCountry();
}

GURL GetPerksMinecraftRealmsUrl(std::string_view country_code) {
  const auto it = kIdsOfPerksMinecraftRealms2023.find(country_code);
  if (it == kIdsOfPerksMinecraftRealms2023.end()) {
    return GURL();
  }

  return net::AppendQueryParameter(GURL(kChromebookPerksUrl),
                                   kChromebookPerksUrlQueryNameId, it->second);
}

const base::flat_map<ActionType, std::string>& GetActionTypeURLs() {
  static const base::NoDestructor<base::flat_map<ActionType, std::string>>
      action_type_urls(
          {{ActionType::kOpenChrome, "chrome://new-tab-page/"},
           {ActionType::kOpenPlayStore,
            "https://play.google.com/store/games?device=chromebook"},
           {ActionType::kOpenGoogleDocs,
            "https://docs.google.com/document/?usp=installed_webapp/"},
           {ActionType::kOpenGooglePhotos, "https://photos.google.com/"},
           {ActionType::kOpenYouTube, "https://www.youtube.com/"},
           {ActionType::kOpenChromebookPerksWeb,
            "https://www.google.com/chromebook/perks/"},
           {ActionType::kOpenChromebookPerksGfnPriority2022,
            "https://www.google.com/chromebook/perks/?id=gfn.priority.2022"},
           {ActionType::kOpenChromebookPerksMinecraft2023,
            "https://www.google.com/chromebook/perks/?id=minecraft.2023"}});
  return *action_type_urls;
}

std::string ToString(ConnectionStateType connection_state_type) {
  switch (connection_state_type) {
    case ConnectionStateType::kOnline:
      return "Online";
    case ConnectionStateType::kConnected:
      return "Connected";
    case ConnectionStateType::kPortal:
      return "Portal";
    case ConnectionStateType::kConnecting:
      return "Connecting";
    case ConnectionStateType::kNotConnected:
      return "NotConnected";
  }
}

bool HasOnlineNetwork(const std::vector<NetworkStatePropertiesPtr>& networks,
                      scalable_iph::Logger* logger) {
  SCALABLE_IPH_LOG(logger) << "Checking networks. Size: " << networks.size();
  for (const NetworkStatePropertiesPtr& network : networks) {
    SCALABLE_IPH_LOG(logger)
        << network->name << ": " << ToString(network->connection_state);
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

bool IsAppValidForProfile(Profile* profile, const std::string& app_id) {
  if (app_id == arc::kPlayStoreAppId &&
      !arc::IsArcPlayStoreEnabledForProfile(profile)) {
    return false;
  }

  if (!arc::IsArcAllowedForProfile(profile)) {
    return false;
  }

  ArcAppListPrefs* const prefs = ArcAppListPrefs::Get(profile);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (!app_info || !app_info->ready) {
    return false;
  }

  return true;
}

void OpenUrlForProfile(Profile* profile,
                       const GURL& url,
                       scalable_iph::Logger* logger) {
  SCALABLE_IPH_LOG(logger) << "Opening a url with ash::NewWindowDelegate. Url: "
                           << url;
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
      return IDR_SCALABLE_IPH_GOOGLE_DOCS_ICON_120_PNG;
    case BubbleIcon::kPrintJobsIcon:
      return IDR_ASH_PRINT_MANAGEMENT_PRINT_MANAGEMENT_192_PNG;
    case BubbleIcon::kYouTubeIcon:
      return IDR_SCALABLE_IPH_YOUTUBE_ICON_120_PNG;
    case BubbleIcon::kPlayStoreIcon:
      return IDR_SCALABLE_IPH_GOOGLE_PLAY_ICON_120_PNG;
    case BubbleIcon::kGooglePhotosIcon:
      return IDR_SCALABLE_IPH_GOOGLE_PHOTOS_ICON_120_PNG;
    case BubbleIcon::kNoIcon:
      NOTREACHED();
  }
#else
  return IDR_PRODUCT_LOGO_128;
#endif  // BUILDFLAG(ENABLE_CROS_SCALABLE_IPH)
}

std::optional<int> GetResourceId(
    NotificationImageType notification_image_type) {
#if BUILDFLAG(ENABLE_CROS_SCALABLE_IPH)
  switch (notification_image_type) {
    case NotificationImageType::kNoImage:
      return std::nullopt;
    case NotificationImageType::kWallpaper:
      return IDR_SCALABLE_IPH_NOTIFICATION_WALLPAPER_1_PNG;
    case NotificationImageType::kMinecraft:
      return IDR_SCALABLE_IPH_NOTIFICATION_MINECRAFT_1x_PNG;
  }
#else
  return std::nullopt;
#endif  // BUILDFLAG(ENABLE_CROS_SCALABLE_IPH)
}

std::optional<std::string> GetNotificationCustomViewType(
    const NotificationParams& params) {
  if (params.image_type == NotificationImageType::kWallpaper) {
    return kWallpaperNotificationType;
  }

  if (params.summary_text == scalable_iph::ScalableIphDelegate::
                                 NotificationSummaryText::kWelcomeTips) {
    return kScalableIphNotificationType;
  }

  return std::nullopt;
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
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {
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

DelegateSessionState GetDelegateSessionState(
    session_manager::SessionState state) {
  switch (state) {
    case session_manager::SessionState::ACTIVE:
      return DelegateSessionState::kActive;
    case session_manager::SessionState::LOCKED:
      return DelegateSessionState::kLocked;
    default:
      return DelegateSessionState::kOther;
  }
}

}  // namespace

ScalableIphDelegateImpl::ScalableIphDelegateImpl(Profile* profile,
                                                 scalable_iph::Logger* logger)
    : profile_(profile), logger_(logger) {
  CHECK(profile_);
  CHECK(logger_);

  SCALABLE_IPH_LOG(GetLogger()) << "Initializing ScalableIphDelegateImpl";

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
      kScalableIphNotificationType,
      base::BindRepeating(&ScalableIphAshNotificationView::CreateView));

  MessageViewFactory::SetCustomNotificationViewFactory(
      kWallpaperNotificationType,
      base::BindRepeating(&WallpaperAshNotificationView::CreateWithPreview));

  synced_printers_manager_ =
      SyncedPrintersManagerFactory::GetForBrowserContext(profile);
  CHECK(synced_printers_manager_);
  synced_printers_manager_observer_.Observe(synced_printers_manager_);
  MaybeNotifyHasSavedPrinters();

  if (features::IsCrossDeviceFeatureSuiteAllowed()) {
    DCHECK(ash::Shell::Get()->system_tray_model()->phone_hub_manager())
        << "PhoneHubManager is expected to be initialized at a specific "
           "timing. "
           "See a comment in "
           "PhoneHubManagerFactory::ServiceIsCreatedWithBrowserContext. Below "
           "PhoneHubManagerFactory::GetForProfile will lazy create a "
           "PhoneHubManager. It should be fine as ScalableIph is also "
           "initialized at the same timing. But it's ideal if PhoneHubManager "
           "is "
           "created at the intended initialization timing instead of our call, "
           "i.e. PhoneHubManager should be already created at this point.";
    phonehub::PhoneHubManager* phone_hub_manager =
        phonehub::PhoneHubManagerFactory::GetForProfile(profile);
    CHECK(phone_hub_manager);
    feature_status_provider_ = phone_hub_manager->GetFeatureStatusProvider();
    CHECK(feature_status_provider_);
    feature_status_provider_observer_.Observe(feature_status_provider_);
    MaybeNotifyPhoneHubOnboardingEligibility();
  }
}

// Remember NOT to interact with `iph_session` from the destructor. See the
// comment of `ScalableIphDelegate::ShowBubble` for details.
ScalableIphDelegateImpl::~ScalableIphDelegateImpl() {
  // Remove the custom notification view factories.
  MessageViewFactory::ClearCustomNotificationViewFactory(
      kScalableIphNotificationType);
  MessageViewFactory::ClearCustomNotificationViewFactory(
      kWallpaperNotificationType);
}

bool ScalableIphDelegateImpl::ShowBubble(
    const scalable_iph::ScalableIphDelegate::BubbleParams& params,
    std::unique_ptr<scalable_iph::IphSession> iph_session) {
  if (!IsEligibleAction(params.button.action.action_type)) {
    SCALABLE_IPH_LOG(GetLogger())
        << "Specified action is not eligible -> Not showing a bubble.";
    return false;
  }

  // TODO(b/323426306): move this out to //ash/scalable_iph.
  SCALABLE_IPH_LOG(GetLogger()) << "Show bubble: " << params;

  // It will be no-op if the `bubble_id_` is an empty string when the first time
  // to show a bubble.
  ash::AnchoredNudgeManager::Get()->Cancel(bubble_id_);
  bubble_id_ = params.bubble_id;
  bubble_iph_session_ = std::move(iph_session);

  ShelfAppButton* anchor_view = nullptr;
  if (!params.anchor_view_app_id.empty()) {
    anchor_view =
        Shell::GetPrimaryRootWindowController()
            ->shelf()
            ->hotseat_widget()
            ->GetShelfView()
            ->GetShelfAppButton(ash::ShelfID(params.anchor_view_app_id));
    if (!anchor_view) {
      // In the case that the specified app ID cannot be found on the shelf,
      // the bubble can't be anchored and will not be shown.
      SCALABLE_IPH_LOG(GetLogger())
          << "Unable to find a view for specified anchor view app id. Anchor "
             "view app id: "
          << params.anchor_view_app_id << " -> Not showing a bubble.";
      bubble_iph_session_.reset();
      bubble_id_ = "";
      return false;
    }
  }

  ash::AnchoredNudgeData nudge_data(
      params.bubble_id, NudgeCatalogName::kScalableIphBubble,
      base::UTF8ToUTF16(params.text), /*anchor_view=*/anchor_view);

  if (!params.title.empty()) {
    nudge_data.title_text = base::UTF8ToUTF16(params.title);
  }

  // Currently, the help app on the shelf is the only view to which a bubble
  // will be anchored to. Therefore, if the anchor_view is non-null, the
  // nudge should be anchored to shelf. Once bubbles fully support anchor views,
  // this behavior may change.
  if (anchor_view) {
    SCALABLE_IPH_LOG(GetLogger()) << "Anchoring bubble UI to the shelf.";
    nudge_data.anchored_to_shelf = true;
  }

  if (!params.button.text.empty()) {
    nudge_data.primary_button_text = base::UTF8ToUTF16(params.button.text);
    nudge_data.primary_button_callback = base::BindRepeating(
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

  return true;
}

bool ScalableIphDelegateImpl::ShowNotification(
    const NotificationParams& params,
    std::unique_ptr<scalable_iph::IphSession> iph_session) {
  if (!IsEligibleAction(params.button.action.action_type)) {
    SCALABLE_IPH_LOG(GetLogger())
        << "Specified action is not eligible -> Not showing a notification.";
    return false;
  }

  // TODO(b/323426306): move this out to //ash/scalable_iph.
  SCALABLE_IPH_LOG(GetLogger()) << "Show notification: " << params;

  std::string notification_source_name = params.source;
  std::string notification_title = params.title;
  std::string notification_text = params.text;

  message_center::RichNotificationData rich_notification_data;
  CHECK(!params.button.text.empty())
      << "Scalable IPH notification should have a button";
  std::string button_text = params.button.text;
  message_center::ButtonInfo button_info;
  button_info.title = base::UTF8ToUTF16(button_text);
  rich_notification_data.buttons.push_back(button_info);

  std::optional<int> notification_image_id = GetResourceId(params.image_type);
  if (notification_image_id) {
    rich_notification_data.image =
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            notification_image_id.value());
  }

  const gfx::VectorIcon* icon = &gfx::kNoneIcon;
  if (params.icon == ScalableIphDelegate::NotificationIcon::kRedeem) {
    icon = &chromeos::kRedeemIcon;
  }

  std::optional<std::string> custom_view_type =
      GetNotificationCustomViewType(params);
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotificationPtr(
          custom_view_type ? message_center::NOTIFICATION_TYPE_CUSTOM
                           : message_center::NOTIFICATION_TYPE_SIMPLE,
          params.notification_id, base::UTF8ToUTF16(notification_title),
          base::UTF8ToUTF16(notification_text),
          base::UTF8ToUTF16(notification_source_name), GURL(), GetNotifierId(),
          rich_notification_data,
          base::MakeRefCounted<ScalableIphNotificationDelegate>(
              std::move(iph_session), params.notification_id,
              params.button.action),
          *icon, message_center::SystemNotificationWarningLevel::NORMAL);

  if (custom_view_type) {
    notification->set_custom_view_type(custom_view_type.value());
  }

  AddOrReplaceNotification(std::move(notification));

  return true;
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
  SCALABLE_IPH_LOG(GetLogger())
      << "Performing action: Action type: " << action_type;

  switch (action_type) {
    case ActionType::kOpenChrome: {
      OpenUrlForProfile(profile_,
                        GURL(GetActionTypeURLs().at(ActionType::kOpenChrome)),
                        GetLogger());
      break;
    }
    case ActionType::kOpenPersonalizationApp: {
      SCALABLE_IPH_LOG(GetLogger())
          << "Opening ash::SystemWebAppType::PERSONALIZATION via "
             "ash::LaunchSystemWebAppAsync.";
      ash::LaunchSystemWebAppAsync(profile_,
                                   ash::SystemWebAppType::PERSONALIZATION);
      break;
    }
    case ActionType::kOpenPlayStore: {
      bool app_launched = false;
      if (IsAppValidForProfile(profile_, arc::kPlayStoreAppId)) {
        app_launched = arc::LaunchApp(
            profile_, arc::kPlayStoreAppId, ui::EF_NONE,
            arc::UserInteractionType::APP_STARTED_FROM_OTHER_APP);
        SCALABLE_IPH_LOG(GetLogger())
            << "Opening Play Store Android app. App launched: " << app_launched;
      }
      if (!app_launched) {
        OpenUrlForProfile(
            profile_, GURL(GetActionTypeURLs().at(ActionType::kOpenPlayStore)),
            GetLogger());
      }
      break;
    }
    case ActionType::kOpenGoogleDocs: {
      OpenUrlForProfile(
          profile_, GURL(GetActionTypeURLs().at(ActionType::kOpenGoogleDocs)),
          GetLogger());
      break;
    }
    case ActionType::kOpenGooglePhotos: {
      bool app_launched = false;
      if (IsAppValidForProfile(profile_, arc::kGooglePhotosAppId)) {
        app_launched = arc::LaunchApp(
            profile_, arc::kGooglePhotosAppId, ui::EF_NONE,
            arc::UserInteractionType::APP_STARTED_FROM_OTHER_APP);
        SCALABLE_IPH_LOG(GetLogger())
            << "Opening Google Photos Android app. App launched: "
            << app_launched;
      }
      if (!app_launched) {
        OpenUrlForProfile(
            profile_,
            GURL(GetActionTypeURLs().at(ActionType::kOpenGooglePhotos)),
            GetLogger());
      }
      break;
    }
    case ActionType::kOpenSettingsPrinter: {
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          profile_, chromeos::settings::mojom::kPrintingDetailsSubpagePath);
      SCALABLE_IPH_LOG(GetLogger())
          << "Opening OSSettings kPrintingDetailsSubpagePath";
      break;
    }
    case ActionType::kOpenPhoneHub: {
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          profile_, chromeos::settings::mojom::kMultiDeviceSectionPath);
      SCALABLE_IPH_LOG(GetLogger())
          << "Opening OSSettings kMultiDeviceSectionPath";
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
        SCALABLE_IPH_LOG(GetLogger()) << "Opening YouTube app via AppService.";
      } else {
        OpenUrlForProfile(
            profile_, GURL(GetActionTypeURLs().at(ActionType::kOpenYouTube)),
            GetLogger());
      }
      break;
    }
    case ActionType::kOpenFileManager: {
      ash::NewWindowDelegate::GetPrimary()->OpenFileManager();
      SCALABLE_IPH_LOG(GetLogger()) << "Opening file manager.";
      break;
    }
    case ActionType::kOpenHelpAppPerks: {
      SCALABLE_IPH_LOG(GetLogger())
          << "Opening ash::SystemWebAppType::HELP via "
             "ash::LaunchSystemWebAppAsync for url: "
          << kHelpAppPerksUrl;

      SystemAppLaunchParams system_app_launch_params;
      system_app_launch_params.url = GURL(kHelpAppPerksUrl);
      ash::LaunchSystemWebAppAsync(profile_, ash::SystemWebAppType::HELP,
                                   system_app_launch_params);
      break;
    }
    case ActionType::kOpenChromebookPerksWeb: {
      OpenUrlForProfile(
          profile_,
          GURL(GetActionTypeURLs().at(ActionType::kOpenChromebookPerksWeb)),
          GetLogger());
      break;
    }
    case ActionType::kOpenChromebookPerksGfnPriority2022: {
      OpenUrlForProfile(profile_,
                        GURL(GetActionTypeURLs().at(
                            ActionType::kOpenChromebookPerksGfnPriority2022)),
                        GetLogger());
      break;
    }
    case ActionType::kOpenChromebookPerksMinecraft2023: {
      OpenUrlForProfile(profile_,
                        GURL(GetActionTypeURLs().at(
                            ActionType::kOpenChromebookPerksMinecraft2023)),
                        GetLogger());
      break;
    }
    case ActionType::kOpenChromebookPerksMinecraftRealms2023: {
      GURL perks_url = GetPerksMinecraftRealmsUrl(GetCountryCode());

      CHECK(!perks_url.is_empty())
          << "No perks url found for " << GetCountryCode()
          << ". Unable to perform an action. This must not happen as "
             "eligibility of an action must be checked by `IsEligibleAction` "
             "before showing it in a UI.";
      CHECK(perks_url.is_valid())
          << "Invalid url is provided. Url is managed on the client side. We "
             "use CHECK as this is a client side invariant.";
      OpenUrlForProfile(profile_, perks_url, GetLogger());
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
  SetHasOnlineNetwork(HasOnlineNetwork(networks, GetLogger()));
}

void ScalableIphDelegateImpl::OnShellDestroying() {
  app_list_controller_observer_.Reset();
  power_manager_client_observer_.Reset();
  session_observer_.Reset();
  shell_observer_.Reset();
}

void ScalableIphDelegateImpl::OnSessionStateChanged(
    session_manager::SessionState state) {
  NotifySessionStateChanged(GetDelegateSessionState(state));
}

void ScalableIphDelegateImpl::SuspendDone(base::TimeDelta sleep_duration) {
  // Do not record event when the lock screen is enabled.
  if (ash::LockScreen::HasInstance()) {
    SCALABLE_IPH_LOG(GetLogger()) << "Observed SuspendDone. But do nothing as "
                                     "the lock screen is enabled.";
    return;
  }

  SCALABLE_IPH_LOG(GetLogger())
      << "Observed SuspendDone. Notifying SuspendDoneWithoutLockScreen.";
  NotifySuspendDoneWithoutLockScreen();
}

void ScalableIphDelegateImpl::OnAppListVisibilityChanged(bool shown,
                                                         int64_t display_id) {
  SCALABLE_IPH_LOG(GetLogger())
      << "App list visibility changed. Shown: " << shown
      << " Display Id: " << display_id;
  for (DelegateObserver& observer : observers_) {
    observer.OnAppListVisibilityChanged(shown);
  }
}

void ScalableIphDelegateImpl::OnSavedPrintersChanged() {
  MaybeNotifyHasSavedPrinters();
}

void ScalableIphDelegateImpl::OnFeatureStatusChanged() {
  CHECK(feature_status_provider_observer_.IsObservingSource(
      feature_status_provider_));

  SCALABLE_IPH_LOG(GetLogger()) << "Phone hub feature status changed observer "
                                   "gets called. Going to check the status.";
  MaybeNotifyPhoneHubOnboardingEligibility();
}

void ScalableIphDelegateImpl::SetFakeFeatureStatusProviderForTesting(
    phonehub::FeatureStatusProvider* feature_status_provider) {
  CHECK(feature_status_provider_observer_.IsObserving())
      << "feature_status_provider_observer_ should be observing a real object.";
  CHECK(!feature_status_provider_observer_.IsObservingSource(
      feature_status_provider))
      << "feature_status_provider_observer_ is already observing a fake.";

  feature_status_provider_ = feature_status_provider;
  feature_status_provider_observer_.Reset();
  feature_status_provider_observer_.Observe(feature_status_provider_);
  MaybeNotifyPhoneHubOnboardingEligibility();
}

bool ScalableIphDelegateImpl::IsEligibleAction(
    scalable_iph::ActionType action_type) {
  if (action_type ==
      scalable_iph::ActionType::kOpenChromebookPerksMinecraftRealms2023) {
    GURL perks_url = GetPerksMinecraftRealmsUrl(GetCountryCode());
    if (perks_url.is_empty()) {
      SCALABLE_IPH_LOG(GetLogger())
          << action_type << " is not eligible for " << GetCountryCode();
      return false;
    }
  }
  return true;
}

void ScalableIphDelegateImpl::SetHasOnlineNetwork(bool has_online_network) {
  if (has_online_network_ == has_online_network) {
    SCALABLE_IPH_LOG(GetLogger())
        << "Has online network state is set to " << has_online_network
        << ". But do nothing as it's the same with the current internal state.";
    return;
  }

  SCALABLE_IPH_LOG(GetLogger())
      << "Has online network state has changed from " << has_online_network_
      << " to " << has_online_network << ". Notifying the state change.";

  has_online_network_ = has_online_network;

  for (DelegateObserver& observer : observers_) {
    observer.OnConnectionChanged(has_online_network_);
  }
}

void ScalableIphDelegateImpl::QueryOnlineNetworkState() {
  SCALABLE_IPH_LOG(GetLogger()) << "Querying network state.";

  remote_cros_network_config_->GetNetworkStateList(
      NetworkFilter::New(FilterType::kActive, NetworkType::kAll, kNoLimit),
      base::BindOnce(&ScalableIphDelegateImpl::OnNetworkStateList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScalableIphDelegateImpl::OnNetworkStateList(
    std::vector<NetworkStatePropertiesPtr> networks) {
  SCALABLE_IPH_LOG(GetLogger()) << "Received network state list.";

  SetHasOnlineNetwork(HasOnlineNetwork(networks, GetLogger()));
}

void ScalableIphDelegateImpl::NotifySessionStateChanged(
    DelegateSessionState session_state) {
  for (DelegateObserver& observer : observers_) {
    observer.OnSessionStateChanged(session_state);
  }
}

void ScalableIphDelegateImpl::NotifySuspendDoneWithoutLockScreen() {
  for (DelegateObserver& observer : observers_) {
    observer.OnSuspendDoneWithoutLockScreen();
  }
}

void ScalableIphDelegateImpl::MaybeNotifyHasSavedPrinters() {
  const bool has_saved_printers =
      !synced_printers_manager_->GetSavedPrinters().empty();

  if (has_saved_printers_ == has_saved_printers) {
    SCALABLE_IPH_LOG(GetLogger())
        << "Checked has saved printers status. Do nothing as it matches with "
           "the current internal state. Has saved printers: "
        << has_saved_printers_;
    return;
  }

  SCALABLE_IPH_LOG(GetLogger())
      << "Has saved printers status has changed from " << has_saved_printers_
      << " to " << has_saved_printers << ". Notifying the status change.";

  has_saved_printers_ = has_saved_printers;

  for (DelegateObserver& observer : observers_) {
    observer.OnHasSavedPrintersChanged(has_saved_printers_);
  }
}

void ScalableIphDelegateImpl::MaybeNotifyPhoneHubOnboardingEligibility() {
  CHECK(feature_status_provider_);
  phonehub::FeatureStatus feature_status =
      feature_status_provider_->GetStatus();

  // `kDisabled` means that a user can enable phone hub via settings. It means
  // that a user has an eligible phone.
  const bool phonehub_onboarding_eligible =
      feature_status == phonehub::FeatureStatus::kEligiblePhoneButNotSetUp ||
      feature_status == phonehub::FeatureStatus::kDisabled;

  SCALABLE_IPH_LOG(GetLogger())
      << "Checking phone hub feature status. Feature status: " << feature_status
      << ". Phone hub onboarding eligible: " << phonehub_onboarding_eligible;

  if (phonehub_onboarding_eligible_ == phonehub_onboarding_eligible) {
    SCALABLE_IPH_LOG(GetLogger())
        << "Do nothing as there is no change in phone hub onboarding eligible. "
           "Phone hub onboarding eligible: "
        << phonehub_onboarding_eligible_;
    return;
  }

  SCALABLE_IPH_LOG(GetLogger())
      << "Phone hub onboarding eligible has changed. Notifying observers. "
         "Phone hub onboarding eligible: from: "
      << phonehub_onboarding_eligible_
      << " to: " << phonehub_onboarding_eligible;

  phonehub_onboarding_eligible_ = phonehub_onboarding_eligible;

  for (DelegateObserver& observer : observers_) {
    observer.OnPhoneHubOnboardingEligibleChanged(phonehub_onboarding_eligible_);
  }
}

void ScalableIphDelegateImpl::OnNudgeButtonClicked(const std::string& bubble_id,
                                                   Action action) {
  SCALABLE_IPH_LOG(GetLogger())
      << "A button in a bubble gets clicked. Bubble id: " << bubble_id
      << " Action: " << action;
  if (bubble_id_ != bubble_id) {
    SCALABLE_IPH_LOG(GetLogger())
        << "Bubble id " << bubble_id << " is an obsolete id.";
    DCHECK(false) << "Callback for an obsolete bubble id gets called "
                  << bubble_id;
    return;
  }
  bubble_iph_session_->PerformAction(action.action_type, action.iph_event_name);
}

void ScalableIphDelegateImpl::OnNudgeDismissed(const std::string& bubble_id) {
  SCALABLE_IPH_LOG(GetLogger())
      << "A bubble gets dismissed. Bubble id: " << bubble_id;
  if (bubble_id_ != bubble_id) {
    SCALABLE_IPH_LOG(GetLogger())
        << "Bubble id " << bubble_id
        << " is an obsolete id. Current active bubble id is " << bubble_id_;
    return;
  }
  bubble_iph_session_.reset();
  bubble_id_ = "";
}

}  // namespace ash
