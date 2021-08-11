// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_SIGNING_KEY_PAIR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_SIGNING_KEY_PAIR_H_

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/ec_signing_key.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise_connectors {

// This class manages the CBCM signing key used by DeviceTrustService and
// DeviceTrustNavigationThrottle.  It saves and loads the key from platform
// specific storage locations and provides methods for accessing properties
// of the key.
//
// The class is implemented on Windows but the work is not yet complete on
// other desktop platforms.  Other platforms are covered by b/194387479 (Mac)
// and b/194891140 (Linux).  Until then this code will continue to store the
// signing key inside the profile prefs on those platforms.
class SigningKeyPair {
 public:
  using KeyTrustLevel =
      enterprise_management::BrowserPublicKeyUploadRequest::KeyTrustLevel;

  SigningKeyPair();
  SigningKeyPair(const SigningKeyPair&) = delete;
  SigningKeyPair& operator=(const SigningKeyPair&) = delete;
  ~SigningKeyPair();

  // Signs `message` and with the key and returns the signature.
  bool SignMessage(const std::string& message, std::string* signature);

  // Exports the public key to `public_key` as an X.509 SubjectPublicKeyInfo
  // block.
  bool ExportPublicKey(std::vector<uint8_t>* public_key);

  // Rotates the key pair.  The device must be enrolled with CBCM to succeeded.
  // Uses a platform mechanism to elevate and run RotateWithElevation().
  bool Rotate();

  // Rotates the key pair.  If no key pair already exists, simply creates a
  // new one. |dm_token| is a base64-encoded string containing the DM token
  // to use when sending the new public key to the DM server.  This function
  // will fail if not called with elevation.
  bool RotateWithElevation(const std::string& dm_token_base64);

  // Set a signing key for testing so that it does not need to be read from
  // platform specific storage.
  void SetKeyPairForTesting(
      std::unique_ptr<crypto::UnexportableSigningKey> key_pair);

 private:
  // Load key if available. Return true on success.
  void Init();

  // Stores the key in a platform specific location.  This method requires
  // elevation since it writes to a location that is shared by all OS users of
  // the device.  Returns true on success.
  bool StoreKeyPair();

  // Loads the key from a platform specific location.  Returns true on success.
  bool LoadKeyPair();

  std::unique_ptr<crypto::UnexportableSigningKey> key_pair_;
  KeyTrustLevel trust_level_ = enterprise_management::
      BrowserPublicKeyUploadRequest::KEY_TRUST_LEVEL_UNSPECIFIED;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_SIGNING_KEY_PAIR_H_
