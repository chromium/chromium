// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/nearby_share_private_certificate.h"

#include <optional>
#include <string>

#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "chrome/browser/nearby_sharing/certificates/test_util.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

TEST(NearbySharePrivateCertificateTest, Construction) {
  NearbySharePrivateCertificate private_certificate(
      nearby_share::mojom::Visibility::kAllContacts,
      GetNearbyShareTestNotBefore(), GetNearbyShareTestMetadata());
  EXPECT_EQ(kNearbyShareNumBytesCertificateId, private_certificate.id().size());
  EXPECT_EQ(nearby_share::mojom::Visibility::kAllContacts,
            private_certificate.visibility());
  EXPECT_EQ(GetNearbyShareTestNotBefore(), private_certificate.not_before());
  EXPECT_EQ(
      GetNearbyShareTestNotBefore() + kNearbyShareCertificateValidityPeriod,
      private_certificate.not_after());
  EXPECT_EQ(GetNearbyShareTestMetadata().SerializeAsString(),
            private_certificate.unencrypted_metadata().SerializeAsString());
}

TEST(NearbySharePrivateCertificateTest, ToFromDictionary) {
  NearbySharePrivateCertificate before(
      nearby_share::mojom::Visibility::kAllContacts,
      GetNearbyShareTestNotBefore(), GetNearbyShareTestMetadata());
  // Generate a few consumed salts.
  for (size_t i = 0; i < 5; ++i)
    ASSERT_TRUE(before.EncryptMetadataKey());

  NearbySharePrivateCertificate after(
      *NearbySharePrivateCertificate::FromDictionary(before.ToDictionary()));

  EXPECT_EQ(before.id(), after.id());
  EXPECT_EQ(before.visibility(), after.visibility());
  EXPECT_EQ(before.not_before(), after.not_before());
  EXPECT_EQ(before.not_after(), after.not_after());
  EXPECT_EQ(before.unencrypted_metadata().SerializeAsString(),
            after.unencrypted_metadata().SerializeAsString());
  EXPECT_EQ(before.secret_key_->key(), after.secret_key_->key());
  EXPECT_EQ(before.metadata_encryption_key_, after.metadata_encryption_key_);
  EXPECT_EQ(before.consumed_salts_, after.consumed_salts_);

  std::vector<uint8_t> before_private_key, after_private_key;
  before.key_pair_->ExportPrivateKey(&before_private_key);
  after.key_pair_->ExportPrivateKey(&after_private_key);
  EXPECT_EQ(before_private_key, after_private_key);
}

TEST(NearbySharePrivateCertificateTest, EncryptMetadataKey) {
  NearbySharePrivateCertificate private_certificate(
      nearby_share::mojom::Visibility::kAllContacts,
      GetNearbyShareTestNotBefore(), GetNearbyShareTestMetadata());
  std::optional<NearbyShareEncryptedMetadataKey> encrypted_metadata_key =
      private_certificate.EncryptMetadataKey();
  ASSERT_TRUE(encrypted_metadata_key);
  EXPECT_EQ(kNearbyShareNumBytesMetadataEncryptionKeySalt,
            encrypted_metadata_key->salt().size());
  EXPECT_EQ(kNearbyShareNumBytesMetadataEncryptionKey,
            encrypted_metadata_key->encrypted_key().size());
}

TEST(NearbySharePrivateCertificateTest, EncryptMetadataKey_FixedData) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          nearby_share::mojom::Visibility::kAllContacts);
  std::optional<NearbyShareEncryptedMetadataKey> encrypted_metadata_key =
      private_certificate.EncryptMetadataKey();
  EXPECT_EQ(GetNearbyShareTestEncryptedMetadataKey().encrypted_key(),
            encrypted_metadata_key->encrypted_key());
  EXPECT_EQ(GetNearbyShareTestEncryptedMetadataKey().salt(),
            encrypted_metadata_key->salt());
}

TEST(NearbySharePrivateCertificateTest,
     EncryptMetadataKey_SaltsExhaustedFailure) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          nearby_share::mojom::Visibility::kAllContacts);
  for (size_t i = 0; i < kNearbyShareMaxNumMetadataEncryptionKeySalts; ++i) {
    EXPECT_TRUE(private_certificate.EncryptMetadataKey());
  }
  EXPECT_FALSE(private_certificate.EncryptMetadataKey());
}

TEST(NearbySharePrivateCertificateTest,
     EncryptMetadataKey_TooManySaltGenerationRetriesFailure) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          nearby_share::mojom::Visibility::kAllContacts);
  EXPECT_TRUE(private_certificate.EncryptMetadataKey());
  while (private_certificate.next_salts_for_testing().size() <
         kNearbyShareMaxNumMetadataEncryptionKeySaltGenerationRetries) {
    private_certificate.next_salts_for_testing().push(GetNearbyShareTestSalt());
  }
  EXPECT_FALSE(private_certificate.EncryptMetadataKey());
}

TEST(NearbySharePrivateCertificateTest, PublicCertificateConversion) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          nearby_share::mojom::Visibility::kSelectedContacts);
  private_certificate.offset_for_testing() = GetNearbyShareTestValidityOffset();
  std::optional<nearby::sharing::proto::PublicCertificate> public_certificate =
      private_certificate.ToPublicCertificate();
  ASSERT_TRUE(public_certificate);
  EXPECT_EQ(GetNearbyShareTestPublicCertificate(
                nearby_share::mojom::Visibility::kSelectedContacts)
                .SerializeAsString(),
            public_certificate->SerializeAsString());
}

TEST(NearbySharePrivateCertificateTest, EncryptDecryptRoundtrip) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          nearby_share::mojom::Visibility::kAllContacts);

  std::optional<NearbyShareDecryptedPublicCertificate>
      decrypted_public_certificate =
          NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
              *private_certificate.ToPublicCertificate(),
              *private_certificate.EncryptMetadataKey());
  ASSERT_TRUE(decrypted_public_certificate);
  EXPECT_EQ(
      private_certificate.unencrypted_metadata().SerializeAsString(),
      decrypted_public_certificate->unencrypted_metadata().SerializeAsString());
}

TEST(NearbySharePrivateCertificateTest, SignVerifyRoundtrip) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          nearby_share::mojom::Visibility::kAllContacts);
  std::optional<std::vector<uint8_t>> signature =
      private_certificate.Sign(GetNearbyShareTestPayloadToSign());
  ASSERT_TRUE(signature);

  std::optional<NearbyShareDecryptedPublicCertificate>
      decrypted_public_certificate =
          NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
              *private_certificate.ToPublicCertificate(),
              *private_certificate.EncryptMetadataKey());
  EXPECT_TRUE(decrypted_public_certificate->VerifySignature(
      GetNearbyShareTestPayloadToSign(), *signature));
}

TEST(NearbySharePrivateCertificateTest, HashAuthenticationToken) {
  NearbySharePrivateCertificate private_certificate =
      GetNearbyShareTestPrivateCertificate(
          nearby_share::mojom::Visibility::kAllContacts);
  EXPECT_EQ(GetNearbyShareTestPayloadHashUsingSecretKey(),
            private_certificate.HashAuthenticationToken(
                GetNearbyShareTestPayloadToSign()));
}
