// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_NOTIFICATION_CONTROLLER_IMPL_H_
#define ASH_ASSISTANT_ASSISTANT_NOTIFICATION_CONTROLLER_IMPL_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/assistant/assistant_notification_expiry_monitor.h"
#include "ash/assistant/model/assistant_notification_model.h"
#include "ash/assistant/model/assistant_notification_model_observer.h"
#include "ash/public/cpp/assistant/controller/assistant_notification_controller.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_notification.h"
#include "chromeos/ash/services/libassistant/public/mojom/notification_delegate.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

// The class to manage Assistant notifications.
class ASH_EXPORT AssistantNotificationControllerImpl
    : public AssistantNotificationController,
      public AssistantNotificationModelObserver,
      public message_center::MessageCenterObserver,
      public libassistant::mojom::NotificationDelegate {
 public:
  using AssistantNotification = assistant::AssistantNotification;

  AssistantNotificationControllerImpl();

  AssistantNotificationControllerImpl(
      const AssistantNotificationControllerImpl&) = delete;
  AssistantNotificationControllerImpl& operator=(
      const AssistantNotificationControllerImpl&) = delete;

  ~AssistantNotificationControllerImpl() override;

  // Returns the underlying model.
  const AssistantNotificationModel* model() const { return &model_; }

  // Provides a pointer to the |assistant| owned by AssistantController.
  void SetAssistant(assistant::Assistant* assistant);

  // AssistantNotificationController:
  void RemoveNotificationById(const std::string& id, bool from_server) override;
  void SetQuietMode(bool enabled) override;

  // libassistant::mojom::NotificationDelegate:
  void AddOrUpdateNotification(AssistantNotification notification) override;
  void RemoveNotificationByGroupingKey(const std::string& grouping_id,
                                       bool from_server) override;
  void RemoveAllNotifications(bool from_server) override;

  // AssistantNotificationModelObserver:
  void OnNotificationAdded(const AssistantNotification& notification) override;
  void OnNotificationUpdated(
      const AssistantNotification& notification) override;
  void OnNotificationRemoved(const AssistantNotification& notification,
                             bool from_server) override;
  void OnAllNotificationsRemoved(bool from_server) override;

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& id) override {}
  void OnNotificationClicked(
      const std::string& id,
      const std::optional<int>& button_index,
      const std::optional<std::u16string>& reply) override;
  void OnNotificationUpdated(const std::string& notification) override {}
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;

 private:
  AssistantNotificationModel model_;
  AssistantNotificationExpiryMonitor expiry_monitor_;

  // Owned by AssistantService
  raw_ptr<assistant::Assistant> assistant_ = nullptr;

  const message_center::NotifierId notifier_id_;

  mojo::Receiver<libassistant::mojom::NotificationDelegate> receiver_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_NOTIFICATION_CONTROLLER_IMPL_H_
