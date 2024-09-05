// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/components/kcer/kcer_nss/kcer_token_impl_nss.h"

#include <certdb.h>
#include <keythi.h>
#include <pkcs11.h>
#include <secerr.h>
#include <stdint.h>

#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <vector>

#include "ash/components/kcer/cert_cache.h"
#include "ash/components/kcer/chaps/high_level_chaps_client.h"
#include "ash/components/kcer/helpers/key_helper.h"
#include "ash/components/kcer/helpers/pkcs12_validator.h"
#include "ash/components/kcer/kcer_histograms.h"
#include "ash/components/kcer/kcer_token.h"
#include "ash/components/kcer/kcer_utils.h"
#include "ash/components/kcer/key_permissions.pb.h"
#include "base/check_is_test.h"
#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/ash/components/chaps_util/chaps_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_key_util.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_database.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "third_party/cros_system_api/constants/pkcs11_custom_attributes.h"

// General pattern for implementing KcerToken methods:
// * The received callbacks for the results must already be bound to correct
// task runners (see base::BindPostTask), so it's not needed to manually post
// them on any particular task runner.
// * Each method must advance task queue on completion. This is important
// because before initialization and during other tasks all new tasks are stored
// in the queue. It can only progress when a previous tasks advances the queue
// or a new task arrives.
// * The most convenient way to advance the queue is to call
// BlockQueueGetUnblocker() and attach the result to the original callback. This
// will advance the queue automatically after the callback is called (or even
// discarded).
// * It's also better to keep the task queue blocked during the execution of a
// callback in case of additional requests from it.

namespace kcer::internal {
namespace {

void RunUnblocker(base::ScopedClosureRunner unblocker) {
  unblocker.RunAndReset();
}

// Returns a vector containing bytes from `value` or an empty vector if `value`
// is nullptr.
std::vector<uint8_t> SECItemToBytes(const crypto::ScopedSECItem& value) {
  return value ? std::vector<uint8_t>(value->data, value->data + value->len)
               : std::vector<uint8_t>();
}

void CleanUpAndDestroyKeys(crypto::ScopedSECKEYPublicKey public_key,
                           crypto::ScopedSECKEYPrivateKey private_key) {
  // Clean up generated keys. PK11_DeleteTokenPrivateKey and
  // PK11_DeleteTokenPublicKey are documented to also destroy the passed
  // SECKEYPublicKey/SECKEYPrivateKey structures.
  if (private_key) {
    PK11_DeleteTokenPrivateKey(/*privKey=*/private_key.release(),
                               /*force=*/false);
  }
  if (public_key) {
    PK11_DeleteTokenPublicKey(/*pubKey=*/public_key.release());
  }
}

base::expected<crypto::ScopedSECKEYPrivateKey, Error>
GetSECKEYPrivateKeyByPkcs11Id(const crypto::ScopedPK11Slot& slot,
                              const std::vector<uint8_t>& pkcs11_id) {
  SECItem sec_key_id;
  sec_key_id.data = const_cast<uint8_t*>(pkcs11_id.data());
  sec_key_id.len = pkcs11_id.size();

  crypto::ScopedSECKEYPrivateKey private_key(
      PK11_FindKeyByKeyID(slot.get(), &sec_key_id, /*wincx=*/nullptr));
  if (!private_key) {
    return base::unexpected(Error::kKeyNotFound);
  }
  return private_key;
}

// Returns ScopedSECKEYPrivateKey if the key was found.
// Returns Error::kKeyNotFound if the key was not found.
// Returns some other error in case of other problems.
base::expected<crypto::ScopedSECKEYPrivateKey, Error> GetSECKEYPrivateKey(
    const crypto::ScopedPK11Slot& slot,
    const PrivateKeyHandle& key) {
  std::vector<uint8_t> pkcs11_id = key.GetPkcs11IdInternal().value();
  if (pkcs11_id.empty()) {
    CHECK(!key.GetSpkiInternal()->empty());
    pkcs11_id = SECItemToBytes(crypto::MakeNssIdFromSpki(
        base::make_span(key.GetSpkiInternal().value())));
  }
  if (pkcs11_id.empty()) {
    return base::unexpected(Error::kFailedToGetKeyId);
  }

  return GetSECKEYPrivateKeyByPkcs11Id(slot, pkcs11_id);
}

void GenerateRsaKeyOnWorkerThread(Token token,
                                  crypto::ScopedPK11Slot slot,
                                  RsaModulusLength modulus_length_bits,
                                  bool hardware_backed,
                                  Kcer::GenerateKeyCallback callback) {
  uint32_t modulus_length_bits_uint =
      static_cast<uint32_t>(modulus_length_bits);
  bool key_gen_success = false;
  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;

  if (hardware_backed) {
    key_gen_success = crypto::GenerateRSAKeyPairNSS(
        slot.get(), modulus_length_bits_uint, /*permanent=*/true, &public_key,
        &private_key);
  } else {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    auto chaps_util = chromeos::ChapsUtil::Create();
    key_gen_success = chaps_util->GenerateSoftwareBackedRSAKey(
        slot.get(), modulus_length_bits_uint, &public_key, &private_key);
#else
    return std::move(callback).Run(base::unexpected(Error::kNotImplemented));
#endif
  }

  if (!key_gen_success) {
    CleanUpAndDestroyKeys(std::move(public_key), std::move(private_key));
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToGenerateKey));
  }
  DCHECK(public_key && private_key);

  crypto::ScopedSECItem public_key_der(
      SECKEY_EncodeDERSubjectPublicKeyInfo(public_key.get()));
  if (!public_key_der) {
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToExportPublicKey));
  }

  Pkcs11Id pkcs11_id(SECItemToBytes(crypto::ScopedSECItem(
      PK11_MakeIDFromPubKey(&public_key->u.rsa.modulus))));
  PublicKeySpki public_key_spki(SECItemToBytes(std::move(public_key_der)));

  return std::move(callback).Run(
      PublicKey(token, std::move(pkcs11_id), std::move(public_key_spki)));
}

void GenerateEcKeyOnWorkerThread(Token token,
                                 crypto::ScopedPK11Slot slot,
                                 EllipticCurve curve,
                                 bool hardware_backed,
                                 Kcer::GenerateKeyCallback callback) {
  bool key_gen_success = false;
  crypto::ScopedSECKEYPublicKey public_key;
  crypto::ScopedSECKEYPrivateKey private_key;

  if (curve != EllipticCurve::kP256) {
    return std::move(callback).Run(base::unexpected(Error::kNotSupported));
  }

  if (hardware_backed) {
    key_gen_success = crypto::GenerateECKeyPairNSS(
        slot.get(), SEC_OID_ANSIX962_EC_PRIME256V1, /*permanent=*/true,
        &public_key, &private_key);
  } else {
    // Shouldn't be needed yet, will be implemented in non-NSS version of
    // Kcer.
    return std::move(callback).Run(base::unexpected(Error::kNotImplemented));
  }

  if (!key_gen_success) {
    CleanUpAndDestroyKeys(std::move(public_key), std::move(private_key));
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToGenerateKey));
  }

  crypto::ScopedSECItem public_key_der(
      SECKEY_EncodeDERSubjectPublicKeyInfo(public_key.get()));
  if (!public_key_der) {
    CleanUpAndDestroyKeys(std::move(public_key), std::move(private_key));
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToExportPublicKey));
  }

  Pkcs11Id pkcs11_id(SECItemToBytes(crypto::ScopedSECItem(
      PK11_MakeIDFromPubKey(&public_key->u.ec.publicValue))));

  return std::move(callback).Run(
      PublicKey(token, std::move(pkcs11_id),
                PublicKeySpki(SECItemToBytes(std::move(public_key_der)))));
}

