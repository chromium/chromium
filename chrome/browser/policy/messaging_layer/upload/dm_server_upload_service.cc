// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/dm_server_upload_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "chrome/browser/policy/messaging_layer/upload/app_install_report_handler.h"
#include "chrome/browser/policy/messaging_layer/util/backoff_settings.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "chrome/browser/policy/messaging_layer/util/task_runner_context.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#endif

namespace reporting {

using DmServerUploader = DmServerUploadService::DmServerUploader;
using ::policy::CloudPolicyClient;

DmServerUploadService::RecordHandler::RecordHandler(CloudPolicyClient* client)
    : client_(client) {}

DmServerUploadService::RecordHandler::~RecordHandler() = default;

DmServerUploader::DmServerUploader(
    std::unique_ptr<std::vector<EncryptedRecord>> records,
    std::vector<std::unique_ptr<RecordHandler>>* handlers,
    CompletionCallback completion_cb,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
    base::TimeDelta max_delay)
    : TaskRunnerContext<CompletionResponse>(std::move(completion_cb),
                                            sequenced_task_runner),
      encrypted_records_(std::move(records)),
      handlers_(handlers),
      max_delay_(max_delay),
      backoff_entry_(GetBackoffEntry()) {}

DmServerUploader::~DmServerUploader() = default;

void DmServerUploader::OnStart() {
  // Early exit if we don't have any handlers or records.
  if (handlers_->empty() || encrypted_records_->empty()) {
    Complete(Status::StatusOK());
    return;
  }
  ProcessRecords();
}

void DmServerUploader::ProcessRecords() {
  Status process_status = Status::StatusOK();

  // Drops records that it cannot parse.
  for (const EncryptedRecord& encrypted_record : *encrypted_records_) {
    if (encrypted_record.has_encryption_info()) {
      process_status =
          Status(error::UNIMPLEMENTED, "Encryption is not supported yet!");
      break;
    }

    WrappedRecord wrapped_record;
    if (!wrapped_record.ParseFromString(
            encrypted_record.encrypted_wrapped_record())) {
      process_status =
          Status(error::INVALID_ARGUMENT, "Unable to parse record");
      break;
    }

    record_infos_.emplace_back(RecordInfo{
        wrapped_record.record(), encrypted_record.sequencing_information()});
  }

  if (record_infos_.empty()) {
    Complete(process_status);
    return;
  }

  HandleRecords();
}

void DmServerUploader::HandleRecords() {
  Status handle_status = Status::StatusOK();

  // Records are handled iteratively since the |CloudPolicyClient| cannot handle
  // multiple requests at one time. Any records that fail to send for any reason
  // are simply dropped, this is similar to the current functionality (i.e.
  // |ArcAppInstallEventLogUploader|).
  // TODO(chromium:1078512) Consider creating a whitelist/blacklist for retry
  // and continue.
  // TODO(chromium:1078512) Cannot verify client state on this thread. Find a
  // way to do that and restructure this loop to handle it.
  for (auto record_info_it = record_infos_.begin();
       record_info_it != record_infos_.end();) {
    for (auto& handler : *handlers_) {
      handle_status = handler->HandleRecord(record_info_it->record);

      // Record was successfully handled - move to the next record.
      if (handle_status.ok()) {
        AddSuccessfulUpload(record_info_it->sequencing_information);

        // We don't need to handle this record again. Delete it and move on.
        record_info_it = record_infos_.erase(record_info_it);
        break;
      }

      // This handler doesn't know how to handle this record - move to the next
      // handler.
      if (handle_status.error_code() == error::INVALID_ARGUMENT) {
        continue;
      }

      // The server is unavailable. Try again later if we haven't tried for too
      // long. Current total delay is 127 seconds or ~2 minutes.
      if (handle_status.error_code() == error::UNAVAILABLE) {
        base::TimeDelta delay = GetNextDelay();
        if (delay >= max_delay_) {
          Complete(Status(error::DEADLINE_EXCEEDED,
                          "Unable to upload all records in provided deadline"));
          return;
        }

        ScheduleAfter(delay, &DmServerUploader::HandleRecords,
                      base::Unretained(this));
        return;
      }
    }

    // Some unhandled error has occurred. Cancel further upload.
    if (!handle_status.ok()) {
      Complete(handle_status);
      return;
    }

    ResetDelay();
  }

  Complete(Status::StatusOK());
}

void DmServerUploader::Complete(Status status) {
  std::vector<SequencingInformation> successful_upload_list;
  for (auto it : successful_uploads_) {
    successful_upload_list.push_back(it.second);
  }

  // No records were uploaded - Return the error.
  if (successful_upload_list.empty() && !status.ok()) {
    Schedule(&DmServerUploader::Response, base::Unretained(this), status);
    return;
  }

  // Records were successfully uploaded - return the list.
  Schedule(&DmServerUploader::Response, base::Unretained(this),
           successful_upload_list);
}

void DmServerUploader::AddSuccessfulUpload(
    SequencingInformation sequencing_information) {
  auto it = successful_uploads_.find(sequencing_information.generation_id());
  if (it == successful_uploads_.end()) {
    successful_uploads_.insert(
        {sequencing_information.generation_id(), sequencing_information});
    return;
  }

  if (it->second.sequencing_id() < sequencing_information.sequencing_id()) {
    it->second = sequencing_information;
    return;
  }

  // Messages were processed out of order. This shouldn't happen, but there are
  // no upload guarantees for DmServerUploadService so it isn't a big deal.
  LOG(WARNING) << "Records were processed out of order: "
               << "Record " << sequencing_information.sequencing_id()
               << " was processed after " << it->second.sequencing_id();
}

base::TimeDelta DmServerUploader::GetNextDelay() {
  base::TimeDelta delay = backoff_entry_->GetTimeUntilRelease();
  backoff_entry_->InformOfRequest(/*succeeded=*/false);
  return delay;
}

void DmServerUploader::ResetDelay() {
  backoff_entry_->InformOfRequest(/*succeeded=*/true);
}

StatusOr<std::unique_ptr<DmServerUploadService>> DmServerUploadService::Create(
    std::unique_ptr<policy::CloudPolicyClient> client,
    ReportSuccessfulUploadCallback upload_cb) {
  if (client == nullptr) {
    return Status(error::INVALID_ARGUMENT, "client may not be nullptr.");
  }
  auto uploader =
      base::WrapUnique(new DmServerUploadService(std::move(client), upload_cb));

  RETURN_IF_ERROR(uploader->InitRecordHandlers());

  return uploader;
}

DmServerUploadService::DmServerUploadService(
    std::unique_ptr<policy::CloudPolicyClient> client,
    ReportSuccessfulUploadCallback upload_cb)
    : client_(std::move(client)),
      upload_cb_(upload_cb),
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})) {}

