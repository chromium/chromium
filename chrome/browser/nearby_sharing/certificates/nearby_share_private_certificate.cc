// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/nearby_share_private_certificate.h"

#include <stdint.h>

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/json/values_util.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/nearby_sharing/certificates/common.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_switches.h"
#include "chromeos/ash/components/nearby/common/proto/timestamp.pb.h"
#include "components/cross_device/logging/logging.h"
#include "crypto/aead.h"
#include "crypto/aes_ctr.h"
#include "crypto/hmac.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"

namespace {

// Dictionary keys used in ToDictionary, FromDictionary
const char kVisibility[] = "visibility";
const char kNotBefore[] = "not_before";
const char kNotAfter[] = "not_after";
const char kKeyPair[] = "key_pair";
const char kSecretKey[] = "secret_key";
const char kMetadataEncryptionKey[] = "metadata_encryption_key";
const char kId[] = "id";
const char kUnencryptedMetadata[] = "unencrypted_metadata";
const char kConsumedSalts[] = "consumed_salts";

// Generates a random validity bound offset in the interval
// [0, kNearbyShareMaxPrivateCertificateValidityBoundOffset).
base::TimeDelta GenerateRandomOffset() {
  return base::RandTimeDeltaUpTo(
      kNearbyShareMaxPrivateCertificateValidityBoundOffset);
}

// Creates an HMAC from |metadata_encryption_key| to be used as a key commitment
// in certificates.
std::optional<std::vector<uint8_t>> CreateMetadataEncryptionKeyTag(
    base::span<const uint8_t> metadata_encryption_key) {
  // This array of 0x00 is used to conform with the GmsCore implementation.
  std::vector<uint8_t> key(kNearbyShareNumBytesMetadataEncryptionKeyTag, 0x00);

  std::vector<uint8_t> result(kNearbyShareNumBytesMetadataEncryptionKeyTag);
  crypto::HMAC hmac(crypto::HMAC::HashAlgorithm::SHA256);
  if (!hmac.Init(key) || !hmac.Sign(metadata_encryption_key, result))
    return std::nullopt;

  return result;
}

std::string EncodeString(std::string_view unencoded_string) {
  std::string encoded_string;
  base::Base64UrlEncode(unencoded_string,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_string);

  return encoded_string;
}

std::optional<std::string> DecodeString(const std::string* encoded_string) {
  if (!encoded_string)
    return std::nullopt;

  std::string decoded_string;
  if (!base::Base64UrlDecode(*encoded_string,
                             base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                             &decoded_string)) {
    return std::nullopt;
  }

  return decoded_string;
}

std::string BytesToEncodedString(base::span<const uint8_t> bytes) {
  return EncodeString(base::as_string_view(bytes));
}

std::optional<std::vector<uint8_t>> EncodedStringToBytes(
    const std::string* str) {
  std::optional<std::string> decoded_str = DecodeString(str);
  return decoded_str ? std::make_optional<std::vector<uint8_t>>(
                           decoded_str->begin(), decoded_str->end())
                     : std::nullopt;
}

std::string SaltsToString(
    const std::set<
        std::array<uint8_t, kNearbyShareNumBytesMetadataEncryptionKeySalt>>&
        salts) {
  std::string str;
  str.reserve(salts.size() * 2 * kNearbyShareNumBytesMetadataEncryptionKeySalt);
  for (const auto& salt : salts) {
    str += base::HexEncode(salt);
  }
  return str;
}

std::set<std::array<uint8_t, kNearbyShareNumBytesMetadataEncryptionKeySalt>>
StringToSalts(const std::string& str) {
  DCHECK(str.size() % (2 * kNearbyShareNumBytesMetadataEncryptionKeySalt) == 0);
  std::vector<uint8_t> salt_bytes;
  base::HexStringToBytes(str, &salt_bytes);
  std::set<std::array<uint8_t, kNearbyShareNumBytesMetadataEncryptionKeySalt>>
      salts;
  for (base::span salt_span(salt_bytes); !salt_span.empty();) {
    std::array<uint8_t, kNearbyShareNumBytesMetadataEncryptionKeySalt> salt;
    base::span(salt).copy_from(
        salt_span.take_first<kNearbyShareNumBytesMetadataEncryptionKeySalt>());
    salts.insert(std::move(salt));
  }
  return salts;
}

// Check for a command-line override the certificate validity period, otherwise
// return the default |kNearbyShareCertificateValidityPeriod|.
base::TimeDelta GetCertificateValidityPeriod() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(
          switches::kNearbyShareCertificateValidityPeriodHours)) {
    return kNearbyShareCertificateValidityPeriod;
  }

  std::string certificate_validity_period_hours_str =
      command_line->GetSwitchValueASCII(
          switches::kNearbyShareCertificateValidityPeriodHours);
  int certificate_validity_period_hours = 0;
  if (!base::StringToInt(certificate_validity_period_hours_str,
                         &certificate_validity_period_hours) ||
      certificate_validity_period_hours < 1) {
    CD_LOG(ERROR, Feature::NS)
        << __func__
        << ": Invalid value provided for certificate validity period override.";
    return kNearbyShareCertificateValidityPeriod;
  }

  return base::Hours(certificate_validity_period_hours);
}

}  // namespace

