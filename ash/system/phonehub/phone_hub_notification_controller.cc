// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_notification_controller.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/message_view_factory.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/timer/timer.h"
#include "chromeos/components/phonehub/notification.h"
#include "chromeos/components/phonehub/notification_interaction_handler.h"
#include "chromeos/components/phonehub/phone_hub_manager.h"
#include "chromeos/components/phonehub/phone_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view.h"

namespace ash {

using phone_hub_metrics::NotificationInteraction;

namespace {
const char kNotifierId[] = "chrome://phonehub";
const char kNotifierIdSeparator[] = "-";
const char kPhoneHubInstantTetherNotificationId[] =
    "chrome://phonehub-instant-tether";
const char kNotificationCustomViewType[] = "phonehub";
const int kReplyButtonIndex = 0;
const int kNotificationHeaderTextWidth = 180;
const int kNotificationAppNameMaxWidth = 140;

// The amount of time the reply button is disabled after sending an inline
// reply. This is used to make sure that all the replies are received by the
// phone in a correct order (a reply sent right after another could cause it to
// be received before the former one).
constexpr base::TimeDelta kInlineReplyDisableTime = base::Seconds(1);

class PhoneHubNotificationView : public message_center::NotificationView {
 public:
  explicit PhoneHubNotificationView(
      const message_center::Notification& notification,
      const std::u16string& phone_name)
      : message_center::NotificationView(notification) {
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

    action_buttons_row_ =
        GetViewByID(message_center::NotificationView::kActionButtonsRow);
    if (!action_buttons_row_->children().empty())
      reply_button_ = static_cast<views::View*>(
          action_buttons_row_->children()[kReplyButtonIndex]);

    inline_reply_ = static_cast<message_center::NotificationInputContainer*>(
        GetViewByID(message_center::NotificationView::kInlineReply));
  }

  ~PhoneHubNotificationView() override = default;
  PhoneHubNotificationView(const PhoneHubNotificationView&) = delete;
  PhoneHubNotificationView& operator=(const PhoneHubNotificationView&) = delete;

  // message_center::NotificationView:
  void OnNotificationInputSubmit(size_t index,
                                 const std::u16string& text) override {
    message_center::NotificationView::OnNotificationInputSubmit(index, text);

    DCHECK(reply_button_);

    // After sending a reply, take the UI back to action buttons and clear out
    // text input.
    inline_reply_->SetVisible(false);
    action_buttons_row_->SetVisible(true);
    inline_reply_->textfield()->SetText(std::u16string());

    // Briefly disable reply button.
    reply_button_->SetEnabled(false);
    enable_reply_timer_ = std::make_unique<base::OneShotTimer>();
    enable_reply_timer_->Start(
        FROM_HERE, kInlineReplyDisableTime,
        base::BindOnce(&PhoneHubNotificationView::EnableReplyButton,
                       base::Unretained(this)));
  }

  void EnableReplyButton() {
    reply_button_->SetEnabled(true);
    enable_reply_timer_.reset();
  }

 private:
  // Owned by view hierarchy.
  views::View* action_buttons_row_ = nullptr;
  views::View* reply_button_ = nullptr;
  message_center::NotificationInputContainer* inline_reply_ = nullptr;

  // Timer that fires to enable reply button after a brief period of time.
  std::unique_ptr<base::OneShotTimer> enable_reply_timer_;
};

}  // namespace

// Delegate for the displayed ChromeOS notification.
class PhoneHubNotificationController::NotificationDelegate
    : public message_center::NotificationObserver {
 public:
  NotificationDelegate(PhoneHubNotificationController* controller,
                       int64_t phone_hub_id,
                       const std::string& cros_id)
      : controller_(controller),
        phone_hub_id_(phone_hub_id),
        cros_id_(cros_id) {}

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
    if (controller_ && !removed_by_phone_hub_)
      controller_->DismissNotification(phone_hub_id_);
  }

