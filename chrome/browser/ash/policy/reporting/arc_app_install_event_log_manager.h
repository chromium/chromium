// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_EVENT_LOG_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_EVENT_LOG_MANAGER_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/policy/reporting/arc_app_install_event_log.h"
#include "chrome/browser/ash/policy/reporting/arc_app_install_event_log_uploader.h"
#include "chrome/browser/ash/policy/reporting/arc_app_install_event_logger.h"
#include "chrome/browser/ash/policy/reporting/install_event_log_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"

class Profile;

namespace policy {
// Owns an |ArcAppInstallEventLog| for log storage and an
// |ArcAppInstallEventLogger| for log collection. The
// |ArcAppInstallEventUploader| is passed to the constructor and must outlive
// |this|.
class ArcAppInstallEventLogManager
    : public InstallEventLogManagerBase,
      public ArcAppInstallEventLogger::Delegate,
      public ArcAppInstallEventLogUploader::Delegate {
 public:
  // All accesses to the |profile|'s app push-install event log file must use
  // the same |log_task_runner_wrapper| to ensure correct I/O serialization.
  // |uploader| must outlive |this|.
  ArcAppInstallEventLogManager(LogTaskRunnerWrapper* log_task_runner_wrapper,
                               ArcAppInstallEventLogUploader* uploader,
                               Profile* profile);

  // Posts a task to |log_task_runner_| that stores the log to file and destroys
  // |log_|. |log_| thus outlives |this| but any pending callbacks are canceled
  // by invalidating weak pointers.
  ~ArcAppInstallEventLogManager() override;

  // Clears all data related to the app-install event log for |profile|. Must
  // not be called while an |ArcAppInstallEventLogManager| exists for |profile|.
  // This method and any other accesses to the |profile|'s app push-install
  // event log must use the same |log_task_runner_wrapper| to ensure correct I/O
  // serialization.
  static void Clear(LogTaskRunnerWrapper* log_task_runner_wrapper,
                    Profile* profile);

  // ArcAppInstallEventLogger::Delegate:
  void Add(
      const std::set<std::string>& packages,
      const enterprise_management::AppInstallReportLogEvent& event) override;
  void GetAndroidId(
      ArcAppInstallEventLogger::Delegate::AndroidIdCallback) const override;

  // ArcAppInstallEventLogUploader::Delegate:
  void SerializeForUpload(
      ArcAppInstallEventLogUploader::Delegate::SerializationCallback callback)
      override;
  void OnUploadSuccess() override;

 private:
  // Once created, |ArcLog| runs in the background and must be accessed and
  // eventually destroyed via |log_task_runner_|. |ArcLog| outlives its parent
  // and stores the current log to disk in its destructor.
  // TODO(crbub/1092387): Remove this class to handle sequence checking in
  // ArcAppInstallEventLog.
  class ArcLog : public InstallEventLogManagerBase::InstallLog<
                     enterprise_management::AppInstallReportLogEvent,
                     ArcAppInstallEventLog> {
   public:
    ArcLog();

    // Stores the current log to disk.
    ~ArcLog() override;

    // Serializes the log to a protobuf for upload.
    std::unique_ptr<enterprise_management::AppInstallReportRequest> Serialize();

   private:
    // Ensures that methods are not called from the wrong thread.
    SEQUENCE_CHECKER(sequence_checker_);
  };

  // |AppLogUpload| is owned by |owner_| and |owner_| outlives it.
  class AppLogUpload : public InstallEventLogManagerBase::LogUpload {
   public:
    explicit AppLogUpload(ArcAppInstallEventLogManager* owner);
    ~AppLogUpload() override;
    void StoreLog() override;
    void RequestUploadForUploader() override;

   private:
    raw_ptr<ArcAppInstallEventLogManager> owner_;
  };

  // Uploads logs to the server.
  const raw_ptr<ArcAppInstallEventLogUploader> uploader_;

  // Helper that owns the log store. Once created, must only be accessed via
  // |log_task_runner_|. Outlives |this| and ensures the extension log is stored
  // to disk in its destructor.
  std::unique_ptr<ArcLog> log_;

  // Handles storing the logs and preparing them for upload.
  std::unique_ptr<AppLogUpload> app_log_upload_;

  // Collects log events and passes them to |this|.
  std::unique_ptr<ArcAppInstallEventLogger> logger_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_EVENT_LOG_MANAGER_H_
