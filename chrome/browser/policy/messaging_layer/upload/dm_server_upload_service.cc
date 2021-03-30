// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/dm_server_upload_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner.h"
#include "chrome/browser/policy/messaging_layer/upload/record_handler_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/task_runner_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace reporting {

using DmServerUploader = DmServerUploadService::DmServerUploader;
using ::policy::CloudPolicyClient;

namespace {
// Thread-safe helper callback class: calls callback once |Decrement|
// is invoked |count| times and then self-destructs. |Increment| can be
// called at any time, provided that the counter has not dropped to 0 yet.
class CollectorCallback {
 public:
  CollectorCallback(size_t count, base::OnceClosure done_cb)
      : count_(count), done_cb_(std::move(done_cb)) {
    DCHECK_GT(count, 0u);
  }
  CollectorCallback(CollectorCallback& other) = delete;
  CollectorCallback& operator=(CollectorCallback& other) = delete;
  ~CollectorCallback() { std::move(done_cb_).Run(); }

  void Decrement() {
    size_t old_count = count_.fetch_sub(1);
    DCHECK_GT(old_count, 0u);
    if (old_count > 1) {
      return;
    }
    delete this;
  }

  void Increment() {
    size_t old_count = count_.fetch_add(1);
    DCHECK_GT(old_count, 0u) << "Cannot increment if already 0";
  }

 private:
  std::atomic<size_t> count_;
  base::OnceClosure done_cb_;
};
}  // namespace

DmServerUploadService::RecordHandler::RecordHandler(
    policy::CloudPolicyClient* client)
    : client_(client) {}

DmServerUploader::DmServerUploader(
    bool need_encryption_key,
    std::unique_ptr<std::vector<EncryptedRecord>> records,
    RecordHandler* handler,
    CompletionCallback completion_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : TaskRunnerContext<CompletionResponse>(std::move(completion_cb),
                                            sequenced_task_runner),
      need_encryption_key_(need_encryption_key),
      encrypted_records_(std::move(records)),
      encryption_key_attached_cb_(encryption_key_attached_cb),
      handler_(handler) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DmServerUploader::~DmServerUploader() = default;

void DmServerUploader::OnStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (handler_ == nullptr) {
    Complete(Status(error::INVALID_ARGUMENT, "handler was null"));
    return;
  }
  // Early exit if we don't have any records and do not need encryption key.
  if (encrypted_records_->empty() && !need_encryption_key_) {
    Complete(
        Status(error::INVALID_ARGUMENT, "No records received for upload."));
    return;
  }

  if (!encrypted_records_->empty()) {
    ProcessRecords();
  }

  HandleRecords();
}

void DmServerUploader::ProcessRecords() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Status process_status;

  const int64_t expected_generation_id =
      encrypted_records_->front().sequencing_information().generation_id();
  int64_t expected_sequencing_id =
      encrypted_records_->front().sequencing_information().sequencing_id();

  // Will stop processing records on the first record that fails to pass.
  size_t records_added = 0;
  for (const EncryptedRecord& encrypted_record : *encrypted_records_) {
    process_status = IsRecordValid(encrypted_record, expected_generation_id,
                                   expected_sequencing_id);
    if (!process_status.ok()) {
      LOG(ERROR) << "Record was received out of order from the StorageModule";
      break;
    }
    records_added += 1;
    expected_sequencing_id++;
  }

  if (records_added == 0) {
    Complete(process_status);
    return;
  }

  // Discarding the remaining records.
  encrypted_records_->resize(records_added);
}

void DmServerUploader::HandleRecords() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  handler_->HandleRecords(
      need_encryption_key_, std::move(encrypted_records_),
      base::BindOnce(&DmServerUploader::Complete, base::Unretained(this)),
      encryption_key_attached_cb_);
}

void DmServerUploader::Complete(CompletionResponse completion_response) {
  Schedule(&DmServerUploader::Response, base::Unretained(this),
           completion_response);
}

Status DmServerUploader::IsRecordValid(
    const EncryptedRecord& encrypted_record,
    const int64_t expected_generation_id,
    const int64_t expected_sequencing_id) const {
  // Test to ensure all records are in the same generation.
  if (encrypted_record.sequencing_information().generation_id() !=
      expected_generation_id) {
    return Status(error::INVALID_ARGUMENT,
                  "Record does not have the correct generation");
  }

  if (encrypted_record.sequencing_information().sequencing_id() !=
      expected_sequencing_id) {
    return Status(error::INVALID_ARGUMENT, "Out of order sequencing_id");
  }

  return Status::StatusOK();
}

void DmServerUploadService::Create(
    policy::CloudPolicyClient* client,
    ReportSuccessfulUploadCallback report_upload_success_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb,
    base::OnceCallback<void(StatusOr<std::unique_ptr<DmServerUploadService>>)>
        created_cb) {
  if (client == nullptr) {
    std::move(created_cb)
        .Run(Status(error::INVALID_ARGUMENT, "client may not be nullptr."));
    return;
  }

  auto uploader = base::WrapUnique(new DmServerUploadService(
      std::move(client), report_upload_success_cb, encryption_key_attached_cb));
  InitRecordHandler(std::move(uploader), std::move(created_cb));
}

DmServerUploadService::DmServerUploadService(
    policy::CloudPolicyClient* client,
    ReportSuccessfulUploadCallback upload_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb)
    : client_(std::move(client)),
      upload_cb_(upload_cb),
      encryption_key_attached_cb_(encryption_key_attached_cb),
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})) {}

DmServerUploadService::~DmServerUploadService() = default;

Status DmServerUploadService::EnqueueUpload(
    bool need_encryption_key,
    std::unique_ptr<std::vector<EncryptedRecord>> records) {
  Start<DmServerUploader>(
      need_encryption_key, std::move(records), handler_.get(),
      base::BindOnce(&DmServerUploadService::UploadCompletion,
                     base::Unretained(this)),
      encryption_key_attached_cb_, sequenced_task_runner_);
  return Status::StatusOK();
}

void DmServerUploadService::InitRecordHandler(
    std::unique_ptr<DmServerUploadService> uploader,
    base::OnceCallback<void(StatusOr<std::unique_ptr<DmServerUploadService>>)>
        created_cb) {
  auto* client = uploader->GetClient();
  if (client == nullptr) {
    std::move(created_cb)
        .Run(Status(error::FAILED_PRECONDITION, "Client was null"));
    return;
  }

  uploader->handler_ = std::make_unique<RecordHandlerImpl>(client);
  std::move(created_cb).Run(std::move(uploader));
}

void DmServerUploadService::UploadCompletion(
    CompletionResponse upload_result) const {
  if (!upload_result.ok()) {
    LOG(WARNING) << upload_result.status();
    return;
  }

  upload_cb_.Run(upload_result.ValueOrDie().sequencing_information,
                 upload_result.ValueOrDie().force_confirm);
}

CloudPolicyClient* DmServerUploadService::GetClient() {
  return client_;
}

}  // namespace reporting