  void Click(const absl::optional<int>& button_index,
             const absl::optional<std::u16string>& reply) override {
    if (!controller_)
      return;

    if (button_index.has_value()) {
      if (button_index.value() == kReplyButtonIndex && reply.has_value())
        controller_->SendInlineReply(phone_hub_id_, reply.value());
    } else {
      controller_->HandleNotificationBodyClick(
          phone_hub_id_, controller_->manager_->GetNotification(phone_hub_id_)
                             ->app_metadata());
    }
  }

  void SettingsClick() override {
    if (controller_)
      controller_->OpenSettings();
  }

 private:
  // The parent controller, which owns this object.
  PhoneHubNotificationController* controller_ = nullptr;

  // The notification ID tracked by PhoneHub.
  const int64_t phone_hub_id_;

  // The notification ID tracked by the CrOS message center.
  const std::string cros_id_;

  // Flag set if the notification was removed by PhoneHub so we avoid a cycle.
  bool removed_by_phone_hub_ = false;

  base::WeakPtrFactory<NotificationDelegate> weak_ptr_factory_{this};
};

PhoneHubNotificationController::PhoneHubNotificationController() {
  if (MessageViewFactory::HasCustomNotificationViewFactory(
          kNotificationCustomViewType))
    return;

  MessageViewFactory::SetCustomNotificationViewFactory(
      kNotificationCustomViewType,
      base::BindRepeating(
          &PhoneHubNotificationController::CreateCustomNotificationView,
          weak_ptr_factory_.GetWeakPtr()));
}

PhoneHubNotificationController::~PhoneHubNotificationController() {
  if (manager_)
    manager_->RemoveObserver(this);
  if (feature_status_provider_)
    feature_status_provider_->RemoveObserver(this);
  if (tether_controller_)
    tether_controller_->RemoveObserver(this);
}