NearbySharePrivateCertificate::NearbySharePrivateCertificate(
    nearby_share::mojom::Visibility visibility,
    base::Time not_before,
    nearby::sharing::proto::EncryptedMetadata unencrypted_metadata)
    : visibility_(visibility),
      not_before_(not_before),
      not_after_(not_before_ + GetCertificateValidityPeriod()),
      private_key_(crypto::keypair::PrivateKey::GenerateEcP256()),
      metadata_encryption_key_(
          GenerateRandomBytes<kNearbyShareNumBytesMetadataEncryptionKey>()),
      unencrypted_metadata_(std::move(unencrypted_metadata)) {
  crypto::RandBytes(secret_key_);
  id_ = crypto::hash::Sha256(secret_key_);
  DCHECK_NE(visibility, nearby_share::mojom::Visibility::kNoOne);
}

NearbySharePrivateCertificate::NearbySharePrivateCertificate(
    nearby_share::mojom::Visibility visibility,
    base::Time not_before,
    base::Time not_after,
    crypto::keypair::PrivateKey private_key,
    base::span<const uint8_t, kNearbyShareNumBytesSecretKey> secret_key,
    base::span<const uint8_t, kNearbyShareNumBytesMetadataEncryptionKey>
        metadata_encryption_key,
    base::span<const uint8_t, kNearbyShareNumBytesCertificateId> id,
    nearby::sharing::proto::EncryptedMetadata unencrypted_metadata,
    std::set<std::array<uint8_t, kNearbyShareNumBytesMetadataEncryptionKeySalt>>
        consumed_salts)
    : visibility_(visibility),
      not_before_(not_before),
      not_after_(not_after),
      private_key_(std::move(private_key)),
      unencrypted_metadata_(std::move(unencrypted_metadata)),
      consumed_salts_(std::move(consumed_salts)) {
  DCHECK_NE(visibility, nearby_share::mojom::Visibility::kNoOne);
  base::span(secret_key_).copy_from(secret_key);
  base::span(id_).copy_from(id);
  base::span(metadata_encryption_key_).copy_from(metadata_encryption_key);
}

NearbySharePrivateCertificate::NearbySharePrivateCertificate(
    const NearbySharePrivateCertificate& other)
    : private_key_(other.private_key_) {
  *this = other;
}

