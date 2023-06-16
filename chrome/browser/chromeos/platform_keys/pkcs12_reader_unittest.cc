// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/chromeos/platform_keys/pkcs12_reader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/x509.h"

namespace chromeos::platform_keys {
namespace {

const char kPkcs12FilePassword[] = "12345";

// Custom X509 object creation allows to avoid calls to X509_free()
// after every test where X509 objects are required.
struct X509Deleter {
  void operator()(X509* cert) { X509_free(cert); }
};
using ScopedX509 = std::unique_ptr<X509, X509Deleter>;
ScopedX509 X509New() {
  return ScopedX509(X509_new());
}

// Custom X509_NAME object creation allows to avoid calls to X509_NAME_free()
// after every test where X509_NAME objects are required.
struct X509NameDeleter {
  void operator()(X509_NAME* name) { X509_NAME_free(name); }
};
using ScopedX509_NAME = std::unique_ptr<X509_NAME, X509NameDeleter>;
ScopedX509_NAME X509NameNew() {
  return ScopedX509_NAME(X509_NAME_new());
}

// Custom BIGNUM object with object's deleter after the test is finished.
struct BIGNUMDeleter {
  void operator()(BIGNUM* num) { BN_free(num); }
};
using ScopedBIGNUM = std::unique_ptr<BIGNUM, BIGNUMDeleter>;
ScopedBIGNUM BIGNUMNew() {
  return ScopedBIGNUM(BN_new());
}

// Tests for testing methods in chaps_util_helper.cc
// ChapsUtilImplTest is testing successful import and values, these tests
// are mainly checking errors handling.
class Pkcs12ReaderTest : public ::testing::Test {
 public:
  Pkcs12ReaderTest() { pkcs12Reader_ = std::make_unique<Pkcs12Reader>(); }
  Pkcs12ReaderTest(const Pkcs12ReaderTest&) = delete;
  Pkcs12ReaderTest& operator=(const Pkcs12ReaderTest&) = delete;
  ~Pkcs12ReaderTest() override = default;

  Pkcs12ReaderStatusCode GetSerialNumberDer(X509* cert) {
    int serial_number_der_size;
    bssl::UniquePtr<uint8_t> serial_number_der;
    return pkcs12Reader_->GetSerialNumberDer(cert, serial_number_der,
                                             serial_number_der_size);
  }

  Pkcs12ReaderStatusCode GetIssuerNameDer(X509* cert) {
    base::span<const uint8_t> issuer_name_data;
    return pkcs12Reader_->GetIssuerNameDer(cert, issuer_name_data);
  }

  Pkcs12ReaderStatusCode GetSubjectNameDer(X509* cert) {
    base::span<const uint8_t> subject_name_data;
    return pkcs12Reader_->GetSubjectNameDer(cert, subject_name_data);
  }

  Pkcs12ReaderStatusCode GetDerEncodedCert(X509* cert) {
    int cert_der_size;
    bssl::UniquePtr<uint8_t> cert_der_ptr;
    return pkcs12Reader_->GetDerEncodedCert(cert, cert_der_ptr, cert_der_size);
  }

  Pkcs12ReaderStatusCode GetLabel(X509* cert) {
    std::string label;
    return pkcs12Reader_->GetLabel(cert, label);
  }

  void SetFieldToX509Name(X509_NAME* X509_name,
                          const char field[],
                          unsigned char value[]) {
    X509_NAME_add_entry_by_txt(X509_name,
                               /*field=*/field,
                               /*type=*/MBSTRING_ASC,
                               /*bytes=*/value,
                               /*len=*/-1,
                               /*loc=*/-1,
                               /*set=*/0);
  }

  void SetOrgDataToX509Name(X509_NAME* X509_name) {
    // Country
    unsigned char country_name[] = "DE";
    SetFieldToX509Name(X509_name, "C", country_name);

    // Company/Organization
    unsigned char org_name[] = "Test company";
    SetFieldToX509Name(X509_name, "O", org_name);

    // Common name
    unsigned char common_name[] = "common_name";
    SetFieldToX509Name(X509_name, "CN", common_name);
  }

