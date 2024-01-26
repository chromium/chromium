// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_DECRYPTED_PUBLIC_CERTIFICATE_H_
#define CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_DECRYPTED_PUBLIC_CERTIFICATE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "crypto/symmetric_key.h"
#include "third_party/nearby/sharing/proto/encrypted_metadata.pb.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

// Stores decrypted metadata and crypto keys for the remote device that uploaded
// this certificate to the Nearby Share server. Use DecryptPublicCertificate()
// to generate an instance. This class provides a method for verifying a signed
// payload during the authentication flow.
class NearbyShareDecryptedPublicCertificate {
 public:
  // Attempts to decrypt the encrypted metadata of the PublicCertificate proto
  // by first decrypting the |encrypted_metadata_key| using the secret key
  // then using the decrypted key to decrypt the metadata. Returns std::nullopt
  // if the metadata was not successfully decrypted or if the proto data is
  // invalid.
  static std::optional<NearbyShareDecryptedPublicCertificate>
  DecryptPublicCertificate(
      const nearby::sharing::proto::PublicCertificate& public_certificate,
      const NearbyShareEncryptedMetadataKey& encrypted_metadata_key);

  NearbyShareDecryptedPublicCertificate(
      const NearbyShareDecryptedPublicCertificate& other);
  NearbyShareDecryptedPublicCertificate& operator=(
      const NearbyShareDecryptedPublicCertificate& other);
  NearbyShareDecryptedPublicCertificate(
      NearbyShareDecryptedPublicCertificate&&);
  NearbyShareDecryptedPublicCertificate& operator=(
      NearbyShareDecryptedPublicCertificate&&);

  virtual ~NearbyShareDecryptedPublicCertificate();

  const std::vector<uint8_t>& id() const { return id_; }
  base::Time not_before() const { return not_before_; }
  base::Time not_after() const { return not_after_; }
  const nearby::sharing::proto::EncryptedMetadata& unencrypted_metadata()
      const {
    return unencrypted_metadata_;
  }
  bool for_self_share() const { return for_self_share_; }

  // Verifies the |signature| of the signed |payload| using |public_key_|.
  // Returns true if verification was successful.
  bool VerifySignature(base::span<const uint8_t> payload,
                       base::span<const uint8_t> signature) const;

  // Creates a hash of the |authentication_token|, using |secret_key_|. The use
  // of HKDF and the output vector size is part of the Nearby Share protocol and
  // conforms with the GmsCore implementation.
  std::vector<uint8_t> HashAuthenticationToken(
      base::span<const uint8_t> authentication_token) const;

 private:
  NearbyShareDecryptedPublicCertificate(
      base::Time not_before,
      base::Time not_after,
      std::unique_ptr<crypto::SymmetricKey> secret_key,
      std::vector<uint8_t> public_key,
      std::vector<uint8_t> id,
      nearby::sharing::proto::EncryptedMetadata unencrypted_metadata,
      bool for_self_share);

  // The begin/end times of the certificate's validity period. To avoid issues
  // with clock skew, these time might be offset compared to the corresponding
  // private certificate.
  base::Time not_before_;
  base::Time not_after_;

  // A 32-byte AES key that was used for metadata key and metadata decryption.
  // Also, used to generate an authentication token hash.
  std::unique_ptr<crypto::SymmetricKey> secret_key_;

  // A P-256 public key used for verification. The bytes comprise a DER-encoded
  // ASN.1 SubjectPublicKeyInfo from the X.509 specification (RFC 5280).
  std::vector<uint8_t> public_key_;

  // An ID for the certificate, most likely generated from the secret key.
  std::vector<uint8_t> id_;

  // Unencrypted device metadata. The proto name is misleading; it holds data
  // that was previously serialized and encrypted.
  nearby::sharing::proto::EncryptedMetadata unencrypted_metadata_;

  // Indicates if this public certificate is from another device owned by the
  // same user.
  bool for_self_share_ = false;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_DECRYPTED_PUBLIC_CERTIFICATE_H_
