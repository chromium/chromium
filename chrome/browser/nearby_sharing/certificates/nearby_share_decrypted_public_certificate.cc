// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/nearby_share_decrypted_public_certificate.h"

#include <utility>

#include "chrome/browser/nearby_sharing/certificates/common.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "chromeos/ash/components/nearby/common/proto/timestamp.pb.h"
#include "components/cross_device/logging/logging.h"
#include "crypto/aead.h"
#include "crypto/encryptor.h"
#include "crypto/hmac.h"
#include "crypto/signature_verifier.h"

namespace {

bool IsDataValid(base::Time not_before,
                 base::Time not_after,
                 base::span<const uint8_t> public_key,
                 crypto::SymmetricKey* secret_key,
                 base::span<const uint8_t> id,
                 base::span<const uint8_t> encrypted_metadata,
                 base::span<const uint8_t> metadata_encryption_key_tag) {
  return not_before < not_after && !public_key.empty() && secret_key &&
         secret_key->key().size() == kNearbyShareNumBytesSecretKey &&
         id.size() == kNearbyShareNumBytesCertificateId &&
         !encrypted_metadata.empty() &&
         metadata_encryption_key_tag.size() ==
             kNearbyShareNumBytesMetadataEncryptionKeyTag;
}

// Attempts to decrypt |encrypted_metadata_key| using the |secret_key|.
// Return std::nullopt if the decryption was unsuccessful.
std::optional<std::vector<uint8_t>> DecryptMetadataKey(
    const NearbyShareEncryptedMetadataKey& encrypted_metadata_key,
    const crypto::SymmetricKey* secret_key) {
  std::unique_ptr<crypto::Encryptor> encryptor =
      CreateNearbyShareCtrEncryptor(secret_key, encrypted_metadata_key.salt());
  if (!encryptor) {
    CD_LOG(ERROR, Feature::NS)
        << "Cannot decrypt metadata key: Could not create CTR encryptor.";
    return std::nullopt;
  }

  std::vector<uint8_t> decrypted_metadata_key;
  if (!encryptor->Decrypt(base::as_bytes(base::make_span(
                              encrypted_metadata_key.encrypted_key())),
                          &decrypted_metadata_key)) {
    return std::nullopt;
  }

  return decrypted_metadata_key;
}

// Attempts to decrypt |encrypted_metadata| with |metadata_encryption_key|,
// using |authentication_key| as the IV. Returns std::nullopt if the decryption
// was unsuccessful.
std::optional<std::vector<uint8_t>> DecryptMetadataPayload(
    base::span<const uint8_t> encrypted_metadata,
    base::span<const uint8_t> metadata_encryption_key,
    const crypto::SymmetricKey* secret_key) {
  // Init() keeps a reference to the input key, so that reference must outlive
  // the lifetime of |aead|.
  std::vector<uint8_t> derived_key = DeriveNearbyShareKey(
      metadata_encryption_key, kNearbyShareNumBytesAesGcmKey);

  crypto::Aead aead(crypto::Aead::AeadAlgorithm::AES_256_GCM);
  aead.Init(derived_key);

  return aead.Open(
      encrypted_metadata,
      /*nonce=*/
      DeriveNearbyShareKey(base::as_bytes(base::make_span(secret_key->key())),
                           kNearbyShareNumBytesAesGcmIv),
      /*additional_data=*/base::span<const uint8_t>());
}

// Returns true if the HMAC of |decrypted_metadata_key| is
// |metadata_encryption_key_tag|.
bool VerifyMetadataEncryptionKeyTag(
    base::span<const uint8_t> decrypted_metadata_key,
    base::span<const uint8_t> metadata_encryption_key_tag) {
  // This array of 0x00 is used to conform with the GmsCore implementation.
  std::vector<uint8_t> key(kNearbyShareNumBytesMetadataEncryptionKeyTag, 0x00);

  std::vector<uint8_t> result(kNearbyShareNumBytesMetadataEncryptionKeyTag);
  crypto::HMAC hmac(crypto::HMAC::HashAlgorithm::SHA256);
  return hmac.Init(key) &&
         hmac.Verify(decrypted_metadata_key, metadata_encryption_key_tag);
}

}  // namespace

