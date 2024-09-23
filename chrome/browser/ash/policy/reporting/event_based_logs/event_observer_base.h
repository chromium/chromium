// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_EVENT_BASED_LOGS_EVENT_OBSERVER_BASE_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_EVENT_BASED_LOGS_EVENT_OBSERVER_BASE_H_

#include <optional>
#include <set>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/reporting/event_based_logs/event_based_log_uploader.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "components/reporting/util/status.h"

namespace policy {

enum class EventBasedUploadStatus {
  // Upload triggered successfully.
  kSuccess,
  // Upload can't be triggered. This failure can occur due to internal reporting
  // pipeline or log collection errors. Callers have option to retry triggering
  // the upload in this case.
  kFailure,
  // Upload is declined because upload wait period is not finished yet.
  kDeclined,
};

// Base class that the event observers who want to upload event based logs needs
// to implement. `EventObserverBase` gives the functionality that is needed to
// trigger the log upload to the server.
//
// EXAMPLE
// class FooEventObserver : public policy::EventObserverBase, FooEventSource {
//  public:
//   // Need to add a new `ash::reporting::TriggerEventType` to define the
//   // observed event.
//   ash::reporting::TriggerEventType GetEventType() override {
//     return ash::reporting::TriggerEventType::<FOO>;
//   }
//   // Select which logs to upload by implementing this.
//   std::set<support_tool::DataCollectorType> GetDataCollectorTypes() override
//   {
//     return {support_tool::DataCollectorType::XX};
//   }
//   // FooEventSource observer.
//   void OnFooOccurred(FooEvent event) {
//     // Trigger log upload here.
//     TriggerLogUpload(upload_id, base::DoNothing());
//   }
//   void OnCriticalFooOccurred(FooEvent event) {
//     // Trigger log upload here. It is possible to override the default time
//     // limit for critical events to upload more frequently. Please reach out
//     // to get guidance if you consider this option.
//     TriggerLogUpload(upload_id, base::BindOnce(&DoSomethingWithUploadResult),
//                      base::Hours(1));
//   }
// };
class EventObserverBase {
 public:
  EventObserverBase();

  EventObserverBase(const EventObserverBase&) = delete;
  EventObserverBase& operator=(const EventObserverBase&) = delete;

  virtual ~EventObserverBase();

  // Returns the event type that has triggered the log upload. Inheritors of
  // this class must add a new event type to `ash::reporting::TriggerEventType`
  // when they start observing a new event.
  virtual ash::reporting::TriggerEventType GetEventType() const = 0;

  // Returns the set of `support_tool::DataCollectorType`s that is related to
  // the event this observer is observing. Inheritors of this class must
  // override this to select which log files to upload.
  virtual std::set<support_tool::DataCollectorType> GetDataCollectorTypes()
      const = 0;

  // Returns the event name of the event that the observer observes.
  std::string GetEventName() const;

  // Triggers the upload of the log types from `GetDataCollectorTypes()`.
  // Delegates the log upload to `EventBasedLogUploader` with the required
  // information. If an upload of the same event type has been done in last
  // `upload_wait_period` hours, the upload won't be triggered. The
  // callers of this function can override `upload_wait_period` to upload more
  // frequently but by default it's 24 hours. This is the recommendation. Please
  // reach out to discuss if your use-case requires different frequency.
  // The callers of this function can set `on_upload_triggered` callback to
  // handle different states according to their use-case.
  void TriggerLogUpload(
      std::optional<std::string> upload_id,
      base::OnceCallback<void(EventBasedUploadStatus)> on_upload_triggered,
      base::TimeDelta upload_wait_period = base::Hours(24));

  void SetLogUploaderForTesting(
      std::unique_ptr<EventBasedLogUploader> log_uploader);

 private:
  // Checks if the upload wait period is finished since the last event based log
  // upload for `GetEventType()`.
  bool IsUploadWaitPeriodFinished(base::TimeDelta upload_wait_period) const;

  // Called by `log_uploader_` when log upload triggered by `TriggerLogUpload()`
  // function is completed. Records the upload time to prefs if upload has
  // succeeded and resets the `log_uploader_`.
  void OnLogUploaderDone(reporting::Status status);

  // This function should be called when upload is enqueued to reporting
  // service. It will record the upload time for the event type to local
  // settings prefs.
  void RecordUploadTime(base::Time timestamp);

  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<EventBasedLogUploader> log_uploader_;
  // This callback is set by the caller of `TriggerLogUpload()` function and
  // will be called when the log upload is triggered. The caller can handle the
  // result status using this callback.
  base::OnceCallback<void(EventBasedUploadStatus)> on_log_upload_triggered_;
  base::WeakPtrFactory<EventObserverBase> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_EVENT_BASED_LOGS_EVENT_OBSERVER_BASE_H_
