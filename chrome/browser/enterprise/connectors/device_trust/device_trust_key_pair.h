// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_KEY_PAIR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_KEY_PAIR_H_

#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "crypto/ec_private_key.h"

namespace enterprise_connectors {

// This class provides functionality used in `DeviceTrustService` class to
// enable Device Trust Connector.
// An instantiation of `DeviceTrustKeyPair` class will be restricted to a single
// instance consisting of a `key_pair_` that is composed of a private key and a
// public key linked to a set of specifics `origins`. This key pair will be
// stored in the local state prefs. In the case of the private key, an encrypted
// version is stored using `OSCrypt`.
// `origins` is a list of URLs which indicates the endpoints where the key pair
// will be allowed to interact with.
// The interaction of an endpoint and the key pair  a.k.a. handshake will be
// handled by a `NavigationHandle` that will fire this flow when a specific URL
// matches with any of the ones in `origins`. Then the  key pair will be
// challenged in the attestation process, generating a challenge-response that
// will be sent back to the endpoint.
//
//  Example:
//    std::unique_ptr<DeviceTrustKeyPair> key_pair =
//        std::make_unique<DeviceTrustKeyPair>();
//
class DeviceTrustKeyPair {
 public:
  DeviceTrustKeyPair();
  DeviceTrustKeyPair(const DeviceTrustKeyPair&) = delete;
  DeviceTrustKeyPair& operator=(const DeviceTrustKeyPair&) = delete;
  ~DeviceTrustKeyPair();

  // Load key pair from prefs if available, if not, it will generate a
  // new EC P-256 key pair and store the encoded encrypted version of it into
  // prefs. Return true on success.
  bool Init();

  // Sign `message` using elliptic curve (EC) P-256 private key.
  bool SignMessage(const std::string& message, std::string* signature);

  // Exports the public key to `public_key` as an X.509 SubjectPublicKeyInfo
  // block.
  bool ExportPublicKey(std::vector<uint8_t>* public_key);

 private:
  std::unique_ptr<crypto::ECPrivateKey> key_pair_;

  // Store encrypted private key and public key into prefs.
  // Returns true on success.
  bool StoreKeyPair();

  // Loads the key pair that was stored in the local state.
  // Returns true on success.
  bool LoadKeyPair();
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_DEVICE_TRUST_KEY_PAIR_H_
