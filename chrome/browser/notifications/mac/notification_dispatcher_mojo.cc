// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/mac/notification_dispatcher_mojo.h"

#include <set>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/mac/mac_notification_provider_factory.h"
#include "chrome/browser/notifications/mac/notification_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

// The initial delay for restarting the notification service. An exponential
// backoff will double this value whenever the OneShotTimer reschedules.
constexpr base::TimeDelta kInitialServiceRestartTimerDelay =
    base::Milliseconds(500);
// Maximum delay between restart attempts. We don't want this to be too low to
// avoid heavy resource usage but also not too high keep notifications working.
constexpr base::TimeDelta kMaximumServiceRestartTimerDelay = base::Seconds(256);
// If the service ran for more than this time we will reset the restart delay to
// |kInitialServiceRestartTimerDelay|.
constexpr base::TimeDelta kServiceRestartTimerResetDelay = base::Seconds(10);

}  // namespace

NotificationDispatcherMojo::NotificationDispatcherMojo(
    std::unique_ptr<MacNotificationProviderFactory> provider_factory)
    : provider_factory_(std::move(provider_factory)),
      next_service_restart_timer_delay_(kInitialServiceRestartTimerDelay) {
  // Force start the notification service once so we show the permission request
  // to users on the first start of Chrome.
  // TODO(crbug.com/40149365): Find a better time to ask for permissions.
  CheckIfServiceCanBeTerminated();
}

NotificationDispatcherMojo::~NotificationDispatcherMojo() = default;

void NotificationDispatcherMojo::DisplayNotification(
    NotificationHandler::Type notification_type,
    Profile* profile,
    const message_center::Notification& notification) {
  no_notifications_checker_.Cancel();
  GetOrCreateService()->DisplayNotification(
      CreateMacNotification(notification_type, profile, notification));
}

void NotificationDispatcherMojo::CloseNotificationWithId(
    const MacNotificationIdentifier& identifier) {
  if (HasNoDisplayedNotifications())
    return;

  GetOrCreateService()->CloseNotification(
      mac_notifications::mojom::NotificationIdentifier::New(
          identifier.notification_id,
          mac_notifications::mojom::ProfileIdentifier::New(
              identifier.profile_id, identifier.incognito)));
  CheckIfServiceCanBeTerminated();
}

void NotificationDispatcherMojo::CloseNotificationsWithProfileId(
    const std::string& profile_id,
    bool incognito) {
  if (HasNoDisplayedNotifications())
    return;

  GetOrCreateService()->CloseNotificationsForProfile(
      mac_notifications::mojom::ProfileIdentifier::New(profile_id, incognito));
  CheckIfServiceCanBeTerminated();
}

void NotificationDispatcherMojo::CloseAllNotifications() {
  if (HasNoDisplayedNotifications())
    return;

  if (service_)
    service_->CloseAllNotifications();
  OnServiceDisconnectedGracefully(ShutdownType::kChromeInitiated);
}

void NotificationDispatcherMojo::GetDisplayedNotificationsForProfileId(
    const std::string& profile_id,
    bool incognito,
    GetDisplayedNotificationsCallback callback) {
  if (HasNoDisplayedNotifications()) {
    std::move(callback).Run(/*notification_ids=*/{},
                            /*supports_synchronization=*/true);
    return;
  }

  no_notifications_checker_.Cancel();
  GetOrCreateService()->GetDisplayedNotifications(
      mac_notifications::mojom::ProfileIdentifier::New(profile_id, incognito),
      /*origin=*/std::nullopt,
      base::BindOnce(&NotificationDispatcherMojo::DispatchGetNotificationsReply,
                     base::Unretained(this), std::move(callback)));
}

void NotificationDispatcherMojo::GetDisplayedNotificationsForProfileIdAndOrigin(
    const std::string& profile_id,
    bool incognito,
    const GURL& origin,
    GetDisplayedNotificationsCallback callback) {
  if (HasNoDisplayedNotifications()) {
    std::move(callback).Run(/*notification_ids=*/{},
                            /*supports_synchronization=*/true);
    return;
  }

  no_notifications_checker_.Cancel();
  GetOrCreateService()->GetDisplayedNotifications(
      mac_notifications::mojom::ProfileIdentifier::New(profile_id, incognito),
      origin,
      base::BindOnce(&NotificationDispatcherMojo::DispatchGetNotificationsReply,
                     base::Unretained(this), std::move(callback)));
}

void NotificationDispatcherMojo::GetAllDisplayedNotifications(
    GetAllDisplayedNotificationsCallback callback) {
  if (HasNoDisplayedNotifications()) {
    std::move(callback).Run(/*notification_ids=*/{});
    return;
  }

  no_notifications_checker_.Cancel();
  GetOrCreateService()->GetDisplayedNotifications(
      /*profile=*/nullptr, /*origin=*/std::nullopt,
      base::BindOnce(
          &NotificationDispatcherMojo::DispatchGetAllNotificationsReply,
          base::Unretained(this), std::move(callback)));
}

