// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_SIGNING_KEY_PAIR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_SIGNING_KEY_PAIR_H_

#include <memory>
#include <string>
#include <vector>

#include "components/policy/proto/device_management_backend.pb.h"

namespace crypto {
class UnexportableSigningKey;
}  // namespace crypto

namespace enterprise_connectors {

class KeyPersistenceDelegate;

// Class in charge of using a stored signing key and providing cryptographic
// functionality.
class SigningKeyPair {
 public:
  using KeyTrustLevel =
      enterprise_management::BrowserPublicKeyUploadRequest::KeyTrustLevel;

  // Uses `persistence_delegate` to create a SigningKeyPair instance based on
  // a key that has already been persisted on the system. Returns nullptr
  // if no key was found.
  static std::unique_ptr<SigningKeyPair> Create(
      KeyPersistenceDelegate* persistence_delegate);

  // Loads the signing key pair from disk and initializes it. Returns nullptr if
  // no key was found. Uses the KeyPersistenceDelegateFactory's default delegate
  // to load the key from persistence.
  // This function does IO and heavy cryptographic calculations, do not call
  // on the main thread.
  static std::unique_ptr<SigningKeyPair> LoadPersistedKey();

  SigningKeyPair(std::unique_ptr<crypto::UnexportableSigningKey> key_pair,
                 KeyTrustLevel trust_level);

  SigningKeyPair(const SigningKeyPair&) = delete;
  SigningKeyPair& operator=(const SigningKeyPair&) = delete;

  ~SigningKeyPair();

  bool is_empty() const {
    return trust_level_ ==
               enterprise_management::BrowserPublicKeyUploadRequest::
                   KEY_TRUST_LEVEL_UNSPECIFIED ||
           !key();
  }

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
