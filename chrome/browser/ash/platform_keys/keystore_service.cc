// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/platform_keys/keystore_service.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/debug/dump_without_crashing.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service_factory.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"
#include "chromeos/ash/components/platform_keys/keystore_service_util.h"
#include "chromeos/ash/components/platform_keys/keystore_types.h"
#include "chromeos/ash/components/platform_keys/platform_keys.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace ash {

namespace {

using chromeos::ExtensionPlatformKeysService;
using chromeos::KeystoreAlgorithm;
using chromeos::KeystoreAlgorithmName;
using chromeos::KeystoreEcdsaParams;
using chromeos::KeystoreError;
using chromeos::KeystoreKeyAttributeType;
using chromeos::KeystoreType;
using chromeos::RsaOaepParams;
using chromeos::RsassaPkcs115Params;
using chromeos::platform_keys::TokenId;
using platform_keys::KeyPermissionsService;
using platform_keys::PlatformKeysService;
using SigningScheme = chromeos::KeystoreSigningScheme;

// Converts a binary blob to a certificate.
scoped_refptr<net::X509Certificate> ParseCertificate(
    const std::vector<uint8_t>& input) {
  // Allow UTF-8 inside PrintableStrings in client certificates. See
  // crbug.com/41347446 and crbug.com/41357486.
  net::X509Certificate::UnsafeCreateOptions options;
  options.printable_string_is_utf8 = true;
  return net::X509Certificate::CreateFromBytesUnsafeOptions(input, options);
}

TokenId KeystoreToToken(KeystoreType type) {
  switch (type) {
    case KeystoreType::kUser:
      return TokenId::kUser;
    case KeystoreType::kDevice:
      return TokenId::kSystem;
  }
  NOTREACHED();
}

// Returns whether the `algorithm_name` can be used for signing. The unknown
// type is considered invalid for signing.
bool IsSigningAlgorithm(KeystoreAlgorithmName algorithm_name) {
  switch (algorithm_name) {
    case KeystoreAlgorithmName::kRsassaPkcs115:
    case KeystoreAlgorithmName::kEcdsa:
      return true;
    case KeystoreAlgorithmName::kRsaOaep:
    case KeystoreAlgorithmName::kUnknown:
      return false;
  }
  NOTREACHED();
}

// The input should be the name of a signing algorithm, which can be validated
// with the `IsSigningAlgorithm()` function above.
std::string StringFromKeystoreAlgorithmName(
    KeystoreAlgorithmName algorithm_name) {
  switch (algorithm_name) {
    case KeystoreAlgorithmName::kRsassaPkcs115:
      return chromeos::keystore_service_util::kWebCryptoRsassaPkcs1v15;
    case KeystoreAlgorithmName::kEcdsa:
      return chromeos::keystore_service_util::kWebCryptoEcdsa;
    case KeystoreAlgorithmName::kRsaOaep:
    case KeystoreAlgorithmName::kUnknown:
      NOTREACHED();
  }
  NOTREACHED();
}

bool UnpackSigningScheme(
    SigningScheme scheme,
    chromeos::platform_keys::KeyType* key_type,
    chromeos::platform_keys::HashAlgorithm* hash_algorithm) {
  using chromeos::platform_keys::HashAlgorithm;
  using chromeos::platform_keys::KeyType;

  switch (scheme) {
    case SigningScheme::kUnknown:
      return false;
    case SigningScheme::kRsassaPkcs1V15None:
      *key_type = KeyType::kRsassaPkcs1V15;
      *hash_algorithm = HashAlgorithm::HASH_ALGORITHM_NONE;
      return true;
    case SigningScheme::kRsassaPkcs1V15Sha1:
      *key_type = KeyType::kRsassaPkcs1V15;
      *hash_algorithm = HashAlgorithm::HASH_ALGORITHM_SHA1;
      return true;
    case SigningScheme::kRsassaPkcs1V15Sha256:
      *key_type = KeyType::kRsassaPkcs1V15;
      *hash_algorithm = HashAlgorithm::HASH_ALGORITHM_SHA256;
      return true;
    case SigningScheme::kRsassaPkcs1V15Sha384:
      *key_type = KeyType::kRsassaPkcs1V15;
      *hash_algorithm = HashAlgorithm::HASH_ALGORITHM_SHA384;
      return true;
    case SigningScheme::kRsassaPkcs1V15Sha512:
      *key_type = KeyType::kRsassaPkcs1V15;
      *hash_algorithm = HashAlgorithm::HASH_ALGORITHM_SHA512;
      return true;
    case SigningScheme::kEcdsaSha1:
      *key_type = KeyType::kEcdsa;
      *hash_algorithm = HashAlgorithm::HASH_ALGORITHM_SHA1;
      return true;
    case SigningScheme::kEcdsaSha256:
      *key_type = KeyType::kEcdsa;
      *hash_algorithm = HashAlgorithm::HASH_ALGORITHM_SHA256;
      return true;
    case SigningScheme::kEcdsaSha384:
      *key_type = KeyType::kEcdsa;
      *hash_algorithm = HashAlgorithm::HASH_ALGORITHM_SHA384;
      return true;
    case SigningScheme::kEcdsaSha512:
      *key_type = KeyType::kEcdsa;
      *hash_algorithm = HashAlgorithm::HASH_ALGORITHM_SHA512;
      return true;
  }
  NOTREACHED();
}

std::optional<chromeos::platform_keys::KeyAttributeType>
UnpackKeystoreKeyAttributeType(KeystoreKeyAttributeType keystore_type) {
  using chromeos::platform_keys::KeyAttributeType;
  switch (keystore_type) {
    case KeystoreKeyAttributeType::kUnknown:
      return std::nullopt;
    case KeystoreKeyAttributeType::kPlatformKeysTag:
      return KeyAttributeType::kPlatformKeysTag;
  }
  NOTREACHED();
}

}  // namespace

