// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/upload_provider.h"

#include <memory>
#include <utility>

#include "base/bind_post_task.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "chrome/browser/policy/messaging_layer/util/get_cloud_policy_client.h"
#include "components/reporting/proto/record.pb.h"
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
      GetCloudPolicyClientCallback build_cloud_policy_client_cb,
      UploadClientBuilderCb upload_client_builder_cb,
      UploadClient::ReportSuccessfulUploadCallback report_successful_upload_cb,
      UploadClient::EncryptionKeyAttachedCallback encryption_key_attached_cb,
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

  struct StoredUploadRequest {
    bool need_encryption_key;
    std::unique_ptr<std::vector<EncryptedRecord>> records;
    base::OnceCallback<void(Status)> enqueued_cb;
  };

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
  void OnSuccessfulUpload(SequencingInformation sequencing_information,
                          bool force_confirm);
  void OnEncryptionKeyAttached(SignedEncryptionInfo signed_encryption_info);

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

  // Callbacks for cloud policy and upload client creation.
  const GetCloudPolicyClientCallback build_cloud_policy_client_cb_;
  const UploadClientBuilderCb upload_client_builder_cb_;
  const UploadClient::ReportSuccessfulUploadCallback
      report_successful_upload_cb_;
  const UploadClient::EncryptionKeyAttachedCallback encryption_key_attached_cb_;

  // Tracking of asynchronous stages.
  std::atomic<bool> upload_client_request_in_progress_{false};
  const std::unique_ptr<::net::BackoffEntry> backoff_entry_;

  // Stored upload requests before upload client is ready.
  std::vector<StoredUploadRequest> stored_uploads_;

  // Upload client (protected by sequenced task runner). Once set, is used
  // repeatedly.
  std::unique_ptr<UploadClient> upload_client_;

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<UploadHelper> weak_ptr_factory_{this};
};

EncryptedReportingUploadProvider::UploadHelper::UploadHelper(
    GetCloudPolicyClientCallback build_cloud_policy_client_cb,
    UploadClientBuilderCb upload_client_builder_cb,
    UploadClient::ReportSuccessfulUploadCallback report_successful_upload_cb,
    UploadClient::EncryptionKeyAttachedCallback encryption_key_attached_cb,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : base::RefCountedDeleteOnSequence<UploadHelper>(sequenced_task_runner),
      sequenced_task_runner_(std::move(sequenced_task_runner)),
      build_cloud_policy_client_cb_(build_cloud_policy_client_cb),
      upload_client_builder_cb_(upload_client_builder_cb),
      report_successful_upload_cb_(report_successful_upload_cb),
      encryption_key_attached_cb_(encryption_key_attached_cb),
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
  upload_client_builder_cb_.Run(
      client_result.ValueOrDie(),
      base::BindPostTask(sequenced_task_runner_,
                         base::BindRepeating(&UploadHelper::OnSuccessfulUpload,
                                             weak_ptr_factory_.GetWeakPtr())),
      base::BindPostTask(
          sequenced_task_runner_,
          base::BindRepeating(&UploadHelper::OnEncryptionKeyAttached,
                              weak_ptr_factory_.GetWeakPtr())),
      base::BindPostTask(sequenced_task_runner_,
                         base::BindOnce(&UploadHelper::OnUploadClientResult,
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

void EncryptedReportingUploadProvider::UploadHelper::OnSuccessfulUpload(
    SequencingInformation sequencing_information,
    bool force_confirm) {
  report_successful_upload_cb_.Run(std::move(sequencing_information),
                                   force_confirm);
}

void EncryptedReportingUploadProvider::UploadHelper::OnEncryptionKeyAttached(
    SignedEncryptionInfo signed_encryption_info) {
  encryption_key_attached_cb_.Run(std::move(signed_encryption_info));
}

void EncryptedReportingUploadProvider::UploadHelper::UpdateUploadClient(
    std::unique_ptr<UploadClient> upload_client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequenced_task_checker_);
  upload_client_ = std::move(upload_client);
  backoff_entry_->InformOfRequest(/*succeeded=*/true);
  upload_client_request_in_progress_ = false;
  // Upload client is ready, upload all previously stored requests (if any).
  for (auto& stored_upload : stored_uploads_) {
    std::move(std::move(stored_upload.enqueued_cb))
        .Run(upload_client_->EnqueueUpload(stored_upload.need_encryption_key,
                                           std::move(stored_upload.records)));
  }
  stored_uploads_.clear();
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
    stored_uploads_.emplace_back(
        StoredUploadRequest{.need_encryption_key = need_encryption_key,
                            .records = std::move(records),
                            .enqueued_cb = std::move(enqueued_cb)});
    return;
  }
  std::move(enqueued_cb)
      .Run(upload_client_->EnqueueUpload(need_encryption_key,
                                         std::move(records)));
}

// EncryptedReportingUploadProvider implementation.

EncryptedReportingUploadProvider::EncryptedReportingUploadProvider(
    UploadClient::ReportSuccessfulUploadCallback report_successful_upload_cb,
    UploadClient::EncryptionKeyAttachedCallback encryption_key_attached_cb,
    GetCloudPolicyClientCallback build_cloud_policy_client_cb,
    UploadClientBuilderCb upload_client_builder_cb)
    : helper_(base::MakeRefCounted<UploadHelper>(
          build_cloud_policy_client_cb,
          upload_client_builder_cb,
          report_successful_upload_cb,
          encryption_key_attached_cb,
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
  return base::BindRepeating(&UploadClient::Create);
}
}  // namespace reporting
