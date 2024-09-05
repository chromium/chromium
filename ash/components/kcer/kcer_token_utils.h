// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_KCER_KCER_TOKEN_UTILS_H_
#define ASH_COMPONENTS_KCER_KCER_TOKEN_UTILS_H_

#include "ash/components/kcer/attributes.pb.h"
#include "ash/components/kcer/chaps/high_level_chaps_client.h"
#include "ash/components/kcer/chaps/session_chaps_client.h"
#include "ash/components/kcer/helpers/pkcs12_reader.h"
#include "ash/components/kcer/kcer.h"

namespace kcer::internal {

// Calculate PKCS#11 id (see CKA_ID) from the bytes of the public key. Designed
// to be backwards compatible with ids produced by NSS.
Pkcs11Id MakePkcs11Id(base::span<const uint8_t> public_key_data);

// Creates Public key SPKI for an RSA public key from its `modulus` and
// `exponent`.
PublicKeySpki MakeRsaSpki(const base::span<const uint8_t>& modulus,
                          const base::span<const uint8_t>& exponent);

// Creates kcer::PublicKey from an RSA public key data.
base::expected<PublicKey, Error> MakeRsaPublicKey(
    Token token,
    base::span<const uint8_t> modulus,
    base::span<const uint8_t> public_exponent);

// Creates Public key SPKI for an EC public key from its `ec_point`.
PublicKeySpki MakeEcSpki(const base::span<const uint8_t>& ec_point);

// Creates kcer::PublicKey from an EC public key data.
base::expected<PublicKey, Error> MakeEcPublicKey(
    Token token,
    base::span<const uint8_t> ec_point);

// Temporary class to share code between KcerTokenImpl and KcerTokenImplNss. Can
// be merged into KcerTokenImpl when KcerTokenImplNss is removed. Mostly
// contains operations that have to communicate with Chaps directly.
class KcerTokenUtils {
 public:
  using ObjectHandle = SessionChapsClient::ObjectHandle;
  using ImportPkcs12Callback =
      base::OnceCallback<void(bool /*did_modify*/,
                              base::expected<void, Error> /*result*/)>;

  // `chaps_client` must outlive KcerTokenUtils.
  KcerTokenUtils(Token token, HighLevelChapsClient* chaps_client);
  ~KcerTokenUtils();

  // Should be called before any other methods.
  void Initialize(SessionChapsClient::SlotId pkcs_11_slot_id);

  // Returns handles for private key objects with PKCS#11 `id` and PKCS11_CKR_OK
  // on success, or some other result code on failure. (In practice there should
  // be only 1 or 0 handles.)
  void FindPrivateKey(Pkcs11Id id,
                      base::OnceCallback<void(std::vector<ObjectHandle>,
                                              uint32_t result_code)> callback);

  // Creates a certificate object in Chaps. Does not check whether such an
  // object already exists. If `kcer_error` is not empty - import failed without
  // talking with Chaps. Otherwise returns the result from Chaps.
  void ImportCert(const bssl::UniquePtr<X509>& cert,
                  const Pkcs11Id& pkcs11_id,
                  const std::string& nickname,
                  const CertDer& cert_der,
                  bool is_hardware_backed,
                  bool mark_as_migrated,
                  base::OnceCallback<void(std::optional<Error> kcer_error,
                                          ObjectHandle cert_handle,
                                          uint32_t result_code)> callback);

  // Imports an EVP_KEY into Chaps as a pair of public and private objects.
  // Skips the actual import if the key already exists.
  struct ImportKeyTask {
    ImportKeyTask(KeyData in_key_data,
                  bool in_hardware_backed,
                  bool in_mark_as_migrated,
                  Kcer::GenerateKeyCallback in_callback);
    ImportKeyTask(ImportKeyTask&& other);
    ~ImportKeyTask();

    KeyData key_data;
    const bool hardware_backed;
    const bool mark_as_migrated;
    Kcer::GenerateKeyCallback callback;
    int attemps_left = 5;
  };
  void ImportKey(ImportKeyTask task);

