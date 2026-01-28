// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_KEYSTORE_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_KEYSTORE_SERVICE_ASH_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/platform_keys/keystore_types.h"
#include "chromeos/ash/components/platform_keys/platform_keys.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {
namespace attestation {
class TpmChallengeKey;
struct TpmChallengeKeyResult;
}  // namespace attestation
namespace platform_keys {
class KeyPermissionsService;
class PlatformKeysService;
}  // namespace platform_keys
}  // namespace ash

namespace crosapi {

// TODO(crbug.com/365902693): Update this comment.
// This class is the ash implementation of the KeystoreService crosapi. It
// allows lacros to expose blessed extension APIs which can query or modify the
// system keystores. This class is affine to the UI thread.
class KeystoreServiceAsh : public KeyedService {
 public:
  using KeystoreType = mojom::KeystoreType;
  using SigningScheme = chromeos::KeystoreSigningScheme;
  using KeystoreKeyAttributeType = chromeos::KeystoreKeyAttributeType;

  using ChallengeAttestationOnlyKeystoreCallback = base::OnceCallback<void(
      chromeos::ChallengeAttestationOnlyKeystoreResult)>;
  using GetKeyStoresCallback =
      base::OnceCallback<void(crosapi::mojom::GetKeyStoresResultPtr)>;
  using SelectClientCertificatesCallback = base::OnceCallback<void(
      crosapi::mojom::KeystoreSelectClientCertificatesResultPtr)>;
  using GetCertificatesCallback =
      base::OnceCallback<void(crosapi::mojom::GetCertificatesResultPtr)>;
  using AddCertificateCallback =
      base::OnceCallback<void(bool, crosapi::mojom::KeystoreError)>;
  using RemoveCertificateCallback =
      base::OnceCallback<void(bool, crosapi::mojom::KeystoreError)>;
  using GetPublicKeyCallback =
      base::OnceCallback<void(crosapi::mojom::GetPublicKeyResultPtr)>;
  using GenerateKeyCallback =
      base::OnceCallback<void(crosapi::mojom::KeystoreBinaryResultPtr)>;
  using RemoveKeyCallback =
      base::OnceCallback<void(bool, crosapi::mojom::KeystoreError)>;
  using SignCallback =
      base::OnceCallback<void(crosapi::mojom::KeystoreBinaryResultPtr)>;
  using GetKeyTagsCallback =
      base::OnceCallback<void(crosapi::mojom::GetKeyTagsResultPtr)>;
  using AddKeyTagsCallback =
      base::OnceCallback<void(bool, crosapi::mojom::KeystoreError)>;
  using CanUserGrantPermissionForKeyCallback = base::OnceCallback<void(bool)>;
  using SetAttributeForKeyCallback =
      base::OnceCallback<void(bool, crosapi::mojom::KeystoreError)>;

  explicit KeystoreServiceAsh(content::BrowserContext* fixed_context);
  // Allows to create the service early. It will use the current primary profile
  // whenever used. The profile should be specified explicitly when possible.
  KeystoreServiceAsh();
  // For testing only.
  explicit KeystoreServiceAsh(
      ash::platform_keys::PlatformKeysService* platform_keys_service,
      ash::platform_keys::KeyPermissionsService* key_permissions_service);
  KeystoreServiceAsh(const KeystoreServiceAsh&) = delete;
  KeystoreServiceAsh& operator=(const KeystoreServiceAsh&) = delete;
  ~KeystoreServiceAsh() override;

