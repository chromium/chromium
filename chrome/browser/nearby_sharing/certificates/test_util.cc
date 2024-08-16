// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/nearby_sharing/certificates/test_util.h"

#include <set>

#include "base/no_destructor.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
#include "third_party/nearby/sharing/proto/timestamp.pb.h"

namespace {

// Sample P-256 public and private keys from RFC 6979 A.2.5 in their respective
// ASN.1 formats: SubjectPublicKeyInfo (RFC 5280) and PKCS #8 PrivateKeyInfo
// (RFC 5208).
const uint8_t kTestPublicKeyBytes[] = {
    0x30, 0x59,
    // Begin AlgorithmIdentifier: ecPublicKey, prime256v1
    0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06,
    0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07,
    // End AlgorithmIdentifier
    0x03, 0x42, 0x00, 0x04,
    // Public key bytes (Ux):
    0x60, 0xfe, 0xd4, 0xba, 0x25, 0x5a, 0x9d, 0x31, 0xc9, 0x61, 0xeb, 0x74,
    0xc6, 0x35, 0x6d, 0x68, 0xc0, 0x49, 0xb8, 0x92, 0x3b, 0x61, 0xfa, 0x6c,
    0xe6, 0x69, 0x62, 0x2e, 0x60, 0xf2, 0x9f, 0xb6,
    // Public key bytes (Uy):
    0x79, 0x03, 0xfe, 0x10, 0x08, 0xb8, 0xbc, 0x99, 0xa4, 0x1a, 0xe9, 0xe9,
    0x56, 0x28, 0xbc, 0x64, 0xf2, 0xf1, 0xb2, 0x0c, 0x2d, 0x7e, 0x9f, 0x51,
    0x77, 0xa3, 0xc2, 0x94, 0xd4, 0x46, 0x22, 0x99};
const uint8_t kTestPrivateKeyBytes[] = {
    0x30, 0x81, 0x87, 0x02, 0x01, 0x00,
    // Begin AlgorithmIdentifier: ecPublicKey, prime256v1
    0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06,
    0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07,
    // End AlgorithmIdentifier
    0x04, 0x6d, 0x30, 0x6b, 0x02, 0x01, 0x01, 0x04, 0x20,
    // Begin private key bytes
    0xc9, 0xaf, 0xa9, 0xd8, 0x45, 0xba, 0x75, 0x16, 0x6b, 0x5c, 0x21, 0x57,
    0x67, 0xb1, 0xd6, 0x93, 0x4e, 0x50, 0xc3, 0xdb, 0x36, 0xe8, 0x9b, 0x12,
    0x7b, 0x8a, 0x62, 0x2b, 0x12, 0x0f, 0x67, 0x21,
    // End private key bytes
    0xa1, 0x44, 0x03, 0x42, 0x00, 0x04,
    // Public key:
    0x60, 0xfe, 0xd4, 0xba, 0x25, 0x5a, 0x9d, 0x31, 0xc9, 0x61, 0xeb, 0x74,
    0xc6, 0x35, 0x6d, 0x68, 0xc0, 0x49, 0xb8, 0x92, 0x3b, 0x61, 0xfa, 0x6c,
    0xe6, 0x69, 0x62, 0x2e, 0x60, 0xf2, 0x9f, 0xb6, 0x79, 0x03, 0xfe, 0x10,
    0x08, 0xb8, 0xbc, 0x99, 0xa4, 0x1a, 0xe9, 0xe9, 0x56, 0x28, 0xbc, 0x64,
    0xf2, 0xf1, 0xb2, 0x0c, 0x2d, 0x7e, 0x9f, 0x51, 0x77, 0xa3, 0xc2, 0x94,
    0xd4, 0x46, 0x22, 0x99};

const uint8_t kTestSecretKey[] = {
    0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe, 0x2b, 0x73, 0xae,
    0xf0, 0x85, 0x7d, 0x77, 0x81, 0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61,
    0x08, 0xd7, 0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4};

const uint8_t kTestCertificateId[] = {
    0xb9, 0x3c, 0x72, 0xb7, 0x4b, 0xc8, 0x48, 0x7d, 0x29, 0x82, 0x70,
    0x05, 0xf8, 0x0d, 0x63, 0x59, 0x18, 0xf9, 0x1b, 0xc2, 0x2b, 0x14,
    0xd7, 0xed, 0x05, 0x71, 0x4d, 0x58, 0xf9, 0x67, 0x02, 0xdd};

const uint8_t kTestMetadataEncryptionKey[] = {0x60, 0x1e, 0xc3, 0x13, 0x77,
                                              0x57, 0x89, 0xa5, 0xb7, 0xa7,
                                              0xf5, 0x04, 0xbb, 0xf3};

const uint8_t kTestMetadataEncryptionKeyTag[] = {
    0x51, 0x9b, 0x16, 0xd8, 0x91, 0xb4, 0x0d, 0x81, 0x11, 0x21, 0xe3,
    0x70, 0x42, 0x80, 0x8f, 0x87, 0x23, 0x6a, 0x84, 0x9b, 0xcd, 0xac,
    0xbc, 0xe3, 0x54, 0xd7, 0xff, 0x53, 0xdf, 0x5d, 0x8a, 0xda};

const uint8_t kTestSalt[] = {0xf0, 0xf1};

const uint8_t kTestEncryptedMetadataKey[] = {0x52, 0x0e, 0x7e, 0x6b, 0x8e,
                                             0xb5, 0x40, 0xe8, 0xe2, 0xbd,
                                             0xa0, 0xee, 0x9d, 0x7b};

// TODO fix
const uint8_t kTestEncryptedMetadata[] = {
    0x4d, 0x59, 0x5d, 0xb6, 0xac, 0x70, 0x00, 0x8f, 0x32, 0x9d, 0x0d,
    0xcf, 0xc3, 0x8b, 0x01, 0x19, 0x1d, 0xad, 0x2e, 0xb4, 0x62, 0xec,
    0xf3, 0xa5, 0xe4, 0x89, 0x51, 0x37, 0x0d, 0x78, 0xad, 0x9d, 0x2e,
    0xe5, 0x99, 0xd5, 0xf7, 0x1d, 0x71, 0x47, 0xef, 0x33, 0xae, 0x4f,
    0xf0, 0xd0, 0x4b, 0xcf, 0x1e, 0xaf, 0x06, 0xfa, 0x08, 0x79, 0x9b,
    0x76, 0x44, 0x13, 0xad, 0x08, 0x68, 0xef, 0x0a, 0xc5, 0x13, 0xd0,
    0xe8, 0xaa, 0xbe, 0x52, 0x28, 0xb1, 0xb6, 0xc4, 0x20};

// Plaintext "sample" (from RFC 6979 A.2.5).
const uint8_t kTestPayloadToSign[] = {0x73, 0x61, 0x6d, 0x70, 0x6c, 0x65};

// One possible signature (from RFC 6979 A.2.5).
const uint8_t kTestSampleSignature[] = {
    0x30,
    0x46,  // length of remaining data
    0x02,
    0x21,  // length of r
    // begin r (note 0x00 padding since leading bit of 0xEF is 1)
    0x00, 0xEF, 0xD4, 0x8B, 0x2A, 0xAC, 0xB6, 0xA8, 0xFD, 0x11, 0x40, 0xDD,
    0x9C, 0xD4, 0x5E, 0x81, 0xD6, 0x9D, 0x2C, 0x87, 0x7B, 0x56, 0xAA, 0xF9,
    0x91, 0xC3, 0x4D, 0x0E, 0xA8, 0x4E, 0xAF, 0x37, 0x16,
    // end r
    0x02,
    0x21,  // length of s
    // begin s (note 0x00 padding since leading bit of 0xF7 is 1)
    0x00, 0xF7, 0xCB, 0x1C, 0x94, 0x2D, 0x65, 0x7C, 0x41, 0xD4, 0x36, 0xC7,
    0xA1, 0xB6, 0xE2, 0x9F, 0x65, 0xF3, 0xE9, 0x00, 0xDB, 0xB9, 0xAF, 0xF4,
    0x06, 0x4D, 0xC4, 0xAB, 0x2F, 0x84, 0x3A, 0xCD, 0xA8
    // end s
};

// The result of HKDF of kTestPayloadToSign, using kTestSecretKey as salt. A
// trivial info parameter is used, and the output length is fixed to be
// kNearbyShareNumBytesAuthenticationTokenHash.
const uint8_t kTestPayloadHashUsingSecretKey[] = {0xE2, 0xCB, 0x90,
                                                  0x58, 0xDE, 0x3A};

const int64_t kTestNotBeforeMillis = 1881702000000;

const int64_t kTestValidityOffsetMillis = 1800000;  // 30 minutes

}  // namespace

