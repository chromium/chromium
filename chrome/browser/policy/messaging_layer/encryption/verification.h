// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_VERIFICATION_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_VERIFICATION_H_

#include <string>

#include "base/strings/string_piece.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"

namespace reporting {

// Helper class that verifies an Ed25519 signed message received from
// the server. It uses boringssl implementation available on the client.
class SignatureVerifier {
 public:
  // Ed25519 |verification_public_key| must consist of ED25519_PUBLIC_KEY_LEN
  // bytes.
  explicit SignatureVerifier(base::StringPiece verification_public_key);

  // Actual verification - returns error status if provided |signature| does not
  // match |message|. Signature must be ED25519_SIGNATURE_LEN bytes.
  Status Verify(base::StringPiece message, base::StringPiece signature);

 private:
  std::string verification_public_key_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_ENCRYPTION_VERIFICATION_H_
