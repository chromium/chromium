// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_notification_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/notification_center/views/ash_notification_view.h"
#include "ash/system/notification_center/message_view_factory.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/notification_interaction_handler.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/phone_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view.h"

namespace ash {

using phone_hub_metrics::NotificationInteraction;
using phonehub::proto::CameraRollItemMetadata;

namespace {
const char kNotifierId[] = "chrome://phonehub";
const char kNotifierIdSeparator[] = "-";
const char kPhoneHubInstantTetherNotificationId[] =
    "chrome://phonehub-instant-tether";
const char kPhoneHubCameraRollNotificationId[] =
    "chrome://phonehub-camera-roll";
const char kNotificationCustomViewType[] = "phonehub";
const char kNotificationCustomCallViewType[] = "phonehub-call";
const int kReplyButtonIndex = 0;
const int kNotificationHeaderTextWidth = 180;
const int kNotificationAppNameMaxWidth = 140;

// The max age since a notification's creation on the Android for it to get
// shown in a heads-up pop up. Notifications older than this will only silently
// get added.
constexpr base::TimeDelta kMaxRecentNotificationAge = base::Seconds(15);

// The amount of time the reply button is disabled after sending an inline
// reply. This is used to make sure that all the replies are received by the
// phone in a correct order (a reply sent right after another could cause it to
// be received before the former one).
constexpr base::TimeDelta kInlineReplyDisableTime = base::Seconds(1);

class PhoneHubAshNotificationView : public AshNotificationView {
 public:
  explicit PhoneHubAshNotificationView(
      const message_center::Notification& notification,
      bool shown_in_popup,
      const std::u16string& phone_name)
      : AshNotificationView(notification, shown_in_popup) {
    // Add customized header.
    message_center::NotificationHeaderView* header_row =
        static_cast<message_center::NotificationHeaderView*>(
            GetViewByID(message_center::NotificationView::kHeaderRow));
    views::View* app_name_view =
        GetViewByID(message_center::NotificationView::kAppNameView);
    views::Label* summary_text_view = static_cast<views::Label*>(
        GetViewByID(message_center::NotificationView::kSummaryTextView));

    // The app name should be displayed in full, leaving the rest of the space
    // for device name. App name will only be truncated when it reached it
    // maximum width.
    int app_name_width = std::min(app_name_view->GetPreferredSize().width(),
                                  kNotificationAppNameMaxWidth);
    int device_name_width = kNotificationHeaderTextWidth - app_name_width;
    header_row->SetSummaryText(
        gfx::ElideText(phone_name, summary_text_view->font_list(),
                       device_name_width, gfx::ELIDE_TAIL));
    custom_view_type_ = notification.custom_view_type();
    if (custom_view_type_ == kNotificationCustomCallViewType) {
      // Expand the action buttons row by default for Call Style notification.
      SetManuallyExpandedOrCollapsed(
          !IsExpanded() ? message_center::ExpandState::USER_EXPANDED
                        : message_center::ExpandState::USER_COLLAPSED);
      SetExpanded(true);
      return;
    }
    action_buttons_row_ =
        GetViewByID(message_center::NotificationView::kActionButtonsRow);
    if (!action_buttons_row_->children().empty())
      reply_button_ = static_cast<views::View*>(
          action_buttons_row_->children()[kReplyButtonIndex]);

    inline_reply_ = static_cast<message_center::NotificationInputContainer*>(
        GetViewByID(message_center::NotificationView::kInlineReply));
  }

  ~PhoneHubAshNotificationView() override = default;
  PhoneHubAshNotificationView(const PhoneHubAshNotificationView&) = delete;
  PhoneHubAshNotificationView& operator=(const PhoneHubAshNotificationView&) =
      delete;

  // message_center::NotificationViewBase
  void ActionButtonPressed(size_t index, const ui::Event& event) override {
    if (custom_view_type_ == kNotificationCustomCallViewType) {
      message_center::MessageCenter::Get()->ClickOnNotificationButton(
          notification_id(), static_cast<int>(index));
    } else {
      AshNotificationView::ActionButtonPressed(index, event);
    }
  }

