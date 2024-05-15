// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/platform_keys.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromeos/crosapi/cpp/keystore_service_util.h"
#include "chromeos/crosapi/mojom/keystore_error.mojom.h"
#include "crypto/openssl_util.h"
#include "net/base/hash_value.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

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

namespace chromeos::platform_keys {

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
  NOTREACHED_IN_MIGRATION();
}

Status StatusFromKeystoreError(crosapi::mojom::KeystoreError error) {
  using crosapi::mojom::KeystoreError;

  switch (error) {
    case KeystoreError::kUnknown:
    case KeystoreError::kUnsupportedKeystoreType:
    case KeystoreError::kUnsupportedAlgorithmType:
    case KeystoreError::kUnsupportedKeyTag:
    case KeystoreError::kMojoUnavailable:
    case KeystoreError::kUnsupportedKeyType:
      // Keystore specific errors shouldn't be passed here.
      NOTREACHED_IN_MIGRATION();
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

  NOTREACHED_IN_MIGRATION();
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
    case KeystoreError::kMojoUnavailable:
      return "The OS is too old.";
    case KeystoreError::kUnsupportedKeyType:
      return "Key type is not supported.";
    default:
      break;
  }
  // Handle platform_keys errors.
  return StatusToString(StatusFromKeystoreError(error));
}

std::string GetSubjectPublicKeyInfo(
    const scoped_refptr<net::X509Certificate>& certificate) {
  std::string_view spki_bytes;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(certificate->cert_buffer()),
          &spki_bytes))
    return {};
  return std::string(spki_bytes);
}

std::vector<uint8_t> GetSubjectPublicKeyInfoBlob(
    const scoped_refptr<net::X509Certificate>& certificate) {
  std::string_view spki_bytes;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(certificate->cert_buffer()),
          &spki_bytes))
    return {};
  return std::vector<uint8_t>(spki_bytes.begin(), spki_bytes.end());
}

// Extracts the public exponent out of an EVP_PKEY and verifies if it is equal
// to 65537 (Fermat number with n=4). This values is enforced by
// platform_keys::GetPublicKey() and platform_keys::GetPublicKeyBySpki().
// The caller of this function needs to have an OpenSSLErrStackTracer or
// otherwise clean up the error stack on failure.
bool VerifyRSAPublicExponent(EVP_PKEY* pkey) {
  RSA* rsa = EVP_PKEY_get0_RSA(pkey);
  if (!rsa) {
    LOG(WARNING) << "Could not get RSA from PKEY.";
    return false;
  }

  const BIGNUM* public_exponent = nullptr;
  RSA_get0_key(rsa, nullptr /* out_n */, &public_exponent, nullptr /* out_d */);
  if (BN_get_word(public_exponent) != 65537L) {
    LOG(ERROR) << "Rejecting RSA public exponent that is unequal 65537.";
    return false;
  }

  return true;
}