KeystoreService::KeystoreService(content::BrowserContext* fixed_context)
    : fixed_platform_keys_service_(
          platform_keys::PlatformKeysServiceFactory::GetForBrowserContext(
              fixed_context)),
      fixed_key_permissions_service_(
          platform_keys::KeyPermissionsServiceFactory::GetForBrowserContext(
              fixed_context)) {
  CHECK(fixed_platform_keys_service_);
  CHECK(fixed_key_permissions_service_);
}

KeystoreService::KeystoreService(PlatformKeysService* platform_keys_service,
                                 KeyPermissionsService* key_permissions_service)
    : fixed_platform_keys_service_(platform_keys_service),
      fixed_key_permissions_service_(key_permissions_service) {
  CHECK(fixed_platform_keys_service_);
  CHECK(fixed_key_permissions_service_);
}

KeystoreService::KeystoreService() = default;
KeystoreService::~KeystoreService() = default;

PlatformKeysService* KeystoreService::GetPlatformKeys() {
  if (fixed_platform_keys_service_) {
    return fixed_platform_keys_service_;
  }

  PlatformKeysService* service =
      platform_keys::PlatformKeysServiceFactory::GetForBrowserContext(
          ProfileManager::GetPrimaryUserProfile());
  CHECK(service);
  return service;
}

KeyPermissionsService* KeystoreService::GetKeyPermissions() {
  if (fixed_key_permissions_service_) {
    return fixed_key_permissions_service_;
  }

  KeyPermissionsService* service =
      platform_keys::KeyPermissionsServiceFactory::GetForBrowserContext(
          ProfileManager::GetPrimaryUserProfile());
  CHECK(service);
  return service;
}

//------------------------------------------------------------------------------

void KeystoreService::ChallengeAttestationOnlyKeystore(
    KeystoreType type,
    const std::vector<uint8_t>& challenge,
    bool migrate,
    KeystoreAlgorithmName algorithm,
    ChallengeAttestationOnlyKeystoreCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ::attestation::KeyType key_crypto_type;
  switch (algorithm) {
    // Use RSA by default for backwards compatibility.
    case KeystoreAlgorithmName::kUnknown:
    case KeystoreAlgorithmName::kRsassaPkcs115:
      key_crypto_type = ::attestation::KEY_TYPE_RSA;
      break;
    case KeystoreAlgorithmName::kEcdsa:
      key_crypto_type = ::attestation::KEY_TYPE_ECC;
      break;
    case KeystoreAlgorithmName::kRsaOaep:
      std::move(callback).Run(
          base::unexpected(chromeos::platform_keys::KeystoreErrorToString(
              KeystoreError::kUnsupportedKeyType)));
      return;
  }

  ::attestation::VerifiedAccessFlow flow_type;
  switch (type) {
    case KeystoreType::kUser:
      flow_type = ::attestation::ENTERPRISE_USER;
      break;
    case KeystoreType::kDevice:
      flow_type = ::attestation::ENTERPRISE_MACHINE;
      break;
  }
  Profile* profile = ProfileManager::GetActiveUserProfile();

  std::string key_name_for_spkac;
  if (migrate && (flow_type == ::attestation::ENTERPRISE_MACHINE)) {
    key_name_for_spkac = base::StrCat(
        {attestation::kEnterpriseMachineKeyForSpkacPrefix, "keystore-",
         base::UnguessableToken::Create().ToString()});
  }

  // The lifetime of this object is bound to the callback.
  std::unique_ptr<attestation::TpmChallengeKey> challenge_key =
      attestation::TpmChallengeKeyFactory::Create();
  attestation::TpmChallengeKey* challenge_key_ptr = challenge_key.get();
  outstanding_challenges_.push_back(std::move(challenge_key));
  challenge_key_ptr->BuildResponse(
      flow_type, profile,
      base::BindOnce(&KeystoreService::DidChallengeAttestationOnlyKeystore,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     challenge_key_ptr),
      std::string(challenge.begin(), challenge.end()),
      /*register_key=*/migrate, key_crypto_type, key_name_for_spkac,
      /*signals=*/std::nullopt);
}

