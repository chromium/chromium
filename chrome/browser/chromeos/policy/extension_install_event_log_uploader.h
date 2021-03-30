// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_EXTENSION_INSTALL_EVENT_LOG_UPLOADER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_EXTENSION_INSTALL_EVENT_LOG_UPLOADER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/install_event_log_uploader_base.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/task_runner_context.h"

namespace enterprise_management {
class ExtensionInstallReportRequest;
}

class Profile;

namespace policy {

// Adapter between the system that captures and stores extension install event
// logs and the policy system which uploads them to the management server.
class ExtensionInstallEventLogUploader : public InstallEventLogUploaderBase {
 public:
  // Result of trying to build a |ReportQueueConfiguration|.
  using ReportQueueConfigResult = ::reporting::StatusOr<
      std::unique_ptr<::reporting::ReportQueueConfiguration>>;

  // Callback for handling a |ReportQueueConfigResult|.
  using ReportQueueConfigResultCallback =
      base::OnceCallback<void(ReportQueueConfigResult)>;

  // Callback for getting a |ReportQueueConfiguration|.
  using GetReportQueueConfigCallback =
      base::RepeatingCallback<void(ReportQueueConfigResultCallback)>;

  // The delegate that event logs will be retrieved from.
  class Delegate {
   public:
    // Callback invoked by the delegate with the extension logs to be uploaded
    // in |report|.
    using ExtensionLogSerializationCallback = base::OnceCallback<void(
        const enterprise_management::ExtensionInstallReportRequest* report)>;

    // Requests that the delegate serialize the current logs into a protobuf
    // and pass it to |callback|.
    virtual void SerializeExtensionLogForUpload(
        ExtensionLogSerializationCallback callback) = 0;

    // Notification to the delegate that the logs passed via the most recent
    // |ExtensionLogSerializationCallback| have been successfully uploaded to
    // the server and can be pruned from storage.
    virtual void OnExtensionLogUploadSuccess() = 0;

   protected:
    virtual ~Delegate();
  };

  explicit ExtensionInstallEventLogUploader(Profile* profile);
  ~ExtensionInstallEventLogUploader() override;

  // Sets the delegate. The delegate must either outlive |this| or be explicitly
  // removed by calling |SetDelegate(nullptr)|. Removing or changing the
  // delegate cancels the pending log upload, if any.
  void SetDelegate(Delegate* delegate);

  // Sets the report queue if it is not already set.
  void SetReportQueue(std::unique_ptr<reporting::ReportQueue> report_queue);

  // Meant to be used in tests for creating the ReportQueueConfiguration.
  void SetBuildReportQueueConfigurationForTests(const std::string& dm_token);

 private:
  // Ensures that only one ReportQueueBuilder is working at one time.
  class ReportQueueBuilderLeaderTracker;

  // ReportQueueBuilder instantiates a ReportQueue and uses
  // |set_report_queue_cb| to set it in the ExtensionInstallEventLogUploader.
  // ReportQueueBuilder ensures that only one ReportQueue instance is
  // built for ExtensionInstallLogUploader.
  // TODO: For testing there ideally there would be a way to capture the
  // ReportQueueConfiguration prior to passing it to the ReportQueue.
  class ReportQueueBuilder : public reporting::TaskRunnerContext<bool> {
   public:
    using SetReportQueueCallback =
        base::OnceCallback<void(std::unique_ptr<reporting::ReportQueue>,
                                base::OnceCallback<void()>)>;

    ReportQueueBuilder(
        SetReportQueueCallback set_report_queue_cb,
        GetReportQueueConfigCallback get_report_queue_config_cb,
        scoped_refptr<ReportQueueBuilderLeaderTracker> leader_tracker,
        base::OnceCallback<void(bool)> completion_cb,
        scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

   private:
    ~ReportQueueBuilder() override;

    // |OnStart| requests leadership promotion from the provided
    // |leader_tracker|. If there is already a leader, |OnStart| will exit.
    // Otherwise it will call |BuildReportQueue|.
    void OnStart() override;

    // Posts the task for building the ReportQueueConfiguration used to
    // instantiate the ReportQueue.
    void BuildReportQueueConfig();

    // Handles the result of the task posted in |BuildReportQueueConfig|.
    void OnReportQueueConfigResult(ReportQueueConfigResult report_queue_result);

    // |BuildReportQueue| uses the |report_queue_config| to build a ReportQueue
    // with ReportQueueProvider::CreateQueue. Sets |OnReportQueueResult| as
    // the completion callback for |ReportQueueProvider::CreateQueue|.
    void BuildReportQueue(std::unique_ptr<::reporting::ReportQueueConfiguration>
                              report_queue_config);

    // |OnReportQueueResult| will evaluate |report_queue_result|. If it is not
    // an OK status, it exits the builder with a |Complete| call. On an OK
    // status it |Schedule|s SetReportQueue.
    void OnReportQueueResult(
        reporting::StatusOr<std::unique_ptr<reporting::ReportQueue>>
            report_queue_result);

    // SetReportQueue will call |set_report_queue_cb_| with the provided
    // |report_queue|.
    void SetReportQueue(std::unique_ptr<reporting::ReportQueue> report_queue);

    // |Schedules| |ReleaseLeader|.
    void Complete();

    // Releases the leader lock if it is held, and then calls |Response|.
    void ReleaseLeader();

    // Callback for setting the ReportQueue in the calling
    // |ExtensionInstallEventLogUploader|.
    SetReportQueueCallback set_report_queue_cb_;

    // Callback for creating the |ReportQueueConfiguration|.
    GetReportQueueConfigCallback get_report_queue_config_cb_;

    // |leader_tracker_| is used to ensure that only one ReportQueueBuilder is
    // active at a time.
    scoped_refptr<ReportQueueBuilderLeaderTracker> leader_tracker_;

    base::OnceCallback<void()> release_leader_cb_;
  };

  // InstallEventLogUploaderBase:
  void CheckDelegateSet() override;
  void PostTaskForStartSerialization() override;
  void CancelClientUpload() override;
  void OnUploadSuccess() override;
  void StartSerialization() override;

  // Callback invoked by the delegate with the extension logs to be uploaded in
  // |report|. Forwards the logs to the client for upload.
  void OnSerialized(
      const enterprise_management::ExtensionInstallReportRequest* report);

  // Enqueues the report for upload.
  void EnqueueReport(
      const enterprise_management::ExtensionInstallReportRequest& report);

  // Handles the status of the report enqueue.
  void OnEnqueueDone(reporting::Status status);

  // The delegate that provides serialized logs to be uploaded.
  Delegate* delegate_ = nullptr;

  // ReportQueueBuilderLeaderTracker for building the ReportQueue,
  // passed to each ReportQueueBuilder in order to track which is the leader.
  scoped_refptr<ReportQueueBuilderLeaderTracker> leader_tracker_;

  // SequencedTaskRunenr for building the ReportQueue.
  scoped_refptr<base::SequencedTaskRunner> report_queue_builder_task_runner_;

  // Callback to generate a ReportQueueConfiguration.
  GetReportQueueConfigCallback get_report_queue_config_cb_;

  // ReportQueue for uploading events.
  std::unique_ptr<reporting::ReportQueue> report_queue_;

  // Weak pointer factory for invalidating callbacks passed to the delegate and
  // scheduled retries when the upload request is canceled or |this| is
  // destroyed.
  base::WeakPtrFactory<ExtensionInstallEventLogUploader> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_EXTENSION_INSTALL_EVENT_LOG_UPLOADER_H_
