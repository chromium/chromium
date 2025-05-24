// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_EVENT_BASED_LOGS_EVENT_BASED_LOG_UPLOADER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_EVENT_BASED_LOGS_EVENT_BASED_LOG_UPLOADER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/support_tool_handler.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/util/status.h"

namespace policy {

class EventBasedLogUploader {
 public:
  using UploadCallback = base::OnceCallback<void(reporting::Status)>;

  EventBasedLogUploader();

  EventBasedLogUploader(const EventBasedLogUploader&) = delete;
  EventBasedLogUploader& operator=(const EventBasedLogUploader&) = delete;

  virtual ~EventBasedLogUploader();

  // Uploads the logs from `data_collectors` to File Storage Server. Runs
  // `on_upload_completed` with upload status when upload is completed. Fails if
  // there's already an ongoing upload triggered.
  virtual void UploadEventBasedLogs(
      std::set<support_tool::DataCollectorType> data_collectors,
      ash::reporting::TriggerEventType event_type,
      std::optional<std::string> upload_id,
      UploadCallback on_upload_completed) = 0;
};

class EventBasedLogUploaderImpl : public EventBasedLogUploader {
 public:
  EventBasedLogUploaderImpl();

  ~EventBasedLogUploaderImpl() override;

  // Convenience function for testing. `/var/spool/support` path can't be
  // used in unit/browser tests so it should be replaced by a temporary
  // directory. The caller test is responsible for cleaning this path up after
  // testing is done and calling `SetTargetDirForTesting(nullptr)` to reset the
  // target dir.
  static void SetTargetDirForTesting(const base::FilePath* target_dir);

  void UploadEventBasedLogs(
      std::set<support_tool::DataCollectorType> data_collectors,
      ash::reporting::TriggerEventType event_type,
      std::optional<std::string> upload_id,
      UploadCallback on_upload_completed) override;

 private:
  const base::FilePath GetTargetDir();

  void OnDataCollected(const PIIMap& detected_pii,
                       std::set<SupportToolError> errors);

  void OnDataExported(base::FilePath exported_path,
                      std::set<SupportToolError> errors);

  void OnReportQueueCreated(
      base::FilePath exported_path,
      std::unique_ptr<reporting::ReportQueue> report_queue);

  void EnqueueEvent(base::FilePath exported_path);

  void OnEventEnqueued(reporting::Status status);

  SEQUENCE_CHECKER(sequence_checker_);
  ash::reporting::TriggerEventType event_type_;
  std::unique_ptr<SupportToolHandler> support_tool_handler_;
  std::optional<std::string> upload_id_;
  UploadCallback on_upload_completed_;
  std::unique_ptr<reporting::ReportQueue> report_queue_;
  base::WeakPtrFactory<EventBasedLogUploaderImpl> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_EVENT_BASED_LOGS_EVENT_BASED_LOG_UPLOADER_H_