  // message_center::NotificationView:
  void OnNotificationInputSubmit(size_t index,
                                 const std::u16string& text) override {
    if (text.empty())
      return;
    AshNotificationView::OnNotificationInputSubmit(index, text);

    DCHECK(reply_button_);

    // After sending a reply, take the UI back to action buttons and clear out
    // text input.
    inline_reply_->SetVisible(false);
    action_buttons_row_->SetVisible(true);
    inline_reply_->textfield()->SetText(std::u16string());

    // Since the focus may still be on the now-hidden buttons used to send a
    // message, refocus on the entire notification.
    CHECK(GetFocusManager());
    GetFocusManager()->SetFocusedView(this);

    // Briefly disable reply button.
    reply_button_->SetEnabled(false);
    enable_reply_timer_.Start(
        FROM_HERE, kInlineReplyDisableTime,
        base::BindOnce(&PhoneHubAshNotificationView::EnableReplyButton,
                       base::Unretained(this)));
  }

  void EnableReplyButton() {
    reply_button_->SetEnabled(true);
    enable_reply_timer_.AbandonAndStop();
  }

 private:
  // Owned by view hierarchy.
  raw_ptr<views::View> action_buttons_row_ = nullptr;
  raw_ptr<views::View> reply_button_ = nullptr;
  raw_ptr<message_center::NotificationInputContainer> inline_reply_ = nullptr;

  // Timer that fires to enable reply button after a brief period of time.
  base::OneShotTimer enable_reply_timer_;
  std::string custom_view_type_;
};

}  // namespace

// Delegate for the displayed ChromeOS notification.
class PhoneHubNotificationController::NotificationDelegate
    : public message_center::NotificationObserver {
 public:
  NotificationDelegate(PhoneHubNotificationController* controller,
                       int64_t phone_hub_id,
                       const std::string& cros_id,
                       phonehub::Notification::Category category)
      : controller_(controller),
        phone_hub_id_(phone_hub_id),
        cros_id_(cros_id),
        category_(category) {}

  virtual ~NotificationDelegate() { controller_ = nullptr; }

  NotificationDelegate(const NotificationDelegate&) = delete;
  NotificationDelegate& operator=(const NotificationDelegate&) = delete;

  // Returns a scoped_refptr that can be passed in the
  // message_center::Notification constructor.
  scoped_refptr<message_center::NotificationDelegate> AsScopedRefPtr() {
    return base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
        weak_ptr_factory_.GetWeakPtr());
  }

  // Called by the controller to remove the notification from the message
  // center.
  void Remove() {
    removed_by_phone_hub_ = true;
    message_center::MessageCenter::Get()->RemoveNotification(cros_id_,
                                                             /*by_user=*/false);
  }

  // message_center::NotificationObserver:
  void Close(bool by_user) override {
    if (!controller_ || removed_by_phone_hub_)
      return;

    if (category_ == phonehub::Notification::Category::kIncomingCall ||
        category_ == phonehub::Notification::Category::kOngoingCall) {
      // TODO(b/203734343): Wait for UX confirm. Call notification is not
      // dismissible in android phone.
      PA_LOG(INFO)
          << "Can't dismiss an Incoming/Ongoing call notification with id: "
          << phone_hub_id_ << ".";
      return;
    }

    controller_->DismissNotification(phone_hub_id_);
  }

  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override {
    if (!controller_)
      return;

    if (!button_index.has_value()) {
      controller_->HandleNotificationBodyClick(
          phone_hub_id_, controller_->manager_->GetNotification(phone_hub_id_)
                             ->app_metadata());
      return;
    }
    if (category_ == phonehub::Notification::Category::kIncomingCall) {
      // TODO(b/199223417): Implement actions.
      switch (*button_index) {
        case BUTTON_ANSWER:
          PA_LOG(INFO) << "answer button clicked";
          break;
        case BUTTON_DECLINE:
          PA_LOG(INFO) << "decline button clicked";
          break;
      }
    } else if (category_ == phonehub::Notification::Category::kOngoingCall) {
      switch (*button_index) {
        case BUTTON_HANGUP:
          PA_LOG(INFO) << "hangup button clicked";
          break;
      }
    } else if (button_index.value() == kReplyButtonIndex && reply.has_value()) {
      controller_->SendInlineReply(phone_hub_id_, reply.value());
    }
  }