void NotificationDispatcherMojo::OnNotificationAction(
    mac_notifications::mojom::NotificationActionInfoPtr info) {
  ProcessMacNotificationResponse(provider_factory_->notification_style(),
                                 std::move(info));
  CheckIfServiceCanBeTerminated();
}

void NotificationDispatcherMojo::UserInitiatedShutdown() {
  OnServiceDisconnectedGracefully(ShutdownType::kUserInitiated);
}

void NotificationDispatcherMojo::CheckIfServiceCanBeTerminated() {
  no_notifications_checker_.Reset(base::BindOnce(
      &NotificationDispatcherMojo::OnServiceDisconnectedGracefully,
      base::Unretained(this), ShutdownType::kChromeInitiated));

  // The service will indicate it is okay to be terminated if there are no
  // displayed notifications left, and (in the case of the UNNotification API)
  // there are no pending permission requests either.
  // If this happens, we close the mojo connection (only if the callback has not
  // been canceled yet).
  GetOrCreateService()->OkayToTerminateService(base::BindOnce(
      [](base::OnceClosure disconnect_closure, bool can_terminate) {
        if (can_terminate) {
          std::move(disconnect_closure).Run();
        }
      },
      no_notifications_checker_.callback()));
}

void NotificationDispatcherMojo::OnServiceDisconnectedGracefully(
    ShutdownType shutdown_type) {
  base::TimeDelta elapsed = base::TimeTicks::Now() - service_start_time_;

  // Log utility process runtime metrics to UMA.
  if (service_) {
    auto emit_time_histogram = [elapsed](const char* name) {
      base::UmaHistogramCustomTimes(name, elapsed, base::Milliseconds(100),
                                    base::Hours(8),
                                    /*buckets=*/50);
    };
    switch (provider_factory_->notification_style()) {
      case mac_notifications::NotificationStyle::kBanner:
        // No need to collect metrics for in-process notifications.
        break;
      case mac_notifications::NotificationStyle::kAlert:
        emit_time_histogram("Notifications.macOS.ServiceProcessRuntime");
        if (shutdown_type == ShutdownType::kUnexpected) {
          emit_time_histogram("Notifications.macOS.ServiceProcessKilled");
        }
        break;
      case mac_notifications::NotificationStyle::kAppShim:
        emit_time_histogram(
            "Notifications.macOS.AppShimProcess.UptimeOnDisconnect");
        if (shutdown_type == ShutdownType::kUnexpected) {
          emit_time_histogram(
              "Notifications.macOS.AppShimProcess."
              "UptimeOnUnexpectedDisconnect");
        }
        break;
    }
  }

  // If the service ran for more than 10 seconds or completed successfully we
  // restore the next delay to the initial value. If it failed sooner than that
  // then double the next time until we hit a maximum value so we don't end up
  // restarting it a lot.
  if (elapsed > kServiceRestartTimerResetDelay ||
      shutdown_type == ShutdownType::kChromeInitiated) {
    next_service_restart_timer_delay_ = kInitialServiceRestartTimerDelay;
  } else if (next_service_restart_timer_delay_ <
             kMaximumServiceRestartTimerDelay) {
    next_service_restart_timer_delay_ = next_service_restart_timer_delay_ * 2;
  }

  if (shutdown_type == ShutdownType::kUnexpected) {
    // Calling CheckIfServiceCanBeTerminated() will force a new connection
    // attempt. base::Unretained(this) is safe here because |this| owns
    // |service_restart_timer_|.
    service_restart_timer_.Start(
        FROM_HERE, next_service_restart_timer_delay_,
        base::BindOnce(
            &NotificationDispatcherMojo::CheckIfServiceCanBeTerminated,
            base::Unretained(this)));
  } else {
    service_restart_timer_.AbandonAndStop();
  }

  no_notifications_checker_.Cancel();
  provider_.reset();
  service_.reset();
  handler_.reset();
}

bool NotificationDispatcherMojo::HasNoDisplayedNotifications() const {
  return !service_ && !service_restart_timer_.IsRunning();
}

mac_notifications::mojom::MacNotificationService*
NotificationDispatcherMojo::GetOrCreateService() {
  if (!service_) {
    service_restart_timer_.AbandonAndStop();
    service_start_time_ = base::TimeTicks::Now();
    provider_ = provider_factory_->LaunchProvider();
    provider_.set_disconnect_handler(base::BindOnce(
        &NotificationDispatcherMojo::OnServiceDisconnectedGracefully,
        base::Unretained(this), ShutdownType::kUnexpected));
    provider_->BindNotificationService(service_.BindNewPipeAndPassReceiver(),
                                       handler_.BindNewPipeAndPassRemote());
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
    CheckIfServiceCanBeTerminated();

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
    CheckIfServiceCanBeTerminated();

  // Initialize the base::flat_set via a std::vector to avoid N^2 runtime.
  base::flat_set<MacNotificationIdentifier> identifiers(
      std::move(notification_ids));
  std::move(callback).Run(std::move(identifiers));
}
