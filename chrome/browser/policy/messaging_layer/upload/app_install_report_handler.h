// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_APP_INSTALL_REPORT_HANDLER_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_APP_INSTALL_REPORT_HANDLER_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/upload/dm_server_upload_service.h"
#include "chrome/browser/policy/messaging_layer/util/shared_queue.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "chrome/browser/policy/messaging_layer/util/task_runner_context.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/proto/record.pb.h"

namespace reporting {

// |AppInstallReportHandler| handles |AppInstallReportRequests|, sending them to
// the server using |CloudPolicyClient|. Since |CloudPolicyClient| will cancel
// any in progress reports if a new report is added, |AppInstallReportHandler|
// ensures that only one report is ever processed at one time by forming a
// queue.
class AppInstallReportHandler : public DmServerUploadService::RecordHandler {
 public:
  // The client uses a boolean value for status, where true indicates success
  // and false indicates failure.
  using ClientCallback = base::OnceCallback<void(bool status)>;

  using ReleaseLeaderCallback = base::OnceCallback<void()>;
  using RequestLeaderPromotionCallback =
      base::OnceCallback<StatusOr<ReleaseLeaderCallback>()>;

  // Tracking the leader needs to outlive |AppInstallReportHandler| so it needs
  // to be wrapped in a scoped_refptr.
  class UploaderLeaderTracker
      : public base::RefCountedThreadSafe<UploaderLeaderTracker> {
   public:
    static scoped_refptr<UploaderLeaderTracker> Create();

    // If there is currently no leader
    // (|has_promoted_app_install_event_uploader_| is false), then the StatusOr
    // will contain a callback to release leadership.  If there is currently a
    // leader an error::RESOURCE_EXHAUSTED is returned (which should be the
    // common case). This will be called on sequence from inside the
    // |AppInstallReportUploader| and so needs no additional protection.
    StatusOr<ReleaseLeaderCallback> RequestLeaderPromotion();

    // Once a AppInstallEventUploader leader drains the queue of reports, it
    // will release its leadership and return, allowing a new
    // AppInstallEventUploader to take leadership and upload events.
    void ReleaseLeader();

   private:
    friend class base::RefCountedThreadSafe<UploaderLeaderTracker>;
    virtual ~UploaderLeaderTracker() = default;

    UploaderLeaderTracker() = default;

    // Flag indicates whether a leader has been promoted.
    bool has_promoted_app_install_event_uploader_{false};
  };

  // AppInstallReportUploader handles enqueuing events on the |report_queue_|,
  // and uploading those events with the |client_|.
  class AppInstallReportUploader : public TaskRunnerContext<bool> {
   public:
    AppInstallReportUploader(
        base::Value report,
        scoped_refptr<SharedQueue<base::Value>> report_queue,
        scoped_refptr<UploaderLeaderTracker> leader_tracker,
        policy::CloudPolicyClient* client,
        ClientCallback client_cb,
        scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

   private:
    ~AppInstallReportUploader() override;

    // AppInstallReportUploader follows the this sequence for handling upload:
    // OnStart(): Pushes a report onto the |report_queue_|
    // OnPushComplete: Called off sequence. Schedules RequestLeaderPromotion on
    //                 sequence
    // RequestLeaderPromotion: Called on sequence. requests promotion to leader
    // if there isn't already one.
    // OnLeaderPromotionResult: Called off sequence - two paths
    //    1. A leader already exists - Call Complete() and then Response().
    //    2. Promoted to leader - begin processing records in the queue.
    //    Schedules |ScheduleNextUpload| on sequence.
    //
    // ScheduleNextPop: Called on sequence. Calls report_queue_->Pop() with
    //                  |StartUpload|.
    // OnPopResult: Called off Sequence, two paths:
    //   1. pop_result indicates there are no more records:
    //      Schedule ReleaseLeaderPromotion on sequence then Complete and
    //      Response.
    //   2. pop_result holds a record: Schedule |UploadRecord|.
    // UploadRecord: Called on sequence. Calls client_->UploadAppInstallReport
    // with |UploadComplete| as the callback.
    // UploadComplete: Called off sequence. Schedule |ScheduleNextPop| on
    //                 sequence.
    //
    // During the ScheduleNextPop loop other requests can be enqueued from other
    // threads while leadership is held. This allows one thread to be busy with
    // the process of uploading, while other threads can push reports onto the
    // queue and return. This is necessary because |CloudPolicyClient| only
    // allows one upload at a time.
    void OnStart() override;
    void OnPushComplete();
    void RequestLeaderPromotion();

    void ScheduleNextPop();
    void OnPopResult(StatusOr<base::Value> pop_result);
    void StartUpload(base::Value record);
    void OnUploadComplete(bool success);

    void Complete();

    base::Value report_;
    scoped_refptr<SharedQueue<base::Value>> report_queue_;
    scoped_refptr<UploaderLeaderTracker> leader_tracker_;
    ReleaseLeaderCallback release_leader_cb_;

    policy::CloudPolicyClient* const client_;
  };

  explicit AppInstallReportHandler(policy::CloudPolicyClient* client);
  ~AppInstallReportHandler() override;

  // Base class RecordHandler method implementation.
  Status HandleRecord(Record record) override;

 protected:
  // Helper method for |ValidateRecord|. Validates destination.
  Status ValidateDestination(const Record& record,
                             Destination expected_destination) const;

 private:
  // Validate record (override for subclass).
  virtual Status ValidateRecord(const Record& record) const;

  // Convert record into base::Value for upload (override for subclass).
  virtual StatusOr<base::Value> ConvertRecord(const Record& record) const;

  // Helper method. Validates CloudPolicyClient state.
  Status ValidateClientState() const;

  scoped_refptr<SharedQueue<base::Value>> report_queue_;
  scoped_refptr<UploaderLeaderTracker> leader_tracker_;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_APP_INSTALL_REPORT_HANDLER_H_
