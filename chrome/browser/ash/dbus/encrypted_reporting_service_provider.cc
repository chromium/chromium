// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/encrypted_reporting_service_provider.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/policy/messaging_layer/upload/event_upload_size_controller.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_provider.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "components/reporting/proto/interface.pb.h"
#include "components/reporting/storage_selector/storage_selector.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status.pb.h"
#include "components/reporting/util/statusor.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
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

}  // namespace

// EncryptedReportingServiceProvider implementation.

EncryptedReportingServiceProvider::EncryptedReportingServiceProvider(
    std::unique_ptr<::reporting::EncryptedReportingUploadProvider>
        upload_provider)
    : origin_thread_id_(base::PlatformThread::CurrentId()),
      origin_thread_runner_(base::ThreadTaskRunnerHandle::Get()),
      upload_provider_(std::move(upload_provider)) {
  DCHECK(upload_provider_.get());
}

EncryptedReportingServiceProvider::~EncryptedReportingServiceProvider() =
    default;

void EncryptedReportingServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  DCHECK(OnOriginThread());

  if (!::reporting::StorageSelector::is_uploader_required()) {
    // We should never get to here, since the provider is only exported
    // when is_uploader_required() is true. Have this code only
    // in order to log configuration inconsistency.
    LOG(ERROR) << "Uploads are not expected in this configuration";
    return;
  }

  exported_object->ExportMethod(
      chromeos::kChromeReportingServiceInterface,
      chromeos::kChromeReportingServiceUploadEncryptedRecordMethod,
      base::BindRepeating(
          &EncryptedReportingServiceProvider::RequestUploadEncryptedRecords,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&EncryptedReportingServiceProvider::OnExported,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EncryptedReportingServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  LOG_IF(ERROR, !success) << "Failed to export " << interface_name << "."
                          << method_name;
}

// static
::reporting::UploadClient::ReportSuccessfulUploadCallback
EncryptedReportingServiceProvider::GetReportSuccessUploadCallback() {
  MissiveClient* const missive_client = MissiveClient::Get();
  return base::BindPostTask(
      missive_client->origin_task_runner(),
      base::BindRepeating(
          [](base::WeakPtr<MissiveClient> missive_client,
             ::reporting::SequenceInformation sequence_information,
             bool force_confirm) {
            if (missive_client) {
              missive_client->ReportSuccess(std::move(sequence_information),
                                            force_confirm);
            }
          },
          missive_client->GetWeakPtr()));
}

// static
::reporting::UploadClient::EncryptionKeyAttachedCallback
EncryptedReportingServiceProvider::GetEncryptionKeyAttachedCallback() {
  MissiveClient* const missive_client = MissiveClient::Get();
  return base::BindPostTask(
      missive_client->origin_task_runner(),
      base::BindRepeating(
          [](base::WeakPtr<MissiveClient> missive_client,
             ::reporting::SignedEncryptionInfo signed_encryption_info) {
            if (missive_client) {
              missive_client->UpdateEncryptionKey(signed_encryption_info);
            }
          },
          missive_client->GetWeakPtr()));
}

void EncryptedReportingServiceProvider::RequestUploadEncryptedRecords(
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

  reporting::EventUploadSizeController event_upload_size_controller(
      network_condition_service_, /*enabled=*/false);
  auto records = std::make_unique<std::vector<reporting::EncryptedRecord>>();
  for (auto& record : request.encrypted_record()) {
    records->push_back(record);
    // Check if we have uploaded enough records after adding each record
    event_upload_size_controller.AccountForRecord(record);
    if (event_upload_size_controller.IsMaximumUploadSizeReached()) {
      break;
    }
  }
  DCHECK(upload_provider_);
  MissiveClient* const missive_client = MissiveClient::Get();
  if (!missive_client) {
    LOG(ERROR) << "No Missive client available";
    SendStatusAsResponse(
        std::move(response), std::move(response_sender),
        reporting::Status(reporting::error::FAILED_PRECONDITION,
                          "No Missive client available"));
    return;
  }

  upload_provider_->RequestUploadEncryptedRecords(
      request.need_encryption_keys(), std::move(records),
      base::BindPostTask(
          origin_thread_runner_,
          base::BindOnce(&SendStatusAsResponse, std::move(response),
                         std::move(response_sender))));
}

bool EncryptedReportingServiceProvider::OnOriginThread() const {
  return base::PlatformThread::CurrentId() == origin_thread_id_;
}

// static
std::unique_ptr<::reporting::EncryptedReportingUploadProvider>
EncryptedReportingServiceProvider::GetDefaultUploadProvider() {
  return std::make_unique<::reporting::EncryptedReportingUploadProvider>(
      GetReportSuccessUploadCallback(), GetEncryptionKeyAttachedCallback());
}
}  // namespace ash
