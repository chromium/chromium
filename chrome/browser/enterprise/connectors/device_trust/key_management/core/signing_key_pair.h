// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_SIGNING_KEY_PAIR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_SIGNING_KEY_PAIR_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace crypto {
class UnexportableSigningKey;
}  // namespace crypto

namespace enterprise_connectors {

// Class in charge of using a stored signing key and providing cryptographic
// functionality.
class SigningKeyPair {
 public:
  using KeyTrustLevel =
      enterprise_management::BrowserPublicKeyUploadRequest::KeyTrustLevel;

  // Uses `persistence_delegate` to create a SigningKeyPair instance based on
  // a key that has already been persisted on the system. Returns absl::nullopt
  // if no key was found.
  static absl::optional<SigningKeyPair> Create(
      KeyPersistenceDelegate* persistence_delegate);

  SigningKeyPair(std::unique_ptr<crypto::UnexportableSigningKey> key_pair,
                 KeyTrustLevel trust_level);

  SigningKeyPair(SigningKeyPair&& other);
  SigningKeyPair& operator=(SigningKeyPair&& other);

  SigningKeyPair(const SigningKeyPair&) = delete;
  SigningKeyPair& operator=(const SigningKeyPair&) = delete;

  ~SigningKeyPair();

  crypto::UnexportableSigningKey* key() const {
    return key_pair_ ? key_pair_.get() : nullptr;
  }

  KeyTrustLevel trust_level() const { return trust_level_; }

 private:
  std::unique_ptr<crypto::UnexportableSigningKey> key_pair_;
  KeyTrustLevel trust_level_ = enterprise_management::
      BrowserPublicKeyUploadRequest::KEY_TRUST_LEVEL_UNSPECIFIED;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_SIGNING_KEY_PAIR_H_
