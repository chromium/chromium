// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_KCER_KCER_TOKEN_IMPL_H_
#define ASH_COMPONENTS_KCER_KCER_TOKEN_IMPL_H_

#include <stdint.h>

#include <deque>
#include <string>

#include "ash/components/kcer/attributes.pb.h"
#include "ash/components/kcer/cert_cache.h"
#include "ash/components/kcer/chaps/high_level_chaps_client.h"
#include "ash/components/kcer/chaps/session_chaps_client.h"
#include "ash/components/kcer/helpers/pkcs12_reader.h"
#include "ash/components/kcer/kcer_token.h"
#include "ash/components/kcer/kcer_token_utils.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "net/cert/cert_database.h"

namespace kcer::internal {

// The implementation of KcerToken that directly communicates with Chaps.
class COMPONENT_EXPORT(KCER) KcerTokenImpl
    : public KcerToken,
      public net::CertDatabase::Observer {
 public:
  enum class CacheState {
    // Cache must be updated before it can be used.
    kOutdated,
    // Cache is currently being updated.
    kUpdating,
    // Cache is up-to-date and can be used.
    kUpToDate,
  };

  // `chaps_client` must outlive KcerTokenImpl.
  KcerTokenImpl(Token token, HighLevelChapsClient* chaps_client);
  ~KcerTokenImpl() override;

  KcerTokenImpl(const KcerTokenImpl&) = delete;
  KcerTokenImpl& operator=(const KcerTokenImpl&) = delete;
  KcerTokenImpl(KcerTokenImpl&&) = delete;
  KcerTokenImpl& operator=(KcerTokenImpl&&) = delete;

  // Returns a weak pointer for the token. The pointer can be used to post tasks
  // for the token.
  base::WeakPtr<KcerToken> GetWeakPtr() override;

  // Initializes the token with a PKCS#11 id of a slot.
  void InitializeWithoutNss(SessionChapsClient::SlotId pkcs11_slot_id) override;

  // Implements net::CertDatabase::Observer.
  void OnClientCertStoreChanged() override;

  // Implements KcerToken.
  void GenerateRsaKey(RsaModulusLength modulus_length_bits,
                      bool hardware_backed,
                      Kcer::GenerateKeyCallback callback) override;
  void GenerateEcKey(EllipticCurve curve,
                     bool hardware_backed,
                     Kcer::GenerateKeyCallback callback) override;
  void ImportKey(Pkcs8PrivateKeyInfoDer pkcs8_private_key_info_der,
                 Kcer::ImportKeyCallback callback) override;
  void ImportCertFromBytes(CertDer cert_der,
                           Kcer::StatusCallback callback) override;
  void ImportPkcs12Cert(Pkcs12Blob pkcs12_blob,
                        std::string password,
                        bool hardware_backed,
                        bool mark_as_migrated,
                        Kcer::StatusCallback callback) override;
  void ExportPkcs12Cert(scoped_refptr<const Cert> cert,
                        Kcer::ExportPkcs12Callback callback) override;
  void RemoveKeyAndCerts(PrivateKeyHandle key,
                         Kcer::StatusCallback callback) override;
  void RemoveCert(scoped_refptr<const Cert> cert,
                  Kcer::StatusCallback callback) override;
  void ListKeys(TokenListKeysCallback callback) override;
  void ListCerts(TokenListCertsCallback callback) override;
  void DoesPrivateKeyExist(PrivateKeyHandle key,
                           Kcer::DoesKeyExistCallback callback) override;
  void Sign(PrivateKeyHandle key,
            SigningScheme signing_scheme,
            DataToSign data,
            Kcer::SignCallback callback) override;
  void SignRsaPkcs1Raw(PrivateKeyHandle key,
                       DigestWithPrefix digest_with_prefix,
                       Kcer::SignCallback callback) override;
  void GetTokenInfo(Kcer::GetTokenInfoCallback callback) override;
  void GetKeyInfo(PrivateKeyHandle key,
                  Kcer::GetKeyInfoCallback callback) override;
  void GetKeyPermissions(PrivateKeyHandle key,
                         Kcer::GetKeyPermissionsCallback callback) override;
  void GetCertProvisioningProfileId(
      PrivateKeyHandle key,
      Kcer::GetCertProvisioningProfileIdCallback callback) override;
  void SetKeyNickname(PrivateKeyHandle key,
                      std::string nickname,
                      Kcer::StatusCallback callback) override;
  void SetKeyPermissions(PrivateKeyHandle key,
                         chaps::KeyPermissions key_permissions,
                         Kcer::StatusCallback callback) override;
  void SetCertProvisioningProfileId(PrivateKeyHandle key,
                                    std::string profile_id,
                                    Kcer::StatusCallback callback) override;

  // Public for tests.
  static constexpr int kDefaultAttempts = 5;

 private:
  using ObjectHandle = SessionChapsClient::ObjectHandle;

  // Wraps `callback` in a helper method that will unblock the queue before
  // running the callback. It'll also unblock the queue if the callback goes out
  // of scope.
  template <typename... Args>
  base::OnceCallback<void(Args...)> BlockQueueGetUnblocker(
      base::OnceCallback<void(Args...)> callback);
  // Immediately unblocks the queue and attempts to perform the next task.
  void UnblockQueueProcessNextTask();

  struct GenerateRsaKeyTask {
    GenerateRsaKeyTask(RsaModulusLength in_modulus_length_bits,
                       bool in_hardware_backed,
                       Kcer::GenerateKeyCallback in_callback);
    GenerateRsaKeyTask(GenerateRsaKeyTask&& other);
    ~GenerateRsaKeyTask();

    const RsaModulusLength modulus_length_bits;
    const bool hardware_backed;
    Kcer::GenerateKeyCallback callback;
    int attemps_left = kDefaultAttempts;
  };
  void GenerateRsaKeyImpl(GenerateRsaKeyTask task);
  void DidGenerateRsaKey(GenerateRsaKeyTask task,
                         ObjectHandle public_key_id,
                         ObjectHandle private_key_id,
                         uint32_t result_code);
  void DidGetRsaPublicKey(GenerateRsaKeyTask task,
                          ObjectHandle public_key_id,
                          ObjectHandle private_key_id,
                          chaps::AttributeList public_key_attributes,
                          uint32_t result_code);
  void DidAssignRsaKeyId(GenerateRsaKeyTask task,
                         ObjectHandle public_key_id,
                         ObjectHandle private_key_id,
                         PublicKey kcer_public_key,
                         uint32_t result_code);

  struct GenerateEcKeyTask {
    GenerateEcKeyTask(EllipticCurve in_curve,
                      bool in_hardware_backed,
                      Kcer::GenerateKeyCallback in_callback);
    GenerateEcKeyTask(GenerateEcKeyTask&& other);
    ~GenerateEcKeyTask();

    const EllipticCurve curve;
    const bool hardware_backed;
    Kcer::GenerateKeyCallback callback;
    int attemps_left = kDefaultAttempts;
  };
  void GenerateEcKeyImpl(GenerateEcKeyTask task);
  void DidGenerateEcKey(GenerateEcKeyTask task,
                        ObjectHandle public_key_id,
                        ObjectHandle private_key_id,
                        uint32_t result_code);
  void DidGetEcPublicKey(GenerateEcKeyTask task,
                         ObjectHandle public_key_id,
                         ObjectHandle private_key_id,
                         chaps::AttributeList public_key_attributes,
                         uint32_t result_code);
  void DidAssignEcKeyId(GenerateEcKeyTask task,
                        ObjectHandle public_key_id,
                        ObjectHandle private_key_id,
                        PublicKey kcer_public_key,
                        uint32_t result_code);

  struct ImportCertFromBytesTask {
    ImportCertFromBytesTask(CertDer in_cert_der,
                            Kcer::StatusCallback in_callback);
    ImportCertFromBytesTask(ImportCertFromBytesTask&& other);
    ~ImportCertFromBytesTask();

    const CertDer cert_der;
    Kcer::StatusCallback callback;
    int attemps_left = kDefaultAttempts;
  };
  void ImportCertFromBytesImpl(ImportCertFromBytesTask task);
  void ImportCertFromBytesWithExistingCerts(
      ImportCertFromBytesTask task,
      std::vector<ObjectHandle> existing_certs,
      uint32_t result_code);
  void ImportCertFromBytesWithKeyHandle(ImportCertFromBytesTask task,
                                        Pkcs11Id pkcs11_id,
                                        std::vector<ObjectHandle> key_handles,
                                        uint32_t result_code);
  void DidImportCertFromBytes(ImportCertFromBytesTask task,
                              std::optional<Error> kcer_error,
                              ObjectHandle cert_handle,
                              uint32_t result_code);

  void DidImportPkcs12Cert(Kcer::StatusCallback callback,
                           bool did_modify,
                           base::expected<void, Error> import_result);

  struct RemoveKeyAndCertsTask {
    RemoveKeyAndCertsTask(PrivateKeyHandle in_key,
                          Kcer::StatusCallback in_callback);
    RemoveKeyAndCertsTask(RemoveKeyAndCertsTask&& other);
    ~RemoveKeyAndCertsTask();

    const PrivateKeyHandle key;
    Kcer::StatusCallback callback;
    int attemps_left = kDefaultAttempts;
  };
  void RemoveKeyAndCertsImpl(RemoveKeyAndCertsTask task);
  void RemoveKeyAndCertsWithObjectHandles(RemoveKeyAndCertsTask task,
                                          std::vector<ObjectHandle> handles,
                                          uint32_t result_code);
  void DidRemoveKeyAndCerts(RemoveKeyAndCertsTask task, uint32_t result_code);

  struct RemoveCertTask {
    RemoveCertTask(scoped_refptr<const Cert> in_cert,
                   Kcer::StatusCallback in_callback);
    RemoveCertTask(RemoveCertTask&& other);
    ~RemoveCertTask();

    const scoped_refptr<const Cert> cert;
    Kcer::StatusCallback callback;
    int attemps_left = kDefaultAttempts;
  };
  void RemoveCertImpl(RemoveCertTask task);
  void RemoveCertWithHandles(RemoveCertTask task,
                             std::vector<ObjectHandle> handles,
                             uint32_t result_code);
  void DidRemoveCert(RemoveCertTask task, uint32_t result_code);

  struct ListKeysTask {
    explicit ListKeysTask(TokenListKeysCallback in_callback);
    ListKeysTask(ListKeysTask&& other);
    ~ListKeysTask();

    TokenListKeysCallback callback;
    int attemps_left = kDefaultAttempts;
  };
  void ListKeysImpl(ListKeysTask task);
  void ListKeysWithRsaHandles(ListKeysTask task,
                              std::vector<ObjectHandle> handles,
                              uint32_t result_code);
  void ListKeysGetOneRsaKey(ListKeysTask task,
                            std::vector<ObjectHandle> handles,
                            std::vector<PublicKey> result_keys);
  void ListKeysDidGetOneRsaKey(ListKeysTask task,
                               std::vector<ObjectHandle> handles,
                               std::vector<PublicKey> result_keys,
                               chaps::AttributeList attributes,
                               uint32_t result_code);
  void ListKeysFindEcKeys(ListKeysTask task,
                          std::vector<PublicKey> result_keys);
  void ListKeysWithEcHandles(ListKeysTask task,
                             std::vector<PublicKey> result_keys,
                             std::vector<ObjectHandle> handles,
                             uint32_t result_code);
  void ListKeysGetOneEcKey(ListKeysTask task,
                           std::vector<ObjectHandle> handles,
                           std::vector<PublicKey> result_keys);
  void ListKeysDidGetOneEcKey(ListKeysTask task,
                              std::vector<ObjectHandle> handles,
                              std::vector<PublicKey> result_keys,
                              chaps::AttributeList attributes,
                              uint32_t result_code);
  void ListKeysDidFindEcPrivateKey(
      ListKeysTask task,
      std::vector<ObjectHandle> handles,
      std::vector<PublicKey> result_keys,
      PublicKey current_public_key,
      std::vector<ObjectHandle> private_key_handles,
      uint32_t result_code);

  struct DoesPrivateKeyExistTask {
    DoesPrivateKeyExistTask(PrivateKeyHandle in_key,
                            Kcer::DoesKeyExistCallback in_callback);
    DoesPrivateKeyExistTask(DoesPrivateKeyExistTask&& other);
    ~DoesPrivateKeyExistTask();

    const PrivateKeyHandle key;
    Kcer::DoesKeyExistCallback callback;
    int attemps_left = kDefaultAttempts;
  };
  void DoesPrivateKeyExistImpl(DoesPrivateKeyExistTask task);
  void DidDoesPrivateKeyExist(DoesPrivateKeyExistTask task,
                              std::vector<ObjectHandle> object_list,
                              uint32_t result_code);

  struct SignTask {
    SignTask(PrivateKeyHandle in_key,
             SigningScheme in_signing_scheme,
             DataToSign in_data,
             Kcer::SignCallback in_callback);
    SignTask(SignTask&& other);
    ~SignTask();

    const PrivateKeyHandle key;
    const SigningScheme signing_scheme;
    const DataToSign data;
    Kcer::SignCallback callback;
    int attemps_left = kDefaultAttempts;
  };
  void SignImpl(SignTask task);
  void SignWithKeyHandle(SignTask task,
                         std::vector<ObjectHandle> key_handles,
                         uint32_t result_code);
  void SignWithKeyHandleAndDigest(
      SignTask task,
      ObjectHandle key_handle,
      base::expected<DigestWithPrefix, Error> digest);
  void DidSign(SignTask task,
               std::vector<uint8_t> signature,
               uint32_t result_code);

  void NotifyCertsChanged(base::OnceClosure callback);

  struct SignRsaPkcs1RawTask {
    SignRsaPkcs1RawTask(PrivateKeyHandle in_key,
                        DigestWithPrefix in_digest_with_prefix,
                        Kcer::SignCallback in_callback);
    SignRsaPkcs1RawTask(SignRsaPkcs1RawTask&& other);
    ~SignRsaPkcs1RawTask();

    const PrivateKeyHandle key;
    const DigestWithPrefix digest_with_prefix;
    Kcer::SignCallback callback;
    int attemps_left = kDefaultAttempts;
  };
  void SignRsaPkcs1RawImpl(SignRsaPkcs1RawTask task);
  void SignRsaPkcs1RawWithKeyHandle(SignRsaPkcs1RawTask task,
                                    std::vector<ObjectHandle> key_handles,
                                    uint32_t result_code);
  void DidSignRsaPkcs1Raw(SignRsaPkcs1RawTask task,
                          std::vector<uint8_t> signature,
                          uint32_t result_code);

  struct GetKeyInfoTask {
    GetKeyInfoTask(PrivateKeyHandle in_key,
                   Kcer::GetKeyInfoCallback in_callback);
    GetKeyInfoTask(GetKeyInfoTask&& other);
    ~GetKeyInfoTask();

    const PrivateKeyHandle key;
    Kcer::GetKeyInfoCallback callback;
    int attemps_left = kDefaultAttempts;
  };
  void GetKeyInfoImpl(GetKeyInfoTask task);
  void GetKeyInfoWithMechanismList(GetKeyInfoTask task,
                                   const std::vector<uint64_t>& mechanism_list,
                                   uint32_t result_code);
  void GetKeyInfoGetAttributes(GetKeyInfoTask task);
  void GetKeyInfoWithAttributes(GetKeyInfoTask task,
                                std::optional<Error> kcer_error,
                                chaps::AttributeList attributes,
                                uint32_t result_code);

  struct GetKeyPermissionsTask {
    GetKeyPermissionsTask(PrivateKeyHandle in_key,
                          Kcer::GetKeyPermissionsCallback in_callback);
    GetKeyPermissionsTask(GetKeyPermissionsTask&& other);
    ~GetKeyPermissionsTask();

    const PrivateKeyHandle key;
    Kcer::GetKeyPermissionsCallback callback;
    int attemps_left = kDefaultAttempts;
  };
  void GetKeyPermissionsImpl(GetKeyPermissionsTask task);
  void GetKeyPermissionsWithAttributes(GetKeyPermissionsTask task,
                                       std::optional<Error> kcer_error,
                                       chaps::AttributeList attributes,
                                       uint32_t result_code);

  struct GetCertProvisioningIdTask {
    GetCertProvisioningIdTask(
        PrivateKeyHandle in_key,
        Kcer::GetCertProvisioningProfileIdCallback in_callback);
    GetCertProvisioningIdTask(GetCertProvisioningIdTask&& other);
    ~GetCertProvisioningIdTask();

    const PrivateKeyHandle key;
    Kcer::GetCertProvisioningProfileIdCallback callback;
    int attemps_left = kDefaultAttempts;
  };
  void GetCertProvisioningIdImpl(GetCertProvisioningIdTask task);
  void GetCertProvisioningIdWithAttributes(GetCertProvisioningIdTask task,
                                           std::optional<Error> kcer_error,
                                           chaps::AttributeList attributes,
                                           uint32_t result_code);

  struct SetKeyAttributeTask {
    SetKeyAttributeTask(PrivateKeyHandle in_key,
                        HighLevelChapsClient::AttributeId in_attribute_id,
                        std::vector<uint8_t> in_attribute_value,
                        Kcer::StatusCallback in_callback);
    SetKeyAttributeTask(SetKeyAttributeTask&& other);
    ~SetKeyAttributeTask();

    const PrivateKeyHandle key;
    const HighLevelChapsClient::AttributeId attribute_id;
    const std::vector<uint8_t> attribute_value;
    Kcer::StatusCallback callback;
    int attemps_left = kDefaultAttempts;
  };
  // Sets the `attribute_value` for the attribute with `attribute_id` on the
  // `key`. Assumes that the `task_queue_` is already blocked.
  void SetKeyAttribute(PrivateKeyHandle key,
                       HighLevelChapsClient::AttributeId attribute_id,
                       std::vector<uint8_t> attribute_value,
                       Kcer::StatusCallback callback);
  void SetKeyAttributeImpl(SetKeyAttributeTask task);
  void SetKeyAttributeWithHandle(SetKeyAttributeTask task,
                                 std::vector<ObjectHandle> private_key_handles,
                                 uint32_t result_code);
  void SetKeyAttributeDidSetAttribute(SetKeyAttributeTask task,
                                      uint32_t result_code);

  struct UpdateCacheTask {
    explicit UpdateCacheTask(base::OnceClosure in_callback);
    UpdateCacheTask(UpdateCacheTask&& other);
    ~UpdateCacheTask();

    base::OnceClosure callback;
    int attemps_left = kDefaultAttempts;
  };
  void UpdateCache();
  void UpdateCacheImpl(UpdateCacheTask task);
  void UpdateCacheWithCertHandles(UpdateCacheTask task,
                                  std::vector<ObjectHandle> handles,
                                  uint32_t result_code);
  void UpdateCacheGetOneCert(UpdateCacheTask task,
                             std::vector<ObjectHandle> handles,
                             std::vector<scoped_refptr<const Cert>> certs);
  void UpdateCacheDidGetOneCert(UpdateCacheTask task,
                                std::vector<ObjectHandle> handles,
                                std::vector<scoped_refptr<const Cert>> certs,
                                chaps::AttributeList attributes,
                                uint32_t result_code);
  void UpdateCacheWithCerts(
      UpdateCacheTask task,
      base::expected<std::vector<scoped_refptr<const Cert>>, Error> certs);

  // If `kcer_error` is not empty, the rest of the values can be discarded.
  // Otherwise `attributes` and `result_code` contain the reply from Chaps
  // (`resul_code` can still contain an error).
  using GetKeyAttributesCallback =
      base::OnceCallback<void(std::optional<Error> kcer_error,
                              chaps::AttributeList attributes,
                              uint32_t result_code)>;

  // Retrieves attributes with `attribute_ids` for the key.
  void GetKeyAttributes(
      PrivateKeyHandle key,
      std::vector<HighLevelChapsClient::AttributeId> attribute_ids,
      GetKeyAttributesCallback callback);
  void GetKeyAttributesWithKeyHandle(
      std::vector<HighLevelChapsClient::AttributeId> attribute_ids,
      GetKeyAttributesCallback callback,
      std::vector<ObjectHandle> private_key_handles,
      uint32_t result_code);

  // Indicates whether the task queue is blocked. Task queue should be
  // blocked until the token is initialized, during the processing of most
  // requests and during the cache update.
  bool is_blocked_ = true;
  CacheState cache_state_ = CacheState::kOutdated;
  // Token type of this KcerToken.
  const Token token_;
  // The id of the slot associated with this token. It's used to perform D-Bus
  // requests to Chaps. The default value is very unlikely to represent any real
  // slot and is not used until it's overwritten in InitializeWithoutNss.
  SessionChapsClient::SlotId pkcs_11_slot_id_ =
      SessionChapsClient::SlotId(0xFFFFFFFF);
  // Indicates whether PSS signatures are supported. This variable caches the
  // value from Chaps, if it's empty, it needs to be retrieved first.
  std::optional<bool> token_supports_pss_;

  // Queue for the tasks that were received while the tast queue was blocked.
  std::deque<base::OnceClosure> task_queue_;
  // Cache for certificates.
  CertCache cert_cache_;

  const raw_ptr<HighLevelChapsClient> chaps_client_;
  KcerTokenUtils kcer_utils_;
  base::WeakPtrFactory<KcerTokenImpl> weak_factory_{this};
};

}  // namespace kcer::internal

#endif  // ASH_COMPONENTS_KCER_KCER_TOKEN_IMPL_H_