  void SettingsClick() override {
    if (controller_)
      controller_->OpenSettings();
  }

  phonehub::Notification::Category Category() { return category_; }

 private:
  // Incoming call buttons that appear in notifications.
  enum IncomingCallButton { BUTTON_DECLINE, BUTTON_ANSWER };
  // Ongoing call buttons that appear in notifications.
  enum OngoingCallButton { BUTTON_HANGUP };

  // The parent controller, which owns this object.
  raw_ptr<PhoneHubNotificationController> controller_ = nullptr;

  // The notification ID tracked by PhoneHub.
  const int64_t phone_hub_id_;

  // The notification ID tracked by the CrOS message center.
  const std::string cros_id_;

  // The category of the notification.
  phonehub::Notification::Category category_;

  // Flag set if the notification was removed by PhoneHub so we avoid a cycle.
  bool removed_by_phone_hub_ = false;

  base::WeakPtrFactory<NotificationDelegate> weak_ptr_factory_{this};
};

PhoneHubNotificationController::PhoneHubNotificationController() {
  if (!MessageViewFactory::HasCustomNotificationViewFactory(
          kNotificationCustomViewType)) {
    MessageViewFactory::SetCustomNotificationViewFactory(
        kNotificationCustomViewType,
        base::BindRepeating(
            &PhoneHubNotificationController::CreateCustomNotificationView,
            weak_ptr_factory_.GetWeakPtr()));
  }

  if (!MessageViewFactory::HasCustomNotificationViewFactory(
          kNotificationCustomCallViewType)) {
    MessageViewFactory::SetCustomNotificationViewFactory(
        kNotificationCustomCallViewType,
        base::BindRepeating(
            &PhoneHubNotificationController::CreateCustomActionNotificationView,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

PhoneHubNotificationController::~PhoneHubNotificationController() {
  if (manager_)
    manager_->RemoveObserver(this);
  if (tether_controller_)
    tether_controller_->RemoveObserver(this);
  if (camera_roll_manager_)
    camera_roll_manager_->RemoveObserver(this);
}

void PhoneHubNotificationController::SetManager(
    phonehub::PhoneHubManager* phone_hub_manager) {
  if (manager_)
    manager_->RemoveObserver(this);
  if (phone_hub_manager) {
    manager_ = phone_hub_manager->GetNotificationManager();
    manager_->AddObserver(this);
  } else {
    manager_ = nullptr;
  }

  if (tether_controller_)
    tether_controller_->RemoveObserver(this);
  if (phone_hub_manager) {
    tether_controller_ = phone_hub_manager->GetTetherController();
    tether_controller_->AddObserver(this);
  } else {
    tether_controller_ = nullptr;
  }

  if (camera_roll_manager_)
    camera_roll_manager_->RemoveObserver(this);
  if (phone_hub_manager) {
    camera_roll_manager_ = phone_hub_manager->GetCameraRollManager();
    if (camera_roll_manager_) {
      camera_roll_manager_->AddObserver(this);
    } else {
      camera_roll_manager_ = nullptr;
    }
  } else {
    camera_roll_manager_ = nullptr;
  }

  if (phone_hub_manager)
    phone_model_ = phone_hub_manager->GetPhoneModel();
  else
    phone_model_ = nullptr;

  if (phone_hub_manager) {
    notification_interaction_handler_ =
        phone_hub_manager->GetNotificationInteractionHandler();
  } else {
    notification_interaction_handler_ = nullptr;
  }
}

const std::u16string PhoneHubNotificationController::GetPhoneName() const {
  if (!phone_model_)
    return std::u16string();
  return phone_model_->phone_name().value_or(std::u16string());
}

void PhoneHubNotificationController::OnNotificationsAdded(
    const base::flat_set<int64_t>& notification_ids) {
  for (int64_t id : notification_ids) {
    SetNotification(manager_->GetNotification(id),
                    /*is_update=*/false);
  }

  LogNotificationCount();
}

void PhoneHubNotificationController::OnNotificationsUpdated(
    const base::flat_set<int64_t>& notification_ids) {
  for (int64_t id : notification_ids) {
    SetNotification(manager_->GetNotification(id),
                    /*is_update=*/true);
  }
}

void PhoneHubNotificationController::OnNotificationsRemoved(
    const base::flat_set<int64_t>& notification_ids) {
  for (int64_t id : notification_ids) {
    auto it = notification_map_.find(id);
    if (it == notification_map_.end())
      continue;
    it->second->Remove();
    notification_map_.erase(it);
  }

  LogNotificationCount();
}

void PhoneHubNotificationController::OnAttemptConnectionScanFailed() {
  // Add a notification if tether failed.
  scoped_refptr<message_center::NotificationDelegate> delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([](std::optional<int> button_index) {
            // When clicked, open Tether Settings page if we can open WebUI
            // settings, otherwise do nothing.
            if (TrayPopupUtils::CanOpenWebUISettings()) {
              Shell::Get()
                  ->system_tray_model()
                  ->client()
                  ->ShowTetherNetworkSettings();
            } else {
              LOG(WARNING) << "Cannot open Tether Settings since it's not "
                              "possible to opening WebUI settings";
            }
          }));
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kPhoneHubInstantTetherNotificationId,
          l10n_util::GetStringUTF16(
              IDS_ASH_PHONE_HUB_NOTIFICATION_HOTSPOT_FAILED_TITLE),
          l10n_util::GetStringUTF16(
              IDS_ASH_PHONE_HUB_NOTIFICATION_HOTSPOT_FAILED_MESSAGE),
          std::u16string() /*display_source */, GURL() /* origin_url */,
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kPhoneHubInstantTetherNotificationId,
              NotificationCatalogName::kPhoneHubTetherFailed),
          message_center::RichNotificationData(), std::move(delegate),
          kPhoneHubEnableHotspotIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void PhoneHubNotificationController::OnCameraRollDownloadError(
    DownloadErrorType error_type,
    const CameraRollItemMetadata& metadata) {
  std::unique_ptr<message_center::Notification> notification;
  switch (error_type) {
    case DownloadErrorType::kGenericError:
      notification = CreateCameraRollGenericNotification(metadata);
      break;
    case DownloadErrorType::kInsufficientStorage:
      notification = CreateCameraRollStorageNotification(metadata);
      break;
    case DownloadErrorType::kNetworkConnection:
      notification = CreateCameraRollNetworkNotification(metadata);
      break;
  }
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

std::unique_ptr<message_center::Notification>
PhoneHubNotificationController::CreateCameraRollGenericNotification(
    const CameraRollItemMetadata& metadata) {
  scoped_refptr<message_center::NotificationDelegate> delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              [](phonehub::CameraRollManager* manager,
                 const CameraRollItemMetadata& metadata,
                 std::optional<int> button_index) {
                // When button is clicked, close notification and retry the
                // download
                if (button_index.has_value()) {
                  message_center::MessageCenter::Get()->RemoveNotification(
                      kPhoneHubCameraRollNotificationId, /*by_user=*/true);
                  manager->DownloadItem(metadata);
                }
              },
              camera_roll_manager_, metadata));
  message_center::NotifierId notifier_id(
      message_center::NotifierType::PHONE_HUB,
      kPhoneHubCameraRollNotificationId);
  message_center::RichNotificationData optional_fields;
  message_center::ButtonInfo button;
  button.title = l10n_util::GetStringUTF16(
      IDS_ASH_PHONE_HUB_CAMERA_ROLL_ERROR_GENERIC_ACTION);
  optional_fields.buttons.push_back(button);
  return CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kPhoneHubCameraRollNotificationId,
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_CAMERA_ROLL_ERROR_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_ASH_PHONE_HUB_CAMERA_ROLL_ERROR_GENERIC_BODY,
          base::UTF8ToUTF16(metadata.file_name())),
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TRAY_ACCESSIBLE_NAME),
      /*origin_url=*/GURL(), notifier_id, optional_fields, std::move(delegate),
      kPhoneHubCameraRollMenuDownloadIcon,
      message_center::SystemNotificationWarningLevel::WARNING);
}

