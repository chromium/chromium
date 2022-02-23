// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_EXTENSION_INSTALL_EVENT_LOG_UPLOADER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_EXTENSION_INSTALL_EVENT_LOG_UPLOADER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/policy/reporting/install_event_log_uploader_base.h"
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

  // Helper for creating a new |ExtensionInstallEventLogUploader| using the
  // specified profile.
  static std::unique_ptr<ExtensionInstallEventLogUploader> Create(
      Profile* profile);

  // Test helper for creating a new |ExtensionInstallEventLogUploader| using the
  // specified profile and mock report queue.
  static std::unique_ptr<ExtensionInstallEventLogUploader> CreateForTest(
      Profile* profile,
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
          report_queue);

  ~ExtensionInstallEventLogUploader() override;

  // Sets the delegate. The delegate must either outlive |this| or be explicitly
  // removed by calling |SetDelegate(nullptr)|. Removing or changing the
  // delegate cancels the pending log upload, if any.
  void SetDelegate(Delegate* delegate);

 private:
  explicit ExtensionInstallEventLogUploader(
      Profile* profile,
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
          report_queue);

  // InstallEventLogUploaderBase:
  void CheckDelegateSet() override;
  void PostTaskForStartSerialization() override;
  void CancelClientUpload() override;
  void OnUploadSuccess() override;
  void StartSerialization() override;

  // Enqueues the report for upload, and is invoked by the delegate with the
  // extension logs to be uploaded in |report|.
  void EnqueueReport(
      const enterprise_management::ExtensionInstallReportRequest* report);

  // Handles the status of the report enqueue.
  void OnEnqueueDone(reporting::Status status);

  // The delegate that provides serialized logs to be uploaded.
  Delegate* delegate_ = nullptr;

  // Speculative report queue for uploading events.
  const std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
      report_queue_;

  // Weak pointer factory for invalidating callbacks passed to the delegate and
  // scheduled retries when the upload request is canceled or |this| is
  // destroyed.
  base::WeakPtrFactory<ExtensionInstallEventLogUploader> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_EXTENSION_INSTALL_EVENT_LOG_UPLOADER_H_