 protected:
  std::unique_ptr<Pkcs12Reader> pkcs12Reader_;
};

TEST_F(Pkcs12ReaderTest, EmptyBigNumReturnsEmptyVector) {
  ScopedBIGNUM bignum = BIGNUMNew();
  BN_zero(bignum.get());
  std::vector<uint8_t> expected_empty_vector({});

  EXPECT_EQ(pkcs12Reader_->BignumToBytes(bignum.get()), expected_empty_vector);
}

TEST_F(Pkcs12ReaderTest, MaxBigNumConvertedCorrectly) {
  ScopedBIGNUM bignum = BIGNUMNew();
  BN_set_u64(bignum.get(), 0xFFFFFFFFFFFFFFFF);
  std::vector<uint8_t> expected_data({
      0xFF,
      0xFF,
      0xFF,
      0xFF,
      0xFF,
      0xFF,
      0xFF,
      0xFF,
  });

  std::vector<uint8_t> bignumToBytes =
      pkcs12Reader_->BignumToBytes(bignum.get());

  EXPECT_EQ(bignumToBytes, expected_data);
}

TEST_F(Pkcs12ReaderTest, BigNumZeroConvertedToEmptyVector) {
  ScopedBIGNUM bignum = BIGNUMNew();
  BN_set_u64(bignum.get(), 0x00000000000000);
  std::vector<uint8_t> expected_data({});

  std::vector<uint8_t> bignumToBytes =
      pkcs12Reader_->BignumToBytes(bignum.get());

  EXPECT_EQ(bignumToBytes, expected_data);
}

TEST_F(Pkcs12ReaderTest, BigNumWithFrontZerosConvertedCorrectly) {
  ScopedBIGNUM bignum = BIGNUMNew();
  BN_set_u64(bignum.get(), 0x00000000000100);
  std::vector<uint8_t> expected_data({0x01, 0x00});

  std::vector<uint8_t> bignumToBytes =
      pkcs12Reader_->BignumToBytes(bignum.get());

  EXPECT_EQ(bignumToBytes, expected_data);
}

TEST_F(Pkcs12ReaderTest, EmptyBigNumConvertedCorrectly) {
  ScopedBIGNUM bignum = BIGNUMNew();
  std::vector<uint8_t> expected_data({});

  std::vector<uint8_t> bignumToBytes =
      pkcs12Reader_->BignumToBytes(bignum.get());

  EXPECT_EQ(bignumToBytes, expected_data);
}

TEST_F(Pkcs12ReaderTest, CertsGetSerialNumber) {
  // Empty certificate, operation will fail.
  {
    X509* cert = nullptr;

    Pkcs12ReaderStatusCode result = GetSerialNumberDer(cert);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kPkcs12CertSerialNumberMissed);
  }

  // Empty serial number, operation will succeed.
  {
    ScopedX509 cert = X509New();

    Pkcs12ReaderStatusCode result = GetSerialNumberDer(cert.get());
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kSuccess);
  }

  // Certificate with normal serial number, operation will succeed.
  // Check only import success, values are checked in ChapsUtilImplTest.
  {
    ScopedX509 cert = X509New();
    ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1);
    int serial_number_der_size;
    bssl::UniquePtr<uint8_t> serial_number_der;

    Pkcs12ReaderStatusCode result = pkcs12Reader_->GetSerialNumberDer(
        cert.get(), serial_number_der, serial_number_der_size);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kSuccess);
  }
}

TEST_F(Pkcs12ReaderTest, GetIssuerNameDer) {
  // Empty certificate, operation will fail.
  {
    Pkcs12ReaderStatusCode result = GetIssuerNameDer(nullptr);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kPkcs12CertIssuerNameMissed);
  }

  // Empty object for the issuer, operation will succeed.
  {
    ScopedX509 cert = X509New();

    Pkcs12ReaderStatusCode result = GetIssuerNameDer(cert.get());
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kSuccess);
  }

  // Certificate with normal issuer name, operation will succeed.
  // Check only import success, values are checked in ChapsUtilImplTest.
  {
    ScopedX509 cert = X509New();
    ScopedX509_NAME issuer = X509NameNew();

    // This only sets org name, country and common name.
    SetOrgDataToX509Name(issuer.get());
    X509_set_issuer_name(cert.get(), issuer.get());
    base::span<const uint8_t> issuer_name_data;

    Pkcs12ReaderStatusCode result =
        pkcs12Reader_->GetIssuerNameDer(cert.get(), issuer_name_data);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kSuccess);
  }
}