void KeystoreService::DidChallengeAttestationOnlyKeystore(
    ChallengeAttestationOnlyKeystoreCallback callback,
    void* challenge_key_ptr,
    const attestation::TpmChallengeKeyResult& result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  chromeos::ChallengeAttestationOnlyKeystoreResult result_to_return;
  if (result.IsSuccess()) {
    result_to_return = std::vector<uint8_t>(result.challenge_response.begin(),
                                            result.challenge_response.end());
  } else {
    result_to_return = base::unexpected(result.GetErrorMessage());
  }
  std::move(callback).Run(std::move(result_to_return));

  // Remove the outstanding challenge_key object.
  bool found = false;
  for (auto it = outstanding_challenges_.begin();
       it != outstanding_challenges_.end(); ++it) {
    if (it->get() == challenge_key_ptr) {
      outstanding_challenges_.erase(it);
      found = true;
      break;
    }
  }
  DCHECK(found);
}

//------------------------------------------------------------------------------

void KeystoreService::GetKeyStores(GetKeyStoresCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetPlatformKeys()->GetTokens(
      base::BindOnce(&KeystoreService::DidGetKeyStores, std::move(callback)));
}

// static
void KeystoreService::DidGetKeyStores(
    GetKeyStoresCallback callback,
    std::vector<TokenId> platform_keys_token_ids,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  chromeos::GetKeyStoresResult result;

  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::vector<KeystoreType> key_stores;
    for (const TokenId token_id : platform_keys_token_ids) {
      switch (token_id) {
        case TokenId::kUser:
          key_stores.push_back(KeystoreType::kUser);
          break;
        case TokenId::kSystem:
          key_stores.push_back(KeystoreType::kDevice);
          break;
      }
    }
    result = std::move(key_stores);
  } else {
    result = base::unexpected(
        chromeos::platform_keys::StatusToKeystoreError(status));
  }

  std::move(callback).Run(std::move(result));
}

//------------------------------------------------------------------------------

void KeystoreService::SelectClientCertificates(
    const std::vector<std::vector<uint8_t>>& certificate_authorities,
    SelectClientCertificatesCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<std::string> cert_authorities_str;
  cert_authorities_str.reserve(certificate_authorities.size());
  for (const std::vector<uint8_t>& ca : certificate_authorities) {
    cert_authorities_str.emplace_back(ca.begin(), ca.end());
  }

  GetPlatformKeys()->SelectClientCertificates(
      std::move(cert_authorities_str),
      base::BindOnce(&KeystoreService::DidSelectClientCertificates,
                     std::move(callback)));
}

// static
void KeystoreService::DidSelectClientCertificates(
    SelectClientCertificatesCallback callback,
    std::unique_ptr<net::CertificateList> matches,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  chromeos::KeystoreSelectClientCertificatesResult result;

  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::vector<std::vector<uint8_t>> output;
    for (scoped_refptr<net::X509Certificate> cert : *matches) {
      CRYPTO_BUFFER* der_buffer = cert->cert_buffer();
      const uint8_t* data = CRYPTO_BUFFER_data(der_buffer);
      std::vector<uint8_t> der_x509_certificate(
          data, UNSAFE_TODO(data + CRYPTO_BUFFER_len(der_buffer)));
      output.push_back(std::move(der_x509_certificate));
    }
    result = std::move(output);
  } else {
    result = base::unexpected(
        chromeos::platform_keys::StatusToKeystoreError(status));
  }

  std::move(callback).Run(std::move(result));
}

