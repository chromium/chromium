// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/encryption/verification.h"

#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace reporting {

SignatureVerifier::SignatureVerifier(base::StringPiece verification_public_key)
    : verification_public_key_(verification_public_key) {}

Status SignatureVerifier::Verify(base::StringPiece message,
                                 base::StringPiece signature) {
  if (signature.size() != ED25519_SIGNATURE_LEN) {
    return Status{error::FAILED_PRECONDITION, "Wrong signature size"};
  }
  if (verification_public_key_.size() != ED25519_PUBLIC_KEY_LEN) {
    return Status{error::FAILED_PRECONDITION, "Wrong public key size"};
  }
  const int result = ED25519_verify(
      reinterpret_cast<const uint8_t*>(message.data()), message.size(),
      reinterpret_cast<const uint8_t*>(signature.data()),
      reinterpret_cast<const uint8_t*>(verification_public_key_.data()));
  if (result != 1) {
    return Status{error::INVALID_ARGUMENT, "Verification failed"};
  }
  return Status::StatusOK();
}

}  // namespace reporting
