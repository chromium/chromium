// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/upload_provider.h"

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "chrome/browser/policy/messaging_layer/util/get_cloud_policy_client.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/backoff_settings.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status.pb.h"
#include "components/reporting/util/statusor.h"
#include "net/base/backoff_entry.h"

namespace reporting {

// EncryptedReportingUploadProvider refcounted helper class.
class EncryptedReportingUploadProvider::UploadHelper
    : public base::RefCountedDeleteOnSequence<UploadHelper> {
 public:
  UploadHelper(
      UploadClient::ReportSuccessfulUploadCallback report_successful_upload_cb,
      UploadClient::EncryptionKeyAttachedCallback encryption_key_attached_cb,
      GetCloudPolicyClientCallback build_cloud_policy_client_cb,
      UploadClientBuilderCb upload_client_builder_cb,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);
  UploadHelper(const UploadHelper& other) = delete;
  UploadHelper& operator=(const UploadHelper& other) = delete;

  // Requests new cloud policy client (can be invoked on any thread)
  void PostNewCloudPolicyClientRequest();

  // Uploads encrypted records (can be invoked on any thread).
  void EnqueueUpload(bool need_encryption_key,
                     std::unique_ptr<std::vector<EncryptedRecord>> records,
                     base::OnceCallback<void(Status)> enqueued_cb) const;

 private:
  friend class base::RefCountedDeleteOnSequence<UploadHelper>;
  friend class base::DeleteHelper<UploadHelper>;

  // Refcounted object can only have private or protected destructor.
  ~UploadHelper();

  // Stages of cloud policy client and upload client creation,
  // scheduled on a sequenced task runner.
  void TryNewCloudPolicyClientRequest();
  void OnCloudPolicyClientResult(
      StatusOr<policy::CloudPolicyClient*> client_result);
  void UpdateUploadClient(std::unique_ptr<UploadClient> client);
  void OnUploadClientResult(
      StatusOr<std::unique_ptr<UploadClient>> client_result);

  // Uploads encrypted records on sequenced task runner (and thus capable of
  // detecting whether upload client is ready or not). If not ready,
  // it will wait and then upload.
  void EnqueueUploadInternal(
      bool need_encryption_key,
      std::unique_ptr<std::vector<EncryptedRecord>> records,
      base::OnceCallback<void(Status)> enqueued_cb);

  // Sequence task runner and checker used during
  // |PostNewCloudPolicyClientRequest| processing.
  // It is also used to protect |upload_client_|.
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(sequenced_task_checker_);

  // Callbacks for successful upload and key delivery.
  const UploadClient::ReportSuccessfulUploadCallback
      report_successful_upload_cb_;
  const UploadClient::EncryptionKeyAttachedCallback encryption_key_attached_cb_;

  // Callbacks for cloud policy and upload client creation.
  const GetCloudPolicyClientCallback build_cloud_policy_client_cb_;
  UploadClientBuilderCb upload_client_builder_cb_;

  // Tracking of asynchronous stages.
  std::atomic<bool> upload_client_request_in_progress_{false};
  const std::unique_ptr<::net::BackoffEntry> backoff_entry_;

  // Stored data from upload requests before upload client is ready.
  // Note that vectors of records submitted for upload are mapped by
  // generation id (which is the same for all records being uploaded at once
  // since they originate from the same priority queue). As a result, if
  // the caller attempts to upload the same records multiple times (e.g.
  // because it did not yet get a confirmation from server), we will only
  // hold to one set of records.
  // Guarded by sequenced_task_runner_.
  base::flat_map</*generation_id*/ int64_t,
                 std::unique_ptr<std::vector<EncryptedRecord>>>
      stored_records_;
  bool stored_need_encryption_key_{false};

  // Upload client (protected by sequenced task runner). Once set, is used
  // repeatedly.
  std::unique_ptr<UploadClient> upload_client_;

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<UploadHelper> weak_ptr_factory_{this};
};

EncryptedReportingUploadProvider::UploadHelper::UploadHelper(
    UploadClient::ReportSuccessfulUploadCallback report_successful_upload_cb,
    UploadClient::EncryptionKeyAttachedCallback encryption_key_attached_cb,
    GetCloudPolicyClientCallback build_cloud_policy_client_cb,
    UploadClientBuilderCb upload_client_builder_cb,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : base::RefCountedDeleteOnSequence<UploadHelper>(sequenced_task_runner),
      sequenced_task_runner_(std::move(sequenced_task_runner)),
      report_successful_upload_cb_(report_successful_upload_cb),
      encryption_key_attached_cb_(encryption_key_attached_cb),
      build_cloud_policy_client_cb_(build_cloud_policy_client_cb),
      upload_client_builder_cb_(std::move(upload_client_builder_cb)),
      backoff_entry_(GetBackoffEntry()) {
  DETACH_FROM_SEQUENCE(sequenced_task_checker_);
}

EncryptedReportingUploadProvider::UploadHelper::~UploadHelper() {
  // Weak pointer factory must be destructed on the sequence.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequenced_task_checker_);
}

void EncryptedReportingUploadProvider::UploadHelper::
    PostNewCloudPolicyClientRequest() {
  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UploadHelper::TryNewCloudPolicyClientRequest,
                                weak_ptr_factory_.GetWeakPtr()));
}

