// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_SCOPED_TPM_SIGNING_KEY_PAIR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_SCOPED_TPM_SIGNING_KEY_PAIR_H_

#include <stdint.h>

#include <vector>

#include "crypto/scoped_mock_unexportable_key_provider.h"

namespace enterprise_connectors {
namespace test {

// Class used in tests to mock retrieval of TPM signing key pairs. Creating an
// instance of this will prevent tests from having to actually store signing
// key pairs (which requires an elevated process).
class ScopedTpmSigningKeyPair {
 public:
  ScopedTpmSigningKeyPair();
  ~ScopedTpmSigningKeyPair();

  // Returns the TPM wrapped key.
  const std::vector<uint8_t>& wrapped_key() { return wrapped_key_; }

 private:
  crypto::ScopedMockUnexportableKeyProvider scoped_key_provider_;
  std::vector<uint8_t> wrapped_key_;
};

}  // namespace test
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_SCOPED_TPM_SIGNING_KEY_PAIR_H_