DmServerUploadService::~DmServerUploadService() {
  if (client_) {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            [](std::unique_ptr<policy::CloudPolicyClient> cloud_policy_client) {
              cloud_policy_client.reset();
            },
            std::move(client_)));
  }
}

Status DmServerUploadService::EnqueueUpload(
    std::unique_ptr<std::vector<EncryptedRecord>> records) {
  Start<DmServerUploader>(
      std::move(records), &record_handlers_,
      base::BindOnce(&DmServerUploadService::UploadCompletion,
                     base::Unretained(this)),
      sequenced_task_runner_, base::TimeDelta::FromMinutes(1));
  return Status::StatusOK();
}

Status DmServerUploadService::InitRecordHandlers() {
  auto* client = GetClient();
  if (client == nullptr) {
    return Status(error::FAILED_PRECONDITION, "Client was null");
  }

  record_handlers_.push_back(std::make_unique<AppInstallReportHandler>(client));

  return Status::StatusOK();
}

void DmServerUploadService::UploadCompletion(
    StatusOr<std::vector<SequencingInformation>> upload_result) const {
  if (!upload_result.ok()) {
    LOG(WARNING) << upload_result.status();
    return;
  }

  for (const auto& info : upload_result.ValueOrDie()) {
    upload_cb_.Run(info);
  }
}

CloudPolicyClient* DmServerUploadService::GetClient() {
  return client_.get();
}

}  // namespace reporting
