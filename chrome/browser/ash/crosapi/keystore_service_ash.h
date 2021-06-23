// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_KEYSTORE_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_KEYSTORE_SERVICE_ASH_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/platform_keys/platform_keys.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {
namespace attestation {
class TpmChallengeKey;
struct TpmChallengeKeyResult;
}  // namespace attestation
}  // namespace ash

namespace chromeos {
namespace platform_keys {
class PlatformKeysService;
class KeyPermissionsService;
}  // namespace platform_keys
}  // namespace chromeos

namespace crosapi {

// This class is the ash implementation of the KeystoreService crosapi. It
// allows lacros to expose blessed extension APIs which can query or modify the
// system keystores. This class is affine to the UI thread.
class KeystoreServiceAsh : public mojom::KeystoreService, public KeyedService {
 public:
  using KeystoreType = mojom::KeystoreType;
  using SigningScheme = mojom::KeystoreSigningScheme;

  explicit KeystoreServiceAsh(content::BrowserContext* fixed_context);
  // Allows to create the service early. It will use the current primary profile
  // whenever used. The profile should be specified explicitly when possible.
  KeystoreServiceAsh();
  // For testing only.
  explicit KeystoreServiceAsh(
      chromeos::platform_keys::PlatformKeysService* platform_keys_service,
      chromeos::platform_keys::KeyPermissionsService* key_permissions_service);
  KeystoreServiceAsh(const KeystoreServiceAsh&) = delete;
  KeystoreServiceAsh& operator=(const KeystoreServiceAsh&) = delete;
  ~KeystoreServiceAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::KeystoreService> receiver);

  // mojom::KeystoreService:
  void ChallengeAttestationOnlyKeystore(
      const std::string& challenge,
      mojom::KeystoreType type,
      bool migrate,
      ChallengeAttestationOnlyKeystoreCallback callback) override;
  void GetKeyStores(GetKeyStoresCallback callback) override;
  void SelectClientCertificates(
      const std::vector<std::vector<uint8_t>>& certificate_authorities,
      SelectClientCertificatesCallback callback) override;
  void GetCertificates(mojom::KeystoreType keystore,
                       GetCertificatesCallback callback) override;
  void AddCertificate(mojom::KeystoreType keystore,
                      const std::vector<uint8_t>& certificate,
                      AddCertificateCallback callback) override;
  void RemoveCertificate(mojom::KeystoreType keystore,
                         const std::vector<uint8_t>& certificate,
                         RemoveCertificateCallback callback) override;
  void GetPublicKey(const std::vector<uint8_t>& certificate,
                    mojom::KeystoreSigningAlgorithmName algorithm_name,
                    GetPublicKeyCallback callback) override;
  void ExtensionGenerateKey(mojom::KeystoreType keystore,
                            mojom::KeystoreSigningAlgorithmPtr algorithm,
                            const absl::optional<std::string>& extension_id,
                            ExtensionGenerateKeyCallback callback) override;
  void ExtensionSign(KeystoreType keystore,
                     const std::vector<uint8_t>& public_key,
                     SigningScheme scheme,
                     const std::vector<uint8_t>& data,
                     const std::string& extension_id,
                     ExtensionSignCallback callback) override;
  void GenerateKey(mojom::KeystoreType keystore,
                   mojom::KeystoreSigningAlgorithmPtr algorithm,
                   GenerateKeyCallback callback) override;
  void RemoveKey(KeystoreType keystore,
                 const std::vector<uint8_t>& public_key,
                 RemoveKeyCallback callback) override;
  void Sign(bool is_keystore_provided,
            KeystoreType keystore,
            const std::vector<uint8_t>& public_key,
            SigningScheme scheme,
            const std::vector<uint8_t>& data,
            SignCallback callback) override;
  void GetKeyTags(const std::vector<uint8_t>& public_key,
                  GetKeyTagsCallback callback) override;
  void AddKeyTags(const std::vector<uint8_t>& public_key,
                  uint64_t tags,
                  AddKeyTagsCallback callback) override;
  void CanUserGrantPermissionForKey(
      const std::vector<uint8_t>& public_key,
      CanUserGrantPermissionForKeyCallback callback) override;

