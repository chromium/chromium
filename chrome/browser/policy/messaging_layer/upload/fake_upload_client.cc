// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/fake_upload_client.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "chrome/browser/policy/messaging_layer/util/upload_declarations.h"
#include "chrome/browser/policy/messaging_layer/util/upload_response_parser.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/encrypted_reporting_json_keys.h"
#include "components/reporting/util/status.h"

namespace reporting {

FakeUploadClient::FakeUploadClient() = default;

FakeUploadClient::~FakeUploadClient() = default;

void FakeUploadClient::Create(CreatedCallback created_cb) {
  std::move(created_cb)
      .Run(base::WrapUnique<UploadClient>(new FakeUploadClient()));
}

void FakeUploadClient::EnqueueUpload(
    bool need_encryption_key,
    int config_file_version,
    std::vector<EncryptedRecord> records,
    ScopedReservation scoped_reservation,
    UploadEnqueuedCallback enqueued_cb,
    ReportSuccessfulUploadCallback report_upload_success_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb,
    ConfigFileAttachedCallback config_file_attached_cb) {
  auto response_cb = base::BindOnce(&FakeUploadClient::OnUploadComplete,
                                    base::Unretained(this),
                                    std::move(report_upload_success_cb),
                                    std::move(encryption_key_attached_cb),
                                    std::move(config_file_attached_cb));
  ReportingServerConnector::UploadEncryptedReport(
      need_encryption_key, config_file_version, std::move(records),
      std::move(scoped_reservation), std::move(enqueued_cb),
      std::move(response_cb));
}

void FakeUploadClient::OnUploadComplete(
    ReportSuccessfulUploadCallback report_upload_success_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb,
    ConfigFileAttachedCallback config_file_attached_cb,
    StatusOr<UploadResponseParser> response) {
  if (!response.has_value()) {
    return;
  }

  auto last_success =
      response.value().last_successfully_uploaded_record_sequence_info();
  if (last_success.has_value()) {
    const auto force_confirm_flag = response.value().force_confirm_flag();
    std::move(report_upload_success_cb)
        .Run(std::move(last_success.value()), force_confirm_flag);
  }

  auto encryption_settings = response.value().encryption_settings();
  if (encryption_settings.has_value()) {
    std::move(encryption_key_attached_cb)
        .Run(std::move(encryption_settings.value()));
  }

  auto config_file = response.value().config_file();
  if (config_file.has_value()) {
    std::move(config_file_attached_cb).Run(std::move(config_file.value()));
  }
}
}  // namespace reporting
