// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/notifications/alert_dispatcher_mojo.h"

#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/mac_notification_provider_factory.h"
#include "chrome/browser/notifications/notification_platform_bridge_mac_utils.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace {

// Handles notification actions from the macOS notification service and calls
// the given |on_action| closure after each action.
class MacNotificationActionHandlerImpl
    : public mac_notifications::mojom::MacNotificationActionHandler {
 public:
  explicit MacNotificationActionHandlerImpl(base::RepeatingClosure on_action)
      : on_action_(std::move(on_action)) {}
  MacNotificationActionHandlerImpl(const MacNotificationActionHandlerImpl&) =
      delete;
  MacNotificationActionHandlerImpl& operator=(
      const MacNotificationActionHandlerImpl&) = delete;
  ~MacNotificationActionHandlerImpl() override = default;

  // mac_notifications::mojom::MacNotificationActionHandler:
  void OnNotificationAction(
      mac_notifications::mojom::NotificationActionInfoPtr info) override {
    ProcessMacNotificationResponse(std::move(info));
    on_action_.Run();
  }

 private:
  base::RepeatingClosure on_action_;
};

}  // namespace

NotificationDispatcherMojo::NotificationDispatcherMojo(
    std::unique_ptr<MacNotificationProviderFactory> provider_factory)
    : provider_factory_(std::move(provider_factory)) {}

NotificationDispatcherMojo::~NotificationDispatcherMojo() = default;

void NotificationDispatcherMojo::DisplayNotification(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification) {
  GetOrCreateService()->DisplayNotification(
      CreateMacNotification(notification_type, profile, notification));
  no_notifications_checker_.Cancel();
}

void NotificationDispatcherMojo::CloseNotificationWithId(
    const MacNotificationIdentifier& identifier) {
  GetOrCreateService()->CloseNotification(
      mac_notifications::mojom::NotificationIdentifier::New(
          identifier.notification_id,
          mac_notifications::mojom::ProfileIdentifier::New(
              identifier.profile_id, identifier.incognito)));
  CheckIfNotificationsRemaining();
}

void NotificationDispatcherMojo::CloseNotificationsWithProfileId(
    const std::string& profile_id,
    bool incognito) {
  GetOrCreateService()->CloseNotificationsForProfile(
      mac_notifications::mojom::ProfileIdentifier::New(profile_id, incognito));
  CheckIfNotificationsRemaining();
}

void NotificationDispatcherMojo::CloseAllNotifications() {
  if (service_)
    service_->CloseAllNotifications();
  OnServiceDisconnectedGracefully(/*gracefully=*/true);
}

void NotificationDispatcherMojo::GetDisplayedNotificationsForProfileId(
    const std::string& profile_id,
    bool incognito,
    GetDisplayedNotificationsCallback callback) {
  GetOrCreateService()->GetDisplayedNotifications(
      mac_notifications::mojom::ProfileIdentifier::New(profile_id, incognito),
      base::BindOnce(&NotificationDispatcherMojo::DispatchGetNotificationsReply,
                     base::Unretained(this), std::move(callback)));
}

void NotificationDispatcherMojo::GetAllDisplayedNotifications(
    GetAllDisplayedNotificationsCallback callback) {
  GetOrCreateService()->GetDisplayedNotifications(
      /*profile=*/nullptr,
      base::BindOnce(
          &NotificationDispatcherMojo::DispatchGetAllNotificationsReply,
          base::Unretained(this), std::move(callback)));
}

void NotificationDispatcherMojo::CheckIfNotificationsRemaining() {
  no_notifications_checker_.Reset(base::BindOnce(
      &NotificationDispatcherMojo::OnServiceDisconnectedGracefully,
      base::Unretained(this), /*gracefully=*/true));

  // This block will be called with all displayed notifications. If there are
  // none left we close the mojo connection (only if the callback has not been
  // canceled yet).
  // TODO(crbug.com/1127306): Revisit this for the UNNotification API as we need
  // to keep the process running during the initial permission request.
  GetOrCreateService()->GetDisplayedNotifications(
      /*profile=*/nullptr,
      base::BindOnce(
          [](base::OnceClosure disconnect_closure,
             std::vector<mac_notifications::mojom::NotificationIdentifierPtr>
                 notifications) {
            if (notifications.empty())
              std::move(disconnect_closure).Run();
          },
          no_notifications_checker_.callback()));
}

void NotificationDispatcherMojo::OnServiceDisconnectedGracefully(
    bool gracefully) {
  if (service_) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - service_start_time_;
    base::UmaHistogramCustomTimes(
        "Notifications.macOS.ServiceProcessRuntime", elapsed,
        base::TimeDelta::FromMilliseconds(100), base::TimeDelta::FromHours(8),
        /*buckets=*/50);
    if (!gracefully) {
      base::UmaHistogramCustomTimes(
          "Notifications.macOS.ServiceProcessKilled", elapsed,
          base::TimeDelta::FromMilliseconds(100), base::TimeDelta::FromHours(8),
          /*buckets=*/50);
    }
  }
  no_notifications_checker_.Cancel();
  provider_.reset();
  service_.reset();
}

mac_notifications::mojom::MacNotificationService*
NotificationDispatcherMojo::GetOrCreateService() {
  if (!service_) {
    service_start_time_ = base::TimeTicks::Now();
    mojo::PendingRemote<mac_notifications::mojom::MacNotificationActionHandler>
        handler_remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<MacNotificationActionHandlerImpl>(base::BindRepeating(
            &NotificationDispatcherMojo::CheckIfNotificationsRemaining,
            base::Unretained(this))),
        handler_remote.InitWithNewPipeAndPassReceiver());

    provider_ = provider_factory_->LaunchProvider(/*in_process=*/false);
    provider_.set_disconnect_handler(base::BindOnce(
        &NotificationDispatcherMojo::OnServiceDisconnectedGracefully,
        base::Unretained(this), /*gracefully=*/false));
    provider_->BindNotificationService(service_.BindNewPipeAndPassReceiver(),
                                       std::move(handler_remote));
  }
  return service_.get();
}

void NotificationDispatcherMojo::DispatchGetNotificationsReply(
    GetDisplayedNotificationsCallback callback,
    std::vector<mac_notifications::mojom::NotificationIdentifierPtr>
        notifications) {
  std::set<std::string> notification_ids;

  for (const auto& notification : notifications)
    notification_ids.insert(notification->id);

  // Check if there are any notifications left after this.
  if (notification_ids.empty())
    CheckIfNotificationsRemaining();

  std::move(callback).Run(std::move(notification_ids),
                          /*supports_synchronization=*/true);
}

void NotificationDispatcherMojo::DispatchGetAllNotificationsReply(
    GetAllDisplayedNotificationsCallback callback,
    std::vector<mac_notifications::mojom::NotificationIdentifierPtr>
        notifications) {
  std::vector<MacNotificationIdentifier> notification_ids;

  for (const auto& notification : notifications) {
    notification_ids.push_back({notification->id, notification->profile->id,
                                notification->profile->incognito});
  }

  // Check if there are any notifications left after this.
  if (notification_ids.empty())
    CheckIfNotificationsRemaining();

  // Initialize the base::flat_set via a std::vector to avoid N^2 runtime.
  base::flat_set<MacNotificationIdentifier> identifiers(
      std::move(notification_ids));
  std::move(callback).Run(std::move(identifiers));
}