std::unique_ptr<message_center::Notification>
PhoneHubNotificationController::CreateCameraRollStorageNotification(
    const CameraRollItemMetadata& metadata) {
  scoped_refptr<message_center::NotificationDelegate> delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([](std::optional<int> button_index) {
            // When button is clicked, close notification and open Storage
            // Management Settings page if we can open WebUI settings.
            if (button_index.has_value()) {
              message_center::MessageCenter::Get()->RemoveNotification(
                  kPhoneHubCameraRollNotificationId, /*by_user=*/true);
              if (TrayPopupUtils::CanOpenWebUISettings()) {
                Shell::Get()
                    ->system_tray_model()
                    ->client()
                    ->ShowStorageSettings();
              } else {
                PA_LOG(WARNING)
                    << "Cannot open Storage Management Settings since it's not "
                       "possible to open WebUI settings";
              }
            }
          }));
  message_center::NotifierId notifier_id(
      message_center::NotifierType::PHONE_HUB,
      kPhoneHubCameraRollNotificationId);
  message_center::RichNotificationData optional_fields;
  message_center::ButtonInfo button;
  button.title = l10n_util::GetStringUTF16(
      IDS_ASH_PHONE_HUB_CAMERA_ROLL_ERROR_STORAGE_ACTION);
  optional_fields.buttons.push_back(button);
  return CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kPhoneHubCameraRollNotificationId,
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_CAMERA_ROLL_ERROR_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_ASH_PHONE_HUB_CAMERA_ROLL_ERROR_STORAGE_BODY,
          base::UTF8ToUTF16(metadata.file_name())),
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TRAY_ACCESSIBLE_NAME),
      /*origin_url=*/GURL(), notifier_id, optional_fields, std::move(delegate),
      kPhoneHubCameraRollMenuDownloadIcon,
      message_center::SystemNotificationWarningLevel::WARNING);
}