void ImportKeyOnWorkerThread(Token token,
                             crypto::ScopedPK11Slot slot,
                             Pkcs8PrivateKeyInfoDer pkcs8_private_key_info_der,
                             Kcer::ImportKeyCallback callback) {
  // TODO(miersh): The import method uses
  // PK11_ImportDERPrivateKeyInfoAndReturnKey inside, which doesn't seem to
  // create a public key object for the imported key. This causes the ListKeys
  // method to not see the key. This should be fixed in Kcer-without-NSS.
  crypto::ScopedSECKEYPrivateKey imported_private_key =
      crypto::ImportNSSKeyFromPrivateKeyInfo(slot.get(),
                                             pkcs8_private_key_info_der.value(),
                                             /*permanent=*/true);
  if (!imported_private_key) {
    return std::move(callback).Run(base::unexpected(Error::kFailedToImportKey));
  }

  crypto::ScopedSECKEYPublicKey public_key(
      SECKEY_ConvertToPublicKey(imported_private_key.get()));
  if (!public_key) {
    CleanUpAndDestroyKeys(std::move(public_key),
                          std::move(imported_private_key));
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToExportPublicKey));
  }

  crypto::ScopedSECItem public_key_der(
      SECKEY_EncodeDERSubjectPublicKeyInfo(public_key.get()));
  if (!public_key_der) {
    CleanUpAndDestroyKeys(std::move(public_key),
                          std::move(imported_private_key));
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToEncodePublicKey));
  }

  Pkcs11Id pkcs11_id(
      SECItemToBytes(crypto::MakeNssIdFromPublicKey(public_key.get())));

  return std::move(callback).Run(
      PublicKey(token, std::move(pkcs11_id),
                PublicKeySpki(SECItemToBytes(std::move(public_key_der)))));
}

void ImportCertOnWorkerThread(
    crypto::ScopedPK11Slot slot,
    CertDer cert_der,
    base::OnceCallback<void(bool /*did_modify*/,
                            base::expected<void, Error> /*result*/)> callback) {
  net::ScopedCERTCertificateList certs =
      net::x509_util::CreateCERTCertificateListFromBytes(
          *cert_der, net::X509Certificate::FORMAT_AUTO);

  if (certs.empty() || (certs.size() != 1)) {
    return std::move(callback).Run(
        /*did_modify=*/false, base::unexpected(Error::kInvalidCertificate));
  }
  CERTCertificate* cert = certs[0].get();

  CK_OBJECT_HANDLE key;
  crypto::ScopedPK11Slot key_slot(PK11_KeyForCertExists(cert, &key, nullptr));
  if (key_slot != slot) {
    return std::move(callback).Run(
        /*did_modify=*/false, base::unexpected(Error::kKeyNotFound));
  }

  if (int res = net::x509_util::ImportUserCert(cert, std::move(key_slot));
      res != net::OK) {
    LOG(ERROR) << "Failed to import certificate, error: " << res;
    return std::move(callback).Run(
        /*did_modify=*/false,
        base::unexpected(Error::kFailedToImportCertificate));
  }

  return std::move(callback).Run(/*did_modify=*/true, {});
}

// Returns true if |public_key| is relevant as a "platform key" that should be
// visible to chrome extensions / subsystems.
bool ShouldIncludePublicKey(SECKEYPublicKey* public_key) {
  crypto::ScopedSECItem cka_id(SECITEM_AllocItem(/*arena=*/nullptr,
                                                 /*item=*/nullptr,
                                                 /*len=*/0));
  if (PK11_ReadRawAttribute(
          /*objType=*/PK11_TypePubKey, public_key, CKA_ID, cka_id.get()) !=
      SECSuccess) {
    return false;
  }

  std::string_view cka_id_str(reinterpret_cast<char*>(cka_id->data),
                              cka_id->len);

  // Only keys generated/stored by extensions/Chrome should be visible to
  // extensions. Oemcrypto stores its key in the TPM, but that should not
  // be exposed. Look at exposing additional attributes or changing the slot
  // that Oemcrypto stores keys, so that it can be done based on properties
  // of the key. See https://crbug/com/1136396
  if (cka_id_str == "arc-oemcrypto") {
    VLOG(0) << "Filtered out arc-oemcrypto public key.";
    return false;
  }
  return true;
}

void ListKeysOnWorkerThread(Token token,
                            crypto::ScopedPK11Slot slot,
                            KcerToken::TokenListKeysCallback callback) {
  std::vector<PublicKey> result;

  crypto::ScopedSECKEYPublicKeyList public_keys(
      PK11_ListPublicKeysInSlot(slot.get(), /*nickname=*/nullptr));
  if (!public_keys) {
    return std::move(callback).Run(std::move(result));
  }

  for (SECKEYPublicKeyListNode* node = PUBKEY_LIST_HEAD(public_keys);
       !PUBKEY_LIST_END(node, public_keys); node = PUBKEY_LIST_NEXT(node)) {
    if (!ShouldIncludePublicKey(node->key)) {
      continue;
    }

    Pkcs11Id pkcs11_id(
        SECItemToBytes(crypto::MakeNssIdFromPublicKey(node->key)));

    base::expected<crypto::ScopedSECKEYPrivateKey, Error> private_key =
        GetSECKEYPrivateKeyByPkcs11Id(slot, pkcs11_id.value());
    if (!private_key.has_value()) {
      // Do not list key pairs without private keys, they might be left behind
      // when NSS fails to delete a public key.
      continue;
    }

    crypto::ScopedSECItem public_key_der(
        SECKEY_EncodeDERSubjectPublicKeyInfo(node->key));
    if (!public_key_der || (public_key_der->len == 0)) {
      LOG(WARNING) << "Could not encode subject public key info.";
      continue;
    }

    result.emplace_back(
        token, std::move(pkcs11_id),
        PublicKeySpki(SECItemToBytes(std::move(public_key_der))));
  }

  std::move(callback).Run(std::move(result));
}

void ListCertsOnWorkerThread(
    crypto::ScopedPK11Slot slot,
    base::OnceCallback<void(net::ScopedCERTCertificateList)> callback) {
  crypto::ScopedCERTCertList cert_list(PK11_ListCertsInSlot(slot.get()));

  net::ScopedCERTCertificateList result;

  if (cert_list) {
    for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
         !CERT_LIST_END(node, cert_list); node = CERT_LIST_NEXT(node)) {
      if (!CERT_IsUserCert(node->cert)) {
        continue;
      }
      result.push_back(net::x509_util::DupCERTCertificate(node->cert));
    }
  }

  std::move(callback).Run(std::move(result));
}

