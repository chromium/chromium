// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/encrypted_reporting_service_provider.h"

#include <memory>
#include <utility>

#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "chrome/browser/policy/messaging_layer/util/get_cloud_policy_client.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "components/reporting/proto/interface.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/backoff_settings.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status.pb.h"
#include "components/reporting/util/statusor.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace {

void SendStatusAsResponse(std::unique_ptr<dbus::Response> response,
                          dbus::ExportedObject::ResponseSender response_sender,
                          const reporting::Status& status) {
  // Build StatusProto
  reporting::StatusProto status_proto;
  status.SaveTo(&status_proto);

  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(status_proto);

  // Send Response
  std::move(response_sender).Run(std::move(response));
}

}  // namespace

EncryptedReportingServiceProvider::EncryptedReportingServiceProvider()
    : sequenced_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(base::TaskTraits())),
      backoff_entry_(reporting::GetBackoffEntry()),
      storage_module_(MissiveClient::Get()->GetMissiveStorageModule()) {}

EncryptedReportingServiceProvider::~EncryptedReportingServiceProvider() =
    default;

void EncryptedReportingServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object->ExportMethod(
      kChromeReportingServiceInterface,
      kChromeReportingServiceUploadEncryptedRecordMethod,
      base::BindRepeating(
          &EncryptedReportingServiceProvider::RequestUploadEncryptedRecord,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&EncryptedReportingServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &EncryptedReportingServiceProvider::PostNewCloudPolicyClientRequest,
          weak_ptr_factory_.GetWeakPtr()));
}

void EncryptedReportingServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "." << method_name;
  }
}

void EncryptedReportingServiceProvider::PostNewCloudPolicyClientRequest() {
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
          [](base::OnceCallback<void(
                 reporting::StatusOr<policy::CloudPolicyClient*>)> callback) {
            reporting::GetCloudPolicyClientCb().Run(std::move(callback));
          },
          base::BindOnce(
              &EncryptedReportingServiceProvider::OnCloudPolicyClientResult,
              weak_ptr_factory_.GetWeakPtr())),
      backoff_entry_->GetTimeUntilRelease());

  // Increase backoff_entry_ for next request.
  backoff_entry_->InformOfRequest(/*succeeded=*/false);
}

void EncryptedReportingServiceProvider::OnCloudPolicyClientResult(
    reporting::StatusOr<policy::CloudPolicyClient*> client_result) {
  if (!client_result.ok()) {
    upload_client_request_in_progress_ = false;
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &EncryptedReportingServiceProvider::PostNewCloudPolicyClientRequest,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  BuildUploadClient(client_result.ValueOrDie());
}

void EncryptedReportingServiceProvider::BuildUploadClient(
    policy::CloudPolicyClient* client) {
  reporting::UploadClient::ReportSuccessfulUploadCallback successful_upload_cb =
      base::BindRepeating(&reporting::StorageModuleInterface::ReportSuccess,
                          storage_module_);

  reporting::UploadClient::EncryptionKeyAttachedCallback encryption_key_cb =
      base::BindRepeating(
          &reporting::StorageModuleInterface::UpdateEncryptionKey,
          storage_module_);

  base::OnceCallback<void(
      reporting::StatusOr<std::unique_ptr<reporting::UploadClient>>)>
      update_upload_client_cb = base::BindOnce(
          &EncryptedReportingServiceProvider::OnUploadClientResult,
          weak_ptr_factory_.GetWeakPtr());

  reporting::UploadClient::Create(client, std::move(successful_upload_cb),
                                  std::move(encryption_key_cb),
                                  std::move(update_upload_client_cb));
}

void EncryptedReportingServiceProvider::OnUploadClientResult(
    reporting::StatusOr<std::unique_ptr<reporting::UploadClient>>
        client_result) {
  if (!client_result.ok()) {
    upload_client_request_in_progress_ = false;
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &EncryptedReportingServiceProvider::PostNewCloudPolicyClientRequest,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&EncryptedReportingServiceProvider::UpdateUploadClient,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(client_result.ValueOrDie())));
}

void EncryptedReportingServiceProvider::UpdateUploadClient(
    std::unique_ptr<reporting::UploadClient> upload_client) {
  upload_client_ = std::move(upload_client);
  backoff_entry_->InformOfRequest(/*succeeded*/ true);
  upload_client_request_in_progress_ = false;
}

void EncryptedReportingServiceProvider::RequestUploadEncryptedRecord(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  auto response = dbus::Response::FromMethodCall(method_call);
  if (upload_client_ == nullptr) {
    reporting::Status status{reporting::error::UNAVAILABLE,
                             "UploadClient isn't ready"};
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
  auto enqueue_status = upload_client_->EnqueueUpload(
      request.need_encryption_keys(), std::move(records));
  SendStatusAsResponse(std::move(response), std::move(response_sender),
                       enqueue_status);
}

}  // namespace chromeos
