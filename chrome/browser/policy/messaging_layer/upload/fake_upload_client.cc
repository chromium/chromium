// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/fake_upload_client.h"

#include <utility>

#include "base/base64.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/upload/record_handler_impl.h"
#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/encrypted_reporting_json_keys.h"
#include "components/reporting/util/status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

FakeUploadClient::FakeUploadClient() = default;

FakeUploadClient::~FakeUploadClient() = default;

void FakeUploadClient::Create(CreatedCallback created_cb) {
  std::move(created_cb)
      .Run(base::WrapUnique<UploadClient>(new FakeUploadClient()));
}

Status FakeUploadClient::EnqueueUpload(
    bool need_encryption_key,
    int config_file_version,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    ReportSuccessfulUploadCallback report_upload_success_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb) {
  UploadEncryptedReportingRequestBuilder builder;
  for (auto record : records) {
    builder.AddRecord(std::move(record), scoped_reservation);
  }
  auto request_result = builder.Build();
  if (!request_result.has_value()) {
    // While it might seem that this should return a bad status, the actual
    // UploadClient would return OK here as the records are processed at a later
    // time, and any failures are expected to be handled elsewhere.
    return Status::StatusOK();
  }

  auto response_cb = base::BindOnce(
      &FakeUploadClient::OnUploadComplete, base::Unretained(this),
      std::move(scoped_reservation), std::move(report_upload_success_cb),
      std::move(encryption_key_attached_cb));

  ReportingServerConnector::UploadEncryptedReport(
      std::move(request_result.value()), std::move(response_cb));
  return Status::StatusOK();
}

void FakeUploadClient::OnUploadComplete(
    ScopedReservation scoped_reservation,
    ReportSuccessfulUploadCallback report_upload_success_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb,
    StatusOr<base::Value::Dict> response) {
  if (!response.has_value()) {
    return;
  }
  const base::Value::Dict* last_success =
      response.value().FindDict(json_keys::kLastSucceedUploadedRecord);
  if (last_success != nullptr) {
    const auto force_confirm_flag =
        last_success->FindBool(json_keys::kForceConfirm);
    bool force_confirm =
        force_confirm_flag.has_value() && force_confirm_flag.value();
    auto seq_info_result =
        RecordHandlerImpl::SequenceInformationValueToProto(*last_success);
    if (seq_info_result.has_value()) {
      std::move(report_upload_success_cb)
          .Run(seq_info_result.value(), force_confirm);
    }
  }

  const base::Value::Dict* signed_encryption_key_record =
      response.value().FindDict(json_keys::kEncryptionSettings);
  if (signed_encryption_key_record != nullptr) {
    const std::string* public_key_str =
        signed_encryption_key_record->FindString(json_keys::kPublicKey);
    const auto public_key_id_result =
        signed_encryption_key_record->FindInt(json_keys::kPublicKeyId);
    const std::string* public_key_signature_str =
        signed_encryption_key_record->FindString(
            json_keys::kPublicKeySignature);
    std::string public_key;
    std::string public_key_signature;
    if (public_key_str != nullptr &&
        base::Base64Decode(*public_key_str, &public_key) &&
        public_key_signature_str != nullptr &&
        base::Base64Decode(*public_key_signature_str, &public_key_signature) &&
        public_key_id_result.has_value()) {
      SignedEncryptionInfo signed_encryption_key;
      signed_encryption_key.set_public_asymmetric_key(public_key);
      signed_encryption_key.set_public_key_id(public_key_id_result.value());
      signed_encryption_key.set_signature(public_key_signature);
      std::move(encryption_key_attached_cb).Run(signed_encryption_key);
    }
  }
}

}  // namespace reporting