void RemoveKeyAndCertsOnWorkerThread(
    crypto::ScopedPK11Slot slot,
    PrivateKeyHandle key,
    base::OnceCallback<void(bool /*did_modify*/,
                            base::expected<void, Error> /*result*/)> callback) {
  base::expected<crypto::ScopedSECKEYPrivateKey, Error> private_key =
      GetSECKEYPrivateKey(slot, key);
  if (!private_key.has_value()) {
    return std::move(callback).Run(/*did_modify=*/false,
                                   base::unexpected(private_key.error()));
  }
  crypto::ScopedSECKEYPrivateKey& sec_private_key = private_key.value();

  // There can be multiple certs that need to be deleted. The loop continues
  // until no more certs are found or too many errors received.
  int errors_allowed = 5;
  int certs_removed = 0;
  while (errors_allowed > 0) {
    net::ScopedCERTCertificate cert(
        PK11_GetCertFromPrivateKey(sec_private_key.get()));
    if (!cert) {
      break;
    }

    if (SEC_DeletePermCertificate(cert.get()) != SECSuccess) {
      --errors_allowed;
      continue;
    }
    ++certs_removed;
  }
  if (errors_allowed == 0) {
    return std::move(callback).Run(
        /*did_modify=*/(certs_removed > 0),
        base::unexpected(Error::kFailedToRemoveCertificate));
  }

  crypto::ScopedSECKEYPublicKey public_key(
      SECKEY_ConvertToPublicKey(sec_private_key.get()));

  if (PK11_DeleteTokenPrivateKey(/*privKey=*/sec_private_key.release(),
                                 /*force=*/false) != SECSuccess) {
    return std::move(callback).Run(
        /*did_modify=*/(certs_removed > 0),
        base::unexpected(Error::kFailedToRemovePrivateKey));
  }

  // TODO(crbug.com/40700534): NSS tends to fail the deletion of the public key,
  // ignore the result for now and make sure it works properly in the non-NSS
  // version of Kcer.
  PK11_DeleteTokenPublicKey(/*privKey=*/public_key.release());

  // Success.
  return std::move(callback).Run(
      /*did_modify=*/(certs_removed > 0), {});
}

void RemoveCertOnWorkerThread(
    crypto::ScopedPK11Slot slot,
    scoped_refptr<const Cert> cert,
    base::OnceCallback<void(bool /*did_modify*/,
                            base::expected<void, Error> /*result*/)> callback) {
  net::ScopedCERTCertificate nss_cert =
      net::x509_util::CreateCERTCertificateFromX509Certificate(
          cert->GetX509Cert().get());
  if (!nss_cert) {
    return std::move(callback).Run(
        /*did_modify=*/false, base::unexpected(Error::kInvalidCertificate));
  }

  if (SEC_DeletePermCertificate(nss_cert.get()) != SECSuccess) {
    return std::move(callback).Run(
        /*did_modify=*/false,
        base::unexpected(Error::kFailedToRemoveCertificate));
  }

  // TODO(miersh): Currently RemoveCertOnWorkerThread() returns "success" even
  // when SEC_DeletePermCertificate doesn't find the certificate. This is
  // acceptable for a "remove" method, but it might be useful to change it
  // after NSS is not used for Kcer.
  std::move(callback).Run(/*did_modify=*/true, {});
}

void DoesPrivateKeyExistOnWorkerThread(crypto::ScopedPK11Slot slot,
                                       PrivateKeyHandle key,
                                       Kcer::DoesKeyExistCallback callback) {
  base::expected<crypto::ScopedSECKEYPrivateKey, Error> private_key =
      GetSECKEYPrivateKey(slot, key);
  if (private_key.has_value()) {
    return std::move(callback).Run(true);
  }
  if ((private_key.error() == Error::kKeyNotFound) ||
      (private_key.error() == Error::kTokenIsNotAvailable)) {
    return std::move(callback).Run(false);
  }
  return std::move(callback).Run(base::unexpected(private_key.error()));
}

bool DoesKeySupportSigningScheme(
    SigningScheme kcer_signing_scheme,
    const crypto::ScopedSECKEYPrivateKey& sec_private_key) {
  switch (kcer_signing_scheme) {
    case SigningScheme::kRsaPkcs1Sha1:
    case SigningScheme::kRsaPkcs1Sha256:
    case SigningScheme::kRsaPkcs1Sha384:
    case SigningScheme::kRsaPkcs1Sha512:
    case SigningScheme::kRsaPssRsaeSha256:
    case SigningScheme::kRsaPssRsaeSha384:
    case SigningScheme::kRsaPssRsaeSha512:
      return (sec_private_key->keyType == ::KeyType::rsaKey);
    case SigningScheme::kEcdsaSecp256r1Sha256:
    case SigningScheme::kEcdsaSecp384r1Sha384:
    case SigningScheme::kEcdsaSecp521r1Sha512:
      return (sec_private_key->keyType == ::KeyType::ecKey);
  }
}