NearbySharePrivateCertificate& NearbySharePrivateCertificate::operator=(
    const NearbySharePrivateCertificate& other) {
  if (this == &other)
    return *this;

  visibility_ = other.visibility_;
  not_before_ = other.not_before_;
  not_after_ = other.not_after_;
  private_key_ = other.private_key_;
  base::span(secret_key_).copy_from(other.secret_key_);
  base::span(id_).copy_from(other.id_);
  base::span(metadata_encryption_key_)
      .copy_from(other.metadata_encryption_key_);
  unencrypted_metadata_ = other.unencrypted_metadata_;
  consumed_salts_ = other.consumed_salts_;
  next_salts_for_testing_ = other.next_salts_for_testing_;
  offset_for_testing_ = other.offset_for_testing_;
  return *this;
}

NearbySharePrivateCertificate::NearbySharePrivateCertificate(
    NearbySharePrivateCertificate&& other) = default;

NearbySharePrivateCertificate& NearbySharePrivateCertificate::operator=(
    NearbySharePrivateCertificate&& other) = default;

NearbySharePrivateCertificate::~NearbySharePrivateCertificate() = default;

std::optional<NearbyShareEncryptedMetadataKey>
NearbySharePrivateCertificate::EncryptMetadataKey() {
  std::optional<
      std::array<uint8_t, kNearbyShareNumBytesMetadataEncryptionKeySalt>>
      salt = GenerateUnusedSalt();
  if (!salt) {
    CD_LOG(ERROR, Feature::NS)
        << "Encryption failed: Salt generation unsuccessful.";
    return std::nullopt;
  }

  auto counter = DeriveNearbyShareKey<crypto::aes_ctr::kCounterSize>(*salt);
  std::array<uint8_t, kNearbyShareNumBytesMetadataEncryptionKey>
      metadata_encryption_key;
  base::span(metadata_encryption_key)
      .copy_from(crypto::aes_ctr::Encrypt(secret_key_, counter,
                                          metadata_encryption_key_));
  return NearbyShareEncryptedMetadataKey(std::move(*salt),
                                         std::move(metadata_encryption_key));
}

std::vector<uint8_t> NearbySharePrivateCertificate::Sign(
    base::span<const uint8_t> payload) const {
  return crypto::sign::Sign(crypto::sign::SignatureKind::ECDSA_SHA256,
                            private_key_, payload);
}

std::array<uint8_t, kNearbyShareNumBytesAuthenticationTokenHash>
NearbySharePrivateCertificate::HashAuthenticationToken(
    base::span<const uint8_t> authentication_token) const {
  return ComputeAuthenticationTokenHash(authentication_token, secret_key_);
}

