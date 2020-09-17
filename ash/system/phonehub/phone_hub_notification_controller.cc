// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_notification_controller.h"

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/components/phonehub/notification.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {

namespace {
const char kNotifierId[] = "chrome://phonehub";
const char kNotifierIdSeparator[] = "-";
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

  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override {
    if (controller_ && reply.has_value())
      controller_->SendInlineReply(phone_hub_id_, reply.value());
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

PhoneHubNotificationController::PhoneHubNotificationController() = default;

PhoneHubNotificationController::~PhoneHubNotificationController() {
  if (manager_)
    manager_->RemoveObserver(this);
}

void PhoneHubNotificationController::SetManager(
    chromeos::phonehub::NotificationManager* manager) {
  if (manager_ == manager)
    return;

  if (manager_)
    manager_->RemoveObserver(this);

  manager_ = manager;
  manager_->AddObserver(this);
}

void PhoneHubNotificationController::OnNotificationsAdded(
    const base::flat_set<int64_t>& notification_ids) {
  for (int64_t id : notification_ids) {
    CreateOrUpdateNotification(manager_->GetNotification(id));
  }
}

void PhoneHubNotificationController::OnNotificationsUpdated(
    const base::flat_set<int64_t>& notification_ids) {
  for (int64_t id : notification_ids) {
    CreateOrUpdateNotification(manager_->GetNotification(id));
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
}

void PhoneHubNotificationController::OpenSettings() {
  // TODO(tengs): Open the PhoneHub settings page.
}

void PhoneHubNotificationController::DismissNotification(
    int64_t notification_id) {
  CHECK(manager_);
  manager_->DismissNotification(notification_id);
}

void PhoneHubNotificationController::SendInlineReply(
    int64_t notification_id,
    const base::string16& inline_reply_text) {
  CHECK(manager_);
  manager_->SendInlineReply(notification_id, inline_reply_text);
}

void PhoneHubNotificationController::CreateOrUpdateNotification(
    const chromeos::phonehub::Notification* notification) {
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

  auto cros_notification = CreateNotification(notification, cros_id, delegate);

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
    NotificationDelegate* delegate) {
  // TODO(tengs): Fill in the notification fields based on the PhoneHub
  // notification data.
  auto notification_type = message_center::NOTIFICATION_TYPE_SIMPLE;
  base::string16 title = notification->title().value_or(base::string16());
  base::string16 message =
      notification->text_content().value_or(base::string16());
  gfx::Image icon;
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId);
  message_center::RichNotificationData optional_fields;

  return std::make_unique<message_center::Notification>(
      notification_type, cros_id, title, message, icon,
      /*display_source=*/base::string16(),
      /*origin_url=*/GURL(), notifier_id, optional_fields,
      delegate->AsScopedRefPtr());
}

}  // namespace ash
