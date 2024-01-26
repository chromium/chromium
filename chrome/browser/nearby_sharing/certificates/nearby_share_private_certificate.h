// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_PRIVATE_CERTIFICATE_H_
#define CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_PRIVATE_CERTIFICATE_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "third_party/nearby/sharing/proto/encrypted_metadata.pb.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

namespace crypto {
class ECPrivateKey;
class SymmetricKey;
}  // namespace crypto

// Stores metadata and crypto keys for the local device. This certificate
// can be converted to a public certificate and sent to select contacts, who
// will then use the certificate for authenticating the local device before
// transferring data. Provides method for signing a payload during
// authentication with a remote device. Provides method for encrypting the
// metadata encryption key, which can then be advertised.
class NearbySharePrivateCertificate {
 public:
  // Inverse operation of ToDictionary(). Returns std::nullopt if the
  // conversion is not successful
  static std::optional<NearbySharePrivateCertificate> FromDictionary(
      const base::Value::Dict& dict);

  // Generates a random EC key pair, secret key, and metadata encryption
  // key. Derives the certificate ID from the secret key. Derives the
  // not-after time from |not_before| and the certificate validity period fixed
  // by the Nearby Share protocol. Visibility cannot be "no one".
  NearbySharePrivateCertificate(
      nearby_share::mojom::Visibility visibility,
      base::Time not_before,
      nearby::sharing::proto::EncryptedMetadata unencrypted_metadata);

  NearbySharePrivateCertificate(
      nearby_share::mojom::Visibility visibility,
      base::Time not_before,
      base::Time not_after,
      std::unique_ptr<crypto::ECPrivateKey> key_pair,
      std::unique_ptr<crypto::SymmetricKey> secret_key,
      std::vector<uint8_t> metadata_encryption_key,
      std::vector<uint8_t> id,
      nearby::sharing::proto::EncryptedMetadata unencrypted_metadata,
      std::set<std::vector<uint8_t>> consumed_salts);

  NearbySharePrivateCertificate(const NearbySharePrivateCertificate& other);
  NearbySharePrivateCertificate& operator=(
      const NearbySharePrivateCertificate& other);
  NearbySharePrivateCertificate(NearbySharePrivateCertificate&& other);
  NearbySharePrivateCertificate& operator=(
      NearbySharePrivateCertificate&& other);

  virtual ~NearbySharePrivateCertificate();

  const std::vector<uint8_t>& id() const { return id_; }
  nearby_share::mojom::Visibility visibility() const { return visibility_; }
  base::Time not_before() const { return not_before_; }
  base::Time not_after() const { return not_after_; }
  const nearby::sharing::proto::EncryptedMetadata& unencrypted_metadata()
      const {
    return unencrypted_metadata_;
  }

  // Encrypts |metadata_encryption_key_| with the |secret_key_|, using a
  // randomly generated 2-byte salt that has not already been consumed. Returns
  // std::nullopt if the encryption failed or if there are no remaining salts.
  // Note: Due to the generation and storage of an unconsumed salt, this method
  // is not thread safe.
  std::optional<NearbyShareEncryptedMetadataKey> EncryptMetadataKey();

  // Signs the input |payload| with the private key from |key_pair_|. Returns
  // std::nullopt if the signing was unsuccessful.
  std::optional<std::vector<uint8_t>> Sign(
      base::span<const uint8_t> payload) const;

  // Creates a hash of the |authentication_token|, using |secret_key_|. The use
  // of HKDF and the output vector size is part of the Nearby Share protocol and
  // conforms with the GmsCore implementation.
  std::vector<uint8_t> HashAuthenticationToken(
      base::span<const uint8_t> authentication_token) const;

  // Converts this private certificate to a public certificate proto that can be
  // shared with select contacts. Returns std::nullopt if the conversion was
  // unsuccessful.
  std::optional<nearby::sharing::proto::PublicCertificate> ToPublicCertificate()
      const;

  // Converts this private certificate to a dictionary value for storage
  // in Prefs.
  base::Value::Dict ToDictionary() const;

  // For testing only.
  base::queue<std::vector<uint8_t>>& next_salts_for_testing() {
    return next_salts_for_testing_;
  }
  std::optional<base::TimeDelta>& offset_for_testing() {
    return offset_for_testing_;
  }

 private:
  // Generates a random 2-byte salt used for encrypting the metadata encryption
  // key. Adds returned salt to |consumed_salts_|. Returns std::nullopt if the
  // maximum number of salts have been exhausted or if an unconsumed salt cannot
  // be found in a fixed number of attempts, though this is highly improbably.
  // Note: This function is not thread safe.
  std::optional<std::vector<uint8_t>> GenerateUnusedSalt();

  // Encrypts |unencrypted_metadata_| with the |metadata_encryption_key_|, using
  // the |secret_key_| as salt.
  std::optional<std::vector<uint8_t>> EncryptMetadata() const;

  // Specifies which contacts can receive the public certificate corresponding
  // to this private certificate.
  nearby_share::mojom::Visibility visibility_;

  // The begin/end times of the certificate's validity period. Note: An offset
  // is not yet applied to these values. To avoid issues with clock skew,
  // offsets should be applied during conversion to a public certificate.
  base::Time not_before_;
  base::Time not_after_;

  // The public/private P-256 key pair used for verification/signing to ensure
  // secret of public certificates. The public key is included in the
  // public certificate, but the private key will never leave the device.
  std::unique_ptr<crypto::ECPrivateKey> key_pair_;

  // A 32-byte AES key used, along with a salt, to encrypt the
  // |metadata_encryption_key_|, after which it can be safely advertised.  Also,
  // used to generate an authentication token hash. Included in the public
  // certificate.
  std::unique_ptr<crypto::SymmetricKey> secret_key_;

  // A 14-byte symmetric key used to encrypt |unencrypted_metadata_|. Not
  // included in public certificate.
  std::vector<uint8_t> metadata_encryption_key_;

  // An ID for the certificate, generated from the secret key.
  std::vector<uint8_t> id_;

  // Unencrypted device metadata. The proto name is misleading; it holds data
  // that will eventually be serialized and encrypted.
  nearby::sharing::proto::EncryptedMetadata unencrypted_metadata_;

  // The set of 2-byte salts already used to encrypt the metadata key.
  std::set<std::vector<uint8_t>> consumed_salts_;

  // For testing only.
  base::queue<std::vector<uint8_t>> next_salts_for_testing_;
  std::optional<base::TimeDelta> offset_for_testing_;

  FRIEND_TEST_ALL_PREFIXES(NearbySharePrivateCertificateTest, ToFromDictionary);
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_PRIVATE_CERTIFICATE_H_
