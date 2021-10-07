// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_SIGNING_KEY_PAIR_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_SIGNING_KEY_PAIR_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace crypto {
class UnexportableKeyProvider;
class UnexportableSigningKey;
}  // namespace crypto

namespace enterprise_connectors {

namespace test {
class ScopedTpmSigningKeyPair;
}  // namespace test

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

  using KeyInfo = std::pair<KeyTrustLevel, std::vector<uint8_t>>;

  // A delegate class that handles persistence of the key pair.  There is an
  // implementation for each platform and also for tests.
  class PersistenceDelegate {
   public:
    virtual ~PersistenceDelegate() = default;

    // Stores the trust level and wrapped key in a platform specific location.
    // This method requires elevation since it writes to a location that is
    // shared by all OS users of the device.  Returns true on success.
    virtual bool StoreKeyPair(KeyTrustLevel trust_level,
                              std::vector<uint8_t> wrapped);

    // Loads the key from a platform specific location.  Returns
    // BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED and an empty vector if the trust level
    // or wrapped bits could not be loaded.
    virtual KeyInfo LoadKeyPair();
  };

  // A delegate class that handles network requests to DM server.  There is an
  // implementation for each platform and also for tests.
  class NetworkDelegate {
   public:
    virtual ~NetworkDelegate() = default;

    // Sends `body`, which is a serialized DeviceManagementRequest, to DM
    // server at `url`.  `dm_token` authn the local machine.  Only the
    // BrowserPublicKeyUploadRequest member is expected to be initialized.
    //
    // The return // value is a string that can be parsed into
    // DeviceManagementResponse.
    virtual std::string SendPublicKeyToDmServerSync(const std::string& url,
                                                    const std::string& dm_token,
                                                    const std::string& body);
  };

  static std::unique_ptr<SigningKeyPair> Create();

  // Factory function that creates a SigningKeyPair with specific delegates
  // used in some tests to control SigningKeyPair behaviour.
  static std::unique_ptr<SigningKeyPair> CreateWithDelegates(
      std::unique_ptr<PersistenceDelegate> persistence_delegate,
      std::unique_ptr<NetworkDelegate> network_delegate);

  SigningKeyPair(const SigningKeyPair&) = delete;
  SigningKeyPair& operator=(const SigningKeyPair&) = delete;
  virtual ~SigningKeyPair();

  // Signs `message` and with the key and returns the signature.
  bool SignMessage(const std::string& message, std::string* signature);

  // Exports the public key to `public_key` as an X.509 SubjectPublicKeyInfo
  // block.
  bool ExportPublicKey(std::vector<uint8_t>* public_key);

  // Rotates the key pair.  If no key pair already exists, simply creates a
  // new one.  `dm_token` the DM token to use when sending the new public key to
  // the DM server.  This function will fail if not called with admin rights.
  bool RotateWithAdminRights(const std::string& dm_token) WARN_UNUSED_RESULT;

 protected:
  SigningKeyPair(std::unique_ptr<PersistenceDelegate> persistence_delegate,
                 std::unique_ptr<NetworkDelegate> network_delegate);

  // Initialize the key pair by loading from persistent storage.
  void Init();

 private:
  friend class test::ScopedTpmSigningKeyPair;

  // Sets a `wrapped` TPM key and force its usage in test environments.
  static void SetTpmKeyWrappedForTesting(const std::vector<uint8_t>& wrapped);

  // Clears any previously stored wrapped TPM key.
  static void ClearTpmKeyWrappedForTesting();

  // Returns the TPM-backed signing key provider for the platform if available.
  std::unique_ptr<crypto::UnexportableKeyProvider> GetTpmBackedKeyProvider();

  // Builds the protobuf message needed to tell DM server about the new public
  // for this device.  `nonce` is an opaque binary blob and should not be
  // treated as an ASCII or UTF-8 string.
  bool BuildUploadPublicKeyRequest(
      KeyTrustLevel new_trust_level,
      const std::unique_ptr<crypto::UnexportableSigningKey>& new_key_pair,
      const std::string& nonce,
      enterprise_management::BrowserPublicKeyUploadRequest* request);

  std::unique_ptr<crypto::UnexportableSigningKey> key_pair_;
  KeyTrustLevel trust_level_ = enterprise_management::
      BrowserPublicKeyUploadRequest::KEY_TRUST_LEVEL_UNSPECIFIED;

  std::unique_ptr<PersistenceDelegate> persistence_delegate_;
  std::unique_ptr<NetworkDelegate> network_delegate_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_DESKTOP_SIGNING_KEY_PAIR_H_
