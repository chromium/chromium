// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_EXTENSION_INSTALL_EVENT_LOG_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_EXTENSION_INSTALL_EVENT_LOG_MANAGER_H_

#include <memory>
#include <set>

#include "chrome/browser/ash/policy/reporting/extension_install_event_log.h"
#include "chrome/browser/ash/policy/reporting/extension_install_event_log_uploader.h"
#include "chrome/browser/ash/policy/reporting/extension_install_event_logger.h"
#include "chrome/browser/ash/policy/reporting/install_event_log_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace policy {
// Owns an |ExtensionInstallEventLog| for log storage and an
// |ExtensionInstallEventLogger| for log collection. The
// |ExtensionInstallEventLogUploader| is passed to the constructor and must
// outlive |this|.
class ExtensionInstallEventLogManager
    : public InstallEventLogManagerBase,
      public ExtensionInstallEventLogger::Delegate,
      public ExtensionInstallEventLogUploader::Delegate {
 public:
  // All accesses to the |profile|'s extension install event log file must use
  // the same |log_task_runner_wrapper| to ensure correct I/O serialization.
  // |uploader| must outlive |this|.
  ExtensionInstallEventLogManager(LogTaskRunnerWrapper* log_task_runner_wrapper,
                                  ExtensionInstallEventLogUploader* uploader,
                                  Profile* profile);

  // Posts a task to |log_task_runner_| that stores the log to file and destroys
  // |log_|. |log_| thus outlives |this| but any pending callbacks are canceled
  // by invalidating weak pointers.
  ~ExtensionInstallEventLogManager() override;

  // Clears all data related to the app-install event log for |profile|. Must
  // not be called while an |ExtensionInstallEventLogManager| exists for
  // |profile|. This method and any other accesses to the |profile|'s extension
  // install event log must use the same |log_task_runner_wrapper| to ensure
  // correct I/O serialization.
  static void Clear(LogTaskRunnerWrapper* log_task_runner_wrapper,
                    Profile* profile);

  // ExtensionInstallEventLogger::Delegate:
  void Add(std::set<extensions::ExtensionId> extensions,
           const enterprise_management::ExtensionInstallReportLogEvent& event)
      override;

  // ExtensionInstallEventLogUploader::Delegate:
  void SerializeExtensionLogForUpload(
      ExtensionInstallEventLogUploader::Delegate::
          ExtensionLogSerializationCallback callback) override;
  void OnExtensionLogUploadSuccess() override;

 private:
  // Once created, |ExtensionLog| runs in the background and must be accessed
  // and eventually destroyed via |log_task_runner_|. |ExtensionLog| outlives
  // its parent and stores the current log to disk in its destructor.
  // TODO(crbub/1092387): Remove this class to handle sequence checking in
  // ExtensionInstallEventLog.
  class ExtensionLog
      : public InstallEventLogManagerBase::InstallLog<
            enterprise_management::ExtensionInstallReportLogEvent,
            ExtensionInstallEventLog> {
   public:
    ExtensionLog();

    // Stores the current log to disk.
    ~ExtensionLog() override;

    // Serializes the log to a protobuf for upload.
    std::unique_ptr<enterprise_management::ExtensionInstallReportRequest>
    Serialize();

   private:
    // Ensures that methods are not called from the wrong thread.
    SEQUENCE_CHECKER(sequence_checker_);
  };

  // |ExtensionLogUpload| is owned by |owner_| and |owner_| outlives it.
  class ExtensionLogUpload : public InstallEventLogManagerBase::LogUpload {
   public:
    explicit ExtensionLogUpload(ExtensionInstallEventLogManager* owner);
    ~ExtensionLogUpload() override;
    void StoreLog() override;
    void RequestUploadForUploader() override;

   private:
    ExtensionInstallEventLogManager* owner_;
  };

  // Uploads logs to the server.
  ExtensionInstallEventLogUploader* const uploader_;

  // Helper that owns the log store. Once created, must only be accessed via
  // |log_task_runner_|. Outlives |this| and ensures the extension log is stored
  // to disk in its destructor.
  std::unique_ptr<ExtensionLog> log_;

  // Handles storing the logs and preparing them for upload.
  std::unique_ptr<ExtensionLogUpload> extension_log_upload_;

  // Collects log events and passes them to |this|.
  std::unique_ptr<ExtensionInstallEventLogger> logger_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_EXTENSION_INSTALL_EVENT_LOG_MANAGER_H_