std::unique_ptr<message_center::Notification>
PhoneHubNotificationController::CreateCameraRollNetworkNotification(
    const CameraRollItemMetadata& metadata) {
  scoped_refptr<message_center::NotificationDelegate> delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([](std::optional<int> button_index) {
            // When button is clicked, close notification and open Network
            // Settings page if we can open WebUI settings.
            if (button_index.has_value()) {
              message_center::MessageCenter::Get()->RemoveNotification(
                  kPhoneHubCameraRollNotificationId, /*by_user=*/true);
              if (TrayPopupUtils::CanOpenWebUISettings()) {
                Shell::Get()->system_tray_model()->client()->ShowSettings(
                    display::kInvalidDisplayId);
              } else {
                PA_LOG(WARNING)
                    << "Cannot open Settings since it's not possible to open "
                       "WebUI settings";
              }
            }
          }));
  message_center::NotifierId notifier_id(
      message_center::NotifierType::PHONE_HUB,
      kPhoneHubCameraRollNotificationId);
  message_center::RichNotificationData optional_fields;
  message_center::ButtonInfo button;
  button.title = l10n_util::GetStringUTF16(
      IDS_ASH_PHONE_HUB_CAMERA_ROLL_ERROR_NETWORK_ACTION);
  optional_fields.buttons.push_back(button);
  return CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kPhoneHubCameraRollNotificationId,
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_CAMERA_ROLL_ERROR_TITLE),
      l10n_util::GetStringFUTF16(
          IDS_ASH_PHONE_HUB_CAMERA_ROLL_ERROR_NETWORK_BODY,
          base::UTF8ToUTF16(metadata.file_name())),
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TRAY_ACCESSIBLE_NAME),
      /*origin_url=*/GURL(), notifier_id, optional_fields, std::move(delegate),
      kPhoneHubCameraRollMenuDownloadIcon,
      message_center::SystemNotificationWarningLevel::WARNING);
}

void PhoneHubNotificationController::OpenSettings() {
  DCHECK(TrayPopupUtils::CanOpenWebUISettings());
  Shell::Get()->system_tray_model()->client()->ShowConnectedDevicesSettings();
}

void PhoneHubNotificationController::DismissNotification(
    int64_t notification_id) {
  CHECK(manager_);
  manager_->DismissNotification(notification_id);
  phone_hub_metrics::LogNotificationInteraction(
      NotificationInteraction::kDismiss);
}

