// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_MEMORY_SIGNING_KEY_PAIR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_MEMORY_SIGNING_KEY_PAIR_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/signing_key_pair.h"

namespace enterprise_connectors {

namespace test {

// A SigningKeyPair that persists signing keys in memory only.  All instances
// share the same underlying persistence.
class MemorySigningKeyPair : public SigningKeyPair {
 public:
  static std::unique_ptr<MemorySigningKeyPair> Create();

  // SigningKeyPair:
  bool StoreKeyPair(KeyTrustLevel trust_level,
                    std::vector<uint8_t> wrapped) override;
  KeyInfo LoadKeyPair() override;

  // Determines whether calls to StoreKeyPair() fail or not.
  void set_force_store_to_fail(bool force_store_to_fail) {
    force_store_to_fail_ = force_store_to_fail;
  }

 private:
  MemorySigningKeyPair();

  bool force_store_to_fail_ = false;
};

// Makes sure to clear the memory used to by MemorySigningKeyPair instances.
// The memory is clear at both constructor and destructor time.
class ScopedMemorySigningKeyPairPersistence {
 public:
  ScopedMemorySigningKeyPairPersistence();
  ~ScopedMemorySigningKeyPairPersistence();
};

}  // namespace test
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_MEMORY_SIGNING_KEY_PAIR_H_