//------------------------------------------------------------------------------

void KeystoreService::GetCertificates(KeystoreType keystore,
                                      GetCertificatesCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PlatformKeysService* platform_keys_service = GetPlatformKeys();
  platform_keys_service->GetCertificates(
      KeystoreToToken(keystore),
      base::BindOnce(&KeystoreService::DidGetCertificates,
                     std::move(callback)));
}

// static
void KeystoreService::DidGetCertificates(
    GetCertificatesCallback callback,
    std::unique_ptr<net::CertificateList> certs,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  chromeos::GetCertificatesResult result;

  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::vector<std::vector<uint8_t>> output;
    for (scoped_refptr<net::X509Certificate> cert : *certs) {
      CRYPTO_BUFFER* der_buffer = cert->cert_buffer();
      const uint8_t* data = CRYPTO_BUFFER_data(der_buffer);
      std::vector<uint8_t> der_x509_certificate(
          data, UNSAFE_TODO(data + CRYPTO_BUFFER_len(der_buffer)));
      output.push_back(std::move(der_x509_certificate));
    }
    result = std::move(output);
  } else {
    result = base::unexpected(
        chromeos::platform_keys::StatusToKeystoreError(status));
  }

  std::move(callback).Run(std::move(result));
}

//------------------------------------------------------------------------------

void KeystoreService::AddCertificate(KeystoreType keystore,
                                     const std::vector<uint8_t>& certificate,
                                     AddCertificateCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  scoped_refptr<net::X509Certificate> cert_x509 = ParseCertificate(certificate);
  if (!cert_x509.get()) {
    std::move(callback).Run(/*is_error=*/true,
                            KeystoreError::kCertificateInvalid);
    return;
  }

  PlatformKeysService* platform_keys_service = GetPlatformKeys();
  platform_keys_service->ImportCertificate(
      KeystoreToToken(keystore), cert_x509,
      base::BindOnce(&KeystoreService::DidImportCertificate,
                     std::move(callback)));
}

void KeystoreService::DidImportCertificate(
    AddCertificateCallback callback,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::move(callback).Run(/*is_error=*/false, KeystoreError::kUnknown);
  } else {
    std::move(callback).Run(
        /*is_error=*/true,
        chromeos::platform_keys::StatusToKeystoreError(status));
  }
}

//------------------------------------------------------------------------------

void KeystoreService::RemoveCertificate(KeystoreType keystore,
                                        const std::vector<uint8_t>& certificate,
                                        RemoveCertificateCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  scoped_refptr<net::X509Certificate> cert_x509 = ParseCertificate(certificate);
  if (!cert_x509.get()) {
    std::move(callback).Run(/*is_error=*/true,
                            KeystoreError::kCertificateInvalid);
    return;
  }

  PlatformKeysService* platform_keys_service = GetPlatformKeys();
  platform_keys_service->RemoveCertificate(
      KeystoreToToken(keystore), cert_x509,
      base::BindOnce(&KeystoreService::DidRemoveCertificate,
                     std::move(callback)));
}

void KeystoreService::DidRemoveCertificate(
    RemoveCertificateCallback callback,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::move(callback).Run(/*is_error=*/false, KeystoreError::kUnknown);
  } else {
    std::move(callback).Run(
        /*is_error=*/true,
        chromeos::platform_keys::StatusToKeystoreError(status));
  }
}

//------------------------------------------------------------------------------

void KeystoreService::GetPublicKey(const std::vector<uint8_t>& certificate,
                                   KeystoreAlgorithmName algorithm_name,
                                   GetPublicKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!IsSigningAlgorithm(algorithm_name)) {
    std::move(callback).Run(
        base::unexpected(KeystoreError::kAlgorithmNotPermittedByCertificate));
    return;
  }

  std::string name = StringFromKeystoreAlgorithmName(algorithm_name);
  chromeos::platform_keys::GetPublicKeyAndAlgorithmOutput output =
      chromeos::platform_keys::GetPublicKeyAndAlgorithm(certificate, name);

  chromeos::GetPublicKeyResult result;
  if (output.status == chromeos::platform_keys::Status::kSuccess) {
    std::optional<KeystoreAlgorithm> signing_algorithm =
        chromeos::keystore_service_util::MakeKeystoreAlgorithmFromDictionary(
            output.algorithm);
    if (signing_algorithm) {
      chromeos::GetPublicKeySuccessResult success_result;
      success_result.public_key = std::move(output.public_key);
      success_result.algorithm_properties =
          std::move(signing_algorithm.value());
      result = std::move(success_result);
    } else {
      result = base::unexpected(KeystoreError::kUnsupportedAlgorithmType);
    }
  } else {
    result = base::unexpected(
        chromeos::platform_keys::StatusToKeystoreError(output.status));
  }
  std::move(callback).Run(std::move(result));
}

