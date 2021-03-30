// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/keystore_service_ash.h"

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key.h"
#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/crosapi/cpp/keystore_service_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace crosapi {

using ExtensionPlatformKeysService = chromeos::ExtensionPlatformKeysService;
using PlatformKeysService = chromeos::platform_keys::PlatformKeysService;
using TokenId = chromeos::platform_keys::TokenId;
using SigningAlgorithmName = crosapi::mojom::KeystoreSigningAlgorithmName;

namespace {

const char kEnterprisePlatformErrorInvalidX509Cert[] =
    "Certificate is not a valid X.509 certificate.";
const char kUnsupportedKeystoreType[] = "The token is not valid.";
const char kUnsupportedAlgorithmType[] = "Algorithm type is not supported.";
const char kUnsupportedLacrosVersion[] = "Internal version incompatibility.";

// Converts a binary blob to a certificate.
scoped_refptr<net::X509Certificate> ParseCertificate(
    const std::vector<uint8_t>& input) {
  // Allow UTF-8 inside PrintableStrings in client certificates. See
  // crbug.com/770323 and crbug.com/788655.
  net::X509Certificate::UnsafeCreateOptions options;
  options.printable_string_is_utf8 = true;
  return net::X509Certificate::CreateFromBytesUnsafeOptions(
      reinterpret_cast<const char*>(input.data()), input.size(), options);
}

ExtensionPlatformKeysService* GetExtensionPlatformKeys() {
  ExtensionPlatformKeysService* service =
      chromeos::ExtensionPlatformKeysServiceFactory::GetForBrowserContext(
          ProfileManager::GetActiveUserProfile());
  CHECK(service);
  return service;
}

PlatformKeysService* GetPlatformKeys() {
  PlatformKeysService* service =
      chromeos::platform_keys::PlatformKeysServiceFactory::GetForBrowserContext(
          ProfileManager::GetActiveUserProfile());
  CHECK(service);
  return service;
}

base::Optional<TokenId> KeystoreToToken(mojom::KeystoreType type) {
  if (!crosapi::mojom::IsKnownEnumValue(type)) {
    return base::nullopt;
  }
  switch (type) {
    case mojom::KeystoreType::kUser:
      return TokenId::kUser;
    case mojom::KeystoreType::kDevice:
      return TokenId::kSystem;
  }
}

base::Optional<std::string> StringFromSigningAlgorithmName(
    SigningAlgorithmName name) {
  switch (name) {
    case SigningAlgorithmName::kRsassaPkcs115:
      return crosapi::keystore_service_util::kWebCryptoRsassaPkcs1v15;
    case SigningAlgorithmName::kEcdsa:
      return crosapi::keystore_service_util::kWebCryptoEcdsa;
    case SigningAlgorithmName::kUnknown:
      return base::nullopt;
  }
}

}  // namespace

KeystoreServiceAsh::KeystoreServiceAsh() = default;

KeystoreServiceAsh::~KeystoreServiceAsh() = default;

void KeystoreServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::KeystoreService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void KeystoreServiceAsh::ChallengeAttestationOnlyKeystore(
    const std::string& challenge,
    mojom::KeystoreType type,
    bool migrate,
    ChallengeAttestationOnlyKeystoreCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!crosapi::mojom::IsKnownEnumValue(type)) {
    std::move(callback).Run(
        mojom::KeystoreStringResult::NewErrorMessage(kUnsupportedKeystoreType));
    return;
  }

  chromeos::attestation::AttestationKeyType key_type;
  switch (type) {
    case mojom::KeystoreType::kUser:
      key_type = chromeos::attestation::KEY_USER;
      break;
    case mojom::KeystoreType::kDevice:
      key_type = chromeos::attestation::KEY_DEVICE;
      break;
  }
  Profile* profile = ProfileManager::GetActiveUserProfile();

  std::string key_name_for_spkac;
  if (migrate && (key_type == chromeos::attestation::KEY_DEVICE)) {
    key_name_for_spkac =
        base::StrCat({ash::attestation::kEnterpriseMachineKeyForSpkacPrefix,
                      "lacros-", base::UnguessableToken::Create().ToString()});
  }

  std::unique_ptr<ash::attestation::TpmChallengeKey> challenge_key =
      ash::attestation::TpmChallengeKeyFactory::Create();
  ash::attestation::TpmChallengeKey* challenge_key_ptr = challenge_key.get();
  outstanding_challenges_.push_back(std::move(challenge_key));
  challenge_key_ptr->BuildResponse(
      key_type, profile,
      base::BindOnce(&KeystoreServiceAsh::DidChallengeAttestationOnlyKeystore,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     challenge_key_ptr),
      challenge,
      /*register_key=*/migrate, key_name_for_spkac);
}