// static
std::optional<NearbyShareDecryptedPublicCertificate>
NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
    const nearby::sharing::proto::PublicCertificate& public_certificate,
    const NearbyShareEncryptedMetadataKey& encrypted_metadata_key) {
  // Note: The PublicCertificate.metadata_encryption_key and
  // PublicCertificate.for_selected_contacts are not returned from the server
  // for remote devices.
  base::Time not_before = base::Time::FromSecondsSinceUnixEpoch(
      public_certificate.start_time().seconds());
  base::Time not_after = base::Time::FromSecondsSinceUnixEpoch(
      public_certificate.end_time().seconds());
  std::vector<uint8_t> public_key(public_certificate.public_key().begin(),
                                  public_certificate.public_key().end());
  std::unique_ptr<crypto::SymmetricKey> secret_key =
      crypto::SymmetricKey::Import(crypto::SymmetricKey::Algorithm::AES,
                                   public_certificate.secret_key());
  std::vector<uint8_t> id(public_certificate.secret_id().begin(),
                          public_certificate.secret_id().end());
  std::vector<uint8_t> encrypted_metadata(
      public_certificate.encrypted_metadata_bytes().begin(),
      public_certificate.encrypted_metadata_bytes().end());
  std::vector<uint8_t> metadata_encryption_key_tag(
      public_certificate.metadata_encryption_key_tag().begin(),
      public_certificate.metadata_encryption_key_tag().end());

  if (!IsDataValid(not_before, not_after, public_key, secret_key.get(), id,
                   encrypted_metadata, metadata_encryption_key_tag)) {
    return std::nullopt;
  }

  // Note: Failure to decrypt the metadata key or failure to confirm that the
  // decrypted metadata key agrees with the key commitment tag should not log an
  // error. When another device advertises their encrypted metadata key, we do
  // not know what public certificate that corresponds to. So, we will
  // potentially be calling DecryptPublicCertificate() on all of our public
  // certificates with the same encrypted metadata key until we find the correct
  // one.
  std::optional<std::vector<uint8_t>> decrypted_metadata_key =
      DecryptMetadataKey(encrypted_metadata_key, secret_key.get());
  if (!decrypted_metadata_key ||
      !VerifyMetadataEncryptionKeyTag(*decrypted_metadata_key,
                                      metadata_encryption_key_tag)) {
    return std::nullopt;
  }

  // If the key was able to be decrypted, we expect the metadata to be able to
  // be decrypted.
  std::optional<std::vector<uint8_t>> decrypted_metadata_bytes =
      DecryptMetadataPayload(encrypted_metadata, *decrypted_metadata_key,
                             secret_key.get());
  if (!decrypted_metadata_bytes) {
    CD_LOG(ERROR, Feature::NS)
        << "Metadata decryption failed: Failed to decrypt metadata "
        << "payload.";
    return std::nullopt;
  }

  nearby::sharing::proto::EncryptedMetadata unencrypted_metadata;
  if (!unencrypted_metadata.ParseFromArray(decrypted_metadata_bytes->data(),
                                           decrypted_metadata_bytes->size())) {
    CD_LOG(ERROR, Feature::NS)
        << "Metadata decryption failed: Failed to parse decrypted "
        << "metadata payload.";
    return std::nullopt;
  }

  return NearbyShareDecryptedPublicCertificate(
      not_before, not_after, std::move(secret_key), std::move(public_key),
      std::move(id), std::move(unencrypted_metadata),
      public_certificate.for_self_share());
}

NearbyShareDecryptedPublicCertificate::NearbyShareDecryptedPublicCertificate(
    base::Time not_before,
    base::Time not_after,
    std::unique_ptr<crypto::SymmetricKey> secret_key,
    std::vector<uint8_t> public_key,
    std::vector<uint8_t> id,
    nearby::sharing::proto::EncryptedMetadata unencrypted_metadata,
    bool for_self_share)
    : not_before_(not_before),
      not_after_(not_after),
      secret_key_(std::move(secret_key)),
      public_key_(std::move(public_key)),
      id_(std::move(id)),
      unencrypted_metadata_(std::move(unencrypted_metadata)),
      for_self_share_(for_self_share) {}

NearbyShareDecryptedPublicCertificate::NearbyShareDecryptedPublicCertificate(
    const NearbyShareDecryptedPublicCertificate& other) {
  *this = other;
}

NearbyShareDecryptedPublicCertificate&
NearbyShareDecryptedPublicCertificate::operator=(
    const NearbyShareDecryptedPublicCertificate& other) {
  if (this == &other)
    return *this;

  not_before_ = other.not_before_;
  not_after_ = other.not_after_;
  secret_key_ = crypto::SymmetricKey::Import(
      crypto::SymmetricKey::Algorithm::AES, other.secret_key_->key());
  public_key_ = other.public_key_;
  id_ = other.id_;
  unencrypted_metadata_ = other.unencrypted_metadata_;
  for_self_share_ = other.for_self_share_;
  return *this;
}

NearbyShareDecryptedPublicCertificate::NearbyShareDecryptedPublicCertificate(
    NearbyShareDecryptedPublicCertificate&&) = default;

NearbyShareDecryptedPublicCertificate&
NearbyShareDecryptedPublicCertificate::operator=(
    NearbyShareDecryptedPublicCertificate&&) = default;

NearbyShareDecryptedPublicCertificate::
    ~NearbyShareDecryptedPublicCertificate() = default;

bool NearbyShareDecryptedPublicCertificate::VerifySignature(
    base::span<const uint8_t> payload,
    base::span<const uint8_t> signature) const {
  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(crypto::SignatureVerifier::ECDSA_SHA256, signature,
                           public_key_)) {
    CD_LOG(ERROR, Feature::NS)
        << "Verification failed: Initialization unsuccessful.";
    return false;
  }

  verifier.VerifyUpdate(payload);

  return verifier.VerifyFinal();
}

std::vector<uint8_t>
NearbyShareDecryptedPublicCertificate::HashAuthenticationToken(
    base::span<const uint8_t> authentication_token) const {
  return ComputeAuthenticationTokenHash(
      authentication_token,
      base::as_bytes(base::make_span(secret_key_->key())));
}
