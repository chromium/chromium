// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/nearby_share_decrypted_public_certificate.h"

#include <optional>

#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "chrome/browser/nearby_sharing/certificates/test_util.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

namespace {

// The for_selected_contacts field of a public certificate proto is irrelevant
// for remote device certificates. Even if set, it is meaningless. It only has
// meaning for private certificates converted to public certificates and
// uploaded to the Nearby server.
const nearby_share::mojom::Visibility kTestPublicCertificateVisibility =
    nearby_share::mojom::Visibility::kNoOne;

}  // namespace

TEST(NearbyShareDecryptedPublicCertificateTest, Decrypt) {
  nearby::sharing::proto::PublicCertificate proto_cert =
      GetNearbyShareTestPublicCertificate(kTestPublicCertificateVisibility);
  proto_cert.set_for_self_share(true);

  std::optional<NearbyShareDecryptedPublicCertificate> cert =
      NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
          proto_cert, GetNearbyShareTestEncryptedMetadataKey());
  EXPECT_TRUE(cert);
  EXPECT_EQ(
      base::Time::FromSecondsSinceUnixEpoch(proto_cert.start_time().seconds()),
      cert->not_before());
  EXPECT_EQ(
      base::Time::FromSecondsSinceUnixEpoch(proto_cert.end_time().seconds()),
      cert->not_after());
  EXPECT_EQ(std::vector<uint8_t>(proto_cert.secret_id().begin(),
                                 proto_cert.secret_id().end()),
            cert->id());
  EXPECT_EQ(GetNearbyShareTestMetadata().SerializeAsString(),
            cert->unencrypted_metadata().SerializeAsString());
  EXPECT_EQ(proto_cert.for_self_share(), cert->for_self_share());
}

TEST(NearbyShareDecryptedPublicCertificateTest, Decrypt_IncorrectKeyFailure) {
  // Input incorrect metadata encryption key.
  EXPECT_FALSE(NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
      GetNearbyShareTestPublicCertificate(kTestPublicCertificateVisibility),
      NearbyShareEncryptedMetadataKey(
          std::vector<uint8_t>(kNearbyShareNumBytesMetadataEncryptionKeySalt,
                               0x00),
          std::vector<uint8_t>(kNearbyShareNumBytesMetadataEncryptionKey,
                               0x00))));
}

TEST(NearbyShareDecryptedPublicCertificateTest,
     Decrypt_MetadataDecryptionFailure) {
  // Use metadata that cannot be decrypted with the given key.
  nearby::sharing::proto::PublicCertificate proto_cert =
      GetNearbyShareTestPublicCertificate(kTestPublicCertificateVisibility);
  proto_cert.set_encrypted_metadata_bytes("invalid metadata");
  EXPECT_FALSE(NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
      proto_cert, GetNearbyShareTestEncryptedMetadataKey()));
}

TEST(NearbyShareDecryptedPublicCertificateTest, Decrypt_InvalidDataFailure) {
  // Do not accept the input PublicCertificate because the validity period does
  // not make sense.
  nearby::sharing::proto::PublicCertificate proto_cert =
      GetNearbyShareTestPublicCertificate(kTestPublicCertificateVisibility);
  proto_cert.mutable_end_time()->set_seconds(proto_cert.start_time().seconds() -
                                             1);
  EXPECT_FALSE(NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
      proto_cert, GetNearbyShareTestEncryptedMetadataKey()));
}

TEST(NearbyShareDecryptedPublicCertificateTest, Verify) {
  std::optional<NearbyShareDecryptedPublicCertificate> cert =
      NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
          GetNearbyShareTestPublicCertificate(kTestPublicCertificateVisibility),
          GetNearbyShareTestEncryptedMetadataKey());
  EXPECT_TRUE(cert->VerifySignature(GetNearbyShareTestPayloadToSign(),
                                    GetNearbyShareTestSampleSignature()));
}

TEST(NearbyShareDecryptedPublicCertificateTest, Verify_InitFailure) {
  // Public key has invalid SubjectPublicKeyInfo format.
  nearby::sharing::proto::PublicCertificate proto_cert =
      GetNearbyShareTestPublicCertificate(kTestPublicCertificateVisibility);
  proto_cert.set_public_key("invalid public key");

  std::optional<NearbyShareDecryptedPublicCertificate> cert =
      NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
          proto_cert, GetNearbyShareTestEncryptedMetadataKey());
  ASSERT_TRUE(cert);
  EXPECT_FALSE(cert->VerifySignature(GetNearbyShareTestPayloadToSign(),
                                     GetNearbyShareTestSampleSignature()));
}

TEST(NearbyShareDecryptedPublicCertificateTest, Verify_WrongSignature) {
  std::optional<NearbyShareDecryptedPublicCertificate> cert =
      NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
          GetNearbyShareTestPublicCertificate(kTestPublicCertificateVisibility),
          GetNearbyShareTestEncryptedMetadataKey());
  EXPECT_FALSE(
      cert->VerifySignature(GetNearbyShareTestPayloadToSign(),
                            /*signature=*/base::span<const uint8_t>()));
}

TEST(NearbyShareDecryptedPublicCertificateTest, HashAuthenticationToken) {
  std::optional<NearbyShareDecryptedPublicCertificate> cert =
      NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
          GetNearbyShareTestPublicCertificate(kTestPublicCertificateVisibility),
          GetNearbyShareTestEncryptedMetadataKey());
  EXPECT_EQ(GetNearbyShareTestPayloadHashUsingSecretKey(),
            cert->HashAuthenticationToken(GetNearbyShareTestPayloadToSign()));
}
