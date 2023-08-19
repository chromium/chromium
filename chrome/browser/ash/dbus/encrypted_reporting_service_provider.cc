// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/encrypted_reporting_service_provider.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/policy/messaging_layer/upload/event_upload_size_controller.h"
#include "chrome/browser/policy/messaging_layer/upload/file_upload_impl.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_provider.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "components/reporting/proto/synced/interface.pb.h"
#include "components/reporting/proto/synced/status.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/storage_selector/storage_selector.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

static constexpr uint64_t kDefaultMemoryAllocation =
    16u * 1024uLL * 1024uLL;  // 16 MiB by default

void SendStatusAsResponse(
    std::unique_ptr<dbus::Response> response,
    dbus::ExportedObject::ResponseSender response_sender,
    ::reporting::UploadEncryptedRecordResponse response_message,
    ::reporting::Status status) {
  // Build `StatusProto` in `response_message`
  status.SaveTo(response_message.mutable_status());

  // Encode whole `response_message`
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(response_message);

  // Send `response`
  std::move(response_sender).Run(std::move(response));
}

}  // namespace

// EncryptedReportingServiceProvider implementation.

EncryptedReportingServiceProvider::EncryptedReportingServiceProvider(
    std::unique_ptr<::reporting::EncryptedReportingUploadProvider>
        upload_provider)
    : origin_thread_id_(base::PlatformThread::CurrentId()),
      origin_thread_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      memory_resource_(base::MakeRefCounted<::reporting::ResourceManager>(
          kDefaultMemoryAllocation)),
      upload_provider_(std::move(upload_provider)) {
  CHECK(upload_provider_.get());
}

EncryptedReportingServiceProvider::~EncryptedReportingServiceProvider() =
    default;

void EncryptedReportingServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  CHECK(OnOriginThread());

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
  chromeos::MissiveClient* const missive_client =
      chromeos::MissiveClient::Get();
  return base::BindPostTask(
      missive_client->origin_task_runner(),
      base::BindRepeating(
          [](base::WeakPtr<chromeos::MissiveClient> missive_client,
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
  chromeos::MissiveClient* const missive_client =
      chromeos::MissiveClient::Get();
  return base::BindPostTask(
      missive_client->origin_task_runner(),
      base::BindRepeating(
          [](base::WeakPtr<chromeos::MissiveClient> missive_client,
             ::reporting::SignedEncryptionInfo signed_encryption_info) {
            if (missive_client) {
              missive_client->UpdateEncryptionKey(
                  std::move(signed_encryption_info));
            }
          },
          missive_client->GetWeakPtr()));
}

void EncryptedReportingServiceProvider::RequestUploadEncryptedRecords(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  CHECK(OnOriginThread());
  auto response = dbus::Response::FromMethodCall(method_call);
  ::reporting::UploadEncryptedRecordResponse response_message;

  if (!::reporting::StorageSelector::is_uploader_required()) {
    // We should never get to here, since the provider is only exported
    // when is_uploader_required() is true. Have this code only as
    // in order to let `missive` daemon to log configuration inconsistency.
    ::reporting::Status status{
        ::reporting::error::FAILED_PRECONDITION,
        "Uploads are not expected in this configuration"};
    LOG(ERROR) << status;
    SendStatusAsResponse(std::move(response), std::move(response_sender),
                         std::move(response_message), status);
    return;
  }

  dbus::MessageReader reader(method_call);
  const char* serialized_request_buf = nullptr;
  size_t serialized_request_buf_size = 0;
  if (!reader.PopArrayOfBytes(
          reinterpret_cast<const uint8_t**>(&serialized_request_buf),
          &serialized_request_buf_size)) {
    ::reporting::Status status{
        ::reporting::error::INVALID_ARGUMENT,
        "Error reading UploadEncryptedRecordRequest as array of bytes"};
    LOG(ERROR) << "Unable to process UploadEncryptedRecordRequest. status: "
               << status;
    SendStatusAsResponse(std::move(response), std::move(response_sender),
                         std::move(response_message), status);
    return;
  }

  ::reporting::ScopedReservation scoped_reservation(serialized_request_buf_size,
                                                    memory_resource_);
  if (!scoped_reservation.reserved()) {
    ::reporting::Status status{::reporting::error::RESOURCE_EXHAUSTED,
                               "UploadEncryptedRecordRequest has exhausted "
                               "assigned memory pool in Chrome"};
    LOG(ERROR) << "Unable to process UploadEncryptedRecordRequest. status: "
               << status;
    SendStatusAsResponse(std::move(response), std::move(response_sender),
                         std::move(response_message), status);
    return;
  }

  ::reporting::UploadEncryptedRecordRequest request;
  if (!request.ParseFromArray(serialized_request_buf,
                              serialized_request_buf_size)) {
    ::reporting::Status status{
        ::reporting::error::INVALID_ARGUMENT,
        "Failed to parse UploadEncryptedRecordRequest from array of "
        "bytes."};
    LOG(ERROR) << "Unable to process UploadEncryptedRecordRequest. status: "
               << status;
    SendStatusAsResponse(std::move(response), std::move(response_sender),
                         std::move(response_message), status);
    return;
  }

  // Missive should always send the remaining storage capacity and new
  // events rate. If not, probably an outdated version of missive is
  // running. In this case, we ignore the effect of remaining storage
  // capacity/new events rate and give it the max/min possible value.
  const auto remaining_storage_capacity =
      request.has_remaining_storage_capacity()
          ? request.remaining_storage_capacity()
          : std::numeric_limits<uint64_t>::max();
  const auto new_events_rate =
      request.has_new_events_rate() ? request.new_events_rate() : 1U;
  // Move events from |request| into a separate vector |records|, using more
  // or less the same amount of memory that has been reserved above.
  auto records{::reporting::EventUploadSizeController::BuildEncryptedRecords(
      request.encrypted_record(),
      ::reporting::EventUploadSizeController(
          network_condition_service_, new_events_rate,
          remaining_storage_capacity,
          ::reporting::FileUploadDelegate::kMaxUploadBufferSize))};

  CHECK(upload_provider_);
  chromeos::MissiveClient* const missive_client =
      chromeos::MissiveClient::Get();
  if (!missive_client) {
    ::reporting::Status status{::reporting::error::FAILED_PRECONDITION,
                               "No Missive client available"};
    LOG(ERROR) << status;
    SendStatusAsResponse(std::move(response), std::move(response_sender),
                         std::move(response_message), status);
    return;
  }

  if (missive_client->is_disabled()) {
    response_message.set_disable(true);  // Signal `missived` to disable itself.
    ::reporting::Status status{::reporting::error::FAILED_PRECONDITION,
                               "Reporting disabled, unsupported API Key"};
    LOG(ERROR) << status;
    SendStatusAsResponse(std::move(response), std::move(response_sender),
                         std::move(response_message), status);
    return;
  }

  upload_provider_->RequestUploadEncryptedRecords(
      request.need_encryption_keys(), std::move(records),
      std::move(scoped_reservation),
      base::BindPostTask(
          origin_thread_runner_,
          base::BindOnce(&SendStatusAsResponse, std::move(response),
                         std::move(response_sender),
                         std::move(response_message))));
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
