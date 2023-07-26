// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_SIGNING_KEY_PAIR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_SIGNING_KEY_PAIR_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/unexportable_key.h"

namespace crypto {
class UnexportableSigningKey;
}  // namespace crypto

namespace enterprise_connectors {

// Class in charge of storing a signing key and its trust level.
class SigningKeyPair : public base::RefCountedThreadSafe<SigningKeyPair> {
 public:
  using KeyTrustLevel =
      enterprise_management::BrowserPublicKeyUploadRequest::KeyTrustLevel;

  SigningKeyPair(std::unique_ptr<crypto::UnexportableSigningKey> signing_key,
                 KeyTrustLevel trust_level);

  SigningKeyPair(const SigningKeyPair&) = delete;
  SigningKeyPair& operator=(const SigningKeyPair&) = delete;

  bool is_empty() const {
    return trust_level_ ==
               enterprise_management::BrowserPublicKeyUploadRequest::
                   KEY_TRUST_LEVEL_UNSPECIFIED ||
           !key();
  }

  crypto::UnexportableSigningKey* key() const {
    return signing_key_ ? signing_key_.get() : nullptr;
  }

  KeyTrustLevel trust_level() const { return trust_level_; }

 private:
  friend class base::RefCountedThreadSafe<SigningKeyPair>;

  ~SigningKeyPair();

  std::unique_ptr<crypto::UnexportableSigningKey> signing_key_;
  KeyTrustLevel trust_level_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_SIGNING_KEY_PAIR_H_