// TODO(b/244408117): This method is mostly a copy of Sign() from
// net/ssl/ssl_platform_key_nss.cc . The original method should be replaced
// during the upcoming refactoring to remove direct usages of NSS.
void SignOnWorkerThread(crypto::ScopedPK11Slot slot,
                        PrivateKeyHandle key,
                        SigningScheme kcer_signing_scheme,
                        DataToSign data_to_sign,
                        Kcer::SignCallback callback) {
  base::expected<crypto::ScopedSECKEYPrivateKey, Error> private_key =
      GetSECKEYPrivateKey(slot, key);
  if (!private_key.has_value()) {
    return std::move(callback).Run(base::unexpected(private_key.error()));
  }

  const crypto::ScopedSECKEYPrivateKey& sec_private_key = private_key.value();
  if (!DoesKeySupportSigningScheme(kcer_signing_scheme, sec_private_key)) {
    return std::move(callback).Run(
        base::unexpected(Error::kKeyDoesNotSupportSigningScheme));
  }
  uint16_t ssl_algorithm = static_cast<uint16_t>(kcer_signing_scheme);

  const EVP_MD* digest_method =
      SSL_get_signature_algorithm_digest(ssl_algorithm);
  uint8_t digest[EVP_MAX_MD_SIZE];
  unsigned digest_len;
  if (!digest_method ||
      !EVP_Digest(data_to_sign->data(), data_to_sign->size(), digest,
                  &digest_len, digest_method, nullptr)) {
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToSignFailedToDigest));
  }
  SECItem digest_item;
  digest_item.data = digest;
  digest_item.len = digest_len;

  CK_MECHANISM_TYPE mechanism = PK11_MapSignKeyType(sec_private_key->keyType);
  SECItem param = {siBuffer, nullptr, 0};
  CK_RSA_PKCS_PSS_PARAMS pss_params;
  bssl::UniquePtr<uint8_t> free_digest_info;
  if (SSL_is_signature_algorithm_rsa_pss(ssl_algorithm)) {
    switch (EVP_MD_type(digest_method)) {
      case NID_sha256:
        pss_params.hashAlg = CKM_SHA256;
        pss_params.mgf = CKG_MGF1_SHA256;
        break;
      case NID_sha384:
        pss_params.hashAlg = CKM_SHA384;
        pss_params.mgf = CKG_MGF1_SHA384;
        break;
      case NID_sha512:
        pss_params.hashAlg = CKM_SHA512;
        pss_params.mgf = CKG_MGF1_SHA512;
        break;
      default:
        return std::move(callback).Run(
            base::unexpected(Error::kUnexpectedSigningScheme));
    }
    // Use the hash length for the salt length.
    pss_params.sLen = EVP_MD_size(digest_method);
    mechanism = CKM_RSA_PKCS_PSS;
    param.data = reinterpret_cast<unsigned char*>(&pss_params);
    param.len = sizeof(pss_params);
  } else if (SSL_get_signature_algorithm_key_type(ssl_algorithm) ==
             EVP_PKEY_RSA) {
    // PK11_SignWithMechanism expects the caller to prepend the DigestInfo for
    // PKCS #1.
    int hash_nid =
        EVP_MD_type(SSL_get_signature_algorithm_digest(ssl_algorithm));
    int is_alloced;
    size_t prefix_len;
    if (!RSA_add_pkcs1_prefix(&digest_item.data, &prefix_len, &is_alloced,
                              hash_nid, digest_item.data, digest_item.len)) {
      return std::move(callback).Run(
          base::unexpected(Error::kFailedToSignFailedToAddPrefix));
    }
    digest_item.len = prefix_len;
    if (is_alloced) {
      free_digest_info.reset(digest_item.data);
    }
  }

  std::vector<uint8_t> signature;

  {
    const int len = PK11_SignatureLen(sec_private_key.get());
    if (len <= 0) {
      return std::move(callback).Run(
          base::unexpected(Error::kFailedToSignFailedToGetSignatureLength));
    }

    signature.resize(len);
    SECItem signature_item;
    signature_item.data = signature.data();
    signature_item.len = signature.size();

    SECStatus rv =
        PK11_SignWithMechanism(sec_private_key.get(), mechanism, &param,
                               &signature_item, &digest_item);
    if (rv != SECSuccess) {
      return std::move(callback).Run(base::unexpected(Error::kFailedToSign));
    }
    signature.resize(signature_item.len);
  }

  // NSS emits raw ECDSA signatures, but BoringSSL expects a DER-encoded
  // ECDSA-Sig-Value.
  if (SSL_get_signature_algorithm_key_type(ssl_algorithm) == EVP_PKEY_EC) {
    if (signature.size() % 2 != 0) {
      return std::move(callback).Run(
          base::unexpected(Error::kFailedToSignBadSignatureLength));
    }
    size_t order_len = signature.size() / 2;

    // Convert the RAW ECDSA signature to a DER-encoded ECDSA-Sig-Value.
    bssl::UniquePtr<ECDSA_SIG> sig(ECDSA_SIG_new());
    if (!sig || !BN_bin2bn(signature.data(), order_len, sig->r) ||
        !BN_bin2bn(signature.data() + order_len, order_len, sig->s)) {
      return std::move(callback).Run(
          base::unexpected(Error::kFailedToDerEncode));
    }

    {
      const int len = i2d_ECDSA_SIG(sig.get(), nullptr);
      if (len <= 0) {
        return std::move(callback).Run(
            base::unexpected(Error::kFailedToSignBadSignatureLength));
      }
      signature.resize(len);
    }

    {
      uint8_t* ptr = signature.data();
      const int len = i2d_ECDSA_SIG(sig.get(), &ptr);
      if (len <= 0) {
        return std::move(callback).Run(
            base::unexpected(Error::kFailedToDerEncode));
      }
      signature.resize(len);
    }
  }

  return std::move(callback).Run(Signature(std::move(signature)));
}

// Checks whether |input_length| is lower or equal to the maximum input length
// for a RSA PKCS#1 v1.5 signature generated using |private_key| with PK11_Sign.
// Returns false if |input_length| is too large.
// If the maximum input length can not be determined (which is possible because
// it queries the PKCS#11 module), returns true and logs a warning.
bool CheckRsaPkcs1SignDigestInputLength(SECKEYPrivateKey* private_key,
                                        size_t input_length) {
  // For RSA keys, PK11_Sign will perform PKCS#1 v1.5 padding, which needs at
  // least 11 bytes. RSA Sign can process an input of max. modulus length.
  // Thus the maximum input length for the sign operation is
  // |modulus_length - 11|.
  int modulus_length_bytes = PK11_GetPrivateModulusLen(private_key);
  if (modulus_length_bytes <= 0) {
    LOG(WARNING) << "Could not determine modulus length";
    return true;
  }
  size_t max_input_length_after_padding =
      static_cast<size_t>(modulus_length_bytes);
  // PKCS#1 v1.5 Padding needs at least this many bytes.
  size_t kMinPaddingLength = 11u;
  return input_length + kMinPaddingLength <= max_input_length_after_padding;
}

// Performs "raw" PKCS1 v1.5 padding + signing by calling PK11_Sign on |key|.
// TODO(b/244408117): This method is mostly a copy of
// `PlatformKeysService::SignRSAPKCS1Raw` from platform_keys_service_nss.cc .
// The original method should be replaced during the upcoming refactoring to
// remove direct usages of NSS.
void SignRsaPkcs1RawOnWorkerThread(crypto::ScopedPK11Slot slot,
                                   PrivateKeyHandle key,
                                   DigestWithPrefix digest_with_prefix,
                                   Kcer::SignCallback callback) {
  base::expected<crypto::ScopedSECKEYPrivateKey, Error> private_key =
      GetSECKEYPrivateKey(slot, key);
  if (!private_key.has_value()) {
    return std::move(callback).Run(base::unexpected(private_key.error()));
  }
  const crypto::ScopedSECKEYPrivateKey& sec_private_key = private_key.value();
  if (sec_private_key->keyType != ::KeyType::rsaKey) {
    return std::move(callback).Run(
        base::unexpected(Error::kKeyDoesNotSupportSigningScheme));
  }

  static_assert(
      sizeof(*digest_with_prefix->data()) == sizeof(char),
      "Can't reinterpret data if its characters are not 8 bit large.");
  SECItem input = {siBuffer,
                   const_cast<unsigned char*>(digest_with_prefix->data()),
                   static_cast<unsigned int>(digest_with_prefix->size())};

  // Compute signature of hash.
  int signature_len = PK11_SignatureLen(sec_private_key.get());
  if (signature_len <= 0) {
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToSignBadSignatureLength));
  }

  std::vector<unsigned char> signature(signature_len);
  SECItem signature_output = {siBuffer, signature.data(),
                              static_cast<unsigned int>(signature.size())};
  if (PK11_Sign(sec_private_key.get(), &signature_output, &input) !=
      SECSuccess) {
    // Input size is checked after a failure - obtaining max input size
    // involves extracting key modulus length which is not a free operation, so
    // don't bother if signing succeeded.
    // Note: It would be better if this could be determined from some library
    // return code (e.g. PORT_GetError), but this was not possible with
    // NSS+chaps at this point.
    if (!CheckRsaPkcs1SignDigestInputLength(sec_private_key.get(),
                                            digest_with_prefix->size())) {
      return std::move(callback).Run(base::unexpected(Error::kInputTooLong));
    }
    return std::move(callback).Run(base::unexpected(Error::kFailedToSign));
  }
  return std::move(callback).Run(Signature(std::move(signature)));
}

