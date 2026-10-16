// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/mac/notification_dispatcher_mojo.h"

#include <algorithm>
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
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/mac/mac_notification_provider_factory.h"
#include "chrome/browser/notifications/mac/notification_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

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

// Wraps `callback` so that if it is destroyed without being run (e.g. when the
// `MacNotificationService` Mojo pipe disconnects or is reset), `callback` is
// posted asynchronously to the current task runner with `default_args` rather
// than invoked synchronously during Mojo teardown.
template <typename Signature, typename... Args>
base::OnceCallback<Signature> WrapCallbackWithAsyncDefaultInvokeIfNotRun(
    base::OnceCallback<Signature> callback,
    Args... default_args) {
  return mojo::WrapCallbackWithDefaultInvokeCallbackIfNotRun(
      std::move(callback),
      base::BindOnce(
          [](Args... args, base::OnceCallback<Signature> cb) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(cb), std::move(args)...));
          },
          std::move(default_args)...));
}

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
                     base::Unretained(this),
                     WrapCallbackWithAsyncDefaultInvokeIfNotRun(
                         std::move(callback), std::set<std::string>(),
                         /*supports_synchronization=*/false)));
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
                     base::Unretained(this),
                     WrapCallbackWithAsyncDefaultInvokeIfNotRun(
                         std::move(callback), std::set<std::string>(),
                         /*supports_synchronization=*/false)));
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
          base::Unretained(this),
          WrapCallbackWithAsyncDefaultInvokeIfNotRun(
              std::move(callback),
              base::flat_set<MacNotificationIdentifier>())));
}

void NotificationDispatcherMojo::OnNotificationAction(
    mac_notifications::mojom::NotificationActionInfoPtr info) {
  OnServiceResponded();
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
  // base::Unretained(this) is safe here because |this| owns |service_|.
  GetOrCreateService()->OkayToTerminateService(base::BindOnce(
      &NotificationDispatcherMojo::OnServiceCanBeTerminated,
      base::Unretained(this), no_notifications_checker_.callback()));
}

void NotificationDispatcherMojo::OnServiceCanBeTerminated(
    base::OnceClosure disconnect_closure,
    bool can_terminate) {
  OnServiceResponded();
  if (can_terminate) {
    std::move(disconnect_closure).Run();
  }
}

void NotificationDispatcherMojo::OnServiceResponded() {
  if (service_first_response_time_) {
    return;
  }
  service_first_response_time_ = base::TimeTicks::Now();
  base::TimeDelta elapsed = *service_first_response_time_ - service_start_time_;
  switch (provider_factory_->notification_style()) {
    case mac_notifications::NotificationStyle::kBanner:
      // No need to collect metrics for in-process notifications.
      break;
    case mac_notifications::NotificationStyle::kAlert:
      base::UmaHistogramMediumTimes(
          "Notifications.macOS.ServiceProcess.TimeToFirstResponse", elapsed);
      break;
    case mac_notifications::NotificationStyle::kAppShim:
      base::UmaHistogramMediumTimes(
          "Notifications.macOS.AppShimProcess.TimeToFirstResponse", elapsed);
      break;
  }
}

void NotificationDispatcherMojo::OnServiceDisconnectedGracefully(
    ShutdownType shutdown_type) {
  base::TimeDelta elapsed = base::TimeTicks::Now() - service_start_time_;
  const bool had_displayed_notifications = !HasNoDisplayedNotifications();

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

  std::optional<base::TimeDelta> elapsed_since_first_response;
  if (service_first_response_time_.has_value()) {
    elapsed_since_first_response =
        base::TimeTicks::Now() - *service_first_response_time_;
  }

  // If the service responded and ran for more than 10 seconds or completed
  // successfully we restore the next delay to the initial value. If it failed
  // sooner than that then double the next time until we hit a maximum value so
  // we don't end up restarting it a lot.
  if (elapsed_since_first_response > kServiceRestartTimerResetDelay ||
      shutdown_type == ShutdownType::kChromeInitiated) {
    next_service_restart_timer_delay_ = kInitialServiceRestartTimerDelay;
  }

  // Once the restart delay has reached its maximum, only keep retrying if the
  // provider wants indefinite retries.
  if (shutdown_type == ShutdownType::kUnexpected &&
      (next_service_restart_timer_delay_ < kMaximumServiceRestartTimerDelay ||
       provider_factory_->ShouldRetryIndefinitely())) {
    // Calling CheckIfServiceCanBeTerminated() will force a new connection
    // attempt. base::Unretained(this) is safe here because |this| owns
    // |service_restart_timer_|.
    service_restart_timer_.Start(
        FROM_HERE, next_service_restart_timer_delay_,
        base::BindOnce(
            &NotificationDispatcherMojo::CheckIfServiceCanBeTerminated,
            base::Unretained(this)));
    next_service_restart_timer_delay_ =
        std::min(next_service_restart_timer_delay_ * 2,
                 kMaximumServiceRestartTimerDelay);
  } else {
    service_restart_timer_.Stop();
  }

  service_first_response_time_.reset();
  may_have_displayed_notifications_ =
      had_displayed_notifications &&
      shutdown_type != ShutdownType::kChromeInitiated;
  no_notifications_checker_.Cancel();
  provider_.reset();
  service_.reset();
  handler_.reset();
}

bool NotificationDispatcherMojo::HasNoDisplayedNotifications() const {
  return !service_ && !service_restart_timer_.IsRunning() &&
         !may_have_displayed_notifications_;
}

mac_notifications::mojom::MacNotificationService*
NotificationDispatcherMojo::GetOrCreateService() {
  if (!service_) {
    service_restart_timer_.Stop();
    service_start_time_ = base::TimeTicks::Now();
    provider_ = provider_factory_->LaunchProvider();
    provider_.set_disconnect_handler(base::BindOnce(
        &NotificationDispatcherMojo::OnServiceDisconnectedGracefully,
        base::Unretained(this), ShutdownType::kUnexpected));
    provider_->BindNotificationService(service_.BindNewPipeAndPassReceiver(),
                                       handler_.BindNewPipeAndPassRemote());
    service_.set_disconnect_handler(base::BindOnce(
        &NotificationDispatcherMojo::OnServiceDisconnectedGracefully,
        base::Unretained(this), ShutdownType::kUnexpected));
  }
  return service_.get();
}

void NotificationDispatcherMojo::DispatchGetNotificationsReply(
    GetDisplayedNotificationsCallback callback,
    std::vector<mac_notifications::mojom::NotificationIdentifierPtr>
        notifications) {
  OnServiceResponded();
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
  OnServiceResponded();
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