// Do not change. Values align with kTestEncryptedMetadata.
const char kTestDeviceName[] = "device_name";
const char kTestMetadataFullName[] = "full_name";
const char kTestMetadataIconUrl[] = "icon_url";
const char kTestUnparsedBluetoothMacAddress[] = "4E:65:61:72:62:79";
const char kTestAccountName[] = "test@google.com";

std::unique_ptr<crypto::ECPrivateKey> GetNearbyShareTestP256KeyPair() {
  return crypto::ECPrivateKey::CreateFromPrivateKeyInfo(kTestPrivateKeyBytes);
}

const std::vector<uint8_t>& GetNearbyShareTestP256PublicKey() {
  static const base::NoDestructor<std::vector<uint8_t>> public_key(
      std::begin(kTestPublicKeyBytes), std::end(kTestPublicKeyBytes));
  return *public_key;
}

std::unique_ptr<crypto::SymmetricKey> GetNearbyShareTestSecretKey() {
  return crypto::SymmetricKey::Import(
      crypto::SymmetricKey::Algorithm::AES,
      std::string(reinterpret_cast<const char*>(kTestSecretKey),
                  kNearbyShareNumBytesSecretKey));
}

const std::vector<uint8_t>& GetNearbyShareTestCertificateId() {
  static const base::NoDestructor<std::vector<uint8_t>> id(
      std::begin(kTestCertificateId), std::end(kTestCertificateId));
  return *id;
}

