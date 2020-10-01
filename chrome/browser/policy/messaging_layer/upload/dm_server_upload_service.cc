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
#include "chrome/browser/policy/messaging_layer/upload/meet_device_telemetry_report_handler.h"
#include "chrome/browser/policy/messaging_layer/util/backoff_settings.h"
#include "chrome/browser/policy/messaging_layer/util/shared_vector.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "chrome/browser/policy/messaging_layer/util/task_runner_context.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"

namespace reporting {
namespace {

StatusOr<Profile*> GetPrimaryProfile() {
  if (!user_manager::UserManager::IsInitialized()) {
    return Status(error::FAILED_PRECONDITION, "User manager not initialized");
  }
  const auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user) {
    return Status(error::FAILED_PRECONDITION, "Primary user not found");
  }
  return chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
}

}  // namespace
}  // namespace reporting
#endif

namespace reporting {

using DmServerUploader = DmServerUploadService::DmServerUploader;
using ::policy::CloudPolicyClient;

DmServerUploadService::RecordHandler::RecordHandler(CloudPolicyClient* client)
    : client_(client) {}

DmServerUploadService::RecordHandler::~RecordHandler() = default;

DmServerUploader::DmServerUploader(
    std::unique_ptr<std::vector<EncryptedRecord>> records,
    scoped_refptr<SharedVector<std::unique_ptr<RecordHandler>>> handlers,
    CompletionCallback completion_cb,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : TaskRunnerContext<CompletionResponse>(std::move(completion_cb),
                                            sequenced_task_runner),
      encrypted_records_(std::move(records)),
      handlers_(handlers) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DmServerUploader::~DmServerUploader() = default;

void DmServerUploader::OnStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Early exit if we don't have any records.
  if (encrypted_records_->empty()) {
    Complete(
        Status(error::INVALID_ARGUMENT, "No records received for upload."));
    return;
  }
  handlers_->IsEmpty(base::BindOnce(
      &DmServerUploader::IsHandlerVectorEmptyCheck, base::Unretained(this)));
}

void DmServerUploader::IsHandlerVectorEmptyCheck(bool handlers_is_empty) {
  // Early Exit if we don't have any handlers.
  if (handlers_is_empty) {
    Complete(Status(error::INTERNAL, "No handlers available for upload."));
    return;
  }
  Schedule(&DmServerUploader::ProcessRecords, base::Unretained(this));
}

void DmServerUploader::ProcessRecords() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Status process_status;

  generation_id_ =
      encrypted_records_->front().sequencing_information().generation_id();

  // Will stop processing records on the first record that fails to pass.
  // Discarding the remaining records.
  for (const EncryptedRecord& encrypted_record : *encrypted_records_) {
    if (process_status = IsRecordValid(encrypted_record),
        !process_status.ok()) {
      break;
    }
  }

  if (record_infos_.empty()) {
    Complete(process_status);
    return;
  }

  HandleRecords();
}

void DmServerUploader::HandleRecords() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Status handle_status = Status::StatusOK();

  // TODO(chromium:1078512) Cannot verify client state on this thread. Find a
  // way to do that and restructure this loop to handle it.
  // Passing raw |record_infos_| pointer is safe since record_infos will not die
  // until after handlers_ does.
  auto execution_cb = base::BindRepeating(
      [](std::vector<RecordInfo>* record_infos,
         base::RepeatingCallback<void(const SequencingInformation&)>
             add_successfull_upload_cb,
         std::unique_ptr<RecordHandler>& record_handler) {
        for (auto record_info_it = record_infos->begin();
             record_info_it != record_infos->end();) {
          auto handle_status =
              record_handler->HandleRecord(record_info_it->record);

          // Record was successfully handled - mark it as such and move on to
          // the next record.
          if (handle_status.ok()) {
            add_successfull_upload_cb.Run(
                record_info_it->sequencing_information);

            // We don't need to handle this record again. Delete it.
            record_info_it = record_infos->erase(record_info_it);
            continue;
          }
          record_info_it++;
        }
      },
      &record_infos_,
      base::BindRepeating(&DmServerUploader::AddSuccessfulUpload,
                          base::Unretained(this)));

  auto predicate_cb = base::BindRepeating(
      [](std::vector<RecordInfo>* record_infos,
         const std::unique_ptr<RecordHandler>& record_handler) {
        return !record_infos->empty();
      },
      &record_infos_);

  handlers_->ExecuteOnEachElement(
      std::move(execution_cb),
      base::BindOnce(&DmServerUploader::OnRecordsHandled,
                     base::Unretained(this)),
      std::move(predicate_cb));
}