//------------------------------------------------------------------------------

void KeystoreService::GenerateKey(KeystoreType keystore,
                                  KeystoreAlgorithm algorithm,
                                  GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PlatformKeysService* platform_keys_service = GetPlatformKeys();
  TokenId token_id = KeystoreToToken(keystore);

  if (auto* params = std::get_if<RsassaPkcs115Params>(&algorithm)) {
    platform_keys_service->GenerateRSAKey(
        token_id, params->rsa_params.modulus_length,
        params->rsa_params.sw_backed,
        base::BindOnce(&KeystoreService::DidGenerateKey, std::move(callback)));
    return;
  }
  if (auto* params = std::get_if<KeystoreEcdsaParams>(&algorithm)) {
    platform_keys_service->GenerateECKey(
        token_id, params->named_curve,
        base::BindOnce(&KeystoreService::DidGenerateKey, std::move(callback)));
    return;
  }
  if (auto* params = std::get_if<RsaOaepParams>(&algorithm)) {
    platform_keys_service->GenerateRSAKey(
        token_id, params->rsa_params.modulus_length,
        params->rsa_params.sw_backed,
        base::BindOnce(&KeystoreService::DidGenerateKey, std::move(callback)));
    return;
  }
  NOTREACHED();
}

// static
void KeystoreService::DidGenerateKey(GenerateKeyCallback callback,
                                     std::vector<uint8_t> public_key,
                                     chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  chromeos::KeystoreBinaryResult result;
  if (status == chromeos::platform_keys::Status::kSuccess) {
    result = std::move(public_key);
  } else {
    result = base::unexpected(
        chromeos::platform_keys::StatusToKeystoreError(status));
  }
  std::move(callback).Run(std::move(result));
}

//------------------------------------------------------------------------------

void KeystoreService::RemoveKey(KeystoreType keystore,
                                const std::vector<uint8_t>& public_key,
                                RemoveKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetPlatformKeys()->RemoveKey(
      KeystoreToToken(keystore), public_key,
      base::BindOnce(&KeystoreService::DidRemoveKey, std::move(callback)));
}

// static
void KeystoreService::DidRemoveKey(RemoveKeyCallback callback,
                                   chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::move(callback).Run(/*is_error=*/false, KeystoreError::kUnknown);
  } else {
    std::move(callback).Run(
        /*is_error=*/true,
        chromeos::platform_keys::StatusToKeystoreError(status));
  }
}

//------------------------------------------------------------------------------

void KeystoreService::Sign(std::optional<KeystoreType> keystore,
                           const std::vector<uint8_t>& public_key,
                           SigningScheme scheme,
                           const std::vector<uint8_t>& data,
                           SignCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::optional<TokenId> token_id;
  if (keystore.has_value()) {
    token_id = KeystoreToToken(*keystore);
  }

  chromeos::platform_keys::KeyType key_type;
  chromeos::platform_keys::HashAlgorithm hash_algorithm;
  if (!UnpackSigningScheme(scheme, &key_type, &hash_algorithm)) {
    std::move(callback).Run(
        base::unexpected(KeystoreError::kUnsupportedAlgorithmType));
    return;
  }

  PlatformKeysService* service = GetPlatformKeys();
  auto cb = base::BindOnce(&KeystoreService::DidSign, std::move(callback));

  switch (key_type) {
    case chromeos::platform_keys::KeyType::kRsassaPkcs1V15:
      if (hash_algorithm == chromeos::platform_keys::HASH_ALGORITHM_NONE) {
        service->SignRSAPKCS1Raw(token_id, data, public_key, std::move(cb));
        return;
      }
      service->SignRsaPkcs1(token_id, data, public_key, hash_algorithm,
                            std::move(cb));
      return;
    case chromeos::platform_keys::KeyType::kEcdsa:
      service->SignEcdsa(token_id, data, public_key, hash_algorithm,
                         std::move(cb));
      return;
    case chromeos::platform_keys::KeyType::kRsaOaep:
      NOTREACHED();
  }
  NOTREACHED();
}

