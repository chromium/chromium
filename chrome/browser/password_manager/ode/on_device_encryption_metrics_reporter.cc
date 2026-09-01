// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/ode/on_device_encryption_metrics_reporter.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"

namespace password_manager {

OnDeviceEncryptionMetricsReporter::OnDeviceEncryptionMetricsReporter(
    std::unique_ptr<OnDeviceEncryptionStateTracker> passkey_tracker,
    std::unique_ptr<OnDeviceEncryptionStateTracker> password_tracker)
    : passkey_tracker_(std::move(passkey_tracker)),
      password_tracker_(std::move(password_tracker)) {
  // During startup, services are actively initializing, and might also start
  // notifying the observers. On the other it is good practice to avoid
  // publishing metrics immediately on startup. Given this, the initialization
  // of observations and publishing of initial metrics is being deferred.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OnDeviceEncryptionMetricsReporter::
                         StartObservationsAndRecordInitialMetrics,
                     weak_ptr_factory_.GetWeakPtr()),
      kInitialStateReportingDelay);
}

OnDeviceEncryptionMetricsReporter::~OnDeviceEncryptionMetricsReporter() =
    default;

void OnDeviceEncryptionMetricsReporter::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  passkey_observation_.Reset();
  password_observation_.Reset();
  passkey_tracker_.reset();
  password_tracker_.reset();
}

void OnDeviceEncryptionMetricsReporter::OnDeviceEncryptionStateChanged(
    OnDeviceEncryptionStateTracker* tracker,
    OnDeviceEncryptionState previous_state,
    OnDeviceEncryptionState new_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tracker == passkey_tracker_.get()) {
    MaybeRecordPasskeyReadiness(new_state);
  } else if (tracker == password_tracker_.get()) {
    MaybeRecordPasswordReadiness(new_state);
  } else {
    NOTREACHED();
  }
}

void OnDeviceEncryptionMetricsReporter::
    OnDeviceEncryptionStateTrackerShuttingDown(
        OnDeviceEncryptionStateTracker* tracker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tracker == passkey_tracker_.get()) {
    passkey_observation_.Reset();
  } else if (tracker == password_tracker_.get()) {
    password_observation_.Reset();
  } else {
    NOTREACHED();
  }
}

void OnDeviceEncryptionMetricsReporter::
    StartObservationsAndRecordInitialMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(passkey_tracker_);
  CHECK(password_tracker_);
  passkey_observation_.Observe(passkey_tracker_.get());
  MaybeRecordPasskeyReadiness(passkey_tracker_->GetEncryptionState());
  password_observation_.Observe(password_tracker_.get());
  MaybeRecordPasswordReadiness(password_tracker_->GetEncryptionState());
}

void OnDeviceEncryptionMetricsReporter::MaybeRecordPasskeyReadiness(
    OnDeviceEncryptionState current_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<OnDeviceEncryptionStateHistogramBucket> bucket =
      ToOnDeviceEncryptionStateHistogramBucket(current_state);
  if (bucket.has_value() && bucket != last_published_passkey_bucket_) {
    last_published_passkey_bucket_ = bucket;
    base::UmaHistogramEnumeration(kPasskeyOnDeviceEncryptionStateHistogram,
                                  *bucket);
  }
}

void OnDeviceEncryptionMetricsReporter::MaybeRecordPasswordReadiness(
    OnDeviceEncryptionState current_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<OnDeviceEncryptionStateHistogramBucket> bucket =
      ToOnDeviceEncryptionStateHistogramBucket(current_state);
  if (bucket.has_value() && bucket != last_published_password_bucket_) {
    last_published_password_bucket_ = bucket;
    base::UmaHistogramEnumeration(kPasswordOnDeviceEncryptionStateHistogram,
                                  *bucket);
  }
}

std::optional<OnDeviceEncryptionStateHistogramBucket>
OnDeviceEncryptionMetricsReporter::ToOnDeviceEncryptionStateHistogramBucket(
    OnDeviceEncryptionState state) {
  switch (state) {
    case OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable:
      // While services are initializing or loading, the encryption state cannot
      // be determined yet. We don't want to record metrics for this
      // transitional state.
      return std::nullopt;
    case OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled:
      return OnDeviceEncryptionStateHistogramBucket::
          kOnDeviceEncryptionNotEnabled;
    case OnDeviceEncryptionState::kDeviceNotReady:
      return OnDeviceEncryptionStateHistogramBucket::kDeviceNotReady;
    case OnDeviceEncryptionState::kDeviceReady:
      return OnDeviceEncryptionStateHistogramBucket::kDeviceReady;
    case OnDeviceEncryptionState::kPasswordAndPasskeySyncDisabled:
      return OnDeviceEncryptionStateHistogramBucket::
          kPasswordAndPasskeySyncDisabled;
  }
  NOTREACHED();
}

}  // namespace password_manager
