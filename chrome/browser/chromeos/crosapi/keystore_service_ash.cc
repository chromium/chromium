// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/keystore_service_ash.h"

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/attestation/tpm_challenge_key.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace crosapi {

using PlatformKeysService = chromeos::platform_keys::PlatformKeysService;
using TokenId = chromeos::platform_keys::TokenId;

namespace {

const char kEnterprisePlatformErrorInvalidX509Cert[] =
    "Certificate is not a valid X.509 certificate.";
const char kUnsupportedKeystoreType[] = "Keystore type is not supported.";
const char kUnsupportedAlgorithmType[] = "Algorithm type is not supported.";

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

}  // namespace

KeystoreServiceAsh::KeystoreServiceAsh(
    mojo::PendingReceiver<mojom::KeystoreService> receiver)
    : receiver_(this, std::move(receiver)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

KeystoreServiceAsh::~KeystoreServiceAsh() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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

  std::unique_ptr<chromeos::attestation::TpmChallengeKey> challenge_key =
      chromeos::attestation::TpmChallengeKeyFactory::Create();
  chromeos::attestation::TpmChallengeKey* challenge_key_ptr =
      challenge_key.get();
  outstanding_challenges_.push_back(std::move(challenge_key));
  //  TODO(https://crbug.com/1127505): Plumb |migrate| param.
  challenge_key_ptr->BuildResponse(
      key_type, profile,
      base::BindOnce(&KeystoreServiceAsh::DidChallengeAttestationOnlyKeystore,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     challenge_key_ptr),
      challenge,
      /*register_key=*/false, /*key_name_for_spkac=*/"");
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
    GenerateKeyCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PlatformKeysService* platform_keys_service = GetPlatformKeys();
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
      platform_keys_service->GenerateRSAKey(
          token_id.value(), algorithm->get_pkcs115()->modulus_length,
          std::move(c));
      break;
    }
    case mojom::KeystoreSigningAlgorithm::Tag::ECDSA: {
      auto c = base::BindOnce(&KeystoreServiceAsh::OnGenerateKey,
                              std::move(callback));
      platform_keys_service->GenerateECKey(
          token_id.value(), algorithm->get_ecdsa()->named_curve, std::move(c));
      break;
    }
    default: {
      std::move(callback).Run(mojom::KeystoreBinaryResult::NewErrorMessage(
          kUnsupportedAlgorithmType));
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

void KeystoreServiceAsh::DidChallengeAttestationOnlyKeystore(
    ChallengeAttestationOnlyKeystoreCallback callback,
    void* challenge_key_ptr,
    const chromeos::attestation::TpmChallengeKeyResult& result) {
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
