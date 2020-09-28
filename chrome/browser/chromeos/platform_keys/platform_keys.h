// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "net/cert/x509_certificate.h"

namespace chromeos {
namespace platform_keys {

// Supported key types.
enum class KeyType { kRsassaPkcs1V15, kEcdsa };

// Supported key attribute types.
enum class KeyAttributeType { kCertificateProvisioningId };

// Supported hash algorithms.
enum HashAlgorithm {
  HASH_ALGORITHM_NONE,  // The value if no hash function is selected.
  HASH_ALGORITHM_SHA1,
  HASH_ALGORITHM_SHA256,
  HASH_ALGORITHM_SHA384,
  HASH_ALGORITHM_SHA512
};

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
  kErrorCertificateNotFound,
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

// Returns the DER encoding of the X.509 Subject Public Key Info of the public
// key in |certificate|.
std::string GetSubjectPublicKeyInfo(
    const scoped_refptr<net::X509Certificate>& certificate);

// Intersects the two certificate lists |certs1| and |certs2| and passes the
// intersection to |callback|. The intersction preserves the order of |certs1|.
void IntersectCertificates(
    const net::CertificateList& certs1,
    const net::CertificateList& certs2,
    const base::Callback<void(std::unique_ptr<net::CertificateList>)>&
        callback);

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
  std::vector<std::string> certificate_authorities;
};

}  // namespace platform_keys
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_PLATFORM_KEYS_H_
