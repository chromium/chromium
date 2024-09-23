// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test support library for response payloads.

#include "chrome/browser/policy/messaging_layer/util/test_response_payload.h"

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "base/values.h"

#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "components/reporting/util/encrypted_reporting_json_keys.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
ResponseBuilder::ResponseBuilder(const base::Value::Dict& request)
    : request_(request.Clone()) {}

ResponseBuilder::ResponseBuilder(base::Value::Dict&& request)
    : request_(std::move(request)) {}

ResponseBuilder::ResponseBuilder(const ResponseBuilder& other)
    : request_(other.request_.Clone()), params_(other.params_) {}

ResponseBuilder& ResponseBuilder::SetForceConfirm(bool force_confirm) {
  params_.force_confirm = force_confirm;
  return *this;
}

ResponseBuilder& ResponseBuilder::SetNull(bool null) {
  params_.null = null;
  return *this;
}

ResponseBuilder& ResponseBuilder::SetRequest(const base::Value::Dict& request) {
  request_ = request.Clone();
  return *this;
}

ResponseBuilder& ResponseBuilder::SetRequest(base::Value::Dict&& request) {
  request_ = std::move(request);
  return *this;
}

ResponseBuilder& ResponseBuilder::SetSuccess(bool success) {
  params_.success = success;
  return *this;
}

StatusOr<base::Value::Dict> ResponseBuilder::Build() const {
  if (params_.null) {
    return base::unexpected(
        Status(error::FAILED_PRECONDITION, "No parameters set"));
  }

  base::Value::Dict response;

  // Attach sequenceInformation.
  if (const base::Value::List* const encrypted_record_list =
          request_.FindList(json_keys::kEncryptedRecordList);
      encrypted_record_list != nullptr) {
    EXPECT_FALSE(encrypted_record_list->empty());

    // Retrieve and process sequence information. The last record is the last
    // successfully uploaded record if the response is successful, or the first
    // failed record if the response is failure.
    const auto seq_info_it = std::prev(encrypted_record_list->cend());
    const auto* const seq_info =
        seq_info_it->GetDict().FindDict(json_keys::kSequenceInformation);
    EXPECT_NE(seq_info, nullptr);
    if (params_.success) {
      response.Set(json_keys::kLastSucceedUploadedRecord, seq_info->Clone());
    } else {
      response.Set(json_keys::kFirstFailedUploadedRecord,
                   base::Value::Dict().Set(json_keys::kFailedUploadedRecord,
                                           seq_info->Clone()));
      response.Set(json_keys::kFirstFailedUploadedRecord,
                   base::Value::Dict().Set(
                       json_keys::kFailureStatus,
                       base::Value::Dict().Set(json_keys::kErrorCode, 12345)));
      response.Set(json_keys::kFirstFailedUploadedRecord,
                   base::Value::Dict().Set(
                       json_keys::kFailureStatus,
                       base::Value::Dict().Set(json_keys::kErrorCode,
                                               "You've got a fake error.")));
      const auto* const last_success_seq_info =
          std::prev(seq_info_it)
              ->GetDict()
              .FindDict(json_keys::kSequenceInformation);
      EXPECT_NE(last_success_seq_info, nullptr);
      response.Set(json_keys::kLastSucceedUploadedRecord,
                   last_success_seq_info->Clone());
    }
  }

  // If forceConfirm confirm is expected, set it.
  if (params_.force_confirm) {
    response.Set(json_keys::kForceConfirm, true);
  }

  // If attach_encryption_settings is true, process that.
  const auto attach_encryption_settings =
      request_.FindBool(json_keys::kAttachEncryptionSettings);
  if (attach_encryption_settings.has_value() &&
      attach_encryption_settings.value()) {
    base::Value::Dict encryption_settings;
    std::string encoded = base::Base64Encode("PUBLIC KEY");
    encryption_settings.Set(json_keys::kPublicKey, std::move(encoded));
    encryption_settings.Set(json_keys::kPublicKeyId, 12345);
    encryption_settings.Set(json_keys::kPublicKeySignature,
                            base::Base64Encode("PUBLIC KEY SIG"));
    response.Set(json_keys::kEncryptionSettings,
                 std::move(encryption_settings));
  }

  // If configurationFileVersion is provided, attach the configuration file.
  const auto configuration_file_version =
      request_.FindInt(json_keys::kConfigurationFileVersion);
  if (configuration_file_version.has_value()) {
    base::Value::Dict configuration_file;
    base::Value::List event_configs;
    base::Value::Dict heartbeat;
    heartbeat.Set(json_keys::kConfigurationFileDestination, "HEARTBEAT_EVENTS");
    heartbeat.Set(json_keys::kConfigurationFileMinimumReleaseVersion, 11111);
    event_configs.Append(std::move(heartbeat));
    base::Value::Dict login;
    login.Set(json_keys::kConfigurationFileDestination, "LOGIN_LOGOUT_EVENTS");
    login.Set(json_keys::kConfigurationFileMinimumReleaseVersion, 22222);
    login.Set(json_keys::kConfigurationFileMaximumReleaseVersion, 33333);
    event_configs.Append(std::move(login));
    base::Value::Dict lock;
    lock.Set(json_keys::kConfigurationFileDestination, "LOCK_UNLOCK_EVENTS");
    event_configs.Append(std::move(lock));
    std::string encoded = base::Base64Encode("Fake signature");
    configuration_file.Set(json_keys::kConfigurationFileSignature,
                           base::Base64Encode("Fake signature"));
    configuration_file.Set(json_keys::kBlockedEventConfigs,
                           std::move(event_configs));
    configuration_file.Set(json_keys::kConfigurationFileVersionResponse,
                           123456);
    response.Set(json_keys::kConfigurationFile, std::move(configuration_file));
  }

  return response;
}

MakeUploadEncryptedReportAction::MakeUploadEncryptedReportAction(
    ResponseBuilder&& response_builder)
    : response_builder_(std::move(response_builder)) {}

void MakeUploadEncryptedReportAction::operator()(
    base::Value::Dict request,
    std::optional<base::Value::Dict> context,
    ReportingServerConnector::ResponseCallback callback) {
  response_builder_.SetRequest(std::move(request));
  auto response_result = response_builder_.Build();
  if (!response_result.has_value()) {
    std::move(callback).Run(base::unexpected(response_result.error()));
    return;
  }
  UploadResponseParser response_parser(
      EncryptedReportingClient::GenerationGuidIsRequired(),
      std::move(response_result.value()));
  std::move(callback).Run(std::move(response_parser));
}

}  // namespace reporting