  void ChallengeAttestationOnlyKeystore(
      mojom::KeystoreType type,
      const std::vector<uint8_t>& challenge,
      bool migrate,
      chromeos::KeystoreAlgorithmName algorithm,
      ChallengeAttestationOnlyKeystoreCallback callback);
  void GetKeyStores(GetKeyStoresCallback callback);
  void SelectClientCertificates(
      const std::vector<std::vector<uint8_t>>& certificate_authorities,
      SelectClientCertificatesCallback callback);
  void GetCertificates(mojom::KeystoreType keystore,
                       GetCertificatesCallback callback);
  void AddCertificate(mojom::KeystoreType keystore,
                      const std::vector<uint8_t>& certificate,
                      AddCertificateCallback callback);
  void RemoveCertificate(mojom::KeystoreType keystore,
                         const std::vector<uint8_t>& certificate,
                         RemoveCertificateCallback callback);
  void GetPublicKey(const std::vector<uint8_t>& certificate,
                    chromeos::KeystoreAlgorithmName algorithm_name,
                    GetPublicKeyCallback callback);
  void GenerateKey(mojom::KeystoreType keystore,
                   mojom::KeystoreAlgorithmPtr algorithm,
                   GenerateKeyCallback callback);
  void RemoveKey(KeystoreType keystore,
                 const std::vector<uint8_t>& public_key,
                 RemoveKeyCallback callback);
  void Sign(std::optional<KeystoreType> keystore,
            const std::vector<uint8_t>& public_key,
            SigningScheme scheme,
            const std::vector<uint8_t>& data,
            SignCallback callback);
  void GetKeyTags(const std::vector<uint8_t>& public_key,
                  GetKeyTagsCallback callback);
  void AddKeyTags(const std::vector<uint8_t>& public_key,
                  uint64_t tags,
                  AddKeyTagsCallback callback);
  void CanUserGrantPermissionForKey(
      const std::vector<uint8_t>& public_key,
      CanUserGrantPermissionForKeyCallback callback);
  void SetAttributeForKey(KeystoreType keystore,
                          const std::vector<uint8_t>& public_key,
                          KeystoreKeyAttributeType attribute_type,
                          const std::vector<uint8_t>& attribute_value,
                          SetAttributeForKeyCallback callback);

 private:
  // Returns a correct instance of PlatformKeysService to use. If a specific
  // browser context was passed into constructor, the corresponding
  // PlatformKeysService instance will be used for all operations.
  // Otherwise the class will use an instance for the primary profile.
  ash::platform_keys::PlatformKeysService* GetPlatformKeys();

  // Returns a correct instance of KeyPermissionsService to use. If a specific
  // browser context was passed into constructor, the corresponding
  // KeyPermissionsService instance will be used for all operations.
  // Otherwise the class will use an instance for the primary profile.
  ash::platform_keys::KeyPermissionsService* GetKeyPermissions();

  // |challenge_key_ptr| is used as a opaque identifier to match against the
  // unique_ptr in outstanding_challenges_. It should not be dereferenced.
  void DidChallengeAttestationOnlyKeystore(
      ChallengeAttestationOnlyKeystoreCallback callback,
      void* challenge_key_ptr,
      const ash::attestation::TpmChallengeKeyResult& result);
  static void DidGetKeyStores(
      GetKeyStoresCallback callback,
      const std::vector<chromeos::platform_keys::TokenId>
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
  static void DidGenerateKey(GenerateKeyCallback callback,
                             std::vector<uint8_t> public_key,
                             chromeos::platform_keys::Status status);
  static void DidRemoveKey(RemoveKeyCallback callback,
                           chromeos::platform_keys::Status status);
  static void DidSign(SignCallback callback,
                      std::vector<uint8_t> signature,
                      chromeos::platform_keys::Status status);
  static void DidGetKeyTags(GetKeyTagsCallback callback,
                            std::optional<bool> corporate,
                            chromeos::platform_keys::Status status);
  static void DidAddKeyTags(AddKeyTagsCallback callback,
                            chromeos::platform_keys::Status status);
  static void DidSetAttributeForKey(SetAttributeForKeyCallback callback,
                                    chromeos::platform_keys::Status status);

  // Can be nullptr, should not be used directly, use GetPlatformKeys() instead.
  // Stores a pointer to a specific PlatformKeysService if it was specified in
  // constructor.
  const raw_ptr<ash::platform_keys::PlatformKeysService>
      fixed_platform_keys_service_ = nullptr;
  // Can be nullptr, should not be used directly, use GetKeyPermissions()
  // instead. Stores a pointer to a specific KeyPermissionsService if it was
  // specified in constructor.
  const raw_ptr<ash::platform_keys::KeyPermissionsService>
      fixed_key_permissions_service_ = nullptr;

  // Container to keep outstanding challenges alive. The challenges should be
  // destroyed together with this service to reduce the chance of them accessing
  // other services that may be deleted by that point.
  std::vector<std::unique_ptr<ash::attestation::TpmChallengeKey>>
      outstanding_challenges_;

  base::WeakPtrFactory<KeystoreServiceAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_KEYSTORE_SERVICE_ASH_H_
