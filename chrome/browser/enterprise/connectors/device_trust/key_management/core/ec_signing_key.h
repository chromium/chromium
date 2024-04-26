// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_EC_SIGNING_KEY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_EC_SIGNING_KEY_H_

#include <memory>
#include <vector>

#include "crypto/ec_private_key.h"
#include "crypto/unexportable_key.h"

namespace enterprise_connectors {

// An implementation of crypto::UnexportableKeyProvider that creates
// crypto::UnexportableSigningKey keys based on crypto::ECPrivateKey.
class ECSigningKeyProvider : public crypto::UnexportableKeyProvider {
 public:
  ECSigningKeyProvider();
  ~ECSigningKeyProvider() override;

  // crypto::UnexportableKeyProvider:
  std::optional<crypto::SignatureVerifier::SignatureAlgorithm> SelectAlgorithm(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override;
  std::unique_ptr<crypto::UnexportableSigningKey> GenerateSigningKeySlowly(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override;
  std::unique_ptr<crypto::UnexportableSigningKey> FromWrappedSigningKeySlowly(
      base::span<const uint8_t> wrapped_key) override;
  bool DeleteSigningKeySlowly(base::span<const uint8_t> wrapped_key) override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_EC_SIGNING_KEY_H_
