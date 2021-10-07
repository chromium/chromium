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

// A persistence delegate that keeps keys in memory and allows the owner to
// control whether the persistence will fail or not.  This is useful in
// implementing MemorySigningKeyPair but can also be used in other test code
// that wants to avoid real persistence.
class InMemorySigningKeyPairPersistenceDelegate
    : public SigningKeyPair::PersistenceDelegate {
 public:
  bool StoreKeyPair(SigningKeyPair::KeyTrustLevel trust_level,
                    std::vector<uint8_t> wrapped) override;
  SigningKeyPair::KeyInfo LoadKeyPair() override;

  void set_force_store_to_fail(bool force_store_to_fail) {
    force_store_to_fail_ = force_store_to_fail;
  }

 private:
  bool force_store_to_fail_ = false;
};

// A network delegate that sends keys to the bit bucket but allows the owner
// to simulate whether the network request fails or not.  This is useful in
// implementing MemorySigningKeyPair but can also be used in other test code
// that wants to avoid real networking.
class InMemorySigningKeyPairNetworkDelegate
    : public SigningKeyPair::NetworkDelegate {
 public:
  InMemorySigningKeyPairNetworkDelegate();
  ~InMemorySigningKeyPairNetworkDelegate() override;

  std::string SendPublicKeyToDmServerSync(const std::string& url,
                                          const std::string& dm_token,
                                          const std::string& body) override;

  void set_force_network_to_fail(bool force_network_to_fail) {
    force_network_to_fail_ = force_network_to_fail;
  }

  const std::string& url() { return url_; }
  const std::string& dm_token() { return dm_token_; }
  const std::string& body() { return body_; }

 private:
  bool force_network_to_fail_ = false;
  std::string url_;
  std::string dm_token_;
  std::string body_;
};

// Creates a SigningKeyPair that stored keys in memory and that
// simulates network requests to DM server.  The output arguments are
// optional and return interfaces to control the behaviour of the delegates.
// The lifetime of the control interfaces is the same as the returned
// signing key pair.
std::unique_ptr<SigningKeyPair> CreateInMemorySigningKeyPair(
    InMemorySigningKeyPairPersistenceDelegate** pdelegate_ptr,
    InMemorySigningKeyPairNetworkDelegate** nelegate_ptr);

// Makes sure to clear the memory used to by InMemorySigningKeyPair*Delegates.
// The memory is clear at both constructor and destructor time.
class ScopedMemorySigningKeyPairPersistence {
 public:
  ScopedMemorySigningKeyPairPersistence();
  ~ScopedMemorySigningKeyPairPersistence();
};

}  // namespace test
}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_MEMORY_SIGNING_KEY_PAIR_H_
