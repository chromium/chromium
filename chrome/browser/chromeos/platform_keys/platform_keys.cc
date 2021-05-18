// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/platform_keys.h"

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromeos/crosapi/cpp/keystore_service_util.h"
#include "chromeos/crosapi/mojom/keystore_error.mojom.h"
#include "net/base/hash_value.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"

namespace {

using crosapi::keystore_service_util::kWebCryptoEcdsa;
using crosapi::keystore_service_util::kWebCryptoNamedCurveP256;
using crosapi::keystore_service_util::kWebCryptoRsassaPkcs1v15;

void IntersectOnWorkerThread(const net::CertificateList& certs1,
                             const net::CertificateList& certs2,
                             net::CertificateList* intersection) {
  std::map<net::SHA256HashValue, scoped_refptr<net::X509Certificate>>
      fingerprints2;

  // Fill the map with fingerprints of certs from |certs2|.
  for (const auto& cert2 : certs2) {
    fingerprints2[net::X509Certificate::CalculateFingerprint256(
        cert2->cert_buffer())] = cert2;
  }

  // Compare each cert from |certs1| with the entries of the map.
  for (const auto& cert1 : certs1) {
    const net::SHA256HashValue fingerprint1 =
        net::X509Certificate::CalculateFingerprint256(cert1->cert_buffer());
    const auto it = fingerprints2.find(fingerprint1);
    if (it == fingerprints2.end())
      continue;
    const auto& cert2 = it->second;
    DCHECK(cert1->EqualsExcludingChain(cert2.get()));
    intersection->push_back(cert1);
  }
}

}  // namespace

