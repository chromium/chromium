// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_connection_scheduler_impl.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash {
namespace eche_app {

constexpr net::BackoffEntry::Policy kRetryBackoffPolicy = {
    0,               // Number of initial errors to ignore.
    10 * 1000,       // Initial delay of 10 seconds in ms.
    2.0,             // Factor by which the waiting time will be multiplied.
    0.2,             // Fuzzing percentage.
    60 * 60 * 1000,  // Maximum delay of 1 hour in ms.
    -1,              // Never discard the entry.
    true,            // Use initial delay.
};

EcheConnectionSchedulerImpl::EcheConnectionSchedulerImpl(
    secure_channel::ConnectionManager* connection_manager,
    FeatureStatusProvider* feature_status_provider)
    : connection_manager_(connection_manager),
      feature_status_provider_(feature_status_provider),
      retry_backoff_(&kRetryBackoffPolicy) {
  DCHECK(connection_manager_);
  DCHECK(feature_status_provider_);
  current_feature_status_ = feature_status_provider_->GetStatus();
  feature_status_provider_->AddObserver(this);
  current_connection_status_ = connection_manager_->GetStatus();
  connection_manager_->AddObserver(this);
}

EcheConnectionSchedulerImpl::~EcheConnectionSchedulerImpl() {
  feature_status_provider_->RemoveObserver(this);
  connection_manager_->RemoveObserver(this);
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void EcheConnectionSchedulerImpl::ScheduleConnectionNow() {
  if (feature_status_provider_->GetStatus() == FeatureStatus::kDisabled &&
      connection_manager_->GetStatus() != ConnectionStatus::kDisconnected) {
    PA_LOG(WARNING) << "ScheduleConnectionNow() could not request a connection "
                    << "attempt because the current feature status is "
                       "kDisabled and current connection status is:"
                    << connection_manager_->GetStatus() << ".";
    return;
  }
  if (feature_status_provider_->GetStatus() != FeatureStatus::kDisconnected &&
      feature_status_provider_->GetStatus() != FeatureStatus::kDisabled) {
    PA_LOG(WARNING) << "ScheduleConnectionNow() could not request a connection "
                    << "attempt because the current feature status is: "
                    << feature_status_provider_->GetStatus() << ".";
    return;
  }

  PA_LOG(VERBOSE) << "ScheduleConnectionNow()";

  connection_manager_->AttemptNearbyConnection();
}

void EcheConnectionSchedulerImpl::OnFeatureStatusChanged() {
  ScheduleConnectionIfNeeded();
}

void EcheConnectionSchedulerImpl::OnConnectionStatusChanged() {
  // When this feature is disabled, we will not be able to get
  // OnFeatureStatusChanged() once the connection state changes, we need to
  // listen to OnConnectionStatusChanged() and then schedule a new connection.
  // For other cases (eg: feature enabled), OnFeatureStatusChanged has
  // been called, so we return directly.
  if (feature_status_provider_->GetStatus() != FeatureStatus::kDisabled)
    return;
  ScheduleConnectionIfNeeded();
}

void EcheConnectionSchedulerImpl::ScheduleConnectionIfNeeded() {
  const FeatureStatus previous_feature_status = current_feature_status_;
  current_feature_status_ = feature_status_provider_->GetStatus();
  const ConnectionStatus previous_connection_status =
      current_connection_status_;
  current_connection_status_ = connection_manager_->GetStatus();

  if (previous_feature_status == current_feature_status_ &&
      previous_connection_status == current_connection_status_)
    return;

  switch (current_feature_status_) {
    // The following feature states indicate that there is an interruption with
    // establishing connection to the host phone or that the feature is blocked
    // from initiating a connection. Disconnect the existing connection, reset
    // backoffs, and return early.
    case FeatureStatus::kIneligible:
      [[fallthrough]];
    case FeatureStatus::kDependentFeature:
      [[fallthrough]];
    case FeatureStatus::kDependentFeaturePending:
      DisconnectAndClearBackoffAttempts();
      return;
    // Connection has been established, clear existing backoffs and return
    // early.
    case FeatureStatus::kConnected:
      ClearBackoffAttempts();
      return;
    // Connection is in progress, return and wait for the result.
    case FeatureStatus::kConnecting:
      return;
    case FeatureStatus::kDisabled:
      [[fallthrough]];
    case FeatureStatus::kDisconnected:
      break;
  }

  if (previous_connection_status == ConnectionStatus::kConnecting) {
    PA_LOG(WARNING) << "Scheduling connection to retry in: "
                    << retry_backoff_.GetTimeUntilRelease().InSeconds()
                    << " seconds.";

    retry_backoff_.InformOfRequest(/*succeeded=*/false);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&EcheConnectionSchedulerImpl::ScheduleConnectionNow,
                       weak_ptr_factory_.GetWeakPtr()),
        retry_backoff_.GetTimeUntilRelease());
  } else if (current_connection_status_ == ConnectionStatus::kDisconnected) {
    PA_LOG(VERBOSE) << "ConnectionStatus has been updated to "
                    << "kDisconnected, scheduling connection now.";
    // Schedule connection now without a delay.
    ScheduleConnectionNow();
  }
}

void EcheConnectionSchedulerImpl::ClearBackoffAttempts() {
  // Remove all pending ScheduleConnectionNow() backoff attempts.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Reset the state of the backoff so that the next backoff retry starts at
  // the default initial delay.
  retry_backoff_.Reset();
}

void EcheConnectionSchedulerImpl::DisconnectAndClearBackoffAttempts() {
  ClearBackoffAttempts();

  // Disconnect existing connection or connection attempt.
  connection_manager_->Disconnect();
}

base::TimeDelta
EcheConnectionSchedulerImpl::GetCurrentBackoffDelayTimeForTesting() {
  return retry_backoff_.GetTimeUntilRelease();
}

int EcheConnectionSchedulerImpl::GetBackoffFailureCountForTesting() {
  return retry_backoff_.failure_count();
}

}  // namespace eche_app
}  // namespace ash