void KeystoreServiceAsh::GetKeyStores(GetKeyStoresCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PlatformKeysService* platform_keys_service = GetPlatformKeys();

  platform_keys_service->GetTokens(
      base::BindOnce(&KeystoreServiceAsh::OnGetTokens, std::move(callback)));
}

void KeystoreServiceAsh::GetCertificates(mojom::KeystoreType keystore,
                                         GetCertificatesCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PlatformKeysService* platform_keys_service = GetPlatformKeys();
  base::Optional<TokenId> token_id = KeystoreToToken(keystore);
  if (!token_id) {
    std::move(callback).Run(mojom::GetCertificatesResult::NewErrorMessage(
        kUnsupportedKeystoreType));
    return;
  }

  platform_keys_service->GetCertificates(
      token_id.value(), base::BindOnce(&KeystoreServiceAsh::OnGetCertificates,
                                       std::move(callback)));
}

void KeystoreServiceAsh::GenerateKey(
    mojom::KeystoreType keystore,
    mojom::KeystoreSigningAlgorithmPtr algorithm,
    const base::Optional<std::string>& extension_id,
    GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!extension_id) {
    std::move(callback).Run(mojom::KeystoreBinaryResult::NewErrorMessage(
        kUnsupportedLacrosVersion));
    return;
  }

  ExtensionPlatformKeysService* ext_platform_keys_service =
      GetExtensionPlatformKeys();
  base::Optional<TokenId> token_id = KeystoreToToken(keystore);
  if (!token_id) {
    std::move(callback).Run(
        mojom::KeystoreBinaryResult::NewErrorMessage(kUnsupportedKeystoreType));
    return;
  }

  switch (algorithm->which()) {
    case mojom::KeystoreSigningAlgorithm::Tag::PKCS115: {
      auto c = base::BindOnce(&KeystoreServiceAsh::OnGenerateKey,
                              std::move(callback));
      ext_platform_keys_service->GenerateRSAKey(
          token_id.value(), algorithm->get_pkcs115()->modulus_length,
          *extension_id, std::move(c));
      break;
    }
    case mojom::KeystoreSigningAlgorithm::Tag::ECDSA: {
      auto c = base::BindOnce(&KeystoreServiceAsh::OnGenerateKey,
                              std::move(callback));
      ext_platform_keys_service->GenerateECKey(
          token_id.value(), algorithm->get_ecdsa()->named_curve, *extension_id,
          std::move(c));
      break;
    }
    default: {
      std::move(callback).Run(mojom::KeystoreBinaryResult::NewErrorMessage(
          chromeos::platform_keys::StatusToString(
              chromeos::platform_keys::Status::kErrorAlgorithmNotSupported)));
      break;
    }
  }
}

void KeystoreServiceAsh::AddCertificate(mojom::KeystoreType keystore,
                                        const std::vector<uint8_t>& certificate,
                                        AddCertificateCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  scoped_refptr<net::X509Certificate> cert_x509 = ParseCertificate(certificate);
  if (!cert_x509.get()) {
    std::move(callback).Run(kEnterprisePlatformErrorInvalidX509Cert);
    return;
  }
  base::Optional<TokenId> token_id = KeystoreToToken(keystore);
  if (!token_id) {
    std::move(callback).Run(kUnsupportedKeystoreType);
    return;
  }

  PlatformKeysService* platform_keys_service = GetPlatformKeys();
  platform_keys_service->ImportCertificate(
      token_id.value(), cert_x509,
      base::BindOnce(&KeystoreServiceAsh::OnImportCertificate,
                     std::move(callback)));
}

void KeystoreServiceAsh::RemoveCertificate(
    mojom::KeystoreType keystore,
    const std::vector<uint8_t>& certificate,
    RemoveCertificateCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  scoped_refptr<net::X509Certificate> cert_x509 = ParseCertificate(certificate);
  if (!cert_x509.get()) {
    std::move(callback).Run(kEnterprisePlatformErrorInvalidX509Cert);
    return;
  }
  base::Optional<TokenId> token_id = KeystoreToToken(keystore);
  if (!token_id) {
    std::move(callback).Run(kUnsupportedKeystoreType);
    return;
  }

  PlatformKeysService* platform_keys_service = GetPlatformKeys();
  platform_keys_service->RemoveCertificate(
      token_id.value(), cert_x509,
      base::BindOnce(&KeystoreServiceAsh::OnRemoveCertificate,
                     std::move(callback)));
}