std::optional<nearby::sharing::proto::PublicCertificate>
NearbySharePrivateCertificate::ToPublicCertificate() const {
  std::vector<uint8_t> public_key = private_key_.ToSubjectPublicKeyInfo();

  std::optional<std::vector<uint8_t>> encrypted_metadata_bytes =
      EncryptMetadata();
  if (!encrypted_metadata_bytes) {
    CD_LOG(ERROR, Feature::NS) << "Failed to encrypt metadata.";
    return std::nullopt;
  }

  std::optional<std::vector<uint8_t>> metadata_encryption_key_tag =
      CreateMetadataEncryptionKeyTag(metadata_encryption_key_);
  if (!metadata_encryption_key_tag) {
    CD_LOG(ERROR, Feature::NS)
        << "Failed to compute metadata encryption key tag.";
    return std::nullopt;
  }

  base::TimeDelta not_before_offset =
      offset_for_testing_.value_or(GenerateRandomOffset());
  base::TimeDelta not_after_offset =
      offset_for_testing_.value_or(GenerateRandomOffset());

  nearby::sharing::proto::PublicCertificate public_certificate;
  public_certificate.set_secret_id(std::string(id_.begin(), id_.end()));
  public_certificate.set_secret_key(
      std::string(secret_key_.begin(), secret_key_.end()));
  public_certificate.set_public_key(
      std::string(public_key.begin(), public_key.end()));
  public_certificate.mutable_start_time()->set_seconds(
      (not_before_ - not_before_offset).InMillisecondsSinceUnixEpoch() / 1000);
  public_certificate.mutable_end_time()->set_seconds(
      (not_after_ + not_after_offset).InMillisecondsSinceUnixEpoch() / 1000);

  // When `visibility_` is set to kYourDevices, under the hood, the visibility
  // is set to Selected Contacts with an empty allowed contact list. The
  // NearbyShare server sends a public certificate to all devices logged into
  // the same GAIA account as this one when the visibility is kSelectedContacts,
  // so if the allowed contact list is empty, then the public certificate is
  // sent out to devices logged into the same GAIA account only; this is
  // effectively being visible only to the user's own devices.
  public_certificate.set_for_selected_contacts(
      visibility_ == nearby_share::mojom::Visibility::kSelectedContacts ||
      visibility_ == nearby_share::mojom::Visibility::kYourDevices);
  public_certificate.set_metadata_encryption_key(std::string(
      metadata_encryption_key_.begin(), metadata_encryption_key_.end()));
  public_certificate.set_encrypted_metadata_bytes(std::string(
      encrypted_metadata_bytes->begin(), encrypted_metadata_bytes->end()));
  public_certificate.set_metadata_encryption_key_tag(
      std::string(metadata_encryption_key_tag->begin(),
                  metadata_encryption_key_tag->end()));

  // Note: Setting |for_self_share| here will cause the server to silently
  // reject the marked certificates. The |for_self_share| field is not set by
  // clients but is set by the server for all downloaded public certificates.

  // TODO (brandosocarras@ b/291132662): indicate that Your Devices visibility
  // public certificates are Your Devices visibility to NS server.

  return public_certificate;
}

base::Value::Dict NearbySharePrivateCertificate::ToDictionary() const {
  std::vector<uint8_t> private_key = private_key_.ToPrivateKeyInfo();

  return base::Value::Dict()
      .Set(kVisibility, static_cast<int>(visibility_))
      .Set(kNotBefore, base::TimeToValue(not_before_))
      .Set(kNotAfter, base::TimeToValue(not_after_))
      .Set(kKeyPair, BytesToEncodedString(private_key))
      .Set(kSecretKey, BytesToEncodedString(secret_key_))
      .Set(kMetadataEncryptionKey,
           BytesToEncodedString(metadata_encryption_key_))
      .Set(kId, BytesToEncodedString(id_))
      .Set(kUnencryptedMetadata,
           EncodeString(unencrypted_metadata_.SerializeAsString()))
      .Set(kConsumedSalts, SaltsToString(consumed_salts_));
}