  void ImportPkcs12(KeyData key_data,
                    std::vector<CertData> certs_data,
                    bool hardware_backed,
                    bool mark_as_migrated,
                    ImportPkcs12Callback callback);

 private:
  void ImportRsaKey(ImportKeyTask task);
  void ImportRsaKeyWithExistingKey(ImportKeyTask task,
                                   bssl::UniquePtr<RSA> rsa_key,
                                   PublicKey kcer_public_key,
                                   std::vector<ObjectHandle> handles,
                                   uint32_t result_code);
  void DidImportRsaPrivateKey(ImportKeyTask task,
                              PublicKey kcer_public_key,
                              std::vector<uint8_t> public_modulus_bytes,
                              std::vector<uint8_t> public_exponent_bytes,
                              ObjectHandle priv_key_handle,
                              uint32_t result_code);
  void ImportEcKey(ImportKeyTask task);
  void ImportEcKeyWithExistingKey(ImportKeyTask task,
                                  bssl::UniquePtr<EC_KEY> ec_key,
                                  PublicKey kcer_public_key,
                                  std::vector<uint8_t> ec_point_oct,
                                  std::vector<ObjectHandle> handles,
                                  uint32_t result_code);
  void DidImportEcPrivateKey(ImportKeyTask task,
                             PublicKey kcer_public_key,
                             std::vector<uint8_t> ec_point_der,
                             std::vector<uint8_t> ec_params_der,
                             ObjectHandle priv_key_handle,
                             uint32_t result_code);
  void DidImportKey(ImportKeyTask task,
                    PublicKey kcer_public_key,
                    ObjectHandle priv_key_handle,
                    ObjectHandle pub_key_handle,
                    uint32_t result_code);

  void ImportPkc12DidImportKey(kcer::KeyType key_type,
                               Pkcs11Id pkcs11_id,
                               std::vector<CertData> certs_data,
                               bool hardware_backed,
                               bool mark_as_migrated,
                               ImportPkcs12Callback callback,
                               base::expected<PublicKey, Error> imported_key);

  struct ImportAllCertsTask {
    ImportAllCertsTask(Pkcs11Id in_pkcs11_id,
                       std::vector<CertData> in_certs_data,
                       bool in_hardware_backed,
                       bool in_mark_as_migrated,
                       bool in_multi_cert_import,
                       KeyType in_key_type,
                       ImportPkcs12Callback in_callback);
    ImportAllCertsTask(ImportAllCertsTask&& other);
    ~ImportAllCertsTask();

    Pkcs11Id pkcs11_id;
    std::vector<CertData> certs_data;
    const bool hardware_backed;
    const bool mark_as_migrated;
    const bool multi_cert_import;
    KeyType key_type;
    ImportPkcs12Callback callback;
    int attemps_left = 5;
  };
  void ImportAllCerts(ImportAllCertsTask task);
  void ImportAllCertsImpl(ImportAllCertsTask task,
                          std::vector<const CertData*> certs_data,
                          int imports_failed);
  void ImportAllCertsDidImportOneCert(
      ImportAllCertsTask task,
      std::vector<const CertData*> certs_data,
      int imports_failed,
      std::optional<Error> kcer_error,
      SessionChapsClient::ObjectHandle cert_handle,
      uint32_t result_code);

  const Token token_;
  // The id of the slot associated with this token. It's used to perform D-Bus
  // requests to Chaps. The default value is very unlikely to represent any real
  // slot and is not used until it's overwritten in Initialize.
  SessionChapsClient::SlotId pkcs_11_slot_id_ =
      SessionChapsClient::SlotId(0xFFFFFFFF);
  const raw_ptr<HighLevelChapsClient> chaps_client_;
  base::WeakPtrFactory<KcerTokenUtils> weak_factory_{this};
};

}  // namespace kcer::internal

#endif  // ASH_COMPONENTS_KCER_KCER_TOKEN_UTILS_H_