void PhoneHubNotificationController::HandleNotificationBodyClick(
    int64_t notification_id,
    const phonehub::Notification::AppMetadata& app_metadata) {
  CHECK(manager_);
  if (!notification_interaction_handler_)
    return;
  const phonehub::Notification* notification =
      manager_->GetNotification(notification_id);
  if (!notification)
    return;
  if (notification->interaction_behavior() ==
      phonehub::Notification::InteractionBehavior::kOpenable) {
    notification_interaction_handler_->HandleNotificationClicked(
        notification_id, app_metadata);
  }
}

void PhoneHubNotificationController::SendInlineReply(
    int64_t notification_id,
    const std::u16string& inline_reply_text) {
  CHECK(manager_);
  manager_->SendInlineReply(notification_id, inline_reply_text);
  phone_hub_metrics::LogNotificationInteraction(
      NotificationInteraction::kInlineReply);
}

void PhoneHubNotificationController::LogNotificationCount() {
  int count = notification_map_.size();
  phone_hub_metrics::LogNotificationCount(count);
}

void PhoneHubNotificationController::SetNotification(
    const phonehub::Notification* notification,
    bool is_update) {
  int64_t phone_hub_id = notification->id();
  std::string cros_id = base::StrCat(
      {kNotifierId, kNotifierIdSeparator, base::NumberToString(phone_hub_id)});

  bool notification_already_exists =
      base::Contains(notification_map_, phone_hub_id);
  if (!notification_already_exists) {
    notification_map_[phone_hub_id] = std::make_unique<NotificationDelegate>(
        this, phone_hub_id, cros_id, notification->category());
  }
  NotificationDelegate* delegate = notification_map_[phone_hub_id].get();

  auto cros_notification =
      CreateNotification(notification, cros_id, delegate, is_update);

  if (notification->category() ==
          phonehub::Notification::Category::kIncomingCall ||
      notification->category() ==
          phonehub::Notification::Category::kOngoingCall) {
    cros_notification->set_custom_view_type(kNotificationCustomCallViewType);
  } else {
    cros_notification->set_custom_view_type(kNotificationCustomViewType);
  }

  phone_hub_metrics::LogNotificationMessageLength(
      cros_notification->message().length());

  auto* message_center = message_center::MessageCenter::Get();
  if (notification_already_exists)
    message_center->UpdateNotification(cros_id, std::move(cros_notification));
  else
    message_center->AddNotification(std::move(cros_notification));
}