const std::vector<uint8_t>& GetNearbyShareTestMetadataEncryptionKey() {
  static const base::NoDestructor<std::vector<uint8_t>> metadata_encryption_key(
      kTestMetadataEncryptionKey,
      kTestMetadataEncryptionKey + kNearbyShareNumBytesMetadataEncryptionKey);
  return *metadata_encryption_key;
}

const std::vector<uint8_t>& GetNearbyShareTestMetadataEncryptionKeyTag() {
  static const base::NoDestructor<std::vector<uint8_t>> tag(
      std::begin(kTestMetadataEncryptionKeyTag),
      std::end(kTestMetadataEncryptionKeyTag));
  return *tag;
}

const std::vector<uint8_t>& GetNearbyShareTestSalt() {
  static const base::NoDestructor<std::vector<uint8_t>> salt(
      std::begin(kTestSalt), std::end(kTestSalt));
  return *salt;
}

const NearbyShareEncryptedMetadataKey&
GetNearbyShareTestEncryptedMetadataKey() {
  static const base::NoDestructor<NearbyShareEncryptedMetadataKey>
      encrypted_metadata_key(
          GetNearbyShareTestSalt(),
          std::vector<uint8_t>(std::begin(kTestEncryptedMetadataKey),
                               std::end(kTestEncryptedMetadataKey)));
  return *encrypted_metadata_key;
}

base::Time GetNearbyShareTestNotBefore() {
  static const base::Time not_before =
      base::Time::FromMillisecondsSinceUnixEpoch(kTestNotBeforeMillis);
  return not_before;
}

base::TimeDelta GetNearbyShareTestValidityOffset() {
  static const base::TimeDelta offset =
      base::Milliseconds(kTestValidityOffsetMillis);
  return offset;
}

const nearby::sharing::proto::EncryptedMetadata& GetNearbyShareTestMetadata() {
  static const base::NoDestructor<nearby::sharing::proto::EncryptedMetadata>
      metadata([] {
        std::array<uint8_t, 6> bytes;
        device::ParseBluetoothAddress(kTestUnparsedBluetoothMacAddress, bytes);

        nearby::sharing::proto::EncryptedMetadata metadata;
        metadata.set_device_name(kTestDeviceName);
        metadata.set_full_name(kTestMetadataFullName);
        metadata.set_icon_url(kTestMetadataIconUrl);
        metadata.set_bluetooth_mac_address(bytes.data(), 6u);
        metadata.set_account_name(kTestAccountName);
        return metadata;
      }());
  return *metadata;
}

const std::vector<uint8_t>& GetNearbyShareTestEncryptedMetadata() {
  static const base::NoDestructor<std::vector<uint8_t>> bytes(
      std::begin(kTestEncryptedMetadata), std::end(kTestEncryptedMetadata));
  return *bytes;
}

const std::vector<uint8_t>& GetNearbyShareTestPayloadToSign() {
  static const base::NoDestructor<std::vector<uint8_t>> payload(
      std::begin(kTestPayloadToSign), std::end(kTestPayloadToSign));
  return *payload;
}

