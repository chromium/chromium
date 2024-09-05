// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_KCER_KCER_NSS_KCER_TOKEN_IMPL_NSS_H_
#define ASH_COMPONENTS_KCER_KCER_NSS_KCER_TOKEN_IMPL_NSS_H_

#include <stdint.h>

#include <queue>
#include <string>
#include <vector>

#include "ash/components/kcer/cert_cache.h"
#include "ash/components/kcer/chaps/high_level_chaps_client.h"
#include "ash/components/kcer/helpers/pkcs12_reader.h"
#include "ash/components/kcer/kcer_token.h"
#include "ash/components/kcer/kcer_token_utils.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/cert_database.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_certificate.h"
#include "third_party/cros_system_api/constants/pkcs11_custom_attributes.h"

namespace kcer::internal {

using KeyPermissionsAttributeId =
    base::StrongAlias<class TagKcerToken0,
                      pkcs11_custom_attributes::CkAttributeType>;
using CertProvisioningIdAttributeId =
    base::StrongAlias<class TagKcerToken1,
                      pkcs11_custom_attributes::CkAttributeType>;

// Implementation of KcerToken that uses NSS as a permanent storage.
// Exported for unit tests only.
class COMPONENT_EXPORT(KCER) KcerTokenImplNss
    : public KcerToken,
      public net::CertDatabase::Observer {
 public:
  enum class State {
    // Cache must be updated before it can be used.
    kCacheOutdated,
    // Cache is currently being updated.
    kCacheUpdating,
    // Cache is up-to-date and can be used.
    kCacheUpToDate,
    // Terminal state, initialization failed.
    kInitializationFailed,
  };

  explicit KcerTokenImplNss(Token token, HighLevelChapsClient* chaps_client);
  ~KcerTokenImplNss() override;

  KcerTokenImplNss(const KcerTokenImplNss&) = delete;
  KcerTokenImplNss& operator=(const KcerTokenImplNss&) = delete;
  KcerTokenImplNss(KcerTokenImplNss&&) = delete;
  KcerTokenImplNss& operator=(KcerTokenImplNss&&) = delete;

  // Returns a weak pointer for the token. The pointer can be used to post tasks
  // for the token.
  base::WeakPtr<KcerToken> GetWeakPtr() override;

  // Initializes the token with the provided NSS slot. If `nss_slot` is nullptr,
  // the initialization is considered failed and the token will return an error
  // for all queued and future requests.
  void InitializeForNss(crypto::ScopedPK11Slot nss_slot) override;

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

  // NSS software database (softoken) doesn't support custom attributes. If
  // attribute translation is enabled, KcerToken will store the attributes in
  // some standard attributes, which is wrong in general, but good enough for
  // tests.
  void SetAttributeTranslationForTesting(bool is_enabled);

 private:
  // Immediately blocks the queue and returns a closure that unblocks it when
  // called or destroyed.
  base::OnceClosure BlockQueueGetUnblocker();
  // Immediately unblocks the queue and attempts to perform the next task.
  void UnblockQueueProcessNextTask();

  // Updates the cached certificates to match the ones in NSS.
  void UpdateCache();
  void UpdateCacheWithCerts(net::ScopedCERTCertificateList certs);

  // Convenience method for calling the callback with the
  // kTokenInitializationFailed error and scheduling the next task.
  template <typename T>
  void HandleInitializationFailed(
      base::OnceCallback<void(base::expected<T, Error>)> callback);

  // Used by operations that may modify the set of certificates on the token. If
  // `did_modify` is true, dispatches a notification that the certificate store
  // changed. Then forwards `result` to `callback`. Note that `did_modify` may
  // be true even if `result` contains an error, because some operations can be
  // partially successful.
  void OnCertsModified(Kcer::StatusCallback callback,
                       bool did_modify,
                       base::expected<void, Error> result);

  // These methods return PKCS#11 attribute IDs that should be passed to NSS,
  // respecting SetAttributeTranslationForTesting.
  KeyPermissionsAttributeId GetKeyPermissionsAttributeId() const;
  CertProvisioningIdAttributeId GetCertProvisioningIdAttributeId() const;

  // Indicates whether fake attribute ids should be used (for testing).
  bool translate_attributes_for_testing_ = false;
  // Indicates whether the task queue is blocked. Task queue should be blocked
  // until NSS is initialized, during the processing of most requests and
  // during updating the cache.
  bool is_blocked_ = true;
  State state_ = State::kCacheOutdated;
  // Token type of this KcerToken.
  const Token token_;
  // The underlying storage for KcerTokenNss. In this context the words "token"
  // and "slot" are synonyms.
  crypto::ScopedPK11Slot slot_;
  // Queue for the tasks that were received while the task queue was blocked.
  std::deque<base::OnceClosure> task_queue_;
  // Cache for certificates.
  CertCache cert_cache_;

  // Created and initialized on the same thread with KcerTokenImplNss, then only
  // accessed on the UI thread. It's safe to post tasks for it, the destruction
  // task is posted from the destructor of this class.
  std::unique_ptr<KcerTokenUtils> kcer_utils_;

  base::WeakPtrFactory<KcerTokenImplNss> weak_factory_{this};
};

}  // namespace kcer::internal

#endif  // ASH_COMPONENTS_KCER_KCER_NSS_KCER_TOKEN_IMPL_NSS_H_