TEST_F(Pkcs12ReaderTest, GetSubjectNameDer) {
  // Empty certificate, operation will fail.
  {
    Pkcs12ReaderStatusCode result = GetSubjectNameDer(nullptr);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kPkcs12CertSubjectNameMissed);
  }

  // Empty object for the subject name, operation will succeed.
  {
    ScopedX509 cert = X509New();

    Pkcs12ReaderStatusCode result = GetIssuerNameDer(cert.get());
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kSuccess);
  }

  // Certificate with normal subject name, operation will succeed.
  // Check only import success, values are checked in ChapsUtilImplTest.
  {
    ScopedX509 cert = X509New();
    ScopedX509_NAME subject = X509NameNew();

    // This only sets org name, country and common name.
    SetOrgDataToX509Name(subject.get());
    X509_set_subject_name(cert.get(), subject.get());
    base::span<const uint8_t> subject_name_data;

    Pkcs12ReaderStatusCode result =
        pkcs12Reader_->GetSubjectNameDer(cert.get(), subject_name_data);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kSuccess);
  }
}

TEST_F(Pkcs12ReaderTest, GetCertDer) {
  // No certificate, operation will fail.
  {
    Pkcs12ReaderStatusCode result = GetDerEncodedCert(nullptr);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kPkcs12CertDerMissed);
  }

  // Empty certificate, operation will fail.
  {
    ScopedX509 cert = X509New();

    Pkcs12ReaderStatusCode result = GetDerEncodedCert(cert.get());
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kPkcs12CertDerFailed);
  }
}

TEST_F(Pkcs12ReaderTest, GetPkcs12KeyAndCerts) {
  // No pkcs12 data, operation will fail.
  {
    bssl::UniquePtr<EVP_PKEY> key;
    bssl::UniquePtr<STACK_OF(X509)> certs;
    const std::vector<uint8_t>& pkcs12_data = {};

    Pkcs12ReaderStatusCode result = pkcs12Reader_->GetPkcs12KeyAndCerts(
        pkcs12_data, kPkcs12FilePassword, key, certs);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kMissedPkcs12Data);
  }

  // Wrong pkcs12 data's, operation will fail.
  {
    bssl::UniquePtr<EVP_PKEY> key;
    bssl::UniquePtr<STACK_OF(X509)> certs;
    const std::vector<uint8_t>& wrong_pkcs12_data = {0, 0, 0, 0, 0,
                                                     0, 0, 0, 0, 0};

    Pkcs12ReaderStatusCode result = pkcs12Reader_->GetPkcs12KeyAndCerts(
        wrong_pkcs12_data, kPkcs12FilePassword, key, certs);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kFailedToParsePkcs12Data);
  }
}

TEST_F(Pkcs12ReaderTest, GetLabel) {
  // Empty certificate, operation will fail.
  {
    Pkcs12ReaderStatusCode result = GetLabel(nullptr);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kPkcs12CertIssuerNameMissed);
  }

  // Empty object for the issuer, operation will succeed.
  {
    ScopedX509 cert = X509New();

    Pkcs12ReaderStatusCode result = GetLabel(cert.get());
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kSuccess);
  }

  // Certificate with normal issuer name, operation will succeed.
  // Check only import success, values are checked in ChapsUtilImplTest.
  {
    ScopedX509 cert = X509New();
    ScopedX509_NAME subject = X509NameNew();

    // This only sets org name, country and common name.
    SetOrgDataToX509Name(subject.get());
    X509_set_subject_name(cert.get(), subject.get());
    std::string label;

    Pkcs12ReaderStatusCode result = pkcs12Reader_->GetLabel(cert.get(), label);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kSuccess);
  }
}

}  // namespace
}  // namespace chromeos::platform_keys
