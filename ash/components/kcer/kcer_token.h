// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_KCER_KCER_TOKEN_H_
#define ASH_COMPONENTS_KCER_KCER_TOKEN_H_

#include <string>
#include <vector>

#include "ash/components/kcer/chaps/high_level_chaps_client.h"
#include "ash/components/kcer/chaps/session_chaps_client.h"
#include "ash/components/kcer/kcer.h"
#include "ash/components/kcer/key_permissions.pb.h"
#include "base/functional/callback.h"
#include "crypto/scoped_nss_types.h"

namespace kcer::internal {

// Provides an interface for a single PKCS#11 token, that actually performs
// storage of keys and certificates. This class is an implementation detail of
// Kcer and should only be used by implementations of the Kcer interface. Each
// KcerToken can be used by multiple instances of Kcer interface. Each instance
// of Kcer interface can use multiple KcerToken-s.
// KcerToken-s are expected to exist on a non-UI thread. Passed `callback`-s
// must already be bound to the correct task runner (see base::BindPostTask).
class COMPONENT_EXPORT(KCER) KcerToken {
 public:
  using TokenListKeysCallback =
      base::OnceCallback<void(base::expected<std::vector<PublicKey>, Error>)>;
  using TokenListCertsCallback = base::OnceCallback<void(
      base::expected<std::vector<scoped_refptr<const Cert>>, Error>)>;

  // Methods to create each of the specializations of KcerToken.
  static std::unique_ptr<KcerToken> CreateWithoutNss(
      Token token,
      HighLevelChapsClient* chaps_client);
  static std::unique_ptr<KcerToken> CreateForNss(
      Token token,
      HighLevelChapsClient* chaps_client);

  KcerToken() = default;
  KcerToken(const KcerToken&) = delete;
  KcerToken& operator=(const KcerToken&) = delete;
  KcerToken(KcerToken&&) = delete;
  KcerToken& operator=(Token&&) = delete;
  virtual ~KcerToken() = default;

  // Returns a weak pointer for the token. The pointer can be used to post tasks
  // for the token.
  virtual base::WeakPtr<KcerToken> GetWeakPtr() = 0;

  // Initialization methods for different specializations of KcerToken. They
  // should be used by the factory that creates instances of Kcer and knows
  // which tokens are used at the moment even when it has only a generic
  // pointer to them. Each KcerToken specialization only needs to implement the
  // single correct relevant method.
  virtual void InitializeWithoutNss(SessionChapsClient::SlotId pkcs11_slot_id) {
  }
  virtual void InitializeForNss(crypto::ScopedPK11Slot nss_slot) {}

  // These methods mirror the methods from the Kcer class, except they are
  // specialized for a single token.
  virtual void GenerateRsaKey(RsaModulusLength modulus_length_bits,
                              bool hardware_backed,
                              Kcer::GenerateKeyCallback callback) = 0;
  virtual void GenerateEcKey(EllipticCurve curve,
                             bool hardware_backed,
                             Kcer::GenerateKeyCallback callback) = 0;
  virtual void ImportKey(Pkcs8PrivateKeyInfoDer pkcs8_private_key_info_der,
                         Kcer::ImportKeyCallback callback) = 0;
  virtual void ImportCertFromBytes(CertDer cert_der,
                                   Kcer::StatusCallback callback) = 0;
  virtual void ImportPkcs12Cert(Pkcs12Blob pkcs12_blob,
                                std::string password,
                                bool hardware_backed,
                                bool mark_as_migrated,
                                Kcer::StatusCallback callback) = 0;
  virtual void ExportPkcs12Cert(scoped_refptr<const Cert> cert,
                                Kcer::ExportPkcs12Callback callback) = 0;
  virtual void RemoveKeyAndCerts(PrivateKeyHandle key,
                                 Kcer::StatusCallback callback) = 0;
  virtual void RemoveCert(scoped_refptr<const Cert> cert,
                          Kcer::StatusCallback callback) = 0;
  virtual void ListKeys(TokenListKeysCallback callback) = 0;
  virtual void ListCerts(TokenListCertsCallback callback) = 0;
  virtual void DoesPrivateKeyExist(PrivateKeyHandle key,
                                   Kcer::DoesKeyExistCallback callback) = 0;
  virtual void Sign(PrivateKeyHandle key,
                    SigningScheme signing_scheme,
                    DataToSign data,
                    Kcer::SignCallback callback) = 0;
  virtual void SignRsaPkcs1Raw(PrivateKeyHandle key,
                               DigestWithPrefix digest_with_prefix,
                               Kcer::SignCallback callback) = 0;
  virtual void GetTokenInfo(Kcer::GetTokenInfoCallback callback) = 0;
  virtual void GetKeyInfo(PrivateKeyHandle key,
                          Kcer::GetKeyInfoCallback callback) = 0;
  virtual void GetKeyPermissions(PrivateKeyHandle key,
                                 Kcer::GetKeyPermissionsCallback callback) = 0;
  virtual void GetCertProvisioningProfileId(
      PrivateKeyHandle key,
      Kcer::GetCertProvisioningProfileIdCallback callback) = 0;
  virtual void SetKeyNickname(PrivateKeyHandle key,
                              std::string nickname,
                              Kcer::StatusCallback callback) = 0;
  virtual void SetKeyPermissions(PrivateKeyHandle key,
                                 chaps::KeyPermissions key_permissions,
                                 Kcer::StatusCallback callback) = 0;
  virtual void SetCertProvisioningProfileId(PrivateKeyHandle key,
                                            std::string profile_id,
                                            Kcer::StatusCallback callback) = 0;
};

}  // namespace kcer::internal

#endif  // ASH_COMPONENTS_KCER_KCER_TOKEN_H_
