// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client_impl.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"

namespace enterprise_connectors {

namespace {

std::unique_ptr<SecureEnclaveClient>* GetTestInstanceStorage() {
  static base::NoDestructor<std::unique_ptr<SecureEnclaveClient>> storage;
  return storage.get();
}

// Returns the result of the comparison of the key `label` and the
// `wrapped_label`.
bool CheckEqual(base::span<const uint8_t> wrapped_label,
                const std::string& label) {
  auto label_span = base::as_bytes(base::make_span(label));
  return base::ranges::equal(wrapped_label, label_span);
}

}  // namespace

// static
std::unique_ptr<SecureEnclaveClient> SecureEnclaveClient::Create() {
  std::unique_ptr<SecureEnclaveClient>& test_instance =
      *GetTestInstanceStorage();
  if (test_instance)
    return std::move(test_instance);
  return std::make_unique<SecureEnclaveClientImpl>();
}

// static
void SecureEnclaveClient::SetInstanceForTesting(
    std::unique_ptr<SecureEnclaveClient> client) {
  DCHECK(client);
  *GetTestInstanceStorage() = std::move(client);
}

// static
std::optional<SecureEnclaveClient::KeyType>
SecureEnclaveClient::GetTypeFromWrappedKey(
    base::span<const uint8_t> wrapped_key_label) {
  if (CheckEqual(wrapped_key_label, constants::kDeviceTrustSigningKeyLabel)) {
    return SecureEnclaveClient::KeyType::kPermanent;
  }

  if (CheckEqual(wrapped_key_label,
                 constants::kTemporaryDeviceTrustSigningKeyLabel)) {
    return SecureEnclaveClient::KeyType::kTemporary;
  }

  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

// static
std::string_view SecureEnclaveClient::GetLabelFromKeyType(
    SecureEnclaveClient::KeyType type) {
  switch (type) {
    case SecureEnclaveClient::KeyType::kTemporary:
      return constants::kTemporaryDeviceTrustSigningKeyLabel;
    case SecureEnclaveClient::KeyType::kPermanent:
      return constants::kDeviceTrustSigningKeyLabel;
  }
}

}  // namespace enterprise_connectors