void GetKeyPermissionsOnWorkerThread(
    KeyPermissionsAttributeId key_permissions_attribute_id,
    crypto::ScopedPK11Slot slot,
    PrivateKeyHandle key,
    Kcer::GetKeyPermissionsCallback callback) {
  base::expected<crypto::ScopedSECKEYPrivateKey, Error> private_key =
      GetSECKEYPrivateKey(slot, key);
  if (!private_key.has_value()) {
    return std::move(callback).Run(base::unexpected(private_key.error()));
  }
  const crypto::ScopedSECKEYPrivateKey& sec_private_key = private_key.value();

  crypto::ScopedSECItem key_permissions_attribute(
      SECITEM_AllocItem(/*arena=*/nullptr,
                        /*item=*/nullptr,
                        /*len=*/0));

  SECStatus status = PK11_ReadRawAttribute(
      /*objType=*/PK11_TypePrivKey, sec_private_key.get(),
      key_permissions_attribute_id.value(), key_permissions_attribute.get());

  if (status != SECSuccess) {
    // CKR_ATTRIBUTE_TYPE_INVALID is a cryptoki function return value which is
    // returned by Chaps if the attribute was not set before for the key. NSS
    // maps this error to SEC_ERROR_BAD_DATA. This error is captured here so
    // as not to return an |error| in cases of retrieving unset key attributes
    // and to return nullopt |attribute_value| instead.
    int error = PORT_GetError();
    if (error == SEC_ERROR_BAD_DATA) {
      return std::move(callback).Run(std::nullopt);
    } else {
      return std::move(callback).Run(
          base::unexpected(Error::kFailedToReadAttribute));
    }
  }

  chaps::KeyPermissions key_permissions;
  if (!key_permissions.ParseFromArray(key_permissions_attribute->data,
                                      key_permissions_attribute->len)) {
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToParseKeyPermissions));
  }
  return std::move(callback).Run(key_permissions);
}

void GetCertProvisioningIdOnWorkerThread(
    CertProvisioningIdAttributeId cert_prov_attribute_id,
    crypto::ScopedPK11Slot slot,
    PrivateKeyHandle key,
    Kcer::GetCertProvisioningProfileIdCallback callback) {
  base::expected<crypto::ScopedSECKEYPrivateKey, Error> private_key =
      GetSECKEYPrivateKey(slot, key);
  if (!private_key.has_value()) {
    return std::move(callback).Run(base::unexpected(private_key.error()));
  }
  const crypto::ScopedSECKEYPrivateKey& sec_private_key = private_key.value();

  crypto::ScopedSECItem cert_prov_attribute(SECITEM_AllocItem(/*arena=*/nullptr,
                                                              /*item=*/nullptr,
                                                              /*len=*/0));

  SECStatus status = PK11_ReadRawAttribute(
      /*objType=*/PK11_TypePrivKey, sec_private_key.get(),
      cert_prov_attribute_id.value(), cert_prov_attribute.get());

  if (status != SECSuccess) {
    // CKR_ATTRIBUTE_TYPE_INVALID is a cryptoki function return value which is
    // returned by Chaps if the attribute was not set before for the key. NSS
    // maps this error to SEC_ERROR_BAD_DATA. This error is captured here so
    // as not to return an |error| in cases of retrieving unset key attributes
    // and to return nullopt |attribute_value| instead.
    int error = PORT_GetError();
    if (error == SEC_ERROR_BAD_DATA) {
      return std::move(callback).Run(std::nullopt);
    } else {
      return std::move(callback).Run(
          base::unexpected(Error::kFailedToReadAttribute));
    }
  }

  return std::move(callback).Run(
      std::string(cert_prov_attribute->data,
                  cert_prov_attribute->data + cert_prov_attribute->len));
}

void GetTokenInfoOnWorkerThread(crypto::ScopedPK11Slot slot,
                                Kcer::GetTokenInfoCallback callback) {
  TokenInfo token_info;
  token_info.pkcs11_id = PK11_GetSlotID(slot.get());
  token_info.token_name = PK11_GetSlotName(slot.get());
  token_info.module_name = PK11_GetModule(slot.get())->commonName;
  return std::move(callback).Run(std::move(token_info));
}

void GetKeyInfoOnWorkerThread(crypto::ScopedPK11Slot slot,
                              PrivateKeyHandle key,
                              Kcer::GetKeyInfoCallback callback) {
  KeyInfo key_info;

  base::expected<crypto::ScopedSECKEYPrivateKey, Error> private_key =
      GetSECKEYPrivateKey(slot, key);
  if (!private_key.has_value()) {
    return std::move(callback).Run(base::unexpected(private_key.error()));
  }
  const crypto::ScopedSECKEYPrivateKey& sec_private_key = private_key.value();

  constexpr CK_ATTRIBUTE_TYPE kKeyInSoftware = CKA_VENDOR_DEFINED + 5;
  // All keys in chaps without the attribute are hardware backed.
  key_info.is_hardware_backed = !PK11_HasAttributeSet(
      slot.get(), sec_private_key->pkcs11ID, kKeyInSoftware,
      /*haslock=*/PR_FALSE);

  switch (SECKEY_GetPrivateKeyType(sec_private_key.get())) {
    case rsaKey:
      key_info.key_type = KeyType::kRsa;
      break;
    case ecKey:
      key_info.key_type = KeyType::kEcc;
      break;
    default:
      return std::move(callback).Run(base::unexpected(Error::kUnknownKeyType));
  }

  key_info.supported_signing_schemes = GetSupportedSigningSchemes(
      PK11_DoesMechanism(slot.get(), CKM_RSA_PKCS_PSS), key_info.key_type);

  char* nickname = PK11_GetPrivateKeyNickname(sec_private_key.get());
  if (nickname) {
    key_info.nickname = nickname;
    PORT_Free(nickname);
  }

  return std::move(callback).Run(std::move(key_info));
}

void SetKeyNicknameOnWorkerThread(crypto::ScopedPK11Slot slot,
                                  PrivateKeyHandle key,
                                  std::string nickname,
                                  Kcer::StatusCallback callback) {
  base::expected<crypto::ScopedSECKEYPrivateKey, Error> private_key =
      GetSECKEYPrivateKey(slot, key);
  if (!private_key.has_value()) {
    return std::move(callback).Run(base::unexpected(private_key.error()));
  }

  if (PK11_SetPrivateKeyNickname(private_key.value().get(), nickname.c_str()) !=
      SECSuccess) {
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToWriteAttribute));
  }
  return std::move(callback).Run({});
}

void SetKeyPermissionsOnWorkerThread(KeyPermissionsAttributeId attribute_id,
                                     crypto::ScopedPK11Slot slot,
                                     PrivateKeyHandle key,
                                     chaps::KeyPermissions key_permissions,
                                     Kcer::StatusCallback callback) {
  base::expected<crypto::ScopedSECKEYPrivateKey, Error> private_key =
      GetSECKEYPrivateKey(slot, key);
  if (!private_key.has_value()) {
    return std::move(callback).Run(base::unexpected(private_key.error()));
  }

  std::vector<uint8_t> serialized_permissions;
  serialized_permissions.resize(key_permissions.ByteSizeLong());
  key_permissions.SerializeToArray(serialized_permissions.data(),
                                   serialized_permissions.size());

  SECItem attribute_value;
  attribute_value.data = serialized_permissions.data();
  attribute_value.len = serialized_permissions.size();

  if (SECStatus res = PK11_WriteRawAttribute(
          /*objType=*/PK11_TypePrivKey, private_key.value().get(),
          attribute_id.value(), &attribute_value);
      res != SECSuccess) {
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToWriteAttribute));
  }
  return std::move(callback).Run({});
}