void PhoneHubNotificationController::SetManager(
    chromeos::phonehub::PhoneHubManager* phone_hub_manager) {
  if (manager_)
    manager_->RemoveObserver(this);
  if (phone_hub_manager) {
    manager_ = phone_hub_manager->GetNotificationManager();
    manager_->AddObserver(this);
  } else {
    manager_ = nullptr;
  }

  if (feature_status_provider_)
    feature_status_provider_->RemoveObserver(this);
  if (phone_hub_manager) {
    feature_status_provider_ = phone_hub_manager->GetFeatureStatusProvider();
    feature_status_provider_->AddObserver(this);
  } else {
    feature_status_provider_ = nullptr;
  }

  if (tether_controller_)
    tether_controller_->RemoveObserver(this);
  if (phone_hub_manager) {
    tether_controller_ = phone_hub_manager->GetTetherController();
    tether_controller_->AddObserver(this);
  } else {
    tether_controller_ = nullptr;
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

void PhoneHubNotificationController::OnFeatureStatusChanged() {
  DCHECK(feature_status_provider_);

  auto status = feature_status_provider_->GetStatus();

  // Various states in which the feature is enabled, even if it is not actually
  // in use (e.g., if Bluetooth is disabled or if the screen is locked).
  bool is_feature_enabled =
      status == chromeos::phonehub::FeatureStatus::kUnavailableBluetoothOff ||
      status == chromeos::phonehub::FeatureStatus::kLockOrSuspended ||
      status == chromeos::phonehub::FeatureStatus::kEnabledButDisconnected ||
      status == chromeos::phonehub::FeatureStatus::kEnabledAndConnecting ||
      status == chromeos::phonehub::FeatureStatus::kEnabledAndConnected;

  // Reset the set of shown notifications when Phone Hub is disabled. If it is
  // enabled, we skip this step to ensure that notifications that have already
  // been shown do not pop up again and spam the user. See
  // https://crbug.com/1157523 for details.
  if (!is_feature_enabled)
    shown_notification_ids_.clear();
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
          base::BindRepeating([](absl::optional<int> button_index) {
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
      CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kPhoneHubInstantTetherNotificationId,
          l10n_util::GetStringUTF16(
              IDS_ASH_PHONE_HUB_NOTIFICATION_HOTSPOT_FAILED_TITLE),
          l10n_util::GetStringUTF16(
              IDS_ASH_PHONE_HUB_NOTIFICATION_HOTSPOT_FAILED_MESSAGE),
          std::u16string() /*display_source */, GURL() /* origin_url */,
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kPhoneHubInstantTetherNotificationId),
          message_center::RichNotificationData(), std::move(delegate),
          kPhoneHubEnableHotspotOnIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
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
    const chromeos::phonehub::Notification::AppMetadata& app_metadata) {
  CHECK(manager_);
  if (!notification_interaction_handler_)
    return;
  const chromeos::phonehub::Notification* notification =
      manager_->GetNotification(notification_id);
  if (!notification)
    return;
  if (notification->interaction_behavior() ==
      chromeos::phonehub::Notification::InteractionBehavior::kOpenable) {
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
    const chromeos::phonehub::Notification* notification,
    bool is_update) {
  int64_t phone_hub_id = notification->id();
  std::string cros_id = base::StrCat(
      {kNotifierId, kNotifierIdSeparator, base::NumberToString(phone_hub_id)});

  bool notification_already_exists =
      base::Contains(notification_map_, phone_hub_id);
  if (!notification_already_exists) {
    notification_map_[phone_hub_id] =
        std::make_unique<NotificationDelegate>(this, phone_hub_id, cros_id);
  }
  NotificationDelegate* delegate = notification_map_[phone_hub_id].get();

  auto cros_notification =
      CreateNotification(notification, cros_id, delegate, is_update);
  cros_notification->set_custom_view_type(kNotificationCustomViewType);
  shown_notification_ids_.insert(phone_hub_id);

  auto* message_center = message_center::MessageCenter::Get();
  if (notification_already_exists)
    message_center->UpdateNotification(cros_id, std::move(cros_notification));
  else
    message_center->AddNotification(std::move(cros_notification));
}

std::unique_ptr<message_center::Notification>
PhoneHubNotificationController::CreateNotification(
    const chromeos::phonehub::Notification* notification,
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
  optional_fields.small_image = app_metadata.icon;
  optional_fields.ignore_accent_color_for_small_image = true;
  optional_fields.timestamp = notification->timestamp();

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

  message_center::ButtonInfo reply_button;
  reply_button.title = l10n_util::GetStringUTF16(
      IDS_ASH_PHONE_HUB_NOTIFICATION_INLINE_REPLY_BUTTON);
  reply_button.placeholder = std::u16string();
  optional_fields.buttons.push_back(reply_button);

  if (TrayPopupUtils::CanOpenWebUISettings()) {
    optional_fields.settings_button_handler =
        message_center::SettingsButtonHandler::DELEGATE;
  }

  return std::make_unique<message_center::Notification>(
      notification_type, cros_id, title, message, icon, display_source,
      /*origin_url=*/GURL(), notifier_id, optional_fields,
      delegate->AsScopedRefPtr());
}

int PhoneHubNotificationController::GetSystemPriorityForNotification(
    const chromeos::phonehub::Notification* notification,
    bool is_update) {
  bool has_notification_been_shown =
      base::Contains(shown_notification_ids_, notification->id());

  // If the same notification was already shown and has not been updated,
  // use LOW_PRIORITY so that the notification is silently added to the
  // notification shade. This ensures that we don't spam users with the same
  // information multiple times.
  if (has_notification_been_shown && !is_update)
    return message_center::LOW_PRIORITY;

  // Use MAX_PRIORITY, which causes the notification to be shown in a popup
  // so that users can see new messages come in as they are chatting. See
  // https://crbug.com/1159063.
  return message_center::MAX_PRIORITY;
}

// static
std::unique_ptr<message_center::MessageView>
PhoneHubNotificationController::CreateCustomNotificationView(
    base::WeakPtr<PhoneHubNotificationController> notification_controller,
    const message_center::Notification& notification,
    bool shown_in_popup) {
  DCHECK_EQ(kNotificationCustomViewType, notification.custom_view_type());

  std::u16string phone_name = std::u16string();
  if (notification_controller)
    phone_name = notification_controller->GetPhoneName();

  return std::make_unique<PhoneHubNotificationView>(notification, phone_name);
}

}  // namespace ash
