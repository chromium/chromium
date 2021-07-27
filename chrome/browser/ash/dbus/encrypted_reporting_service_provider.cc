// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/encrypted_reporting_service_provider.h"

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
#include "chromeos/dbus/missive/missive_client.h"
#include "components/reporting/proto/interface.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/storage_selector/storage_selector.h"
#include "components/reporting/util/backoff_settings.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status.pb.h"
#include "components/reporting/util/statusor.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "net/base/backoff_entry.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

void SendStatusAsResponse(std::unique_ptr<dbus::Response> response,
                          dbus::ExportedObject::ResponseSender response_sender,
                          reporting::Status status) {
  // Build StatusProto
  reporting::StatusProto status_proto;
  status.SaveTo(&status_proto);

  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(status_proto);

  // Send Response
  std::move(response_sender).Run(std::move(response));
}

void BuildUploadClient(
    scoped_refptr<reporting::StorageModuleInterface> storage_module,
    policy::CloudPolicyClient* client,
    reporting::UploadClient::CreatedCallback update_upload_client_cb) {
  reporting::UploadClient::ReportSuccessfulUploadCallback successful_upload_cb =
      base::BindRepeating(&reporting::StorageModuleInterface::ReportSuccess,
                          storage_module);

  reporting::UploadClient::EncryptionKeyAttachedCallback encryption_key_cb =
      base::BindRepeating(
          &reporting::StorageModuleInterface::UpdateEncryptionKey,
          storage_module);

  reporting::UploadClient::Create(client, std::move(successful_upload_cb),
                                  std::move(encryption_key_cb),
                                  std::move(update_upload_client_cb));
}

}  // namespace

// EncryptedReportingServiceProvider refcounted helper class.
class EncryptedReportingServiceProvider::UploadHelper
    : public base::RefCountedDeleteOnSequence<UploadHelper> {
 public:
  UploadHelper(
      reporting::GetCloudPolicyClientCallback build_cloud_policy_client_cb,
      UploadClientBuilderCb upload_client_builder_cb,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);
  UploadHelper(const UploadHelper& other) = delete;
  UploadHelper& operator=(const UploadHelper& other) = delete;

  // Requests new cloud policy client (can be invoked on any thread)
  void PostNewCloudPolicyClientRequest();

  // Uploads encrypted records (can be invoked on any thread).
  void EnqueueUpload(
      bool need_encryption_key,
      std::unique_ptr<std::vector<reporting::EncryptedRecord>> records,
      base::OnceCallback<void(reporting::Status)> enqueued_cb) const;

 private:
  friend class base::RefCountedDeleteOnSequence<UploadHelper>;
  friend class base::DeleteHelper<UploadHelper>;

  // Refcounted object can only have private or protected destructor.
  ~UploadHelper();

  // Stages of cloud policy client and upload client creation,
  // scheduled on a sequenced task runner.
  void TryNewCloudPolicyClientRequest();
  void OnCloudPolicyClientResult(
      reporting::StatusOr<policy::CloudPolicyClient*> client_result);
  void UpdateUploadClient(std::unique_ptr<reporting::UploadClient> client);
  void OnUploadClientResult(
      reporting::StatusOr<std::unique_ptr<reporting::UploadClient>>
          client_result);

  // Uploads encrypted records on sequenced task runner (and thus capable of
  // detecting whether upload client is ready or not)
  void EnqueueUploadInternal(
      bool need_encryption_key,
      std::unique_ptr<std::vector<reporting::EncryptedRecord>> records,
      base::OnceCallback<void(reporting::Status)> enqueued_cb) const;

  // Sequence task runner and checker used during
  // |PostNewCloudPolicyClientRequest| processing.
  // It is also used to protect |upload_client_|.
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(sequenced_task_checker_);

  // Callbacks for cloud policy and upload client creation.
  const reporting::GetCloudPolicyClientCallback build_cloud_policy_client_cb_;
  const UploadClientBuilderCb upload_client_builder_cb_;

  // Tracking of asynchronous stages.
  std::atomic<bool> upload_client_request_in_progress_{false};
  const std::unique_ptr<::net::BackoffEntry> backoff_entry_;

  // Upload client (protected by sequenced task runner). Once set, is used
  // repeatedly.
  std::unique_ptr<reporting::UploadClient> upload_client_;

  // Storage module, referring to missived.
  const scoped_refptr<reporting::StorageModuleInterface> storage_module_;

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction.
  base::WeakPtrFactory<UploadHelper> weak_ptr_factory_{this};
};

EncryptedReportingServiceProvider::UploadHelper::UploadHelper(
    reporting::GetCloudPolicyClientCallback build_cloud_policy_client_cb,
    UploadClientBuilderCb upload_client_builder_cb,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : base::RefCountedDeleteOnSequence<UploadHelper>(sequenced_task_runner),
      sequenced_task_runner_(std::move(sequenced_task_runner)),
      build_cloud_policy_client_cb_(build_cloud_policy_client_cb),
      upload_client_builder_cb_(upload_client_builder_cb),
      backoff_entry_(reporting::GetBackoffEntry()),
      storage_module_(MissiveClient::Get()->GetMissiveStorageModule()) {
  DETACH_FROM_SEQUENCE(sequenced_task_checker_);
}

EncryptedReportingServiceProvider::UploadHelper::~UploadHelper() {
  // Weak pointer factory must be destructed on the sequence.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequenced_task_checker_);
}

void EncryptedReportingServiceProvider::UploadHelper::
    PostNewCloudPolicyClientRequest() {
  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UploadHelper::TryNewCloudPolicyClientRequest,
                                weak_ptr_factory_.GetWeakPtr()));
}