void SetCertProvisioningProfileIdOnWorkerThread(
    CertProvisioningIdAttributeId attribute_id,
    crypto::ScopedPK11Slot slot,
    PrivateKeyHandle key,
    std::string cert_prov_id,
    Kcer::StatusCallback callback) {
  base::expected<crypto::ScopedSECKEYPrivateKey, Error> private_key =
      GetSECKEYPrivateKey(slot, key);
  if (!private_key.has_value()) {
    return std::move(callback).Run(base::unexpected(private_key.error()));
  }

  SECItem attribute_value;
  attribute_value.data = reinterpret_cast<uint8_t*>(cert_prov_id.data());
  attribute_value.len = cert_prov_id.size();

  if (SECStatus res = PK11_WriteRawAttribute(
          /*objType=*/PK11_TypePrivKey, private_key.value().get(),
          attribute_id.value(), &attribute_value);
      res != SECSuccess) {
    return std::move(callback).Run(
        base::unexpected(Error::kFailedToWriteAttribute));
  }
  return std::move(callback).Run({});
}

scoped_refptr<const Cert> BuildKcerCert(
    Token token,
    const net::ScopedCERTCertificate& nss_cert) {
  Pkcs11Id id_bytes(SECItemToBytes(crypto::MakeNssIdFromSpki(base::make_span(
      nss_cert->derPublicKey.data, nss_cert->derPublicKey.len))));

  std::string nickname;
  if (nss_cert->nickname) {
    nickname = nss_cert->nickname;
  }

  return base::MakeRefCounted<Cert>(
      token, std::move(id_bytes), std::move(nickname),
      net::x509_util::CreateX509CertificateFromCERTCertificate(nss_cert.get()));
}

}  // namespace

KcerTokenImplNss::KcerTokenImplNss(Token token,
                                   HighLevelChapsClient* chaps_client)
    : token_(token),
      kcer_utils_(std::make_unique<KcerTokenUtils>(token, chaps_client)) {}

KcerTokenImplNss::~KcerTokenImplNss() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  net::CertDatabase::GetInstance()->RemoveObserver(this);
  content::GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                 std::move(kcer_utils_));
}

void KcerTokenImplNss::InitializeForNss(crypto::ScopedPK11Slot nss_slot) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (nss_slot) {
    kcer_utils_->Initialize(
        SessionChapsClient::SlotId(PK11_GetSlotID(nss_slot.get())));
    slot_ = std::move(nss_slot);
    net::CertDatabase::GetInstance()->AddObserver(this);
  } else {
    state_ = State::kInitializationFailed;
  }

  // This is supposed to be the first time the task queue is unblocked, no
  // other tasks should be already running.
  UnblockQueueProcessNextTask();
}

base::WeakPtr<KcerToken> KcerTokenImplNss::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void KcerTokenImplNss::OnClientCertStoreChanged() {
  state_ = State::kCacheOutdated;

  // If task queue is not blocked, trigger cache update immediately.
  if (!is_blocked_) {
    UnblockQueueProcessNextTask();
  }
}

void KcerTokenImplNss::GenerateRsaKey(RsaModulusLength modulus_length_bits,
                                      bool hardware_backed,
                                      Kcer::GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kInitializationFailed) [[unlikely]] {
    return HandleInitializationFailed(std::move(callback));
  }
  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImplNss::GenerateRsaKey, weak_factory_.GetWeakPtr(),
        modulus_length_bits, hardware_backed, std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = std::move(callback).Then(BlockQueueGetUnblocker());

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GenerateRsaKeyOnWorkerThread, token_,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     modulus_length_bits, hardware_backed,
                     std::move(unblocking_callback)));
}

void KcerTokenImplNss::GenerateEcKey(EllipticCurve curve,
                                     bool hardware_backed,
                                     Kcer::GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kInitializationFailed) [[unlikely]] {
    return HandleInitializationFailed(std::move(callback));
  } else if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImplNss::GenerateEcKey, weak_factory_.GetWeakPtr(), curve,
        hardware_backed, std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = std::move(callback).Then(BlockQueueGetUnblocker());

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GenerateEcKeyOnWorkerThread, token_,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     curve, hardware_backed, std::move(unblocking_callback)));
}

void KcerTokenImplNss::ImportKey(
    Pkcs8PrivateKeyInfoDer pkcs8_private_key_info_der,
    Kcer::ImportKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kInitializationFailed) [[unlikely]] {
    return HandleInitializationFailed(std::move(callback));
  } else if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImplNss::ImportKey, weak_factory_.GetWeakPtr(),
        std::move(pkcs8_private_key_info_der), std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = std::move(callback).Then(BlockQueueGetUnblocker());

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ImportKeyOnWorkerThread, token_,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(pkcs8_private_key_info_der),
                     std::move(unblocking_callback)));
}

void KcerTokenImplNss::ImportCertFromBytes(CertDer cert_der,
                                           Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kInitializationFailed) [[unlikely]] {
    return HandleInitializationFailed(std::move(callback));
  }
  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImplNss::ImportCertFromBytes, weak_factory_.GetWeakPtr(),
        std::move(cert_der), std::move(callback)));
  }

  // Block task queue, attach queue unblocking and notification sending to the
  // callback.
  auto wrapped_callback = base::BindPostTask(
      content::GetIOThreadTaskRunner({}),
      base::BindOnce(&KcerTokenImplNss::OnCertsModified,
                     weak_factory_.GetWeakPtr(),
                     std::move(callback).Then(BlockQueueGetUnblocker())));

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ImportCertOnWorkerThread,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(cert_der), std::move(wrapped_callback)));
}

//==============================================================================

void KcerTokenImplNss::ImportPkcs12Cert(Pkcs12Blob pkcs12_blob,
                                        std::string password,
                                        bool hardware_backed,
                                        bool mark_as_migrated,
                                        Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kInitializationFailed) [[unlikely]] {
    return HandleInitializationFailed(std::move(callback));
  }
  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImplNss::ImportPkcs12Cert, weak_factory_.GetWeakPtr(),
        std::move(pkcs12_blob), std::move(password), hardware_backed,
        mark_as_migrated, std::move(callback)));
  }

  RecordKcerPkcs12ImportUmaEvent(
      KcerPkcs12ImportEvent::AttemptedPkcs12ChapsImport);

  // Block task queue, attach queue unblocking and notification sending to the
  // callback.
  auto wrapped_callback = base::BindPostTask(
      content::GetIOThreadTaskRunner({}),
      base::BindOnce(&KcerTokenImplNss::OnCertsModified,
                     weak_factory_.GetWeakPtr(),
                     std::move(callback).Then(BlockQueueGetUnblocker())));

  Pkcs12Reader pkcs12_reader;
  KeyData key_data;
  bssl::UniquePtr<STACK_OF(X509)> certs;
  Pkcs12ReaderStatusCode get_key_and_cert_status =
      pkcs12_reader.GetPkcs12KeyAndCerts(pkcs12_blob.value(), password,
                                         key_data.key, certs);
  if (get_key_and_cert_status != Pkcs12ReaderStatusCode::kSuccess) {
    return std::move(wrapped_callback)
        .Run(
            /*did_modify=*/false, base::unexpected(ConvertPkcs12ParsingError(
                                      get_key_and_cert_status)));
  }

  Pkcs12ReaderStatusCode enrich_key_data_result =
      pkcs12_reader.EnrichKeyData(key_data);
  if ((enrich_key_data_result != Pkcs12ReaderStatusCode::kSuccess) ||
      key_data.cka_id_value.empty()) {
    return std::move(wrapped_callback)
        .Run(/*did_modify=*/false, base::unexpected(Error::kFailedToGetKeyId));
  }

  std::vector<CertData> certs_data;
  Pkcs12ReaderStatusCode prepare_certs_status = ValidateAndPrepareCertData(
      cert_cache_, pkcs12_reader, std::move(certs), key_data, certs_data);
  if (prepare_certs_status == Pkcs12ReaderStatusCode::kAlreadyExists) {
    return std::move(wrapped_callback)
        .Run(/*did_modify=*/false, base::unexpected(Error::kAlreadyExists));
  }
  if ((prepare_certs_status != Pkcs12ReaderStatusCode::kSuccess) ||
      certs_data.empty()) {
    return std::move(wrapped_callback)
        .Run(/*did_modify=*/false, base::unexpected(Error::kInvalidPkcs12));
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&KcerTokenUtils::ImportPkcs12,
                     base::Unretained(kcer_utils_.get()), std::move(key_data),
                     std::move(certs_data), hardware_backed, mark_as_migrated,
                     std::move(wrapped_callback)));
}

