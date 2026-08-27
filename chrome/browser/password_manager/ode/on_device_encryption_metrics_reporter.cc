// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/ode/on_device_encryption_metrics_reporter.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"

namespace password_manager {

OnDeviceEncryptionMetricsReporter::OnDeviceEncryptionMetricsReporter(
    std::unique_ptr<OnDeviceEncryptionStateTracker> passkey_tracker,
    std::unique_ptr<OnDeviceEncryptionStateTracker> password_tracker)
    : passkey_tracker_(std::move(passkey_tracker)),
      password_tracker_(std::move(password_tracker)) {
  if (passkey_tracker_) {
    passkey_observation_.Observe(passkey_tracker_.get());
    // TODO(crbug.com/540854648): In this case the metrics should be published
    // with delay.
    MaybeRecordPasskeyReadiness(passkey_tracker_->GetEncryptionState());
  }
  if (password_tracker_) {
    password_observation_.Observe(password_tracker_.get());
    // TODO(crbug.com/540854648): In this case the metrics should be published
    // with delay.
    MaybeRecordPasswordReadiness(password_tracker_->GetEncryptionState());
  }
}

OnDeviceEncryptionMetricsReporter::~OnDeviceEncryptionMetricsReporter() =
    default;

void OnDeviceEncryptionMetricsReporter::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

void OnDeviceEncryptionMetricsReporter::MaybeRecordPasskeyReadiness(
    OnDeviceEncryptionState current_state) {
  std::optional<OnDeviceEncryptionStateHistogramBucket> bucket =
      ToOnDeviceEncryptionStateHistogramBucket(current_state);
  if (bucket.has_value()) {
    base::UmaHistogramEnumeration(kPasskeyOnDeviceEncryptionStateHistogram,
                                  *bucket);
  }
}

void OnDeviceEncryptionMetricsReporter::MaybeRecordPasswordReadiness(
    OnDeviceEncryptionState current_state) {
  std::optional<OnDeviceEncryptionStateHistogramBucket> bucket =
      ToOnDeviceEncryptionStateHistogramBucket(current_state);
  if (bucket.has_value()) {
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
  }
  NOTREACHED();
}

}  // namespace password_manager
