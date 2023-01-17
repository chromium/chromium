// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/common.h"

#include "base/containers/span.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_private_certificate.h"
#include "chrome/browser/nearby_sharing/certificates/test_util.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(NearbyShareCertificatesCommonTest, AuthenticationTokenHash) {
  EXPECT_EQ(GetNearbyShareTestPayloadHashUsingSecretKey(),
            ComputeAuthenticationTokenHash(
                GetNearbyShareTestPayloadToSign(),
                base::as_bytes(
                    base::make_span(GetNearbyShareTestSecretKey()->key()))));
}

TEST(NearbyShareCertificatesCommonTest, ValidityPeriod_PrivateCertificate) {
  NearbySharePrivateCertificate cert = GetNearbyShareTestPrivateCertificate(
      nearby_share::mojom::Visibility::kAllContacts);
  const bool use_public_certificate_tolerance = false;

  // Set time before validity period.
  base::Time now = cert.not_before() - base::Milliseconds(1);
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_FALSE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time at inclusive lower bound of validity period.
  now = cert.not_before();
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_TRUE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time in the middle of the validity period.
  now = cert.not_before() + (cert.not_after() - cert.not_before()) / 2;
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_TRUE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time at non-inclusive upper bound of validity period.
  now = cert.not_after();
  EXPECT_TRUE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_FALSE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time after validity period.
  now = cert.not_after() + base::Milliseconds(1);
  EXPECT_TRUE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_FALSE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));
}

TEST(NearbyShareCertificatesCommonTest, ValidityPeriod_PublicCertificate) {
  NearbyShareDecryptedPublicCertificate cert =
      *NearbyShareDecryptedPublicCertificate::DecryptPublicCertificate(
          GetNearbyShareTestPublicCertificate(
              nearby_share::mojom::Visibility::kAllContacts),
          GetNearbyShareTestEncryptedMetadataKey());
  const bool use_public_certificate_tolerance = true;

  // Set time before validity period, outside of tolerance.
  base::Time now = cert.not_before() -
                   kNearbySharePublicCertificateValidityBoundOffsetTolerance -
                   base::Milliseconds(1);
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_FALSE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time before validity period, at inclusive bound with tolerance.
  now = cert.not_before() -
        kNearbySharePublicCertificateValidityBoundOffsetTolerance;
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_TRUE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time before validity period, inside of tolerance.
  now = cert.not_before() -
        kNearbySharePublicCertificateValidityBoundOffsetTolerance / 2;
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_TRUE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time at inclusive lower bound of validity period.
  now = cert.not_before();
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_TRUE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time in the middle of the validity period.
  now = cert.not_before() + (cert.not_after() - cert.not_before()) / 2;
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_TRUE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time at upper bound of validity period.
  now = cert.not_after();
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_TRUE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time after validity period, inside of tolerance.
  now = cert.not_after() +
        kNearbySharePublicCertificateValidityBoundOffsetTolerance / 2;
  EXPECT_FALSE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_TRUE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time after validity period, at non-inclusive tolerance bound.
  now = cert.not_after() +
        kNearbySharePublicCertificateValidityBoundOffsetTolerance;
  EXPECT_TRUE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_FALSE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));

  // Set time after validity period, outside of tolerance.
  now = cert.not_after() +
        kNearbySharePublicCertificateValidityBoundOffsetTolerance +
        base::Milliseconds(1);
  EXPECT_TRUE(IsNearbyShareCertificateExpired(
      now, cert.not_after(), use_public_certificate_tolerance));
  EXPECT_FALSE(IsNearbyShareCertificateWithinValidityPeriod(
      now, cert.not_before(), cert.not_after(),
      use_public_certificate_tolerance));
}