//==============================================================================

void KcerTokenImplNss::ExportPkcs12Cert(scoped_refptr<const Cert> cert,
                                        Kcer::ExportPkcs12Callback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // TODO(244408716): Implement.
}

void KcerTokenImplNss::RemoveKeyAndCerts(PrivateKeyHandle key,
                                         Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kInitializationFailed) [[unlikely]] {
    return HandleInitializationFailed(std::move(callback));
  }
  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImplNss::RemoveKeyAndCerts, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(callback)));
  }

  // Block task queue, attach queue unblocking and notification sending to the
  // callback.
  auto wrapped_callback = base::BindPostTask(
      content::GetIOThreadTaskRunner({}),
      base::BindOnce(&KcerTokenImplNss::OnCertsModified,
                     weak_factory_.GetWeakPtr(),
                     std::move(callback).Then(BlockQueueGetUnblocker())));

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&RemoveKeyAndCertsOnWorkerThread,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(key), std::move(wrapped_callback)));
}

void KcerTokenImplNss::RemoveCert(scoped_refptr<const Cert> cert,
                                  Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kInitializationFailed) [[unlikely]] {
    return HandleInitializationFailed(std::move(callback));
  } else if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImplNss::RemoveCert, weak_factory_.GetWeakPtr(),
        std::move(cert), std::move(callback)));
  }

  // Block task queue, attach queue unblocking and notification sending to the
  // callback.
  auto wrapped_callback = base::BindPostTask(
      content::GetIOThreadTaskRunner({}),
      base::BindOnce(&KcerTokenImplNss::OnCertsModified,
                     weak_factory_.GetWeakPtr(),
                     std::move(callback).Then(BlockQueueGetUnblocker())));

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&RemoveCertOnWorkerThread,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(cert), std::move(wrapped_callback)));
}

void KcerTokenImplNss::ListKeys(TokenListKeysCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kInitializationFailed) [[unlikely]] {
    return HandleInitializationFailed(std::move(callback));
  } else if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(&KcerTokenImplNss::ListKeys,
                                                weak_factory_.GetWeakPtr(),
                                                std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = std::move(callback).Then(BlockQueueGetUnblocker());

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ListKeysOnWorkerThread, token_,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(unblocking_callback)));
}

void KcerTokenImplNss::ListCerts(TokenListCertsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kInitializationFailed) [[unlikely]] {
    return HandleInitializationFailed(std::move(callback));
  }
  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(&KcerTokenImplNss::ListCerts,
                                                weak_factory_.GetWeakPtr(),
                                                std::move(callback)));
  }
  return std::move(callback)
      .Then(BlockQueueGetUnblocker())
      .Run(cert_cache_.GetAllCerts());
}

void KcerTokenImplNss::DoesPrivateKeyExist(
    PrivateKeyHandle key,
    Kcer::DoesKeyExistCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kInitializationFailed) [[unlikely]] {
    return HandleInitializationFailed(std::move(callback));
  } else if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImplNss::DoesPrivateKeyExist, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = std::move(callback).Then(BlockQueueGetUnblocker());

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&DoesPrivateKeyExistOnWorkerThread,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(key), std::move(unblocking_callback)));
}

void KcerTokenImplNss::Sign(PrivateKeyHandle key,
                            SigningScheme signing_scheme,
                            DataToSign data,
                            Kcer::SignCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kInitializationFailed) [[unlikely]] {
    return HandleInitializationFailed(std::move(callback));
  }
  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImplNss::Sign, weak_factory_.GetWeakPtr(), std::move(key),
        signing_scheme, std::move(data), std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = std::move(callback).Then(BlockQueueGetUnblocker());

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SignOnWorkerThread,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(key), signing_scheme, std::move(data),
                     std::move(unblocking_callback)));
}

void KcerTokenImplNss::SignRsaPkcs1Raw(PrivateKeyHandle key,
                                       DigestWithPrefix digest_with_prefix,
                                       Kcer::SignCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kInitializationFailed) [[unlikely]] {
    return HandleInitializationFailed(std::move(callback));
  }
  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImplNss::SignRsaPkcs1Raw, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(digest_with_prefix), std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = std::move(callback).Then(BlockQueueGetUnblocker());

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SignRsaPkcs1RawOnWorkerThread,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(key), std::move(digest_with_prefix),
                     std::move(unblocking_callback)));
}

void KcerTokenImplNss::GetTokenInfo(Kcer::GetTokenInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kInitializationFailed) [[unlikely]] {
    return HandleInitializationFailed(std::move(callback));
  }
  if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(&KcerTokenImplNss::GetTokenInfo,
                                                weak_factory_.GetWeakPtr(),
                                                std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = std::move(callback).Then(BlockQueueGetUnblocker());

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GetTokenInfoOnWorkerThread,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(unblocking_callback)));
}

void KcerTokenImplNss::GetKeyInfo(PrivateKeyHandle key,
                                  Kcer::GetKeyInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kInitializationFailed) [[unlikely]] {
    return HandleInitializationFailed(std::move(callback));
  } else if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImplNss::GetKeyInfo, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = std::move(callback).Then(BlockQueueGetUnblocker());

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GetKeyInfoOnWorkerThread,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(key), std::move(unblocking_callback)));
}

void KcerTokenImplNss::GetKeyPermissions(
    PrivateKeyHandle key,
    Kcer::GetKeyPermissionsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kInitializationFailed) [[unlikely]] {
    return HandleInitializationFailed(std::move(callback));
  } else if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImplNss::GetKeyPermissions, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = std::move(callback).Then(BlockQueueGetUnblocker());

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GetKeyPermissionsOnWorkerThread,
                     GetKeyPermissionsAttributeId(),
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(key), std::move(unblocking_callback)));
}