void EncryptedReportingUploadProvider::UploadHelper::
    TryNewCloudPolicyClientRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequenced_task_checker_);
  if (upload_client_ != nullptr) {
    return;
  }
  if (upload_client_request_in_progress_) {
    return;
  }
  upload_client_request_in_progress_ = true;

  sequenced_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<EncryptedReportingUploadProvider::UploadHelper>
                 self) {
            if (!self) {
              return;  // Provider expired
            }
            self->build_cloud_policy_client_cb_.Run(base::BindPostTask(
                self->sequenced_task_runner_,
                base::BindOnce(&UploadHelper::OnCloudPolicyClientResult,
                               self->weak_ptr_factory_.GetWeakPtr())));
          },
          weak_ptr_factory_.GetWeakPtr()),
      backoff_entry_->GetTimeUntilRelease());

  // Increase backoff_entry_ for next request.
  backoff_entry_->InformOfRequest(/*succeeded=*/false);
}

void EncryptedReportingUploadProvider::UploadHelper::OnCloudPolicyClientResult(
    StatusOr<policy::CloudPolicyClient*> client_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequenced_task_checker_);
  if (!client_result.ok()) {
    upload_client_request_in_progress_ = false;
    TryNewCloudPolicyClientRequest();
    return;
  }
  std::move(upload_client_builder_cb_)
      .Run(client_result.ValueOrDie(),
           base::BindPostTask(
               sequenced_task_runner_,
               base::BindRepeating(&UploadHelper::OnUploadClientResult,
                                   weak_ptr_factory_.GetWeakPtr())));
}

void EncryptedReportingUploadProvider::UploadHelper::OnUploadClientResult(
    StatusOr<std::unique_ptr<UploadClient>> client_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequenced_task_checker_);
  if (!client_result.ok()) {
    upload_client_request_in_progress_ = false;
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&UploadHelper::PostNewCloudPolicyClientRequest,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UploadHelper::UpdateUploadClient,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(client_result.ValueOrDie())));
}

void EncryptedReportingUploadProvider::UploadHelper::UpdateUploadClient(
    std::unique_ptr<UploadClient> upload_client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequenced_task_checker_);
  upload_client_ = std::move(upload_client);
  backoff_entry_->InformOfRequest(/*succeeded=*/true);
  upload_client_request_in_progress_ = false;

  // Upload client is ready, upload all previously stored requests (if any).
  auto records = std::make_unique<std::vector<EncryptedRecord>>();
  while (!stored_records_.empty() || stored_need_encryption_key_) {
    if (!stored_records_.empty()) {
      records = std::move(stored_records_.begin()->second);
      stored_records_.erase(stored_records_.begin());
    }
    const bool need_encryption_key =
        std::exchange(stored_need_encryption_key_, false);
    const auto result = upload_client_->EnqueueUpload(
        need_encryption_key, std::move(records), report_successful_upload_cb_,
        encryption_key_attached_cb_);
    if (!result.ok()) {
      LOG(ERROR) << "Upload failed, error=" << result;
    }
  }
}

void EncryptedReportingUploadProvider::UploadHelper::EnqueueUpload(
    bool need_encryption_key,
    std::unique_ptr<std::vector<EncryptedRecord>> records,
    base::OnceCallback<void(Status)> enqueued_cb) const {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UploadHelper::EnqueueUploadInternal,
                     weak_ptr_factory_.GetWeakPtr(), need_encryption_key,
                     std::move(records), std::move(enqueued_cb)));
}

void EncryptedReportingUploadProvider::UploadHelper::EnqueueUploadInternal(
    bool need_encryption_key,
    std::unique_ptr<std::vector<EncryptedRecord>> records,
    base::OnceCallback<void(Status)> enqueued_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequenced_task_checker_);
  if (upload_client_ == nullptr) {
    stored_need_encryption_key_ |= need_encryption_key;
    int64_t generation_id = 0;
    if (records && !records->empty() &&
        records->begin()->has_sequence_information() &&
        records->begin()->sequence_information().has_generation_id()) {
      generation_id = records->begin()->sequence_information().generation_id();
    }
    stored_records_.emplace(generation_id, std::move(records));
    // Report success even though the upload has not been executed.
    // Actual success is reported through two permanent repeating callbacks.
    std::move(enqueued_cb).Run(Status::StatusOK());
    return;
  }
  std::move(enqueued_cb)
      .Run(upload_client_->EnqueueUpload(
          need_encryption_key, std::move(records), report_successful_upload_cb_,
          encryption_key_attached_cb_));
}

// EncryptedReportingUploadProvider implementation.

EncryptedReportingUploadProvider::EncryptedReportingUploadProvider(
    UploadClient::ReportSuccessfulUploadCallback report_successful_upload_cb,
    UploadClient::EncryptionKeyAttachedCallback encryption_key_attached_cb,
    GetCloudPolicyClientCallback build_cloud_policy_client_cb,
    UploadClientBuilderCb upload_client_builder_cb)
    : helper_(base::MakeRefCounted<UploadHelper>(
          report_successful_upload_cb,
          encryption_key_attached_cb,
          build_cloud_policy_client_cb,
          std::move(upload_client_builder_cb),
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::TaskPriority::BEST_EFFORT, base::MayBlock()}))) {
  helper_->PostNewCloudPolicyClientRequest();
}

EncryptedReportingUploadProvider::~EncryptedReportingUploadProvider() = default;

void EncryptedReportingUploadProvider::RequestUploadEncryptedRecords(
    bool need_encryption_key,
    std::unique_ptr<std::vector<EncryptedRecord>> records,
    base::OnceCallback<void(Status)> result_cb) {
  DCHECK(helper_);
  helper_->EnqueueUpload(need_encryption_key, std::move(records),
                         std::move(result_cb));
}

// static
EncryptedReportingUploadProvider::UploadClientBuilderCb
EncryptedReportingUploadProvider::GetUploadClientBuilder() {
  return base::BindOnce(&UploadClient::Create);
}
}  // namespace reporting
