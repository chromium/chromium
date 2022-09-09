// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_SECURE_ENCLAVE_SIGNING_KEY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_SECURE_ENCLAVE_SIGNING_KEY_H_

#include <memory>

#include "base/containers/span.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"
#include "crypto/unexportable_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

// An implementation of crypto::UnexportableKeyProvider for mac using the
// Secure Enclave key.
class SecureEnclaveSigningKeyProvider : public crypto::UnexportableKeyProvider {
 public:
  // Takes a parameter of key `type` (Permanent or Temporary) that the key
  // provider will represent.
  explicit SecureEnclaveSigningKeyProvider(SecureEnclaveClient::KeyType type);
  ~SecureEnclaveSigningKeyProvider() override;

  // crypto::UnexportableKeyProvider:
  absl::optional<crypto::SignatureVerifier::SignatureAlgorithm> SelectAlgorithm(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override;
  std::unique_ptr<crypto::UnexportableSigningKey> GenerateSigningKeySlowly(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override;
  std::unique_ptr<crypto::UnexportableSigningKey> FromWrappedSigningKeySlowly(
      base::span<const uint8_t> wrapped_key_label) override;

 private:
  // The key type (Permanent or Temporary) the provider represents.
  SecureEnclaveClient::KeyType provider_key_type_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_MAC_SECURE_ENCLAVE_SIGNING_KEY_H_
