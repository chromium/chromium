// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/nearby_share_private_certificate.h"

#include <string_view>
#include <utility>

#include "base/base64url.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/json/values_util.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/nearby_sharing/certificates/common.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_switches.h"
#include "chromeos/ash/components/nearby/common/proto/timestamp.pb.h"
#include "components/cross_device/logging/logging.h"
#include "crypto/aead.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/encryptor.h"
#include "crypto/hmac.h"
#include "crypto/sha2.h"
#include "crypto/symmetric_key.h"

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

// Generates a certificate identifier by hashing the input secret |key|.
std::vector<uint8_t> CreateCertificateIdFromSecretKey(
    const crypto::SymmetricKey& key) {
  DCHECK_EQ(crypto::kSHA256Length, kNearbyShareNumBytesCertificateId);
  std::vector<uint8_t> id(kNearbyShareNumBytesCertificateId);
  crypto::SHA256HashString(key.key(), id.data(), id.size());

  return id;
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

std::string EncodeString(const std::string& unencoded_string) {
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

std::string BytesToEncodedString(const std::vector<uint8_t>& bytes) {
  return EncodeString(std::string(bytes.begin(), bytes.end()));
}

std::optional<std::vector<uint8_t>> EncodedStringToBytes(
    const std::string* str) {
  std::optional<std::string> decoded_str = DecodeString(str);
  return decoded_str ? std::make_optional<std::vector<uint8_t>>(
                           decoded_str->begin(), decoded_str->end())
                     : std::nullopt;
}

std::string SaltsToString(const std::set<std::vector<uint8_t>>& salts) {
  std::string str;
  str.reserve(salts.size() * 2 * kNearbyShareNumBytesMetadataEncryptionKeySalt);
  for (const std::vector<uint8_t>& salt : salts) {
    str += base::HexEncode(salt);
  }
  return str;
}

std::set<std::vector<uint8_t>> StringToSalts(const std::string& str) {
  const size_t chars_per_salt =
      2 * kNearbyShareNumBytesMetadataEncryptionKeySalt;
  DCHECK(str.size() % chars_per_salt == 0);
  std::set<std::vector<uint8_t>> salts;
  for (size_t i = 0; i < str.size(); i += chars_per_salt) {
    std::vector<uint8_t> salt;
    base::HexStringToBytes(std::string_view(&str[i], chars_per_salt), &salt);
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
      key_pair_(crypto::ECPrivateKey::Create()),
      secret_key_(crypto::SymmetricKey::GenerateRandomKey(
          crypto::SymmetricKey::Algorithm::AES,
          /*key_size_in_bits=*/8 * kNearbyShareNumBytesSecretKey)),
      metadata_encryption_key_(
          GenerateRandomBytes(kNearbyShareNumBytesMetadataEncryptionKey)),
      id_(CreateCertificateIdFromSecretKey(*secret_key_)),
      unencrypted_metadata_(std::move(unencrypted_metadata)) {
  DCHECK_NE(visibility, nearby_share::mojom::Visibility::kNoOne);
}

NearbySharePrivateCertificate::NearbySharePrivateCertificate(
    nearby_share::mojom::Visibility visibility,
    base::Time not_before,
    base::Time not_after,
    std::unique_ptr<crypto::ECPrivateKey> key_pair,
    std::unique_ptr<crypto::SymmetricKey> secret_key,
    std::vector<uint8_t> metadata_encryption_key,
    std::vector<uint8_t> id,
    nearby::sharing::proto::EncryptedMetadata unencrypted_metadata,
    std::set<std::vector<uint8_t>> consumed_salts)
    : visibility_(visibility),
      not_before_(not_before),
      not_after_(not_after),
      key_pair_(std::move(key_pair)),
      secret_key_(std::move(secret_key)),
      metadata_encryption_key_(std::move(metadata_encryption_key)),
      id_(std::move(id)),
      unencrypted_metadata_(std::move(unencrypted_metadata)),
      consumed_salts_(std::move(consumed_salts)) {
  DCHECK_NE(visibility, nearby_share::mojom::Visibility::kNoOne);
}

NearbySharePrivateCertificate::NearbySharePrivateCertificate(
    const NearbySharePrivateCertificate& other) {
  *this = other;
}

NearbySharePrivateCertificate& NearbySharePrivateCertificate::operator=(
    const NearbySharePrivateCertificate& other) {
  if (this == &other)
    return *this;

  visibility_ = other.visibility_;
  not_before_ = other.not_before_;
  not_after_ = other.not_after_;
  key_pair_ = other.key_pair_->Copy();
  secret_key_ = crypto::SymmetricKey::Import(
      crypto::SymmetricKey::Algorithm::AES, other.secret_key_->key());
  metadata_encryption_key_ = other.metadata_encryption_key_;
  id_ = other.id_;
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
  std::optional<std::vector<uint8_t>> salt = GenerateUnusedSalt();
  if (!salt) {
    CD_LOG(ERROR, Feature::NS)
        << "Encryption failed: Salt generation unsuccessful.";
    return std::nullopt;
  }

  std::unique_ptr<crypto::Encryptor> encryptor =
      CreateNearbyShareCtrEncryptor(secret_key_.get(), *salt);
  if (!encryptor) {
    CD_LOG(ERROR, Feature::NS)
        << "Encryption failed: Could not create CTR encryptor.";
    return std::nullopt;
  }

  DCHECK_EQ(kNearbyShareNumBytesMetadataEncryptionKey,
            metadata_encryption_key_.size());
  std::vector<uint8_t> encrypted_metadata_key;
  if (!encryptor->Encrypt(metadata_encryption_key_, &encrypted_metadata_key)) {
    CD_LOG(ERROR, Feature::NS)
        << "Encryption failed: Could not encrypt metadata key.";
    return std::nullopt;
  }

  return NearbyShareEncryptedMetadataKey(*salt, encrypted_metadata_key);
}

std::optional<std::vector<uint8_t>> NearbySharePrivateCertificate::Sign(
    base::span<const uint8_t> payload) const {
  std::unique_ptr<crypto::ECSignatureCreator> signer(
      crypto::ECSignatureCreator::Create(key_pair_.get()));

  std::vector<uint8_t> signature;
  if (!signer->Sign(payload, &signature)) {
    CD_LOG(ERROR, Feature::NS) << "Signing failed.";
    return std::nullopt;
  }

  return signature;
}

std::vector<uint8_t> NearbySharePrivateCertificate::HashAuthenticationToken(
    base::span<const uint8_t> authentication_token) const {
  return ComputeAuthenticationTokenHash(
      authentication_token,
      base::as_bytes(base::make_span(secret_key_->key())));
}

std::optional<nearby::sharing::proto::PublicCertificate>
NearbySharePrivateCertificate::ToPublicCertificate() const {
  std::vector<uint8_t> public_key;
  if (!key_pair_->ExportPublicKey(&public_key)) {
    CD_LOG(ERROR, Feature::NS) << "Failed to export public key.";
    return std::nullopt;
  }

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
  public_certificate.set_secret_key(secret_key_->key());
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
  std::vector<uint8_t> key_pair;
  key_pair_->ExportPrivateKey(&key_pair);

  return base::Value::Dict()
      .Set(kVisibility, static_cast<int>(visibility_))
      .Set(kNotBefore, base::TimeToValue(not_before_))
      .Set(kNotAfter, base::TimeToValue(not_after_))
      .Set(kKeyPair, BytesToEncodedString(key_pair))
      .Set(kSecretKey, EncodeString(secret_key_->key()))
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

  std::unique_ptr<crypto::ECPrivateKey> key_pair =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(*bytes_opt);

  str_opt = DecodeString(dict.FindString(kSecretKey));
  if (!str_opt)
    return std::nullopt;

  std::unique_ptr<crypto::SymmetricKey> secret_key =
      crypto::SymmetricKey::Import(crypto::SymmetricKey::Algorithm::AES,
                                   *str_opt);

  bytes_opt = EncodedStringToBytes(dict.FindString(kMetadataEncryptionKey));
  if (!bytes_opt)
    return std::nullopt;

  std::vector<uint8_t> metadata_encryption_key = *bytes_opt;

  bytes_opt = EncodedStringToBytes(dict.FindString(kId));
  if (!bytes_opt)
    return std::nullopt;

  std::vector<uint8_t> id = *bytes_opt;

  str_opt = DecodeString(dict.FindString(kUnencryptedMetadata));
  if (!str_opt)
    return std::nullopt;

  nearby::sharing::proto::EncryptedMetadata unencrypted_metadata;
  if (!unencrypted_metadata.ParseFromString(*str_opt))
    return std::nullopt;

  str_ptr = dict.FindString(kConsumedSalts);
  if (!str_ptr)
    return std::nullopt;

  std::set<std::vector<uint8_t>> consumed_salts = StringToSalts(*str_ptr);

  return NearbySharePrivateCertificate(
      visibility, not_before, not_after, std::move(key_pair),
      std::move(secret_key), std::move(metadata_encryption_key), std::move(id),
      std::move(unencrypted_metadata), std::move(consumed_salts));
}

std::optional<std::vector<uint8_t>>
NearbySharePrivateCertificate::GenerateUnusedSalt() {
  if (consumed_salts_.size() >= kNearbyShareMaxNumMetadataEncryptionKeySalts) {
    CD_LOG(ERROR, Feature::NS) << "All salts exhausted for certificate.";
    return std::nullopt;
  }

  for (size_t attempt = 0;
       attempt < kNearbyShareMaxNumMetadataEncryptionKeySaltGenerationRetries;
       ++attempt) {
    std::vector<uint8_t> salt;
    if (next_salts_for_testing_.empty()) {
      salt = GenerateRandomBytes(2u);
    } else {
      salt = next_salts_for_testing_.front();
      next_salts_for_testing_.pop();
    }
    DCHECK_EQ(2u, salt.size());

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
  std::vector<uint8_t> derived_key = DeriveNearbyShareKey(
      metadata_encryption_key_, kNearbyShareNumBytesAesGcmKey);

  crypto::Aead aead(crypto::Aead::AeadAlgorithm::AES_256_GCM);
  aead.Init(derived_key);

  std::vector<uint8_t> metadata_array(unencrypted_metadata_.ByteSizeLong());
  unencrypted_metadata_.SerializeToArray(metadata_array.data(),
                                         metadata_array.size());

  return aead.Seal(
      metadata_array,
      /*nonce=*/
      DeriveNearbyShareKey(base::as_bytes(base::make_span(secret_key_->key())),
                           kNearbyShareNumBytesAesGcmIv),
      /*additional_data=*/base::span<const uint8_t>());
}
