// Copyright 2022 The Chromium Authors. All rights reserved.
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

ResponseBuilder& ResponseBuilder::SetForceConfirm(bool force_confirm) {
  force_confirm_ = force_confirm;
  return *this;
}

ResponseBuilder& ResponseBuilder::SetSuccess(bool success) {
  success_ = success;
  return *this;
}

base::Value::Dict ResponseBuilder::Build() const {
  base::Value::Dict response;

  // Attach sequenceInformation.
  if (const base::Value::List* const encrypted_record_list =
          request_.FindList("encryptedRecord");
      encrypted_record_list != nullptr) {
    EXPECT_FALSE(encrypted_record_list->empty());

    // Retrieve and process sequence information
    const auto* const seq_info =
        encrypted_record_list->back().GetDict().FindDict("sequenceInformation");
    EXPECT_NE(seq_info, nullptr);
    if (success_) {
      response.Set("lastSucceedUploadedRecord", seq_info->Clone());
    } else {
      response.SetByDottedPath("firstFailedUploadedRecord.failedUploadedRecord",
                               seq_info->Clone());
      response.SetByDottedPath("firstFailedUploadedRecord.failureStatus.code",
                               12345);
      response.SetByDottedPath(
          "firstFailedUploadedRecord.failureStatus.errorMessage",
          "You've got a fake error.");
    }
  }

  // If forceConfirm confirm is expected, set it.
  if (force_confirm_) {
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

  return response;
}

}  // namespace reporting