 private:
  // Returns a correct instance of PlatformKeysService to use. If a specific
  // browser context was passed into constructor, the corresponding
  // PlatformKeysService instance will be used for all operations.
  // Otherwise the class will use an instance for the primary profile.
  chromeos::platform_keys::PlatformKeysService* GetPlatformKeys();

  // Returns a correct instance of KeyPermissionsService to use. If a specific
  // browser context was passed into constructor, the corresponding
  // KeyPermissionsService instance will be used for all operations.
  // Otherwise the class will use an instance for the primary profile.
  chromeos::platform_keys::KeyPermissionsService* GetKeyPermissions();

  // |challenge| is used as a opaque identifier to match against the
  // unique_ptr in outstanding_challenges_. It should not be dereferenced.
  void DidChallengeAttestationOnlyKeystore(
      ChallengeAttestationOnlyKeystoreCallback callback,
      void* challenge,
      const ash::attestation::TpmChallengeKeyResult& result);
  static void DidGetKeyStores(
      GetKeyStoresCallback callback,
      std::unique_ptr<std::vector<chromeos::platform_keys::TokenId>>
          platform_keys_token_ids,
      chromeos::platform_keys::Status status);
  static void DidSelectClientCertificates(
      SelectClientCertificatesCallback callback,
      std::unique_ptr<net::CertificateList> matches,
      chromeos::platform_keys::Status status);
  static void DidGetCertificates(GetCertificatesCallback callback,
                                 std::unique_ptr<net::CertificateList> certs,
                                 chromeos::platform_keys::Status status);
  static void DidImportCertificate(AddCertificateCallback callback,
                                   chromeos::platform_keys::Status status);
  static void DidRemoveCertificate(RemoveCertificateCallback callback,
                                   chromeos::platform_keys::Status status);
  static void DidExtensionGenerateKey(
      ExtensionGenerateKeyCallback callback,
      const std::string& public_key,
      absl::optional<crosapi::mojom::KeystoreError> error);
  static void DidExtensionSign(ExtensionSignCallback callback,
                               const std::string& signature,
                               absl::optional<mojom::KeystoreError> error);
  static void DidGenerateKey(GenerateKeyCallback callback,
                             const std::string& public_key,
                             chromeos::platform_keys::Status status);
  static void DidRemoveKey(RemoveKeyCallback callback,
                           chromeos::platform_keys::Status status);
  static void DidSign(SignCallback callback,
                      const std::string& signature,
                      chromeos::platform_keys::Status status);
  static void DidGetKeyTags(GetKeyTagsCallback callback,
                            absl::optional<bool> corporate,
                            chromeos::platform_keys::Status status);
  static void DidAddKeyTags(AddKeyTagsCallback callback,
                            chromeos::platform_keys::Status status);

  // Can be nullptr, should not be used directly, use GetPlatformKeys() instead.
  // Stores a pointer to a specific PlatformKeysService if it was specified in
  // constructor.
  chromeos::platform_keys::PlatformKeysService* const
      fixed_platform_keys_service_ = nullptr;
  // Can be nullptr, should not be used directly, use GetKeyPermissions()
  // instead. Stores a pointer to a specific KeyPermissionsService if it was
  // specified in constructor.
  chromeos::platform_keys::KeyPermissionsService* const
      fixed_key_permissions_service_ = nullptr;

  // Container to keep outstanding challenges alive.
  std::vector<std::unique_ptr<ash::attestation::TpmChallengeKey>>
      outstanding_challenges_;
  mojo::ReceiverSet<mojom::KeystoreService> receivers_;

  base::WeakPtrFactory<KeystoreServiceAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_KEYSTORE_SERVICE_ASH_H_
