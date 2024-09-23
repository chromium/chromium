// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/keystore_error.mojom.h"
#include "net/cert/x509_certificate.h"

namespace chromeos::platform_keys {

// Supported key types.
enum class KeyType { kRsassaPkcs1V15, kEcdsa };

// Supported symmetric key types.
enum class SymKeyType { kAesCbc, kHmac, kSp800Kdf };

// Supported key attribute types.
enum class KeyAttributeType { kCertificateProvisioningId, kKeyPermissions };

// Supported hash algorithms.
enum HashAlgorithm {
  HASH_ALGORITHM_NONE,  // The value if no hash function is selected.
  HASH_ALGORITHM_SHA1,
  HASH_ALGORITHM_SHA256,
  HASH_ALGORITHM_SHA384,
  HASH_ALGORITHM_SHA512
};

enum class OperationType { kEncrypt, kDecrypt };

// Supported token IDs.
// A token is a store for keys or certs and can provide cryptographic
// operations.
// ChromeOS provides itself a user token and conditionally a system wide token.
enum class TokenId { kUser, kSystem };

// The service possible statuses.
// For every platform keys service operation callback, a status is passed
// signaling the success or failure of the operation.
enum class Status {
  kSuccess,
  kErrorAlgorithmNotSupported,
  kErrorAlgorithmNotPermittedByCertificate,
  kErrorCertificateNotFound,
  kErrorCertificateInvalid,
  kErrorInputTooLong,
  kErrorGrantKeyPermissionForExtension,
  kErrorInternal,
  kErrorKeyAttributeRetrievalFailed,
  kErrorKeyAttributeSettingFailed,
  kErrorKeyNotAllowedForSigning,
  kErrorKeyNotFound,
  kErrorShutDown,
  // kNetError* are for errors occurred during net::* operations.
  kNetErrorAddUserCertFailed,
  kNetErrorCertificateDateInvalid,
  kNetErrorCertificateInvalid,
};

// These strings can be used to be passed to extensions as well as for logging
// purposes.
// Note: Do not change already existing status-to-string translations, since
// extensions may hardcode specific messages.
std::string StatusToString(Status status);

// Convert platform_keys::Status into a KeystoreError. Status::kSuccess should
// not be passed in the function.
crosapi::mojom::KeystoreError StatusToKeystoreError(Status status);

// Creates platform_keys::Status into a KeystoreError. Keystore specific errors
// are not supported and should be processed separately.
Status StatusFromKeystoreError(crosapi::mojom::KeystoreError error);

// Converts KeystoreError code into an error message.
// Note: Do not change already existing error-to-string translations, since
// extensions may hardcode specific messages.
std::string KeystoreErrorToString(crosapi::mojom::KeystoreError error);

// Returns the DER encoding of the X.509 Subject Public Key Info of the public
// key in |certificate|.
std::string GetSubjectPublicKeyInfo(
    const scoped_refptr<net::X509Certificate>& certificate);
std::vector<uint8_t> GetSubjectPublicKeyInfoBlob(
    const scoped_refptr<net::X509Certificate>& certificate);

// Intersects the two certificate lists |certs1| and |certs2| and passes the
// intersection to |callback|. The intersection preserves the order of |certs1|.
void IntersectCertificates(
    const net::CertificateList& certs1,
    const net::CertificateList& certs2,
    base::OnceCallback<void(std::unique_ptr<net::CertificateList>)> callback);

// The output for GetPublicKeyAndAlgorithm.
struct GetPublicKeyAndAlgorithmOutput {
  GetPublicKeyAndAlgorithmOutput();
  GetPublicKeyAndAlgorithmOutput(GetPublicKeyAndAlgorithmOutput&&);
  ~GetPublicKeyAndAlgorithmOutput();

