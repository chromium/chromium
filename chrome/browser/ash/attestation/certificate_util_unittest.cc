// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/certificate_util.h"

#include <string>

#include "base/time/time.h"
#include "chromeos/ash/components/attestation/fake_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace attestation {

constexpr base::TimeDelta kOneDay = base::Days(1);
constexpr base::TimeDelta kOneDayBefore = base::Days(-1);
constexpr base::TimeDelta kExpiryTreshold = kOneDay;

TEST(CheckCertificateExpiryTest, CertificateValid) {
  std::string valid_certificate;
  ASSERT_TRUE(
      GetFakeCertificatePEM(/*expiry=*/2 * kOneDay, &valid_certificate));

  EXPECT_EQ(CheckCertificateExpiry(valid_certificate, kExpiryTreshold),
            CertificateExpiryStatus::kValid);
}

TEST(CheckCertificateExpiryTest, CertificateExpiresSoon) {
  std::string expiring_soon_certificate;
  ASSERT_TRUE(GetFakeCertificatePEM(/*expiry=*/kOneDay / 2,
                                    &expiring_soon_certificate));

  EXPECT_EQ(CheckCertificateExpiry(expiring_soon_certificate, kExpiryTreshold),
            CertificateExpiryStatus::kExpiringSoon);
}

TEST(CheckCertificateExpiryTest, IntermediateCertificateExpiresSoon) {
  std::string valid_certificate;
  ASSERT_TRUE(
      GetFakeCertificatePEM(/*expiry=*/2 * kOneDay, &valid_certificate));

  std::string expiring_soon_certificate;
  ASSERT_TRUE(GetFakeCertificatePEM(/*expiry=*/kOneDay / 2,
                                    &expiring_soon_certificate));

  const std::string certificate_chain =
      valid_certificate + expiring_soon_certificate + valid_certificate;

  EXPECT_EQ(CheckCertificateExpiry(certificate_chain, kExpiryTreshold),
            CertificateExpiryStatus::kExpiringSoon);
}

TEST(CheckCertificateExpiryTest, CertificateExpired) {
  std::string expired_certificate;
  ASSERT_TRUE(
      GetFakeCertificatePEM(/*expiry=*/kOneDayBefore, &expired_certificate));

  EXPECT_EQ(CheckCertificateExpiry(expired_certificate, kExpiryTreshold),
            CertificateExpiryStatus::kExpired);
}

TEST(CheckCertificateExpiryTest, IntermediateCertificateExpired) {
  std::string valid_certificate;
  ASSERT_TRUE(
      GetFakeCertificatePEM(/*expiry=*/2 * kOneDay, &valid_certificate));

  std::string expired_certificate;
  ASSERT_TRUE(
      GetFakeCertificatePEM(/*expiry=*/kOneDayBefore, &expired_certificate));

  const std::string certificate_chain =
      valid_certificate + expired_certificate + valid_certificate;

  EXPECT_EQ(CheckCertificateExpiry(certificate_chain, kExpiryTreshold),
            CertificateExpiryStatus::kExpired);
}

TEST(CheckCertificateExpiryTest, CertificateEmpty) {
  std::string empty_certificate;

  EXPECT_EQ(CheckCertificateExpiry(empty_certificate, kExpiryTreshold),
            CertificateExpiryStatus::kInvalidPemChain);
}

TEST(CheckCertificateExpiryTest, CertificateInvalidPemChain) {
  std::string invalid_pem_certificate = "invalid_pem";

  EXPECT_EQ(CheckCertificateExpiry(invalid_pem_certificate, kExpiryTreshold),
            CertificateExpiryStatus::kInvalidPemChain);
}

TEST(CheckCertificateExpiryTest, CertificateInvalidX509) {
  std::string not_x509_certificate =
      "-----BEGIN CERTIFICATE-----\n"
      "Vm0wd2QyUXlWa1pOVldoVFYwZDRWVll3WkRSV1JteFZVMjA1VjFadGVEQmFWVll3WVd4YWMx"
      "TnNiRlZXYkhCUVdWZHplRll5VGtWUwpiSEJPVWpKb1RWZFhkR0ZUTWs1eVRsWmtZUXBTYlZK"
      "d1ZXcEtiMDFzWkZkV2JVWlVZbFpHTTFSc1dsZFZaM0JwVTBWS2RsWkdZM2hpCk1rbDRWMnhX"
      "VkdGc1NsaFpiRnBIVGtaYVNFNVZkRmRhTTBKd1ZteGFkMVpXWkZobFIzUnBDazFXY0VoV01X"
      "aHpZV3hLV1ZWc1ZscGkKUm5Cb1dsZDRXbVZWTlZkYVIyaFdWMFZLVlZacVFsZFRNVnBYV2ta"
      "b2JGSXpVbGREYlVwWFYydG9WMDF1VW5aWmExcExZMnMxVjFScwpjRmdLVTBWS1dWWnRjRWRq"
      "TWs1elYyNVNVRll5YUZkV01GWkxWbXhhVlZGc1pGUk5Wa3BJVmpKNGIyRnNTbGxWYkVKRVlr"
      "VndWbFZ0CmVHOVdNVWw2WVVkb1dGWnNjRXhXTUZwWFpGWk9jd3BhUjJkTFdWUkNkMDVzV2to"
      "TlZGSmFWbTFTUjFSV1ZsZFdNa3BKVVd4a1YwMUcKV2t4V01uaGhWMGRXU0dSRk9WTk5WWEJa"
      "Vm1wR2IySXhXblJTV0hCV1lrWktSVmxZY0VkbGJGbDVDbU5GVGxkTlZtdzJWbGMxWVZkdApS"
      "WGhqUlhSaFZucEdTRlZ0TVZOU2QzQmhVbTFPVEZkWGVGWmtNbEY0VjJ0V1UySkhVbFpVVjNS"
      "M1pXeFdXR1ZHWkZWaVJYQmFWa2QwCk5GSkdjRFlLVFVSc1JGcDZNRGxEWnowOUNnPT0K\n"
      "-----END CERTIFICATE-----\n";
  EXPECT_EQ(CheckCertificateExpiry(not_x509_certificate, kExpiryTreshold),
            CertificateExpiryStatus::kInvalidX509);
}

}  // namespace attestation
}  // namespace ash