bool GetPublicKey(const scoped_refptr<net::X509Certificate>& certificate,
                  net::X509Certificate::PublicKeyType* key_type,
                  size_t* key_size_bits) {
  net::X509Certificate::PublicKeyType key_type_tmp =
      net::X509Certificate::kPublicKeyTypeUnknown;
  size_t key_size_bits_tmp = 0;
  net::X509Certificate::GetPublicKeyInfo(certificate->cert_buffer(),
                                         &key_size_bits_tmp, &key_type_tmp);

  if (key_type_tmp == net::X509Certificate::kPublicKeyTypeUnknown) {
    LOG(WARNING) << "Could not extract public key of certificate.";
    return false;
  }
  if (key_type_tmp != net::X509Certificate::kPublicKeyTypeRSA &&
      key_type_tmp != net::X509Certificate::kPublicKeyTypeECDSA) {
    LOG(WARNING) << "Keys of other types than RSA and EC are not supported.";
    return false;
  }

  std::string spki = GetSubjectPublicKeyInfo(certificate);
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(spki.data()), spki.size());
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_public_key(&cbs));
  if (!pkey) {
    LOG(WARNING) << "Could not extract public key of certificate.";
    return false;
  }

  switch (EVP_PKEY_id(pkey.get())) {
    case EVP_PKEY_RSA: {
      if (!VerifyRSAPublicExponent(pkey.get())) {
        return false;
      }
      break;
    }
    case EVP_PKEY_EC: {
      EC_KEY* ec = EVP_PKEY_get0_EC_KEY(pkey.get());
      if (!ec) {
        LOG(WARNING) << "Could not get EC from PKEY.";
        return false;
      }

      if (EC_GROUP_get_curve_name(EC_KEY_get0_group(ec)) !=
          NID_X9_62_prime256v1) {
        LOG(WARNING) << "Only P-256 named curve is supported.";
        return false;
      }
      break;
    }
    default: {
      LOG(WARNING) << "Only RSA and EC keys are supported.";
      return false;
    }
  }

  *key_type = key_type_tmp;
  *key_size_bits = key_size_bits_tmp;
  return true;
}

bool GetPublicKeyBySpki(const std::string& spki,
                        net::X509Certificate::PublicKeyType* key_type,
                        size_t* key_size_bits) {
  net::X509Certificate::PublicKeyType key_type_tmp =
      net::X509Certificate::kPublicKeyTypeUnknown;

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(spki.data()), spki.size());
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_public_key(&cbs));
  if (!pkey) {
    LOG(WARNING) << "Could not extract public key from SPKI.";
    return false;
  }
  switch (EVP_PKEY_id(pkey.get())) {
    case EVP_PKEY_RSA: {
      if (!VerifyRSAPublicExponent(pkey.get())) {
        return false;
      }
      key_type_tmp = net::X509Certificate::kPublicKeyTypeRSA;
      break;
    }
    case EVP_PKEY_EC: {
      EC_KEY* ec = EVP_PKEY_get0_EC_KEY(pkey.get());
      if (!ec) {
        LOG(WARNING) << "Could not get EC from PKEY.";
        return false;
      }
      if (EC_GROUP_get_curve_name(EC_KEY_get0_group(ec)) !=
          NID_X9_62_prime256v1) {
        LOG(WARNING) << "Only P-256 named curve is supported.";
        return false;
      }
      key_type_tmp = net::X509Certificate::kPublicKeyTypeECDSA;
      break;
    }
    default: {
      LOG(WARNING) << "Only RSA and EC keys are supported.";
      return false;
    }
  }

  *key_type = key_type_tmp;
  *key_size_bits = base::saturated_cast<size_t>(EVP_PKEY_bits(pkey.get()));
  return true;
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
          possibly_invalid_cert_der, options);
  if (!cert_x509) {
    output.status = Status::kErrorCertificateInvalid;
    return output;
  }

  PublicKeyInfo key_info;
  key_info.public_key_spki_der =
      chromeos::platform_keys::GetSubjectPublicKeyInfo(cert_x509);
  if (!chromeos::platform_keys::GetPublicKey(cert_x509, &key_info.key_type,
                                             &key_info.key_size_bits)) {
    output.status = Status::kErrorAlgorithmNotSupported;
    return output;
  }

  chromeos::platform_keys::Status check_result =
      chromeos::platform_keys::CheckKeyTypeAndAlgorithm(key_info.key_type,
                                                        algorithm_name);
  if (check_result != chromeos::platform_keys::Status::kSuccess) {
    output.status = check_result;
    return output;
  }

  std::optional<base::Value::Dict> algorithm =
      BuildWebCryptoAlgorithmDictionary(key_info);
  DCHECK(algorithm.has_value());
  output.algorithm = std::move(algorithm.value());

  output.public_key = std::vector<uint8_t>(key_info.public_key_spki_der.begin(),
                                           key_info.public_key_spki_der.end());
  output.status = Status::kSuccess;
  return output;
}