void DmServerUploader::OnRecordsHandled() {
  Status status = record_infos_.empty()
                      ? Status::StatusOK()
                      : Status(error::FAILED_PRECONDITION,
                               "Unable to connect to the server and upload "
                               "some or all records");
  Complete(status);
}

void DmServerUploader::Complete(Status status) {
  // Records were successfully uploaded - return the highest record processed.
  // Any unprocessed record will be attempted again later.
  if (highest_successful_sequence_.has_value()) {
    Schedule(&DmServerUploader::Response, base::Unretained(this),
             highest_successful_sequence_.value());
    return;
  }

  // No records were uploaded, return the status.
  Schedule(&DmServerUploader::Response, base::Unretained(this), status);
}

Status DmServerUploader::IsRecordValid(
    const EncryptedRecord& encrypted_record) {
  // Test to ensure all records are in the same generation.
  if (encrypted_record.sequencing_information().generation_id() !=
      generation_id_) {
    return Status(error::INVALID_ARGUMENT,
                  "Record does not have the correct generation");
  }

  // Parse the WrappedRecord from the EncryptedRecord.
  WrappedRecord wrapped_record;
  if (!wrapped_record.ParseFromString(
          encrypted_record.encrypted_wrapped_record())) {
    return Status(error::INVALID_ARGUMENT, "Unable to parse record");
  }

  record_infos_.emplace_back(RecordInfo{
      wrapped_record.record(), encrypted_record.sequencing_information()});
  return Status::StatusOK();
}

void DmServerUploader::AddSuccessfulUpload(
    const SequencingInformation& sequencing_information) {
  Schedule(&DmServerUploader::ProcessSuccessfulUploadAddition,
           base::Unretained(this), sequencing_information);
}

void DmServerUploader::ProcessSuccessfulUploadAddition(
    SequencingInformation sequencing_information) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If this is the first successful record - set highest to this record.
  if (!highest_successful_sequence_.has_value()) {
    highest_successful_sequence_ = sequencing_information;
    return;
  }

  // If messages were processed out of order log a warning. This shouldn't
  // happen, but there are no upload guarantees for DmServerUploadService so it
  // isn't a big deal.
  if (sequencing_information.sequencing_id() <
      highest_successful_sequence_->sequencing_id()) {
    LOG(WARNING) << "Records were processed out of order: "
                 << "Record " << sequencing_information.sequencing_id()
                 << " was processed after "
                 << highest_successful_sequence_->sequencing_id();
    return;
  }

  // If messages are duplicated log a warning. This shouldn't happen, but the
  // current system already has potential for duplicated events.
  if (sequencing_information.sequencing_id() ==
      highest_successful_sequence_->sequencing_id()) {
    LOG(WARNING) << "Record upload was duplicated: "
                 << "Record " << sequencing_information.sequencing_id()
                 << " was processed multiple times.";
    return;
  }

  highest_successful_sequence_ = sequencing_information;
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
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})),
      record_handlers_(SharedVector<std::unique_ptr<RecordHandler>>::Create()) {
}

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
      std::move(records), record_handlers_,
      base::BindOnce(&DmServerUploadService::UploadCompletion,
                     base::Unretained(this)),
      sequenced_task_runner_);
  return Status::StatusOK();
}

Status DmServerUploadService::InitRecordHandlers() {
  auto* client = GetClient();
  if (client == nullptr) {
    return Status(error::FAILED_PRECONDITION, "Client was null");
  }

  record_handlers_->PushBack(std::make_unique<AppInstallReportHandler>(client),
                             base::DoNothing());

  // Temporary wrapper for MeetDeviceTelementry
#ifdef OS_CHROMEOS
  ASSIGN_OR_RETURN(Profile* const primary_profile, GetPrimaryProfile());
  record_handlers_->PushBack(std::make_unique<MeetDeviceTelemetryReportHandler>(
                                 primary_profile, client),
                             base::DoNothing());
#endif  // OS_CHROMEOS

  return Status::StatusOK();
}

void DmServerUploadService::UploadCompletion(
    StatusOr<SequencingInformation> upload_result) const {
  if (!upload_result.ok()) {
    LOG(WARNING) << upload_result.status();
    return;
  }

  upload_cb_.Run(upload_result.ValueOrDie());
}

CloudPolicyClient* DmServerUploadService::GetClient() {
  return client_.get();
}

}  // namespace reporting