void KeystoreServiceAsh::GetPublicKey(
    const std::vector<uint8_t>& certificate,
    mojom::KeystoreSigningAlgorithmName algorithm_name,
    GetPublicKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Optional<std::string> name =
      StringFromSigningAlgorithmName(algorithm_name);
  if (!name) {
    std::move(callback).Run(mojom::GetPublicKeyResult::NewErrorMessage(
        chromeos::platform_keys::StatusToString(
            chromeos::platform_keys::Status::
                kErrorAlgorithmNotPermittedByCertificate)));
    return;
  }

  chromeos::platform_keys::GetPublicKeyAndAlgorithmOutput output =
      chromeos::platform_keys::GetPublicKeyAndAlgorithm(certificate,
                                                        name.value());

  mojom::GetPublicKeyResultPtr result_ptr = mojom::GetPublicKeyResult::New();
  if (output.status == chromeos::platform_keys::Status::kSuccess) {
    base::Optional<crosapi::mojom::KeystoreSigningAlgorithmPtr>
        signing_algorithm =
            crosapi::keystore_service_util::SigningAlgorithmFromDictionary(
                output.algorithm);
    if (signing_algorithm) {
      mojom::GetPublicKeySuccessResultPtr success_result_ptr =
          mojom::GetPublicKeySuccessResult::New();
      success_result_ptr->public_key = std::move(output.public_key);
      success_result_ptr->algorithm_properties =
          std::move(signing_algorithm.value());
      result_ptr->set_success_result(std::move(success_result_ptr));
    } else {
      result_ptr->set_error_message(kUnsupportedAlgorithmType);
    }
  } else {
    result_ptr->set_error_message(
        chromeos::platform_keys::StatusToString(output.status));
  }
  std::move(callback).Run(std::move(result_ptr));
}

void KeystoreServiceAsh::Sign(KeystoreType keystore,
                              const std::vector<uint8_t>& public_key,
                              SigningScheme scheme,
                              const std::vector<uint8_t>& data,
                              const std::string& extension_id,
                              SignCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Optional<TokenId> token_id = KeystoreToToken(keystore);
  if (!token_id) {
    std::move(callback).Run(
        mojom::KeystoreBinaryResult::NewErrorMessage(kUnsupportedKeystoreType));
    return;
  }

  ExtensionPlatformKeysService* service = GetExtensionPlatformKeys();
  chromeos::platform_keys::HashAlgorithm hash_algorithm;
  chromeos::platform_keys::KeyType key_type;
  switch (scheme) {
    case SigningScheme::kUnknown:
      std::move(callback).Run(mojom::KeystoreBinaryResult::NewErrorMessage(
          kUnsupportedAlgorithmType));
      return;
    case SigningScheme::kRsassaPkcs1V15None:
      service->SignRSAPKCS1Raw(
          token_id, std::string(data.begin(), data.end()),
          std::string(public_key.begin(), public_key.end()), extension_id,
          base::BindOnce(&KeystoreServiceAsh::OnDidSign, std::move(callback)));
      return;
    case SigningScheme::kRsassaPkcs1V15Sha1:
      key_type = chromeos::platform_keys::KeyType::kRsassaPkcs1V15;
      hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA1;
      break;
    case SigningScheme::kRsassaPkcs1V15Sha256:
      key_type = chromeos::platform_keys::KeyType::kRsassaPkcs1V15;
      hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA256;
      break;
    case SigningScheme::kRsassaPkcs1V15Sha384:
      key_type = chromeos::platform_keys::KeyType::kRsassaPkcs1V15;
      hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA384;
      break;
    case SigningScheme::kRsassaPkcs1V15Sha512:
      key_type = chromeos::platform_keys::KeyType::kRsassaPkcs1V15;
      hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA512;
      break;
    case SigningScheme::kEcdsaSha1:
      key_type = chromeos::platform_keys::KeyType::kEcdsa;
      hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA1;
      break;
    case SigningScheme::kEcdsaSha256:
      key_type = chromeos::platform_keys::KeyType::kEcdsa;
      hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA256;
      break;
    case SigningScheme::kEcdsaSha384:
      key_type = chromeos::platform_keys::KeyType::kEcdsa;
      hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA384;
      break;
    case SigningScheme::kEcdsaSha512:
      key_type = chromeos::platform_keys::KeyType::kEcdsa;
      hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA512;
      break;
  }

  service->SignDigest(
      token_id, std::string(data.begin(), data.end()),
      std::string(public_key.begin(), public_key.end()), key_type,
      hash_algorithm, extension_id,
      base::BindOnce(&KeystoreServiceAsh::OnDidSign, std::move(callback)));
}