// static
void KeystoreService::DidSign(SignCallback callback,
                              std::vector<uint8_t> signature,
                              chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::move(callback).Run(std::move(signature));
  } else {
    std::move(callback).Run(base::unexpected(
        chromeos::platform_keys::StatusToKeystoreError(status)));
  }
}

//------------------------------------------------------------------------------

void KeystoreService::GetKeyTags(const std::vector<uint8_t>& public_key,
                                 GetKeyTagsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GetKeyPermissions()->IsCorporateKey(
      public_key,
      base::BindOnce(&KeystoreService::DidGetKeyTags, std::move(callback)));
}

// static
void KeystoreService::DidGetKeyTags(GetKeyTagsCallback callback,
                                    std::optional<bool> corporate,
                                    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  using KeyTag = chromeos::KeyTag;

  chromeos::GetKeyTagsResult result;

  if (status == chromeos::platform_keys::Status::kSuccess) {
    DCHECK(corporate.has_value());
    static_assert(sizeof(uint64_t) >= sizeof(KeyTag),
                  "Too many enum values for uint64_t");

    uint64_t tags = static_cast<uint64_t>(KeyTag::kNoTags);
    if (corporate.value()) {
      tags |= static_cast<uint64_t>(KeyTag::kCorporate);
    }
    result = tags;
  } else {
    result = base::unexpected(
        chromeos::platform_keys::StatusToKeystoreError(status));
  }

  std::move(callback).Run(std::move(result));
}

//------------------------------------------------------------------------------

void KeystoreService::AddKeyTags(const std::vector<uint8_t>& public_key,
                                 uint64_t tags,
                                 AddKeyTagsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (tags == static_cast<uint64_t>(chromeos::KeyTag::kNoTags)) {
    std::move(callback).Run(/*is_error=*/false, KeystoreError::kUnknown);
    return;
  }

  if (tags == static_cast<uint64_t>(chromeos::KeyTag::kCorporate)) {
    GetKeyPermissions()->SetCorporateKey(
        public_key,
        base::BindOnce(&KeystoreService::DidAddKeyTags, std::move(callback)));
    return;
  }

  std::move(callback).Run(/*is_error=*/true, KeystoreError::kUnsupportedKeyTag);
}

// static
void KeystoreService::DidAddKeyTags(AddKeyTagsCallback callback,
                                    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::move(callback).Run(/*is_error=*/false, KeystoreError::kUnknown);
  } else {
    std::move(callback).Run(
        /*is_error=*/true,
        chromeos::platform_keys::StatusToKeystoreError(status));
  }
}

//------------------------------------------------------------------------------

void KeystoreService::CanUserGrantPermissionForKey(
    const std::vector<uint8_t>& public_key,
    CanUserGrantPermissionForKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetKeyPermissions()->CanUserGrantPermissionForKey(public_key,
                                                    std::move(callback));
}

//------------------------------------------------------------------------------

void KeystoreService::SetAttributeForKey(
    KeystoreType keystore,
    const std::vector<uint8_t>& public_key,
    KeystoreKeyAttributeType keystore_attribute_type,
    const std::vector<uint8_t>& attribute_value,
    SetAttributeForKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto attribute_type = UnpackKeystoreKeyAttributeType(keystore_attribute_type);
  if (!attribute_type.has_value()) {
    std::move(callback).Run(/*is_error=*/true,
                            KeystoreError::kKeyAttributeSettingFailed);
    return;
  }

  PlatformKeysService* service = GetPlatformKeys();
  auto cb = base::BindOnce(&KeystoreService::DidSetAttributeForKey,
                           std::move(callback));

  service->SetAttributeForKey(KeystoreToToken(keystore), public_key,
                              attribute_type.value(), attribute_value,
                              std::move(cb));
}

// static
void KeystoreService::DidSetAttributeForKey(
    SetAttributeForKeyCallback callback,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::move(callback).Run(/*is_error=*/false, KeystoreError::kUnknown);
  } else {
    std::move(callback).Run(
        /*is_error=*/true,
        chromeos::platform_keys::StatusToKeystoreError(status));
  }
}
}  // namespace ash
