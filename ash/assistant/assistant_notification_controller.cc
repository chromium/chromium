// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_notification_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/new_window_controller.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kNotificationId[] = "assistant";
constexpr char kNotifierAssistant[] = "assistant";

// Delegate for an assistant notification.
class AssistantNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  AssistantNotificationDelegate(
      base::WeakPtr<AssistantNotificationController> notification_controller,
      base::WeakPtr<AssistantController> assistant_controller,
      chromeos::assistant::mojom::AssistantNotificationPtr notification)
      : notification_controller_(std::move(notification_controller)),
        assistant_controller_(std::move(assistant_controller)),
        notification_(std::move(notification)) {
    DCHECK(notification_);
  }

  // message_center::NotificationDelegate:
  void Close(bool by_user) override {
    // If |by_user| is true, means that this close action is initiated by user,
    // need to dismiss this notification at server to notify other devices.
    // If |by_user| is false, means that this close action is initiated from the
    // server, so do not need to dismiss this notification again.
    if (by_user && notification_controller_)
      notification_controller_->DismissNotification(notification_.Clone());
  }

  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override {
    const auto& action_url =
        button_index.has_value()
            ? notification_->buttons[button_index.value()]->action_url
            : notification_->action_url;
    // Open the action url if it is valid.
    if (action_url.is_valid() &&
        (action_url.SchemeIsHTTPOrHTTPS() ||
         assistant::util::IsDeepLinkUrl(action_url)) &&
        assistant_controller_) {
      assistant_controller_->OpenUrl(action_url);
      Close(/*by_user=*/true);
      return;
    }

    if (notification_controller_) {
      // Action index 0 is the top level action and the first button's action
      // index is 1.
      const int action_index =
          button_index.has_value() ? button_index.value() + 1 : 0;
      notification_controller_->RetrieveNotification(notification_.Clone(),
                                                     action_index);
    }
  }

 private:
  // Refcounted.
  ~AssistantNotificationDelegate() override = default;

  base::WeakPtr<AssistantNotificationController> notification_controller_;

  base::WeakPtr<AssistantController> assistant_controller_;

  chromeos::assistant::mojom::AssistantNotificationPtr notification_;

  DISALLOW_COPY_AND_ASSIGN(AssistantNotificationDelegate);
};

std::string GetNotificationId(const std::string& grouping_key) {
  return kNotificationId + grouping_key;
}

message_center::NotifierId GetNotifierId() {
  return message_center::NotifierId(
      message_center::NotifierId::SYSTEM_COMPONENT, kNotifierAssistant);
}

}  // namespace

AssistantNotificationController::AssistantNotificationController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      assistant_notification_subscriber_binding_(this),
      notifier_id_(GetNotifierId()),
      weak_factory_(this) {}

AssistantNotificationController::~AssistantNotificationController() = default;

void AssistantNotificationController::SetAssistant(
    chromeos::assistant::mojom::Assistant* assistant) {
  assistant_ = assistant;

  // Subscribe to Assistant notification events.
  chromeos::assistant::mojom::AssistantNotificationSubscriberPtr ptr;
  assistant_notification_subscriber_binding_.Bind(mojo::MakeRequest(&ptr));
  assistant_->AddAssistantNotificationSubscriber(std::move(ptr));
}

void AssistantNotificationController::RetrieveNotification(
    AssistantNotificationPtr notification,
    int action_index) {
  assistant_->RetrieveNotification(std::move(notification), action_index);
}

void AssistantNotificationController::DismissNotification(
    AssistantNotificationPtr notification) {
  assistant_->DismissNotification(std::move(notification));
}

void AssistantNotificationController::OnShowNotification(
    AssistantNotificationPtr notification) {
  DCHECK(assistant_);

  // Do not show notification if the setting is false.
  if (!Shell::Get()->voice_interaction_controller()->notification_enabled())
    return;

  // Create the specified |notification| that should be rendered in the
  // |message_center| for the interaction.
  const base::string16 title = base::UTF8ToUTF16(notification->title);
  const base::string16 message = base::UTF8ToUTF16(notification->message);
  const base::string16 display_source =
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_NOTIFICATION_DISPLAY_SOURCE);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center::RichNotificationData data;
  for (const auto& button : notification->buttons) {
    data.buttons.push_back(
        message_center::ButtonInfo(base::UTF8ToUTF16(button->label)));
  }

  std::unique_ptr<message_center::Notification> system_notification =
      message_center::Notification::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          GetNotificationId(notification->grouping_key), title, message,
          display_source, GURL(), notifier_id_, data,
          new AssistantNotificationDelegate(weak_factory_.GetWeakPtr(),
                                            assistant_controller_->GetWeakPtr(),
                                            notification.Clone()),
          kNotificationAssistantIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  system_notification->set_priority(message_center::DEFAULT_PRIORITY);
  message_center->AddNotification(std::move(system_notification));
}

void AssistantNotificationController::OnRemoveNotification(
    const std::string& grouping_key) {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  if (grouping_key.empty()) {
    // Remove all assistant notifications by NotifierId.
    message_center->RemoveNotificationsForNotifierId(notifier_id_);
  } else {
    // Remove the notification with |grouping_key|. It is no-op if no
    // corresponding notification is found in |message_center|.
    message_center->RemoveNotification(GetNotificationId(grouping_key),
                                       /*by_user=*/false);
  }
}

}  // namespace ash
