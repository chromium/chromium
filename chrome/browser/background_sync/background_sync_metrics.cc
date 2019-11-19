// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/background_sync_metrics.h"

#include "base/bind.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/origin.h"

BackgroundSyncMetrics::BackgroundSyncMetrics(
    ukm::UkmBackgroundRecorderService* ukm_background_service)
    : ukm_background_service_(ukm_background_service) {
  DCHECK(ukm_background_service_);
}

BackgroundSyncMetrics::~BackgroundSyncMetrics() = default;

void BackgroundSyncMetrics::MaybeRecordOneShotSyncRegistrationEvent(
    const url::Origin& origin,
    bool can_fire,
    bool is_reregistered) {
  ukm_background_service_->GetBackgroundSourceIdIfAllowed(
      origin,
      base::BindOnce(
          &BackgroundSyncMetrics::DidGetBackgroundSourceId,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(
              &BackgroundSyncMetrics::RecordOneShotSyncRegistrationEvent,
              weak_ptr_factory_.GetWeakPtr(), can_fire, is_reregistered)));
}

void BackgroundSyncMetrics::MaybeRecordPeriodicSyncRegistrationEvent(
    const url::Origin& origin,
    int min_interval,
    bool is_reregistered) {
  ukm_background_service_->GetBackgroundSourceIdIfAllowed(
      origin,
      base::BindOnce(
          &BackgroundSyncMetrics::DidGetBackgroundSourceId,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(
              &BackgroundSyncMetrics::RecordPeriodicSyncRegistrationEvent,
              weak_ptr_factory_.GetWeakPtr(), min_interval, is_reregistered)));
}

void BackgroundSyncMetrics::MaybeRecordOneShotSyncCompletionEvent(
    const url::Origin& origin,
    blink::ServiceWorkerStatusCode status_code,
    int num_attempts,
    int max_attempts) {
  ukm_background_service_->GetBackgroundSourceIdIfAllowed(
      origin, base::BindOnce(
                  &BackgroundSyncMetrics::DidGetBackgroundSourceId,
                  weak_ptr_factory_.GetWeakPtr(),
                  base::BindOnce(
                      &BackgroundSyncMetrics::RecordOneShotSyncCompletionEvent,
                      weak_ptr_factory_.GetWeakPtr(), status_code, num_attempts,
                      max_attempts)));
}

void BackgroundSyncMetrics::MaybeRecordPeriodicSyncEventCompletion(
    const url::Origin& origin,
    blink::ServiceWorkerStatusCode status_code,
    int num_attempts,
    int max_attempts) {
  ukm_background_service_->GetBackgroundSourceIdIfAllowed(
      origin, base::BindOnce(
                  &BackgroundSyncMetrics::DidGetBackgroundSourceId,
                  weak_ptr_factory_.GetWeakPtr(),
                  base::BindOnce(
                      &BackgroundSyncMetrics::RecordPeriodicSyncEventCompletion,
                      weak_ptr_factory_.GetWeakPtr(), status_code, num_attempts,
                      max_attempts)));
}

void BackgroundSyncMetrics::DidGetBackgroundSourceId(
    RecordCallback record_callback,
    base::Optional<ukm::SourceId> source_id) {
  // This background event did not meet the requirements for the UKM service.
  if (!source_id)
    return;

  std::move(record_callback).Run(*source_id);
  if (ukm_event_recorded_for_testing_)
    std::move(ukm_event_recorded_for_testing_).Run();
}

void BackgroundSyncMetrics::RecordOneShotSyncRegistrationEvent(
    bool can_fire,
    bool is_reregistered,
    ukm::SourceId source_id) {
  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();
  DCHECK(recorder);

  ukm::builders::BackgroundSyncRegistered(source_id)
      .SetCanFire(can_fire)
      .SetIsReregistered(is_reregistered)
      .Record(recorder);
}

void BackgroundSyncMetrics::RecordPeriodicSyncRegistrationEvent(
    int min_interval,
    bool is_reregistered,
    ukm::SourceId source_id) {
  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();
  DCHECK(recorder);

  ukm::builders::PeriodicBackgroundSyncRegistered(source_id)
      .SetMinIntervalMs(ukm::GetExponentialBucketMin(
          min_interval, kUkmEventDataBucketSpacing))
      .SetIsReregistered(is_reregistered)
      .Record(recorder);
}

void BackgroundSyncMetrics::RecordOneShotSyncCompletionEvent(
    blink::ServiceWorkerStatusCode status_code,
    int num_attempts,
    int max_attempts,
    ukm::SourceId source_id) {
  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();
  DCHECK(recorder);

  ukm::builders::BackgroundSyncCompleted(source_id)
      .SetStatus(static_cast<int>(status_code))
      .SetNumAttempts(num_attempts)
      .SetMaxAttempts(max_attempts)
      .Record(recorder);
}

void BackgroundSyncMetrics::RecordPeriodicSyncEventCompletion(
    blink::ServiceWorkerStatusCode status_code,
    int num_attempts,
    int max_attempts,
    ukm::SourceId source_id) {
  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();
  DCHECK(recorder);

  ukm::builders::PeriodicBackgroundSyncEventCompleted(source_id)
      .SetStatus(static_cast<int>(status_code))
      .SetNumAttempts(num_attempts)
      .SetMaxAttempts(max_attempts)
      .Record(recorder);
}
