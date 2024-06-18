// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_EVENT_BASED_LOGS_EVENT_BASED_LOG_UPLOADER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_EVENT_BASED_LOGS_EVENT_BASED_LOG_UPLOADER_H_

#include <optional>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "components/reporting/util/status.h"

namespace policy {

class EventBasedLogUploader {
 public:
  using UploadCallback = base::OnceCallback<void(reporting::Status)>;

  EventBasedLogUploader();

  EventBasedLogUploader(const EventBasedLogUploader&) = delete;
  EventBasedLogUploader& operator=(const EventBasedLogUploader&) = delete;

  virtual ~EventBasedLogUploader();

  // Generated a GUID to identify the log upload. This upload ID can be used in
  // server to connect the events with the log files. It'll use
  // base::uuid::GenerateRandomV4() to create this ID.
  static std::string GenerateUploadId();

  // Uploads the logs from `data_collectors` to File Storage Server. Runs
  // `on_upload_completed` with upload status when upload is completed.
  virtual void UploadEventBasedLogs(
      std::set<support_tool::DataCollectorType> data_collectors,
      ash::reporting::TriggerEventType event_type,
      std::optional<std::string> upload_id,
      UploadCallback on_upload_completed) = 0;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<EventBasedLogUploader> weak_ptr_factory_{this};
};

class EventBasedLogUploaderImpl : public EventBasedLogUploader {
 public:
  EventBasedLogUploaderImpl();

  ~EventBasedLogUploaderImpl() override;

  // TODO: b/330675989 - Note that this function doesn't do anything yet. It's
  // just a placeholder. Add real log upload logic when the blocker is resolved
  // and File Storage Server is ready to accept event based log uploads.
  void UploadEventBasedLogs(
      std::set<support_tool::DataCollectorType> data_collectors,
      ash::reporting::TriggerEventType event_type,
      std::optional<std::string> upload_id,
      UploadCallback on_upload_completed) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<EventBasedLogUploader> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_EVENT_BASED_LOGS_EVENT_BASED_LOG_UPLOADER_H_