void KcerTokenImplNss::GetCertProvisioningProfileId(
    PrivateKeyHandle key,
    Kcer::GetCertProvisioningProfileIdCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kInitializationFailed) [[unlikely]] {
    return HandleInitializationFailed(std::move(callback));
  } else if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImplNss::GetCertProvisioningProfileId,
        weak_factory_.GetWeakPtr(), std::move(key), std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = std::move(callback).Then(BlockQueueGetUnblocker());

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GetCertProvisioningIdOnWorkerThread,
                     GetCertProvisioningIdAttributeId(),
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(key), std::move(unblocking_callback)));
}

void KcerTokenImplNss::SetKeyNickname(PrivateKeyHandle key,
                                      std::string nickname,
                                      Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kInitializationFailed) [[unlikely]] {
    return HandleInitializationFailed(std::move(callback));
  } else if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImplNss::SetKeyNickname, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(nickname), std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = std::move(callback).Then(BlockQueueGetUnblocker());

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SetKeyNicknameOnWorkerThread,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(key), std::move(nickname),
                     std::move(unblocking_callback)));
}

void KcerTokenImplNss::SetKeyPermissions(PrivateKeyHandle key,
                                         chaps::KeyPermissions key_permissions,
                                         Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kInitializationFailed) [[unlikely]] {
    return HandleInitializationFailed(std::move(callback));
  } else if (is_blocked_) {
    return task_queue_.push_back(base::BindOnce(
        &KcerTokenImplNss::SetKeyPermissions, weak_factory_.GetWeakPtr(),
        std::move(key), std::move(key_permissions), std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = std::move(callback).Then(BlockQueueGetUnblocker());

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SetKeyPermissionsOnWorkerThread,
                     GetKeyPermissionsAttributeId(),
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(key), std::move(key_permissions),
                     std::move(unblocking_callback)));
}

void KcerTokenImplNss::SetCertProvisioningProfileId(
    PrivateKeyHandle key,
    std::string profile_id,
    Kcer::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kInitializationFailed) [[unlikely]] {
    return HandleInitializationFailed(std::move(callback));
  } else if (is_blocked_) {
    return task_queue_.push_back(
        base::BindOnce(&KcerTokenImplNss::SetCertProvisioningProfileId,
                       weak_factory_.GetWeakPtr(), std::move(key),
                       std::move(profile_id), std::move(callback)));
  }

  // Block task queue, attach unblocking task to the callback.
  auto unblocking_callback = std::move(callback).Then(BlockQueueGetUnblocker());

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SetCertProvisioningProfileIdOnWorkerThread,
                     GetCertProvisioningIdAttributeId(),
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(key), std::move(profile_id),
                     std::move(unblocking_callback)));
}

// `did_modify` indicates whether a modification was actually made (which should
// trigger a notification).
void KcerTokenImplNss::OnCertsModified(Kcer::StatusCallback callback,
                                       bool did_modify,
                                       base::expected<void, Error> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (did_modify) {
    net::CertDatabase::GetInstance()->NotifyObserversClientCertStoreChanged();
  }
  // The Notify... above will post a task to invalidate the cache. Calling the
  // original callback for a request will automatically trigger updating cache
  // and executing the next request. Post a task with the original callback
  // (instead of calling it synchronously), so the cache update and the next
  // request happen after the notification.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

base::OnceClosure KcerTokenImplNss::BlockQueueGetUnblocker() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  CHECK(!is_blocked_);
  is_blocked_ = true;

  auto unblock_on_io = base::BindPostTask(
      content::GetIOThreadTaskRunner({}),
      base::BindOnce(&KcerTokenImplNss::UnblockQueueProcessNextTask,
                     weak_factory_.GetWeakPtr()));

  // `unblocker` is executed either manually or on destruction.
  base::ScopedClosureRunner unblocker(std::move(unblock_on_io));
  // Pack `unblocker` into a closure, so it can be attached to a callback.
  return base::BindOnce(&RunUnblocker, std::move(unblocker));
}

void KcerTokenImplNss::UnblockQueueProcessNextTask() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  is_blocked_ = false;

  if (state_ == State::kCacheOutdated) {
    return UpdateCache();
  }

  if (task_queue_.empty()) {
    return;
  }

  base::OnceClosure next_task = std::move(task_queue_.front());
  task_queue_.pop_front();
  std::move(next_task).Run();
}

void KcerTokenImplNss::UpdateCache() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Block task queue, create callback for the current sequence.
  auto callback =
      base::BindPostTask(content::GetIOThreadTaskRunner({}),
                         base::BindOnce(&KcerTokenImplNss::UpdateCacheWithCerts,
                                        weak_factory_.GetWeakPtr())
                             .Then(BlockQueueGetUnblocker()));

  state_ = State::kCacheUpdating;

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ListCertsOnWorkerThread,
                     crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())),
                     std::move(callback)));
}

void KcerTokenImplNss::UpdateCacheWithCerts(
    net::ScopedCERTCertificateList new_certs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == State::kCacheOutdated) {
    // If the status switched from kUpdating, then new update happened since
    // the cache started to update, `new_certs` might already be outdated.
    // Skip re-building the cache and try again.
    return;
  }

  std::vector<scoped_refptr<const Cert>> new_cache;

  // For every cert that was found, either take it from the previous cache or
  // create a kcer::Cert object for it.
  for (const net::ScopedCERTCertificate& new_cert : new_certs) {
    const base::span<const uint8_t> cert_span(
        net::x509_util::CERTCertificateAsSpan(new_cert.get()));
    scoped_refptr<const Cert> cert = cert_cache_.FindCert(cert_span);
    if (!cert) {
      cert = BuildKcerCert(token_, new_cert);
    }
    new_cache.push_back(std::move(cert));
  }

  // Rebuilding the cache implicitly removes all the certs that are not in the
  // permanent storage anymore. The certs themself will be fully destroyed
  // when the last ref-counting reference to them is destroyed.
  cert_cache_ = CertCache(new_cache);
  state_ = State::kCacheUpToDate;
}

template <typename T>
void KcerTokenImplNss::HandleInitializationFailed(
    base::OnceCallback<void(base::expected<T, Error>)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(base::unexpected(Error::kTokenInitializationFailed));
  // Multiple tasks might be handled in a row, schedule the next task
  // asynchronously to not overload the stack and not occupy the thread for
  // too long.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&KcerTokenImplNss::UnblockQueueProcessNextTask,
                                weak_factory_.GetWeakPtr()));
}

void KcerTokenImplNss::SetAttributeTranslationForTesting(bool is_enabled) {
  CHECK_IS_TEST();
  translate_attributes_for_testing_ = is_enabled;
}

KeyPermissionsAttributeId KcerTokenImplNss::GetKeyPermissionsAttributeId()
    const {
  if (translate_attributes_for_testing_) [[unlikely]] {
    CHECK_IS_TEST();
    return KeyPermissionsAttributeId(CKA_END_DATE);
  }
  return KeyPermissionsAttributeId(
      pkcs11_custom_attributes::kCkaChromeOsKeyPermissions);
}

CertProvisioningIdAttributeId
KcerTokenImplNss::GetCertProvisioningIdAttributeId() const {
  if (translate_attributes_for_testing_) [[unlikely]] {
    CHECK_IS_TEST();
    return CertProvisioningIdAttributeId(CKA_START_DATE);
  }
  return CertProvisioningIdAttributeId(
      pkcs11_custom_attributes::kCkaChromeOsBuiltinProvisioningProfileId);
}

}  // namespace kcer::internal
