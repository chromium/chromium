// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/crosapi/keystore_service_ash.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

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
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/dbus/attestation/attestation_ca.pb.h"
#include "chromeos/crosapi/cpp/keystore_service_util.h"
#include "chromeos/crosapi/mojom/keystore_error.mojom.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom-shared.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace crosapi {

namespace {

using SigningAlgorithmName = mojom::KeystoreSigningAlgorithmName;
using SigningScheme = mojom::KeystoreSigningScheme;
using ::ash::platform_keys::KeyPermissionsService;
using ::ash::platform_keys::PlatformKeysService;
using ::chromeos::ExtensionPlatformKeysService;
using ::chromeos::platform_keys::TokenId;

const char kUnsupportedKeystoreType[] = "The token is not valid.";
const char kDeprecatedMethodError[] = "Deprecated method was called.";

// Converts a binary blob to a certificate.
scoped_refptr<net::X509Certificate> ParseCertificate(
    const std::vector<uint8_t>& input) {
  // Allow UTF-8 inside PrintableStrings in client certificates. See
  // crbug.com/770323 and crbug.com/788655.
  net::X509Certificate::UnsafeCreateOptions options;
  options.printable_string_is_utf8 = true;
  return net::X509Certificate::CreateFromBytesUnsafeOptions(input, options);
}

std::optional<TokenId> KeystoreToToken(mojom::KeystoreType type) {
  if (!crosapi::mojom::IsKnownEnumValue(type)) {
    return std::nullopt;
  }
  switch (type) {
    case mojom::KeystoreType::kUser:
      return TokenId::kUser;
    case mojom::KeystoreType::kDevice:
      return TokenId::kSystem;
  }
}

std::optional<std::string> StringFromSigningAlgorithmName(
    SigningAlgorithmName name) {
  switch (name) {
    case SigningAlgorithmName::kRsassaPkcs115:
      return crosapi::keystore_service_util::kWebCryptoRsassaPkcs1v15;
    case SigningAlgorithmName::kEcdsa:
      return crosapi::keystore_service_util::kWebCryptoEcdsa;
    case SigningAlgorithmName::kUnknown:
      return std::nullopt;
  }
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
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace

KeystoreServiceAsh::KeystoreServiceAsh(content::BrowserContext* fixed_context)
    : fixed_platform_keys_service_(
          ash::platform_keys::PlatformKeysServiceFactory::GetForBrowserContext(
              fixed_context)),
      fixed_key_permissions_service_(
          ash::platform_keys::KeyPermissionsServiceFactory::
              GetForBrowserContext(fixed_context)) {
  CHECK(fixed_platform_keys_service_);
  CHECK(fixed_key_permissions_service_);
}

KeystoreServiceAsh::KeystoreServiceAsh(
    PlatformKeysService* platform_keys_service,
    KeyPermissionsService* key_permissions_service)
    : fixed_platform_keys_service_(platform_keys_service),
      fixed_key_permissions_service_(key_permissions_service) {
  CHECK(fixed_platform_keys_service_);
  CHECK(fixed_key_permissions_service_);
}

KeystoreServiceAsh::KeystoreServiceAsh() = default;
KeystoreServiceAsh::~KeystoreServiceAsh() = default;

void KeystoreServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::KeystoreService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

PlatformKeysService* KeystoreServiceAsh::GetPlatformKeys() {
  if (fixed_platform_keys_service_) {
    return fixed_platform_keys_service_;
  }

  PlatformKeysService* service =
      ash::platform_keys::PlatformKeysServiceFactory::GetForBrowserContext(
          ProfileManager::GetPrimaryUserProfile());
  CHECK(service);
  return service;
}

KeyPermissionsService* KeystoreServiceAsh::GetKeyPermissions() {
  if (fixed_key_permissions_service_) {
    return fixed_key_permissions_service_;
  }

  KeyPermissionsService* service =
      ash::platform_keys::KeyPermissionsServiceFactory::GetForBrowserContext(
          ProfileManager::GetPrimaryUserProfile());
  CHECK(service);
  return service;
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::ChallengeAttestationOnlyKeystore(
    mojom::KeystoreType type,
    const std::vector<uint8_t>& challenge,
    bool migrate,
    mojom::KeystoreSigningAlgorithmName algorithm,
    ChallengeAttestationOnlyKeystoreCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!crosapi::mojom::IsKnownEnumValue(type)) {
    std::move(callback).Run(
        mojom::ChallengeAttestationOnlyKeystoreResult::NewErrorMessage(
            kUnsupportedKeystoreType));
    return;
  }

  attestation::KeyType key_crypto_type;
  switch (algorithm) {
    // Use RSA by default for backwards compatibility.
    case mojom::KeystoreSigningAlgorithmName::kUnknown:
    case mojom::KeystoreSigningAlgorithmName::kRsassaPkcs115:
      key_crypto_type = attestation::KEY_TYPE_RSA;
      break;
    case mojom::KeystoreSigningAlgorithmName::kEcdsa:
      key_crypto_type = attestation::KEY_TYPE_ECC;
      break;
  }

  attestation::VerifiedAccessFlow flow_type;
  switch (type) {
    case mojom::KeystoreType::kUser:
      flow_type = attestation::ENTERPRISE_USER;
      break;
    case mojom::KeystoreType::kDevice:
      flow_type = attestation::ENTERPRISE_MACHINE;
      break;
  }
  Profile* profile = ProfileManager::GetActiveUserProfile();

  std::string key_name_for_spkac;
  if (migrate && (flow_type == attestation::ENTERPRISE_MACHINE)) {
    key_name_for_spkac = base::StrCat(
        {ash::attestation::kEnterpriseMachineKeyForSpkacPrefix, "keystore-",
         base::UnguessableToken::Create().ToString()});
  }

  // The lifetime of this object is bound to the callback.
  std::unique_ptr<ash::attestation::TpmChallengeKey> challenge_key =
      ash::attestation::TpmChallengeKeyFactory::Create();
  ash::attestation::TpmChallengeKey* challenge_key_ptr = challenge_key.get();
  outstanding_challenges_.push_back(std::move(challenge_key));
  challenge_key_ptr->BuildResponse(
      flow_type, profile,
      base::BindOnce(&KeystoreServiceAsh::DidChallengeAttestationOnlyKeystore,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     challenge_key_ptr),
      std::string(challenge.begin(), challenge.end()),
      /*register_key=*/migrate, key_crypto_type, key_name_for_spkac,
      /*signals=*/std::nullopt);
}

void KeystoreServiceAsh::DidChallengeAttestationOnlyKeystore(
    ChallengeAttestationOnlyKeystoreCallback callback,
    void* challenge_key_ptr,
    const ash::attestation::TpmChallengeKeyResult& result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  crosapi::mojom::ChallengeAttestationOnlyKeystoreResultPtr result_ptr;
  if (result.IsSuccess()) {
    result_ptr =
        mojom::ChallengeAttestationOnlyKeystoreResult::NewChallengeResponse(
            std::vector<uint8_t>(result.challenge_response.begin(),
                                 result.challenge_response.end()));
  } else {
    result_ptr = mojom::ChallengeAttestationOnlyKeystoreResult::NewErrorMessage(
        result.GetErrorMessage());
  }
  std::move(callback).Run(std::move(result_ptr));

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

void KeystoreServiceAsh::DEPRECATED_ChallengeAttestationOnlyKeystore(
    const std::string& challenge,
    mojom::KeystoreType type,
    bool migrate,
    DEPRECATED_ChallengeAttestationOnlyKeystoreCallback callback) {
  LOG(ERROR) << "DEPRECATED_ChallengeAttestationOnlyKeystore was called.";
  base::debug::DumpWithoutCrashing();

  std::move(callback).Run(
      mojom::DEPRECATED_KeystoreStringResult::NewErrorMessage(
          kDeprecatedMethodError));
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::GetKeyStores(GetKeyStoresCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetPlatformKeys()->GetTokens(base::BindOnce(
      &KeystoreServiceAsh::DidGetKeyStores, std::move(callback)));
}

// static
void KeystoreServiceAsh::DidGetKeyStores(
    GetKeyStoresCallback callback,
    const std::vector<TokenId> platform_keys_token_ids,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  mojom::GetKeyStoresResultPtr result_ptr;

  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::vector<mojom::KeystoreType> key_stores;
    for (const TokenId token_id : platform_keys_token_ids) {
      switch (token_id) {
        case TokenId::kUser:
          key_stores.push_back(mojom::KeystoreType::kUser);
          break;
        case TokenId::kSystem:
          key_stores.push_back(mojom::KeystoreType::kDevice);
          break;
      }
    }
    result_ptr = mojom::GetKeyStoresResult::NewKeyStores(std::move(key_stores));
  } else {
    result_ptr = mojom::GetKeyStoresResult::NewError(
        chromeos::platform_keys::StatusToKeystoreError(status));
  }

  std::move(callback).Run(std::move(result_ptr));
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::DEPRECATED_GetKeyStores(
    DEPRECATED_GetKeyStoresCallback callback) {
  LOG(ERROR) << "DEPRECATED_GetKeyStores method was called.";
  base::debug::DumpWithoutCrashing();

  std::move(callback).Run(mojom::DEPRECATED_GetKeyStoresResult::NewErrorMessage(
      kDeprecatedMethodError));
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::SelectClientCertificates(
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
      base::BindOnce(&KeystoreServiceAsh::DidSelectClientCertificates,
                     std::move(callback)));
}

// static
void KeystoreServiceAsh::DidSelectClientCertificates(
    SelectClientCertificatesCallback callback,
    std::unique_ptr<net::CertificateList> matches,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  mojom::KeystoreSelectClientCertificatesResultPtr result_ptr;

  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::vector<std::vector<uint8_t>> output;
    for (scoped_refptr<net::X509Certificate> cert : *matches) {
      CRYPTO_BUFFER* der_buffer = cert->cert_buffer();
      const uint8_t* data = CRYPTO_BUFFER_data(der_buffer);
      std::vector<uint8_t> der_x509_certificate(
          data, data + CRYPTO_BUFFER_len(der_buffer));
      output.push_back(std::move(der_x509_certificate));
    }
    result_ptr = mojom::KeystoreSelectClientCertificatesResult::NewCertificates(
        std::move(output));
  } else {
    result_ptr = mojom::KeystoreSelectClientCertificatesResult::NewError(
        chromeos::platform_keys::StatusToKeystoreError(status));
  }

  std::move(callback).Run(std::move(result_ptr));
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::GetCertificates(mojom::KeystoreType keystore,
                                         GetCertificatesCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PlatformKeysService* platform_keys_service = GetPlatformKeys();
  std::optional<TokenId> token_id = KeystoreToToken(keystore);
  if (!token_id) {
    std::move(callback).Run(mojom::GetCertificatesResult::NewError(
        mojom::KeystoreError::kUnsupportedKeystoreType));
    return;
  }

  platform_keys_service->GetCertificates(
      token_id.value(), base::BindOnce(&KeystoreServiceAsh::DidGetCertificates,
                                       std::move(callback)));
}

// static
void KeystoreServiceAsh::DidGetCertificates(
    GetCertificatesCallback callback,
    std::unique_ptr<net::CertificateList> certs,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  mojom::GetCertificatesResultPtr result_ptr;

  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::vector<std::vector<uint8_t>> output;
    for (scoped_refptr<net::X509Certificate> cert : *certs) {
      CRYPTO_BUFFER* der_buffer = cert->cert_buffer();
      const uint8_t* data = CRYPTO_BUFFER_data(der_buffer);
      std::vector<uint8_t> der_x509_certificate(
          data, data + CRYPTO_BUFFER_len(der_buffer));
      output.push_back(std::move(der_x509_certificate));
    }
    result_ptr =
        mojom::GetCertificatesResult::NewCertificates(std::move(output));
  } else {
    result_ptr = mojom::GetCertificatesResult::NewError(
        chromeos::platform_keys::StatusToKeystoreError(status));
  }

  std::move(callback).Run(std::move(result_ptr));
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::DEPRECATED_GetCertificates(
    mojom::KeystoreType keystore,
    DEPRECATED_GetCertificatesCallback callback) {
  LOG(ERROR) << "DEPRECATED_GetCertificates method was called.";
  base::debug::DumpWithoutCrashing();

  std::move(callback).Run(
      mojom::DEPRECATED_GetCertificatesResult::NewErrorMessage(
          kDeprecatedMethodError));
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::AddCertificate(mojom::KeystoreType keystore,
                                        const std::vector<uint8_t>& certificate,
                                        AddCertificateCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  scoped_refptr<net::X509Certificate> cert_x509 = ParseCertificate(certificate);
  if (!cert_x509.get()) {
    std::move(callback).Run(/*is_error=*/true,
                            mojom::KeystoreError::kCertificateInvalid);
    return;
  }
  std::optional<TokenId> token_id = KeystoreToToken(keystore);
  if (!token_id) {
    std::move(callback).Run(/*is_error=*/true,
                            mojom::KeystoreError::kUnsupportedKeystoreType);
    return;
  }

  PlatformKeysService* platform_keys_service = GetPlatformKeys();
  platform_keys_service->ImportCertificate(
      token_id.value(), cert_x509,
      base::BindOnce(&KeystoreServiceAsh::DidImportCertificate,
                     std::move(callback)));
}

void KeystoreServiceAsh::DidImportCertificate(
    AddCertificateCallback callback,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::move(callback).Run(/*is_error=*/false, mojom::KeystoreError::kUnknown);
  } else {
    std::move(callback).Run(
        /*is_error=*/true,
        chromeos::platform_keys::StatusToKeystoreError(status));
  }
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::DEPRECATED_AddCertificate(
    mojom::KeystoreType keystore,
    const std::vector<uint8_t>& certificate,
    DEPRECATED_AddCertificateCallback callback) {
  LOG(ERROR) << "DEPRECATED_AddCertificate method was called.";
  base::debug::DumpWithoutCrashing();

  std::move(callback).Run(kDeprecatedMethodError);
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::RemoveCertificate(
    mojom::KeystoreType keystore,
    const std::vector<uint8_t>& certificate,
    RemoveCertificateCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  scoped_refptr<net::X509Certificate> cert_x509 = ParseCertificate(certificate);
  if (!cert_x509.get()) {
    std::move(callback).Run(/*is_error=*/true,
                            mojom::KeystoreError::kCertificateInvalid);
    return;
  }
  std::optional<TokenId> token_id = KeystoreToToken(keystore);
  if (!token_id) {
    std::move(callback).Run(/*is_error=*/true,
                            mojom::KeystoreError::kUnsupportedKeystoreType);
    return;
  }

  PlatformKeysService* platform_keys_service = GetPlatformKeys();
  platform_keys_service->RemoveCertificate(
      token_id.value(), cert_x509,
      base::BindOnce(&KeystoreServiceAsh::DidRemoveCertificate,
                     std::move(callback)));
}

void KeystoreServiceAsh::DidRemoveCertificate(
    RemoveCertificateCallback callback,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::move(callback).Run(/*is_error=*/false, mojom::KeystoreError::kUnknown);
  } else {
    std::move(callback).Run(
        /*is_error=*/true,
        chromeos::platform_keys::StatusToKeystoreError(status));
  }
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::DEPRECATED_RemoveCertificate(
    mojom::KeystoreType keystore,
    const std::vector<uint8_t>& certificate,
    DEPRECATED_RemoveCertificateCallback callback) {
  LOG(ERROR) << "DEPRECATED_RemoveCertificate method was called.";
  base::debug::DumpWithoutCrashing();

  std::move(callback).Run(kDeprecatedMethodError);
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::GetPublicKey(
    const std::vector<uint8_t>& certificate,
    mojom::KeystoreSigningAlgorithmName algorithm_name,
    GetPublicKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::optional<std::string> name =
      StringFromSigningAlgorithmName(algorithm_name);
  if (!name) {
    std::move(callback).Run(mojom::GetPublicKeyResult::NewError(
        mojom::KeystoreError::kAlgorithmNotPermittedByCertificate));
    return;
  }

  chromeos::platform_keys::GetPublicKeyAndAlgorithmOutput output =
      chromeos::platform_keys::GetPublicKeyAndAlgorithm(certificate,
                                                        name.value());

  mojom::GetPublicKeyResultPtr result_ptr;
  if (output.status == chromeos::platform_keys::Status::kSuccess) {
    std::optional<crosapi::mojom::KeystoreSigningAlgorithmPtr>
        signing_algorithm =
            crosapi::keystore_service_util::SigningAlgorithmFromDictionary(
                output.algorithm);
    if (signing_algorithm) {
      mojom::GetPublicKeySuccessResultPtr success_result_ptr =
          mojom::GetPublicKeySuccessResult::New();
      success_result_ptr->public_key = std::move(output.public_key);
      success_result_ptr->algorithm_properties =
          std::move(signing_algorithm.value());
      result_ptr = mojom::GetPublicKeyResult::NewSuccessResult(
          std::move(success_result_ptr));
    } else {
      result_ptr = mojom::GetPublicKeyResult::NewError(
          crosapi::mojom::KeystoreError::kUnsupportedAlgorithmType);
    }
  } else {
    result_ptr = mojom::GetPublicKeyResult::NewError(
        chromeos::platform_keys::StatusToKeystoreError(output.status));
  }
  std::move(callback).Run(std::move(result_ptr));
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::DEPRECATED_GetPublicKey(
    const std::vector<uint8_t>& certificate,
    mojom::KeystoreSigningAlgorithmName algorithm_name,
    DEPRECATED_GetPublicKeyCallback callback) {
  LOG(ERROR) << "DEPRECATED_GetPublicKey method was called.";
  base::debug::DumpWithoutCrashing();

  std::move(callback).Run(mojom::DEPRECATED_GetPublicKeyResult::NewErrorMessage(
      kDeprecatedMethodError));
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::DEPRECATED_ExtensionGenerateKey(
    mojom::KeystoreType keystore,
    mojom::KeystoreSigningAlgorithmPtr algorithm,
    const std::optional<std::string>& extension_id,
    DEPRECATED_ExtensionGenerateKeyCallback callback) {
  LOG(ERROR) << "DEPRECATED_ExtensionGenerateKey method was called.";
  base::debug::DumpWithoutCrashing();

  std::move(callback).Run(
      mojom::DEPRECATED_ExtensionKeystoreBinaryResult::NewErrorMessage(
          kDeprecatedMethodError));
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::DEPRECATED_ExtensionSign(
    KeystoreType keystore,
    const std::vector<uint8_t>& public_key,
    SigningScheme scheme,
    const std::vector<uint8_t>& data,
    const std::string& extension_id,
    DEPRECATED_ExtensionSignCallback callback) {
  LOG(ERROR) << "DEPRECATED_ExtensionSign method was called.";
  base::debug::DumpWithoutCrashing();

  std::move(callback).Run(
      mojom::DEPRECATED_ExtensionKeystoreBinaryResult::NewErrorMessage(
          kDeprecatedMethodError));
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::GenerateKey(
    mojom::KeystoreType keystore,
    mojom::KeystoreSigningAlgorithmPtr algorithm,
    GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PlatformKeysService* platform_keys_service = GetPlatformKeys();
  std::optional<TokenId> token_id = KeystoreToToken(keystore);
  if (!token_id) {
    std::move(callback).Run(mojom::KeystoreBinaryResult::NewError(
        mojom::KeystoreError::kUnsupportedKeystoreType));
    return;
  }

  using Tag = mojom::KeystoreSigningAlgorithm::Tag;
  switch (algorithm->which()) {
    case Tag::kPkcs115: {
      platform_keys_service->GenerateRSAKey(
          token_id.value(), algorithm->get_pkcs115()->modulus_length,
          algorithm->get_pkcs115()->sw_backed,
          base::BindOnce(&KeystoreServiceAsh::DidGenerateKey,
                         std::move(callback)));
      return;
    }
    case Tag::kEcdsa: {
      platform_keys_service->GenerateECKey(
          token_id.value(), algorithm->get_ecdsa()->named_curve,
          base::BindOnce(&KeystoreServiceAsh::DidGenerateKey,
                         std::move(callback)));
      return;
    }
    default: {
      std::move(callback).Run(mojom::KeystoreBinaryResult::NewError(
          mojom::KeystoreError::kAlgorithmNotSupported));
      return;
    }
  }
}

// static
void KeystoreServiceAsh::DidGenerateKey(
    GenerateKeyCallback callback,
    std::vector<uint8_t> public_key,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  crosapi::mojom::KeystoreBinaryResultPtr result_ptr;
  if (status == chromeos::platform_keys::Status::kSuccess) {
    result_ptr = mojom::KeystoreBinaryResult::NewBlob(std::move(public_key));
  } else {
    result_ptr = mojom::KeystoreBinaryResult::NewError(
        chromeos::platform_keys::StatusToKeystoreError(status));
  }
  std::move(callback).Run(std::move(result_ptr));
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::RemoveKey(KeystoreType keystore,
                                   const std::vector<uint8_t>& public_key,
                                   RemoveKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::optional<TokenId> token_id = KeystoreToToken(keystore);
  if (!token_id) {
    std::move(callback).Run(/*is_error=*/true,
                            mojom::KeystoreError::kUnsupportedKeystoreType);
    return;
  }

  GetPlatformKeys()->RemoveKey(
      token_id.value(), public_key,
      base::BindOnce(&KeystoreServiceAsh::DidRemoveKey, std::move(callback)));
}

// static
void KeystoreServiceAsh::DidRemoveKey(RemoveKeyCallback callback,
                                      chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::move(callback).Run(/*is_error=*/false, mojom::KeystoreError::kUnknown);
  } else {
    std::move(callback).Run(
        /*is_error=*/true,
        chromeos::platform_keys::StatusToKeystoreError(status));
  }
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::Sign(bool is_keystore_provided,
                              KeystoreType keystore,
                              const std::vector<uint8_t>& public_key,
                              SigningScheme scheme,
                              const std::vector<uint8_t>& data,
                              SignCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::optional<TokenId> token_id;
  if (is_keystore_provided) {
    token_id = KeystoreToToken(keystore);
    if (!token_id) {
      std::move(callback).Run(mojom::KeystoreBinaryResult::NewError(
          mojom::KeystoreError::kUnsupportedKeystoreType));
      return;
    }
  }

  chromeos::platform_keys::KeyType key_type;
  chromeos::platform_keys::HashAlgorithm hash_algorithm;
  if (!UnpackSigningScheme(scheme, &key_type, &hash_algorithm)) {
    std::move(callback).Run(mojom::KeystoreBinaryResult::NewError(
        mojom::KeystoreError::kUnsupportedAlgorithmType));
    return;
  }

  PlatformKeysService* service = GetPlatformKeys();
  auto cb = base::BindOnce(&KeystoreServiceAsh::DidSign, std::move(callback));

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
  }
}

// static
void KeystoreServiceAsh::DidSign(SignCallback callback,
                                 std::vector<uint8_t> signature,
                                 chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::move(callback).Run(
        mojom::KeystoreBinaryResult::NewBlob(std::move(signature)));
  } else {
    std::move(callback).Run(mojom::KeystoreBinaryResult::NewError(
        chromeos::platform_keys::StatusToKeystoreError(status)));
  }
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::GetKeyTags(const std::vector<uint8_t>& public_key,
                                    GetKeyTagsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GetKeyPermissions()->IsCorporateKey(
      public_key,
      base::BindOnce(&KeystoreServiceAsh::DidGetKeyTags, std::move(callback)));
}

// static
void KeystoreServiceAsh::DidGetKeyTags(GetKeyTagsCallback callback,
                                       std::optional<bool> corporate,
                                       chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  using KeyTag = crosapi::mojom::KeyTag;

  crosapi::mojom::GetKeyTagsResultPtr result_ptr;

  if (status == chromeos::platform_keys::Status::kSuccess) {
    DCHECK(corporate.has_value());
    static_assert(sizeof(uint64_t) >= sizeof(KeyTag),
                  "Too many enum values for uint64_t");

    uint64_t tags = static_cast<uint64_t>(KeyTag::kNoTags);
    if (corporate.value()) {
      tags |= static_cast<uint64_t>(KeyTag::kCorporate);
    }
    result_ptr = crosapi::mojom::GetKeyTagsResult::NewTags(tags);
  } else {
    result_ptr = crosapi::mojom::GetKeyTagsResult::NewError(
        chromeos::platform_keys::StatusToKeystoreError(status));
  }

  std::move(callback).Run(std::move(result_ptr));
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::AddKeyTags(const std::vector<uint8_t>& public_key,
                                    uint64_t tags,
                                    AddKeyTagsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (tags == static_cast<uint64_t>(mojom::KeyTag::kNoTags)) {
    std::move(callback).Run(/*is_error=*/false,
                            crosapi::mojom::KeystoreError::kUnknown);
    return;
  }

  if (tags == static_cast<uint64_t>(mojom::KeyTag::kCorporate)) {
    GetKeyPermissions()->SetCorporateKey(
        public_key, base::BindOnce(&KeystoreServiceAsh::DidAddKeyTags,
                                   std::move(callback)));
    return;
  }

  std::move(callback).Run(/*is_error=*/true,
                          crosapi::mojom::KeystoreError::kUnsupportedKeyTag);
}

// static
void KeystoreServiceAsh::DidAddKeyTags(AddKeyTagsCallback callback,
                                       chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::move(callback).Run(/*is_error=*/false, mojom::KeystoreError::kUnknown);
  } else {
    std::move(callback).Run(
        /*is_error=*/true,
        chromeos::platform_keys::StatusToKeystoreError(status));
  }
}

//------------------------------------------------------------------------------

void KeystoreServiceAsh::CanUserGrantPermissionForKey(
    const std::vector<uint8_t>& public_key,
    CanUserGrantPermissionForKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Strictly speaking, crosapi::CanUserGrantPermissionForKeyCallback is a
  // different type than platform_keys::CanUserGrantPermissionForKeyCallback.
  // But as long as signatures are the same, we can just pass `callback`.
  GetKeyPermissions()->CanUserGrantPermissionForKey(public_key,
                                                    std::move(callback));
}

}  // namespace crosapi