std::optional<NearbySharePrivateCertificate>
NearbySharePrivateCertificate::FromDictionary(const base::Value::Dict& dict) {
  std::optional<int> int_opt;
  const std::string* str_ptr;
  std::optional<std::string> str_opt;
  std::optional<base::Time> time_opt;
  std::optional<std::vector<uint8_t>> bytes_opt;

  int_opt = dict.FindInt(kVisibility);
  if (!int_opt)
    return std::nullopt;

  nearby_share::mojom::Visibility visibility =
      static_cast<nearby_share::mojom::Visibility>(*int_opt);

  time_opt = base::ValueToTime(dict.Find(kNotBefore));
  if (!time_opt)
    return std::nullopt;

  base::Time not_before = *time_opt;

  time_opt = base::ValueToTime(dict.Find(kNotAfter));
  if (!time_opt)
    return std::nullopt;

  base::Time not_after = *time_opt;

  bytes_opt = EncodedStringToBytes(dict.FindString(kKeyPair));
  if (!bytes_opt)
    return std::nullopt;

  auto private_key_opt =
      crypto::keypair::PrivateKey::FromPrivateKeyInfo(*bytes_opt);
  if (!private_key_opt) {
    return std::nullopt;
  }

  bytes_opt = EncodedStringToBytes(dict.FindString(kSecretKey));
  if (!bytes_opt) {
    return std::nullopt;
  }

  std::array<uint8_t, kNearbyShareNumBytesSecretKey> secret_key;
  base::span(secret_key).copy_from(*bytes_opt);

  bytes_opt = EncodedStringToBytes(dict.FindString(kMetadataEncryptionKey));
  if (!bytes_opt)
    return std::nullopt;

  std::array<uint8_t, kNearbyShareNumBytesMetadataEncryptionKey>
      metadata_encryption_key;
  base::span(metadata_encryption_key).copy_from(*bytes_opt);

  bytes_opt = EncodedStringToBytes(dict.FindString(kId));
  if (!bytes_opt)
    return std::nullopt;

  std::array<uint8_t, kNearbyShareNumBytesCertificateId> id;
  base::span(id).copy_from(*bytes_opt);

  str_opt = DecodeString(dict.FindString(kUnencryptedMetadata));
  if (!str_opt)
    return std::nullopt;

  nearby::sharing::proto::EncryptedMetadata unencrypted_metadata;
  if (!unencrypted_metadata.ParseFromString(*str_opt))
    return std::nullopt;

  str_ptr = dict.FindString(kConsumedSalts);
  if (!str_ptr)
    return std::nullopt;

  std::set<std::array<uint8_t, kNearbyShareNumBytesMetadataEncryptionKeySalt>>
      consumed_salts = StringToSalts(*str_ptr);

  return NearbySharePrivateCertificate(
      visibility, not_before, not_after, std::move(*private_key_opt),
      std::move(secret_key), std::move(metadata_encryption_key), std::move(id),
      std::move(unencrypted_metadata), std::move(consumed_salts));
}

std::optional<
    std::array<uint8_t, kNearbyShareNumBytesMetadataEncryptionKeySalt>>
NearbySharePrivateCertificate::GenerateUnusedSalt() {
  if (consumed_salts_.size() >= kNearbyShareMaxNumMetadataEncryptionKeySalts) {
    CD_LOG(ERROR, Feature::NS) << "All salts exhausted for certificate.";
    return std::nullopt;
  }

  for (size_t attempt = 0;
       attempt < kNearbyShareMaxNumMetadataEncryptionKeySaltGenerationRetries;
       ++attempt) {
    std::array<uint8_t, kNearbyShareNumBytesMetadataEncryptionKeySalt> salt;
    if (next_salts_for_testing_.empty()) {
      salt =
          GenerateRandomBytes<kNearbyShareNumBytesMetadataEncryptionKeySalt>();
    } else {
      salt = next_salts_for_testing_.front();
      next_salts_for_testing_.pop();
    }

    if (!base::Contains(consumed_salts_, salt)) {
      consumed_salts_.insert(salt);
      return salt;
    }
  }

  CD_LOG(ERROR, Feature::NS)
      << "Salt generation exceeded max number of retries. This is "
         "highly improbable.";
  return std::nullopt;
}

std::optional<std::vector<uint8_t>>
NearbySharePrivateCertificate::EncryptMetadata() const {
  // Init() keeps a reference to the input key, so that reference must outlive
  // the lifetime of |aead|.
  auto derived_key = DeriveNearbyShareKey<kNearbyShareNumBytesAesGcmKey>(
      metadata_encryption_key_);

  crypto::Aead aead(crypto::Aead::AeadAlgorithm::AES_256_GCM);
  aead.Init(derived_key);

  std::vector<uint8_t> metadata_array(unencrypted_metadata_.ByteSizeLong());
  unencrypted_metadata_.SerializeToArray(metadata_array.data(),
                                         metadata_array.size());

  return aead.Seal(
      metadata_array,
      /*nonce=*/
      DeriveNearbyShareKey<kNearbyShareNumBytesAesGcmIv>(secret_key_),
      /*additional_data=*/base::span<const uint8_t>());
}