const std::vector<uint8_t>& GetNearbyShareTestSampleSignature() {
  static const base::NoDestructor<std::vector<uint8_t>> signature(
      std::begin(kTestSampleSignature), std::end(kTestSampleSignature));
  return *signature;
}

const std::vector<uint8_t>& GetNearbyShareTestPayloadHashUsingSecretKey() {
  static const base::NoDestructor<std::vector<uint8_t>> hash(
      std::begin(kTestPayloadHashUsingSecretKey),
      std::end(kTestPayloadHashUsingSecretKey));
  return *hash;
}

NearbySharePrivateCertificate GetNearbyShareTestPrivateCertificate(
    nearby_share::mojom::Visibility visibility,
    base::Time not_before) {
  NearbySharePrivateCertificate cert(
      visibility, not_before,
      not_before + kNearbyShareCertificateValidityPeriod,
      GetNearbyShareTestP256KeyPair(), GetNearbyShareTestSecretKey(),
      GetNearbyShareTestMetadataEncryptionKey(),
      GetNearbyShareTestCertificateId(), GetNearbyShareTestMetadata(),
      /*consumed_salts=*/std::set<std::vector<uint8_t>>());
  cert.next_salts_for_testing().push(GetNearbyShareTestSalt());
  return cert;
}

nearby::sharing::proto::PublicCertificate GetNearbyShareTestPublicCertificate(
    nearby_share::mojom::Visibility visibility,
    base::Time not_before) {
  nearby::sharing::proto::PublicCertificate cert;
  cert.set_secret_id(std::string(GetNearbyShareTestCertificateId().begin(),
                                 GetNearbyShareTestCertificateId().end()));
  cert.set_secret_key(GetNearbyShareTestSecretKey()->key());
  cert.set_public_key(std::string(GetNearbyShareTestP256PublicKey().begin(),
                                  GetNearbyShareTestP256PublicKey().end()));
  cert.mutable_start_time()->set_seconds(
      (not_before - GetNearbyShareTestValidityOffset())
          .InMillisecondsSinceUnixEpoch() /
      1000);
  cert.mutable_end_time()->set_seconds((not_before +
                                        kNearbyShareCertificateValidityPeriod +
                                        GetNearbyShareTestValidityOffset())
                                           .InMillisecondsSinceUnixEpoch() /
                                       1000);
  cert.set_for_selected_contacts(
      visibility == nearby_share::mojom::Visibility::kSelectedContacts);
  cert.set_metadata_encryption_key(
      std::string(GetNearbyShareTestMetadataEncryptionKey().begin(),
                  GetNearbyShareTestMetadataEncryptionKey().end()));
  cert.set_encrypted_metadata_bytes(
      std::string(GetNearbyShareTestEncryptedMetadata().begin(),
                  GetNearbyShareTestEncryptedMetadata().end()));
  cert.set_metadata_encryption_key_tag(
      std::string(GetNearbyShareTestMetadataEncryptionKeyTag().begin(),
                  GetNearbyShareTestMetadataEncryptionKeyTag().end()));
  return cert;
}

std::vector<NearbySharePrivateCertificate>
GetNearbyShareTestPrivateCertificateList(
    nearby_share::mojom::Visibility visibility) {
  std::vector<NearbySharePrivateCertificate> list;
  for (size_t i = 0; i < kNearbyShareNumPrivateCertificates; ++i) {
    list.push_back(GetNearbyShareTestPrivateCertificate(
        visibility, GetNearbyShareTestNotBefore() +
                        i * kNearbyShareCertificateValidityPeriod));
  }
  return list;
}

std::vector<nearby::sharing::proto::PublicCertificate>
GetNearbyShareTestPublicCertificateList(
    nearby_share::mojom::Visibility visibility) {
  std::vector<nearby::sharing::proto::PublicCertificate> list;
  for (size_t i = 0; i < kNearbyShareNumPrivateCertificates; ++i) {
    list.push_back(GetNearbyShareTestPublicCertificate(
        visibility, GetNearbyShareTestNotBefore() +
                        i * kNearbyShareCertificateValidityPeriod));
  }
  return list;
}

const NearbyShareDecryptedPublicCertificate&
GetNearbyShareTestDecryptedPublicCertificate() {
  static const base::NoDestructor<NearbyShareDecryptedPublicCertificate> cert(
      *NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
          GetNearbyShareTestPublicCertificate(
              nearby_share::mojom::Visibility::kAllContacts),
          GetNearbyShareTestEncryptedMetadataKey()));
  return *cert;
}