// static
void KeystoreServiceAsh::OnGetTokens(
    GetKeyStoresCallback callback,
    std::unique_ptr<std::vector<TokenId>> platform_keys_token_ids,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  mojom::GetKeyStoresResultPtr result_ptr = mojom::GetKeyStoresResult::New();

  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::vector<mojom::KeystoreType> key_stores;
    for (auto token_id : *platform_keys_token_ids) {
      switch (token_id) {
        case TokenId::kUser:
          key_stores.push_back(mojom::KeystoreType::kUser);
          break;
        case TokenId::kSystem:
          key_stores.push_back(mojom::KeystoreType::kDevice);
          break;
      }
    }
    result_ptr->set_key_stores(std::move(key_stores));
  } else {
    result_ptr->set_error_message(
        chromeos::platform_keys::StatusToString(status));
  }

  std::move(callback).Run(std::move(result_ptr));
}

// static
void KeystoreServiceAsh::OnGetCertificates(
    GetCertificatesCallback callback,
    std::unique_ptr<net::CertificateList> certs,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  mojom::GetCertificatesResultPtr result_ptr =
      mojom::GetCertificatesResult::New();

  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::vector<std::vector<uint8_t>> output;
    for (scoped_refptr<net::X509Certificate> cert : *certs) {
      CRYPTO_BUFFER* der_buffer = cert->cert_buffer();
      const uint8_t* data = CRYPTO_BUFFER_data(der_buffer);
      std::vector<uint8_t> der_x509_certificate(
          data, data + CRYPTO_BUFFER_len(der_buffer));
      output.push_back(std::move(der_x509_certificate));
    }
    result_ptr->set_certificates(std::move(output));
  } else {
    result_ptr->set_error_message(
        chromeos::platform_keys::StatusToString(status));
  }

  std::move(callback).Run(std::move(result_ptr));
}

// static
void KeystoreServiceAsh::OnGenerateKey(GenerateKeyCallback callback,
                                       const std::string& public_key,
                                       chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  crosapi::mojom::KeystoreBinaryResultPtr result_ptr =
      mojom::KeystoreBinaryResult::New();
  if (status == chromeos::platform_keys::Status::kSuccess) {
    result_ptr->set_blob(
        std::vector<uint8_t>(public_key.begin(), public_key.end()));
  } else {
    result_ptr->set_error_message(
        chromeos::platform_keys::StatusToString(status));
  }
  std::move(callback).Run(std::move(result_ptr));
}

void KeystoreServiceAsh::OnImportCertificate(
    AddCertificateCallback callback,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status == chromeos::platform_keys::Status::kSuccess)
    std::move(callback).Run(/*error=*/"");
  else
    std::move(callback).Run(chromeos::platform_keys::StatusToString(status));
}

void KeystoreServiceAsh::OnRemoveCertificate(
    RemoveCertificateCallback callback,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status == chromeos::platform_keys::Status::kSuccess)
    std::move(callback).Run(/*error=*/"");
  else
    std::move(callback).Run(chromeos::platform_keys::StatusToString(status));
}

// static
void KeystoreServiceAsh::OnDidSign(SignCallback callback,
                                   const std::string& signature,
                                   chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (status == chromeos::platform_keys::Status::kSuccess) {
    std::move(callback).Run(mojom::KeystoreBinaryResult::NewBlob(
        std::vector<uint8_t>(signature.begin(), signature.end())));
  } else {
    std::move(callback).Run(mojom::KeystoreBinaryResult::NewErrorMessage(
        chromeos::platform_keys::StatusToString(status)));
  }
}

void KeystoreServiceAsh::DidChallengeAttestationOnlyKeystore(
    ChallengeAttestationOnlyKeystoreCallback callback,
    void* challenge_key_ptr,
    const ash::attestation::TpmChallengeKeyResult& result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  crosapi::mojom::KeystoreStringResultPtr result_ptr =
      mojom::KeystoreStringResult::New();
  if (result.IsSuccess()) {
    result_ptr->set_challenge_response(result.challenge_response);
  } else {
    result_ptr->set_error_message(result.GetErrorMessage());
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

}  // namespace crosapi