void EncryptedReportingServiceProvider::UploadHelper::
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
          [](base::WeakPtr<EncryptedReportingServiceProvider::UploadHelper>
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

void EncryptedReportingServiceProvider::UploadHelper::OnCloudPolicyClientResult(
    reporting::StatusOr<policy::CloudPolicyClient*> client_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequenced_task_checker_);
  if (!client_result.ok()) {
    upload_client_request_in_progress_ = false;
    TryNewCloudPolicyClientRequest();
    return;
  }
  upload_client_builder_cb_.Run(
      storage_module_, client_result.ValueOrDie(),
      base::BindPostTask(sequenced_task_runner_,
                         base::BindOnce(&UploadHelper::OnUploadClientResult,
                                        weak_ptr_factory_.GetWeakPtr())));
}

void EncryptedReportingServiceProvider::UploadHelper::OnUploadClientResult(
    reporting::StatusOr<std::unique_ptr<reporting::UploadClient>>
        client_result) {
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

void EncryptedReportingServiceProvider::UploadHelper::UpdateUploadClient(
    std::unique_ptr<reporting::UploadClient> upload_client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequenced_task_checker_);
  upload_client_ = std::move(upload_client);
  backoff_entry_->InformOfRequest(/*succeeded=*/true);
  upload_client_request_in_progress_ = false;
}

void EncryptedReportingServiceProvider::UploadHelper::EnqueueUpload(
    bool need_encryption_key,
    std::unique_ptr<std::vector<reporting::EncryptedRecord>> records,
    base::OnceCallback<void(reporting::Status)> enqueued_cb) const {
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UploadHelper::EnqueueUploadInternal,
                     weak_ptr_factory_.GetWeakPtr(), need_encryption_key,
                     std::move(records), std::move(enqueued_cb)));
}

void EncryptedReportingServiceProvider::UploadHelper::EnqueueUploadInternal(
    bool need_encryption_key,
    std::unique_ptr<std::vector<reporting::EncryptedRecord>> records,
    base::OnceCallback<void(reporting::Status)> enqueued_cb) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequenced_task_checker_);
  if (upload_client_ == nullptr) {
    std::move(enqueued_cb)
        .Run(reporting::Status{reporting::error::UNAVAILABLE,
                               "UploadClient isn't ready"});
    return;
  }
  std::move(enqueued_cb)
      .Run(upload_client_->EnqueueUpload(need_encryption_key,
                                         std::move(records)));
}

// EncryptedReportingServiceProvider implementation.

EncryptedReportingServiceProvider::EncryptedReportingServiceProvider(
    reporting::GetCloudPolicyClientCallback build_cloud_policy_client_cb,
    UploadClientBuilderCb upload_client_builder_cb)
    : origin_thread_id_(base::PlatformThread::CurrentId()),
      origin_thread_runner_(base::ThreadTaskRunnerHandle::Get()),
      helper_(base::MakeRefCounted<UploadHelper>(
          build_cloud_policy_client_cb,
          upload_client_builder_cb,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::TaskPriority::BEST_EFFORT, base::MayBlock()}))) {}

EncryptedReportingServiceProvider::~EncryptedReportingServiceProvider() =
    default;

void EncryptedReportingServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  DCHECK(OnOriginThread());

  if (!::reporting::StorageSelector::is_uploader_required()) {
    // In LaCros configuration only Ash chrome is expected to receive
    // uploads.
    LOG(WARNING) << "Uploads are not expected in this configuration";
    return;
  }

  exported_object->ExportMethod(
      chromeos::kChromeReportingServiceInterface,
      chromeos::kChromeReportingServiceUploadEncryptedRecordMethod,
      base::BindRepeating(
          &EncryptedReportingServiceProvider::RequestUploadEncryptedRecord,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&EncryptedReportingServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  helper_->PostNewCloudPolicyClientRequest();
}

void EncryptedReportingServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
  }
}

void EncryptedReportingServiceProvider::RequestUploadEncryptedRecord(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DCHECK(OnOriginThread());
  auto response = dbus::Response::FromMethodCall(method_call);

  if (!::reporting::StorageSelector::is_uploader_required()) {
    // We should never get to here, since the provider is only exported
    // when is_uploader_required() is true. Have this code only as
    // in order to let `missive` daemon to log configuration inconsistency.
    reporting::Status status{reporting::error::FAILED_PRECONDITION,
                             "Uploads are not expected in this configuration"};
    LOG(ERROR) << "Uploads are not expected in this configuration";
    SendStatusAsResponse(std::move(response), std::move(response_sender),
                         status);
    return;
  }

  reporting::UploadEncryptedRecordRequest request;
  dbus::MessageReader reader(method_call);
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    reporting::Status status{
        reporting::error::INVALID_ARGUMENT,
        "Message was not decipherable as an UploadEncryptedRecordRequest"};
    LOG(ERROR) << "Unable to process UploadEncryptedRecordRequest. status: "
               << status;
    SendStatusAsResponse(std::move(response), std::move(response_sender),
                         status);
    return;
  }

  auto records = std::make_unique<std::vector<reporting::EncryptedRecord>>();
  for (auto& record : request.encrypted_record()) {
    records->push_back(std::move(record));
  }
  DCHECK(helper_);
  helper_->EnqueueUpload(
      request.need_encryption_keys(), std::move(records),
      base::BindPostTask(
          origin_thread_runner_,
          base::BindOnce(&SendStatusAsResponse, std::move(response),
                         std::move(response_sender))));
}

// static
EncryptedReportingServiceProvider::UploadClientBuilderCb
EncryptedReportingServiceProvider::GetUploadClientBuilder() {
  return base::BindRepeating(&BuildUploadClient);
}

bool EncryptedReportingServiceProvider::OnOriginThread() const {
  return base::PlatformThread::CurrentId() == origin_thread_id_;
}

}  // namespace ash
