// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test support library for response payloads.

#include "chrome/browser/policy/messaging_layer/util/test_response_payload.h"

#include "base/base64.h"
#include "base/values.h"

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

absl::optional<base::Value::Dict> ResponseBuilder::Build() const {
  if (params_.null) {
    return absl::nullopt;
  }

  base::Value::Dict response;

  // Attach sequenceInformation.
  if (const base::Value::List* const encrypted_record_list =
          request_.FindList("encryptedRecord");
      encrypted_record_list != nullptr) {
    EXPECT_FALSE(encrypted_record_list->empty());

    // Retrieve and process sequence information. The last record is the last
    // successfully uploaded record if the response is successful, or the first
    // failed record if the response is failure.
    const auto seq_info_it = std::prev(encrypted_record_list->cend());
    const auto* const seq_info =
        seq_info_it->GetDict().FindDict("sequenceInformation");
    EXPECT_NE(seq_info, nullptr);
    if (params_.success) {
      response.Set("lastSucceedUploadedRecord", seq_info->Clone());
    } else {
      response.SetByDottedPath("firstFailedUploadedRecord.failedUploadedRecord",
                               seq_info->Clone());
      response.SetByDottedPath("firstFailedUploadedRecord.failureStatus.code",
                               12345);
      response.SetByDottedPath(
          "firstFailedUploadedRecord.failureStatus.errorMessage",
          "You've got a fake error.");
      const auto* const last_success_seq_info =
          std::prev(seq_info_it)->GetDict().FindDict("sequenceInformation");
      EXPECT_NE(last_success_seq_info, nullptr);
      response.Set("lastSucceedUploadedRecord", last_success_seq_info->Clone());
    }
  }

  // If forceConfirm confirm is expected, set it.
  if (params_.force_confirm) {
    response.Set("forceConfirm", true);
  }

  // If attach_encryption_settings is true, process that.
  const auto attach_encryption_settings =
      request_.FindBool("attachEncryptionSettings");
  if (attach_encryption_settings.has_value() &&
      attach_encryption_settings.value()) {
    base::Value::Dict encryption_settings;
    std::string encoded;
    base::Base64Encode("PUBLIC KEY", &encoded);
    encryption_settings.Set("publicKey", std::move(encoded));
    encryption_settings.Set("publicKeyId", 12345);
    base::Base64Encode("PUBLIC KEY SIG", &encoded);
    encryption_settings.Set("publicKeySignature", std::move(encoded));
    response.Set("encryptionSettings", std::move(encryption_settings));
  }

  // If attach_configuration_file is true, process that.
  const auto attach_configuration_file =
      request_.FindBool("attachConfigurationFile");
  if (attach_configuration_file.has_value() &&
      attach_configuration_file.value()) {
    base::Value::Dict configuration_file;
    base::Value::List event_configs;
    base::Value::Dict heartbeat;
    heartbeat.Set("destination", "HEARTBEAT_EVENTS");
    heartbeat.Set("minimumReleaseVersion", 11111);
    event_configs.Append(std::move(heartbeat));
    base::Value::Dict login;
    login.Set("destination", "LOGIN_LOGOUT_EVENTS");
    login.Set("minimumReleaseVersion", 22222);
    login.Set("maximumReleaseVersion", 33333);
    event_configs.Append(std::move(login));
    base::Value::Dict lock;
    lock.Set("destination", "LOCK_UNLOCK_EVENTS");
    event_configs.Append(std::move(lock));
    std::string encoded;
    base::Base64Encode("Fake signature", &encoded);
    configuration_file.Set("configurationFileSignature", std::move(encoded));
    configuration_file.Set("eventConfigs", std::move(event_configs));
    response.Set("configurationFile", std::move(configuration_file));
  }

  return response;
}

MakeUploadEncryptedReportAction::MakeUploadEncryptedReportAction(
    ResponseBuilder&& response_builder)
    : response_builder_(std::move(response_builder)) {}

void MakeUploadEncryptedReportAction::operator()(
    base::Value::Dict request,
    absl::optional<base::Value::Dict> context,
    ::policy::CloudPolicyClient::ResponseCallback callback) {
  response_builder_.SetRequest(std::move(request));
  std::move(callback).Run(response_builder_.Build());
}

}  // namespace reporting