std::unique_ptr<message_center::Notification>
PhoneHubNotificationController::CreateNotification(
    const phonehub::Notification* notification,
    const std::string& cros_id,
    NotificationDelegate* delegate,
    bool is_update) {
  message_center::NotifierId notifier_id(
      message_center::NotifierType::PHONE_HUB, kNotifierId);

  auto notification_type = message_center::NOTIFICATION_TYPE_CUSTOM;

  std::u16string title = notification->title().value_or(std::u16string());
  std::u16string message =
      notification->text_content().value_or(std::u16string());

  auto app_metadata = notification->app_metadata();
  std::u16string display_source = app_metadata.visible_app_name;

  message_center::RichNotificationData optional_fields;
  optional_fields.small_image = app_metadata.monochrome_icon_mask.has_value()
                                    ? app_metadata.monochrome_icon_mask.value()
                                    : app_metadata.color_icon;
  optional_fields.timestamp = notification->timestamp();
  optional_fields.accessible_name = l10n_util::GetStringFUTF16(
      IDS_ASH_PHONE_HUB_NOTIFICATION_ACCESSIBLE_NAME, display_source, title,
      message, PhoneHubNotificationController::GetPhoneName());
  if (app_metadata.icon_is_monochrome) {
    optional_fields.accent_color = app_metadata.icon_color;
    optional_fields.ignore_accent_color_for_small_image = true;
    optional_fields.ignore_accent_color_for_text = false;
    optional_fields.small_image_needs_additional_masking = true;
  } else {
    optional_fields.ignore_accent_color_for_small_image = true;
    optional_fields.ignore_accent_color_for_text = false;
    optional_fields.small_image_needs_additional_masking = false;
  }

  auto shared_image = notification->shared_image();
  if (shared_image.has_value())
    optional_fields.image = shared_image.value();

  const gfx::Image& icon = notification->contact_image().value_or(gfx::Image());

  optional_fields.priority =
      GetSystemPriorityForNotification(notification, is_update);

  // If the notification was updated, set renotify to true so that the
  // notification pops up again and is visible to the user. See
  // https://crbug.com/1159063.
  if (is_update)
    optional_fields.renotify = true;

  switch (notification->category()) {
    case phonehub::Notification::Category::kIncomingCall: {
      message_center::ButtonInfo decline_button;
      decline_button.title = l10n_util::GetStringUTF16(
          IDS_ASH_PHONE_HUB_NOTIFICATION_CALL_DECLINE_BUTTON);
      optional_fields.buttons.push_back(decline_button);

      message_center::ButtonInfo answer_button;
      answer_button.title = l10n_util::GetStringUTF16(
          IDS_ASH_PHONE_HUB_NOTIFICATION_CALL_ANSWER_BUTTON);
      optional_fields.buttons.push_back(answer_button);
      break;
    }
    case phonehub::Notification::Category::kOngoingCall: {
      message_center::ButtonInfo hangup_button;
      hangup_button.title = l10n_util::GetStringUTF16(
          IDS_ASH_PHONE_HUB_NOTIFICATION_CALL_HANGUP_BUTTON);
      optional_fields.buttons.push_back(hangup_button);
      break;
    }
    default: {
      message_center::ButtonInfo reply_button;
      reply_button.title = l10n_util::GetStringUTF16(
          IDS_ASH_PHONE_HUB_NOTIFICATION_INLINE_REPLY_BUTTON);
      // Setting a placeholder is needed to show the input field
      reply_button.placeholder = std::u16string();
      optional_fields.buttons.push_back(reply_button);
      break;
    }
  }

  if (TrayPopupUtils::CanOpenWebUISettings()) {
    optional_fields.settings_button_handler =
        message_center::SettingsButtonHandler::DELEGATE;
  }

  return std::make_unique<message_center::Notification>(
      notification_type, cros_id, title, message,
      ui::ImageModel::FromImage(icon), display_source,
      /*origin_url=*/GURL(), notifier_id, optional_fields,
      delegate->AsScopedRefPtr());
}

int PhoneHubNotificationController::GetSystemPriorityForNotification(
    const phonehub::Notification* notification,
    bool is_update) {
  bool is_recent = (notification->timestamp() + kMaxRecentNotificationAge) >
                   base::Time::Now();

  // Use MAX_PRIORITY, which causes the notification to be shown in a popup
  // so that users can see new messages come in as they are chatting.
  if (is_recent || is_update)
    return message_center::MAX_PRIORITY;

  // Silently add older notifications that are likely to be stale.
  return message_center::LOW_PRIORITY;
}

std::u16string GetPhoneName(base::WeakPtr<ash::PhoneHubNotificationController>
                                notification_controller) {
  return (notification_controller) ? notification_controller->GetPhoneName()
                                   : std::u16string();
}

// static
std::unique_ptr<message_center::MessageView>
PhoneHubNotificationController::CreateCustomNotificationView(
    base::WeakPtr<PhoneHubNotificationController> notification_controller,
    const message_center::Notification& notification,
    bool shown_in_popup) {
  DCHECK(notification.custom_view_type() == kNotificationCustomViewType);

  return std::make_unique<PhoneHubAshNotificationView>(
      notification, shown_in_popup, ash::GetPhoneName(notification_controller));
}

// static
std::unique_ptr<message_center::MessageView>
PhoneHubNotificationController::CreateCustomActionNotificationView(
    base::WeakPtr<PhoneHubNotificationController> notification_controller,
    const message_center::Notification& notification,
    bool shown_in_popup) {
  DCHECK(notification.custom_view_type() == kNotificationCustomCallViewType);

  return std::make_unique<PhoneHubAshNotificationView>(
      notification, shown_in_popup, ash::GetPhoneName(notification_controller));
}

}  // namespace ash