  Status status = Status::kSuccess;
  std::vector<uint8_t> public_key;  // Only set if status == kSuccess
  base::Value::Dict algorithm;      // Only set if status == kSuccess
};

// This is a convenient wrapper around GetPublicKey which also builds a
// WebCrypto algorithm dictionary and performs error checking.
GetPublicKeyAndAlgorithmOutput GetPublicKeyAndAlgorithm(
    const std::vector<uint8_t>& possibly_invalid_cert_der,
    const std::string& algorithm_name);

struct PublicKeyInfo {
  PublicKeyInfo();
  ~PublicKeyInfo();

  // The X.509 Subject Public Key Info of the key in DER encoding.
  std::string public_key_spki_der;

  // The type of the key.
  net::X509Certificate::PublicKeyType key_type =
      net::X509Certificate::kPublicKeyTypeUnknown;

  // The size of the key in bits.
  size_t key_size_bits = 0;
};

// Checks if the certificate key type and the algorithm are
//    - valid
//    - supported
//    - compatible
// Returns Status::kSuccess if they are, or the correct error reason if they
// are not.
Status CheckKeyTypeAndAlgorithm(net::X509Certificate::PublicKeyType key_type,
                                const std::string& algorithm_name);

// Returns the certificate key type that supports the given algorithm,
// or |kPublicKeyTypeUnknown| if the algorithm is unknown or unsupported.
net::X509Certificate::PublicKeyType GetKeyTypeForAlgorithm(
    const std::string& algorithm_name);

// Builds a partial WebCrypto Algorithm object from the parameters available in
// |key_info|. This supports both RSA and EC keys.
// Returns std::nullopt if the key is of an unsupported type (so not RSA or
// EC).
std::optional<base::Value::Dict> BuildWebCryptoAlgorithmDictionary(
    const PublicKeyInfo& key_info);

// Builds a partial WebCrypto Algorithm object from the parameters available in
// |key_info|, which must be the info of an RSA key. This doesn't include
// sign/hash parameters and thus isn't complete. platform_keys::GetPublicKey()
// enforced the public exponent 65537.
void BuildWebCryptoRSAAlgorithmDictionary(const PublicKeyInfo& key_info,
                                          base::Value::Dict* algorithm);

// Builds a partial WebCrypto Algorithm object from the parameters available in
// |key_info|, which must be the info of an EC key. For more information about
// EcKeyAlgorithm dictionary, please refer to:
// https://www.w3.org/TR/WebCryptoAPI/#EcKeyAlgorithm-dictionary
void BuildWebCryptoEcdsaAlgorithmDictionary(const PublicKeyInfo& key_info,
                                            base::Value::Dict* algorithm);

// Obtains information about the public key in |certificate|.
// If |certificate| contains an RSA key, sets |key_size_bits| to the modulus
// length, and |key_type| to type RSA and returns true.
// If |certificate| contains any other key type, or if the public exponent of
// the RSA key in |certificate| is not F4, returns false and does not update any
// of the output parameters.
// All pointer arguments must not be null.
bool GetPublicKey(const scoped_refptr<net::X509Certificate>& certificate,
                  net::X509Certificate::PublicKeyType* key_type,
                  size_t* key_size_bits);

// Obtains information about the public key in |spki|.
// If |spki| is an RSA key, sets |key_size_bits| to the modulus
// length, and |key_type| to type RSA and returns true.
// If |spki| is any other key type, returns false and does not update any
// of the output parameters.
// All pointer arguments must not be null.
bool GetPublicKeyBySpki(const std::string& spki,
                        net::X509Certificate::PublicKeyType* key_type,
                        size_t* key_size_bits);

struct ClientCertificateRequest {
  ClientCertificateRequest();
  ClientCertificateRequest(const ClientCertificateRequest& other);
  ~ClientCertificateRequest();

  // The list of the types of certificates requested, sorted in order of the
  // server's preference.
  std::vector<net::X509Certificate::PublicKeyType> certificate_key_types;

  // List of distinguished names of certificate authorities allowed by the
  // server. Each entry must be a DER-encoded X.509 DistinguishedName.
  std::vector<std::vector<uint8_t>> certificate_authorities;
};

}  // namespace chromeos::platform_keys

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_H_