PublicKeyInfo::PublicKeyInfo() = default;
PublicKeyInfo::~PublicKeyInfo() = default;

Status CheckKeyTypeAndAlgorithm(net::X509Certificate::PublicKeyType key_type,
                                const std::string& algorithm_name) {
  if (key_type != net::X509Certificate::kPublicKeyTypeRSA &&
      key_type != net::X509Certificate::kPublicKeyTypeECDSA) {
    return Status::kErrorAlgorithmNotSupported;
  }

  if (algorithm_name != kWebCryptoRsassaPkcs1v15 &&
      algorithm_name != kWebCryptoEcdsa) {
    return Status::kErrorAlgorithmNotSupported;
  }

  if (key_type !=
      chromeos::platform_keys::GetKeyTypeForAlgorithm(algorithm_name)) {
    return Status::kErrorAlgorithmNotPermittedByCertificate;
  }

  return Status::kSuccess;
}

net::X509Certificate::PublicKeyType GetKeyTypeForAlgorithm(
    const std::string& algorithm_name) {
  // Currently, the only supported combinations are:
  // 1- A certificate declaring rsaEncryption in the SubjectPublicKeyInfo used
  // with the RSASSA-PKCS1-v1.5 algorithm.
  // 2- A certificate declaring id-ecPublicKey in the SubjectPublicKeyInfo used
  // with the ECDSA algorithm.
  if (algorithm_name == kWebCryptoRsassaPkcs1v15)
    return net::X509Certificate::kPublicKeyTypeRSA;
  if (algorithm_name == kWebCryptoEcdsa)
    return net::X509Certificate::kPublicKeyTypeECDSA;
  return net::X509Certificate::kPublicKeyTypeUnknown;
}

std::optional<base::Value::Dict> BuildWebCryptoAlgorithmDictionary(
    const PublicKeyInfo& key_info) {
  switch (key_info.key_type) {
    case net::X509Certificate::kPublicKeyTypeRSA: {
      base::Value::Dict result;
      BuildWebCryptoRSAAlgorithmDictionary(key_info, &result);
      return result;
    }
    case net::X509Certificate::kPublicKeyTypeECDSA: {
      base::Value::Dict result;
      BuildWebCryptoEcdsaAlgorithmDictionary(key_info, &result);
      return result;
    }
    default:
      return std::nullopt;
  }
}

void BuildWebCryptoRSAAlgorithmDictionary(const PublicKeyInfo& key_info,
                                          base::Value::Dict* algorithm) {
  CHECK_EQ(net::X509Certificate::kPublicKeyTypeRSA, key_info.key_type);
  algorithm->Set("name", kWebCryptoRsassaPkcs1v15);
  algorithm->Set("modulusLength", static_cast<int>(key_info.key_size_bits));

  // Equals 65537.
  static constexpr uint8_t kDefaultPublicExponent[] = {0x01, 0x00, 0x01};
  algorithm->Set("publicExponent",
                 base::Value::BlobStorage(std::begin(kDefaultPublicExponent),
                                          std::end(kDefaultPublicExponent)));
}

void BuildWebCryptoEcdsaAlgorithmDictionary(const PublicKeyInfo& key_info,
                                            base::Value::Dict* algorithm) {
  CHECK_EQ(net::X509Certificate::kPublicKeyTypeECDSA, key_info.key_type);
  algorithm->Set("name", kWebCryptoEcdsa);

  // Only P-256 named curve is supported.
  algorithm->Set("namedCurve", kWebCryptoNamedCurveP256);
}

ClientCertificateRequest::ClientCertificateRequest() = default;

ClientCertificateRequest::ClientCertificateRequest(
    const ClientCertificateRequest& other) = default;

ClientCertificateRequest::~ClientCertificateRequest() = default;

}  // namespace chromeos::platform_keys