namespace chromeos {
namespace platform_keys {

std::string StatusToString(Status status) {
  switch (status) {
    case Status::kSuccess:
      return "The operation was successfully executed.";
    case Status::kErrorAlgorithmNotSupported:
      return "Algorithm not supported.";
    case Status::kErrorAlgorithmNotPermittedByCertificate:
      return "The requested Algorithm is not permitted by the certificate.";
    case Status::kErrorCertificateNotFound:
      return "Certificate could not be found.";
    case Status::kErrorCertificateInvalid:
      return "Certificate is not a valid X.509 certificate.";
    case Status::kErrorInputTooLong:
      return "Input too long.";
    case Status::kErrorGrantKeyPermissionForExtension:
      return "Tried to grant permission for a key although prohibited (either "
             "key is a corporate key or this account is managed).";
    case Status::kErrorInternal:
      return "Internal Error.";
    case Status::kErrorKeyAttributeRetrievalFailed:
      return "Key attribute value retrieval failed.";
    case Status::kErrorKeyAttributeSettingFailed:
      return "Setting key attribute value failed.";
    case Status::kErrorKeyNotAllowedForSigning:
      return "This key is not allowed for signing. Either it was used for "
             "signing before or it was not correctly generated.";
    case Status::kErrorKeyNotFound:
      return "Key not found.";
    case Status::kErrorShutDown:
      return "Delegate shut down.";
    case Status::kNetErrorAddUserCertFailed:
      return net::ErrorToString(net::ERR_ADD_USER_CERT_FAILED);
    case Status::kNetErrorCertificateDateInvalid:
      return net::ErrorToString(net::ERR_CERT_DATE_INVALID);
    case Status::kNetErrorCertificateInvalid:
      return net::ErrorToString(net::ERR_CERT_INVALID);
  }
}

crosapi::mojom::KeystoreError StatusToKeystoreError(Status status) {
  DCHECK(status != Status::kSuccess);
  using crosapi::mojom::KeystoreError;

  switch (status) {
    case Status::kSuccess:
      return KeystoreError::kUnknown;
    case Status::kErrorAlgorithmNotSupported:
      return KeystoreError::kAlgorithmNotSupported;
    case Status::kErrorAlgorithmNotPermittedByCertificate:
      return KeystoreError::kAlgorithmNotPermittedByCertificate;
    case Status::kErrorCertificateNotFound:
      return KeystoreError::kCertificateNotFound;
    case Status::kErrorCertificateInvalid:
      return KeystoreError::kCertificateInvalid;
    case Status::kErrorInputTooLong:
      return KeystoreError::kInputTooLong;
    case Status::kErrorGrantKeyPermissionForExtension:
      return KeystoreError::kGrantKeyPermissionForExtension;
    case Status::kErrorInternal:
      return KeystoreError::kInternal;
    case Status::kErrorKeyAttributeRetrievalFailed:
      return KeystoreError::kKeyAttributeRetrievalFailed;
    case Status::kErrorKeyAttributeSettingFailed:
      return KeystoreError::kKeyAttributeSettingFailed;
    case Status::kErrorKeyNotAllowedForSigning:
      return KeystoreError::kKeyNotAllowedForSigning;
    case Status::kErrorKeyNotFound:
      return KeystoreError::kKeyNotFound;
    case Status::kErrorShutDown:
      return KeystoreError::kShutDown;
    case Status::kNetErrorAddUserCertFailed:
      return KeystoreError::kNetAddUserCertFailed;
    case Status::kNetErrorCertificateDateInvalid:
      return KeystoreError::kNetCertificateDateInvalid;
    case Status::kNetErrorCertificateInvalid:
      return KeystoreError::kNetCertificateInvalid;
  }
  NOTREACHED();
}

Status StatusFromKeystoreError(crosapi::mojom::KeystoreError error) {
  using crosapi::mojom::KeystoreError;

  switch (error) {
      // Keystore specific errors shouldn't be passed here.
    case KeystoreError::kUnknown:
    case KeystoreError::kUnsupportedKeystoreType:
    case KeystoreError::kUnsupportedAlgorithmType:
      DCHECK(false);
      return Status::kErrorInternal;

    case KeystoreError::kAlgorithmNotSupported:
      return Status::kErrorAlgorithmNotSupported;
    case KeystoreError::kAlgorithmNotPermittedByCertificate:
      return Status::kErrorAlgorithmNotPermittedByCertificate;
    case KeystoreError::kCertificateNotFound:
      return Status::kErrorCertificateNotFound;
    case KeystoreError::kCertificateInvalid:
      return Status::kErrorCertificateInvalid;
    case KeystoreError::kInputTooLong:
      return Status::kErrorInputTooLong;
    case KeystoreError::kGrantKeyPermissionForExtension:
      return Status::kErrorGrantKeyPermissionForExtension;
    case KeystoreError::kInternal:
      return Status::kErrorInternal;
    case KeystoreError::kKeyAttributeRetrievalFailed:
      return Status::kErrorKeyAttributeRetrievalFailed;
    case KeystoreError::kKeyAttributeSettingFailed:
      return Status::kErrorKeyAttributeSettingFailed;
    case KeystoreError::kKeyNotAllowedForSigning:
      return Status::kErrorKeyNotAllowedForSigning;
    case KeystoreError::kKeyNotFound:
      return Status::kErrorKeyNotFound;
    case KeystoreError::kShutDown:
      return Status::kErrorShutDown;
    case KeystoreError::kNetAddUserCertFailed:
      return Status::kNetErrorAddUserCertFailed;
    case KeystoreError::kNetCertificateDateInvalid:
      return Status::kNetErrorCertificateDateInvalid;
    case KeystoreError::kNetCertificateInvalid:
      return Status::kNetErrorCertificateInvalid;
  }

  NOTREACHED();
}

std::string KeystoreErrorToString(crosapi::mojom::KeystoreError error) {
  using crosapi::mojom::KeystoreError;

  // Handle Keystore specific errors.
  switch (error) {
    case KeystoreError::kUnknown:
      return "Unknown keystore error.";
    case KeystoreError::kUnsupportedKeystoreType:
      return "The token is not valid.";
    case KeystoreError::kUnsupportedAlgorithmType:
      return "Algorithm type is not supported.";
    default:
      break;
  }
  // Handle platform_keys errors.
  return StatusToString(StatusFromKeystoreError(error));
}

void IntersectCertificates(
    const net::CertificateList& certs1,
    const net::CertificateList& certs2,
    base::OnceCallback<void(std::unique_ptr<net::CertificateList>)> callback) {
  std::unique_ptr<net::CertificateList> intersection(new net::CertificateList);
  net::CertificateList* const intersection_ptr = intersection.get();

  // This is triggered by a call to the
  // chrome.platformKeys.selectClientCertificates extensions API. Completion
  // does not affect browser responsiveness, hence the BEST_EFFORT priority.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&IntersectOnWorkerThread, certs1, certs2,
                     intersection_ptr),
      base::BindOnce(std::move(callback), std::move(intersection)));
}

GetPublicKeyAndAlgorithmOutput::GetPublicKeyAndAlgorithmOutput() = default;
GetPublicKeyAndAlgorithmOutput::GetPublicKeyAndAlgorithmOutput(
    GetPublicKeyAndAlgorithmOutput&&) = default;
GetPublicKeyAndAlgorithmOutput::~GetPublicKeyAndAlgorithmOutput() = default;

