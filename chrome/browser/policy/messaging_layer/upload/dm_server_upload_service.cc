// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/dm_server_upload_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/policy/messaging_layer/upload/record_handler_impl.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/task_runner_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace reporting {

using DmServerUploader = DmServerUploadService::DmServerUploader;

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

DmServerUploadService::RecordHandler::RecordHandler() = default;

DmServerUploader::DmServerUploader(
    bool need_encryption_key,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    RecordHandler* handler,
    ReportSuccessfulUploadCallback report_success_upload_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb,
    CompletionCallback completion_cb,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : TaskRunnerContext<CompletionResponse>(std::move(completion_cb),
                                            sequenced_task_runner),
      need_encryption_key_(need_encryption_key),
      encrypted_records_(std::move(records)),
      scoped_reservation_(std::move(scoped_reservation)),
      report_success_upload_cb_(std::move(report_success_upload_cb)),
      encryption_key_attached_cb_(std::move(encryption_key_attached_cb)),
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
  if (encrypted_records_.empty() && !need_encryption_key_) {
    Complete(
        Status(error::INVALID_ARGUMENT, "No records received for upload."));
    return;
  }

  if (!encrypted_records_.empty()) {
    ProcessRecords();
  }

  HandleRecords();
}

void DmServerUploader::ProcessRecords() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Status process_status;

  const int64_t expected_generation_id =
      encrypted_records_.front().sequence_information().generation_id();
  int64_t expected_sequencing_id =
      encrypted_records_.front().sequence_information().sequencing_id();

  // Will stop processing records on the first record that fails to pass.
  size_t records_added = 0;
  for (const auto& encrypted_record : encrypted_records_) {
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
  encrypted_records_.resize(records_added);
}

void DmServerUploader::HandleRecords() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  handler_->HandleRecords(
      need_encryption_key_, std::move(encrypted_records_),
      std::move(scoped_reservation_),
      base::BindOnce(&DmServerUploader::Complete, base::Unretained(this)),
      std::move(encryption_key_attached_cb_));
}

void DmServerUploader::Finalize(CompletionResponse upload_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (upload_result.ok()) {
    std::move(report_success_upload_cb_)
        .Run(upload_result.ValueOrDie().sequence_information,
             upload_result.ValueOrDie().force_confirm);
  } else {
    // Log any error except NOT_FOUND, which is a transient state for managed
    // device, and an uninteresting one for unmanaged ones.
    LOG_IF(WARNING, upload_result.status().code() != error::NOT_FOUND)
        << upload_result.status();
  }
  Response(upload_result);
}

void DmServerUploader::Complete(CompletionResponse upload_result) {
  Schedule(&DmServerUploader::Finalize, base::Unretained(this), upload_result);
}

Status DmServerUploader::IsRecordValid(
    const EncryptedRecord& encrypted_record,
    const int64_t expected_generation_id,
    const int64_t expected_sequencing_id) const {
  // Test to ensure all records are in the same generation.
  if (encrypted_record.sequence_information().generation_id() !=
      expected_generation_id) {
    return Status(error::INVALID_ARGUMENT,
                  "Record does not have the correct generation");
  }

  if (encrypted_record.sequence_information().sequencing_id() !=
      expected_sequencing_id) {
    return Status(error::INVALID_ARGUMENT, "Out of order sequencing_id");
  }

  return Status::StatusOK();
}

void DmServerUploadService::Create(
    base::OnceCallback<void(StatusOr<std::unique_ptr<DmServerUploadService>>)>
        created_cb) {
  auto uploader = base::WrapUnique(new DmServerUploadService());
  InitRecordHandler(std::move(uploader), std::move(created_cb));
}

DmServerUploadService::DmServerUploadService()
    : sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})) {}

DmServerUploadService::~DmServerUploadService() = default;

Status DmServerUploadService::EnqueueUpload(
    bool need_encryption_key,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    ReportSuccessfulUploadCallback report_upload_success_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb) {
  Start<DmServerUploader>(need_encryption_key, std::move(records),
                          std::move(scoped_reservation), handler_.get(),
                          std::move(report_upload_success_cb),
                          std::move(encryption_key_attached_cb),
                          base::DoNothing(), sequenced_task_runner_);
  return Status::StatusOK();
}

void DmServerUploadService::InitRecordHandler(
    std::unique_ptr<DmServerUploadService> uploader,
    base::OnceCallback<void(StatusOr<std::unique_ptr<DmServerUploadService>>)>
        created_cb) {
  uploader->handler_ = std::make_unique<RecordHandlerImpl>();
  std::move(created_cb).Run(std::move(uploader));
}
}  // namespace reporting
