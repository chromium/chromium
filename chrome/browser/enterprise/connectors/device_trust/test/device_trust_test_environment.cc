// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/test/device_trust_test_environment.h"

#include <string_view>

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"

namespace enterprise_connectors {

DeviceTrustTestEnvironment::DeviceTrustTestEnvironment(
    std::string_view thread_name,
    HttpResponseCode upload_response_code)
    : worker_thread_(std::string(thread_name)),
      upload_response_code_(upload_response_code) {
  scoped_in_memory_key_persistence_delegate_factory_.emplace();

  key_persistence_delegate_ = scoped_in_memory_key_persistence_delegate_factory_
                                  ->CreateKeyPersistenceDelegate();
}

DeviceTrustTestEnvironment::~DeviceTrustTestEnvironment() = default;

bool DeviceTrustTestEnvironment::KeyExists() {
  return (bool)key_persistence_delegate_->LoadKeyPair(
      KeyStorageType::kPermanent, nullptr);
}

void DeviceTrustTestEnvironment::SetUploadResult(
    HttpResponseCode upload_response_code) {
  upload_response_code_ = upload_response_code;
}

void DeviceTrustTestEnvironment::SetExpectedDMToken(
    std::string expected_dm_token) {
  expected_dm_token_ = expected_dm_token;
}

void DeviceTrustTestEnvironment::SetExpectedClientID(
    std::string expected_client_id) {
  expected_client_id_ = expected_client_id;
}

}  // namespace enterprise_connectors