GetPublicKeyAndAlgorithmOutput GetPublicKeyAndAlgorithm(
    const std::vector<uint8_t>& possibly_invalid_cert_der,
    const std::string& algorithm_name) {
  GetPublicKeyAndAlgorithmOutput output;

  if (possibly_invalid_cert_der.empty()) {
    output.status = Status::kErrorCertificateInvalid;
    return output;
  }

  // Allow UTF-8 inside PrintableStrings in client certificates. See
  // crbug.com/770323 and crbug.com/788655.
  net::X509Certificate::UnsafeCreateOptions options;
  options.printable_string_is_utf8 = true;
  scoped_refptr<net::X509Certificate> cert_x509 =
      net::X509Certificate::CreateFromBytesUnsafeOptions(
          reinterpret_cast<const char*>(possibly_invalid_cert_der.data()),
          possibly_invalid_cert_der.size(), options);
  if (!cert_x509) {
    output.status = Status::kErrorCertificateInvalid;
    return output;
  }

  PublicKeyInfo key_info;
  key_info.public_key_spki_der =
      chromeos::platform_keys::GetSubjectPublicKeyInfo(cert_x509);
  if (!chromeos::platform_keys::GetPublicKey(cert_x509, &key_info.key_type,
                                             &key_info.key_size_bits) ||
      (key_info.key_type != net::X509Certificate::kPublicKeyTypeRSA &&
       key_info.key_type != net::X509Certificate::kPublicKeyTypeECDSA)) {
    output.status = Status::kErrorAlgorithmNotSupported;
    return output;
  }

  // Currently, the only supported combinations are:
  // 1- A certificate declaring rsaEncryption in the SubjectPublicKeyInfo used
  // with the RSASSA-PKCS1-v1.5 algorithm.
  // 2- A certificate declaring id-ecPublicKey in the SubjectPublicKeyInfo used
  // with the ECDSA algorithm.
  if (algorithm_name == kWebCryptoRsassaPkcs1v15) {
    if (key_info.key_type != net::X509Certificate::kPublicKeyTypeRSA) {
      output.status = Status::kErrorAlgorithmNotPermittedByCertificate;
      return output;
    }

    BuildWebCryptoRSAAlgorithmDictionary(key_info, &output.algorithm);
    output.public_key =
        std::vector<uint8_t>(key_info.public_key_spki_der.begin(),
                             key_info.public_key_spki_der.end());
    output.status = Status::kSuccess;
    return output;
  }

  if (algorithm_name == kWebCryptoEcdsa) {
    if (key_info.key_type != net::X509Certificate::kPublicKeyTypeECDSA) {
      output.status = Status::kErrorAlgorithmNotPermittedByCertificate;
      return output;
    }

    BuildWebCryptoEcdsaAlgorithmDictionary(key_info, &output.algorithm);
    output.public_key =
        std::vector<uint8_t>(key_info.public_key_spki_der.begin(),
                             key_info.public_key_spki_der.end());
    output.status = Status::kSuccess;
    return output;
  }

  output.status = Status::kErrorAlgorithmNotPermittedByCertificate;
  return output;
}

PublicKeyInfo::PublicKeyInfo() = default;
PublicKeyInfo::~PublicKeyInfo() = default;

void BuildWebCryptoRSAAlgorithmDictionary(const PublicKeyInfo& key_info,
                                          base::DictionaryValue* algorithm) {
  CHECK_EQ(net::X509Certificate::kPublicKeyTypeRSA, key_info.key_type);
  algorithm->SetStringKey("name", kWebCryptoRsassaPkcs1v15);
  algorithm->SetKey("modulusLength",
                    base::Value(static_cast<int>(key_info.key_size_bits)));

  // Equals 65537.
  static constexpr uint8_t kDefaultPublicExponent[] = {0x01, 0x00, 0x01};
  algorithm->SetKey("publicExponent",
                    base::Value(base::make_span(kDefaultPublicExponent)));
}

void BuildWebCryptoEcdsaAlgorithmDictionary(const PublicKeyInfo& key_info,
                                            base::DictionaryValue* algorithm) {
  CHECK_EQ(net::X509Certificate::kPublicKeyTypeECDSA, key_info.key_type);
  algorithm->SetStringKey("name", kWebCryptoEcdsa);

  // Only P-256 named curve is supported.
  algorithm->SetStringKey("namedCurve", kWebCryptoNamedCurveP256);
}

ClientCertificateRequest::ClientCertificateRequest() = default;

ClientCertificateRequest::ClientCertificateRequest(
    const ClientCertificateRequest& other) = default;

ClientCertificateRequest::~ClientCertificateRequest() = default;

}  // namespace platform_keys
}  // namespace chromeos
