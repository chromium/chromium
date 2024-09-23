// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/event_based_logs/event_observer_base.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/policy_pref_names.h"
#include "chrome/browser/ash/policy/reporting/event_based_logs/event_based_log_uploader.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "components/prefs/pref_service.h"
#include "components/reporting/util/status.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"

namespace {

// Returns when the given event was last uploaded, or std::nullopt if it has
// never been uploaded before.
std::optional<base::Time> GetLastUploadTimeOf(const PrefService& local_state,
                                              const std::string& event_name) {
  const base::Value::Dict& last_upload_times =
      local_state.GetDict(policy::prefs::kEventBasedLogLastUploadTimes);
  const base::Value* event_upload_time = last_upload_times.Find(event_name);

  // If the last upload time is not stored in the local state, it means that
  // this is the first uploaded event of the given type.
  if (!event_upload_time) {
    return std::nullopt;
  }

  std::optional<base::Time> upload_time = base::ValueToTime(*event_upload_time);
  // Upload time must be a valid time.
  CHECK(upload_time.has_value());

  return upload_time;
}

}  // namespace

namespace policy {

EventObserverBase::EventObserverBase() = default;
EventObserverBase::~EventObserverBase() = default;

std::string EventObserverBase::GetEventName() const {
  return ash::reporting::TriggerEventType_Name(GetEventType());
}

void EventObserverBase::SetLogUploaderForTesting(
    std::unique_ptr<EventBasedLogUploader> log_uploader) {
  CHECK_IS_TEST();
  log_uploader_ = std::move(log_uploader);
}

void EventObserverBase::TriggerLogUpload(
    std::optional<std::string> upload_id,
    base::OnceCallback<void(EventBasedUploadStatus)> on_upload_triggered,
    base::TimeDelta upload_wait_period) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsUploadWaitPeriodFinished(upload_wait_period)) {
    LOG(WARNING) << "Event based log upload is dropped because upload "
                    "wait period isn't finished for event type: "
                 << GetEventName();
    std::move(on_upload_triggered).Run(EventBasedUploadStatus::kDeclined);
    return;
  }
  on_log_upload_triggered_ = std::move(on_upload_triggered);
  // `log_uploader_` could already be set for testing.
  if (!log_uploader_) {
    log_uploader_ = std::make_unique<EventBasedLogUploaderImpl>();
  }
  log_uploader_->UploadEventBasedLogs(
      GetDataCollectorTypes(), GetEventType(), upload_id,
      base::BindOnce(&EventObserverBase::OnLogUploaderDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EventObserverBase::OnLogUploaderDone(reporting::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!status.ok()) {
    LOG(ERROR)
        << GetEventName()
        << " event based log upload failed on reporting pipeline with error: "
        << status.error_message();
    std::move(on_log_upload_triggered_).Run(EventBasedUploadStatus::kFailure);
    return;
  }
  VLOG(0) << GetEventName()
          << " event based log upload completed successfully.";
  RecordUploadTime(base::Time::NowFromSystemTime());
  std::move(on_log_upload_triggered_).Run(EventBasedUploadStatus::kSuccess);
}

bool EventObserverBase::IsUploadWaitPeriodFinished(
    base::TimeDelta upload_wait_period) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<base::Time> upload_time =
      GetLastUploadTimeOf(*g_browser_process->local_state(), GetEventName());
  // If upload time is not stored in local state, it means that the event is
  // uploaded for the first time.
  if (!upload_time.has_value()) {
    return true;
  }

  base::TimeDelta time_since_last_upload =
      base::Time::NowFromSystemTime() - upload_time.value();
  return time_since_last_upload >= upload_wait_period;
}

void EventObserverBase::RecordUploadTime(base::Time timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ::prefs::ScopedDictionaryPrefUpdate last_upload_times_update(
      g_browser_process->local_state(), prefs::kEventBasedLogLastUploadTimes);
  last_upload_times_update->Set(GetEventName(), base::TimeToValue(timestamp));
}

}  // namespace policy
