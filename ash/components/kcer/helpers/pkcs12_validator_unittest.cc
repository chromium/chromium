// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/components/kcer/helpers/pkcs12_validator.h"

#include "ash/components/kcer/kcer_nss/test_utils.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace kcer::internal {
namespace {

std::string GetPassword(const std::string& file_name) {
  if (file_name == "client.p12") {
    return "12345";
  }
  if (file_name == "client_with_ec_key.p12") {
    return "123456";
  }
  ADD_FAILURE() << "GetPassword() is called with an unexpected file name";
  return "";
}

scoped_refptr<const Cert> MakeKcerCert(
    const std::string& nickname,
    scoped_refptr<net::X509Certificate> cert) {
  // CertCache only cares about the `cert`, other fields are can be anything.
  return base::MakeRefCounted<Cert>(Token::kUser, Pkcs11Id(), nickname,
                                    std::move(cert));
}

scoped_refptr<const Cert> MakeKcerCertFromBsslCert(const std::string& nickname,
                                                   X509* cert) {
  int cert_der_size = 0;
  bssl::UniquePtr<uint8_t> cert_der;
  Pkcs12Reader pkcs12_reader;
  Pkcs12ReaderStatusCode get_cert_der_result =
      pkcs12_reader.GetDerEncodedCert(cert, cert_der, cert_der_size);
  if (get_cert_der_result != Pkcs12ReaderStatusCode::kSuccess) {
    ADD_FAILURE() << "GetDerEncodedCert failed";
    return nullptr;
  }

  scoped_refptr<net::X509Certificate> x509_cert =
      net::X509Certificate::CreateFromBytes(
          base::make_span(cert_der.get(), size_t(cert_der_size)));
  return MakeKcerCert("name", x509_cert);
}

class KcerPkcs12ValidatorTest : public ::testing::Test {
 public:
  void GetData(const char* file_name,
               KeyData& key_data,
               bssl::UniquePtr<STACK_OF(X509)>& certs) {
    Pkcs12Blob pkcs12_blob(ReadTestFile(file_name));
    std::string password(GetPassword(file_name));

    Pkcs12ReaderStatusCode get_key_and_cert_status =
        pkcs12_reader_.GetPkcs12KeyAndCerts(pkcs12_blob.value(), password,
                                            key_data.key, certs);
    if (get_key_and_cert_status != Pkcs12ReaderStatusCode::kSuccess) {
      ADD_FAILURE() << "GetPkcs12KeyAndCerts failed";
      return;
    }

    if (pkcs12_reader_.EnrichKeyData(key_data) !=
        Pkcs12ReaderStatusCode::kSuccess) {
      ADD_FAILURE() << "EnrichKeyData failed";
      return;
    }
  }

  Pkcs12Reader pkcs12_reader_;
  CertCache cert_cache_;
};

// Test that ValidateAndPrepareCertData() returns success on validating a
// correct RSA PKCS#12 data and chooses the correct nickname.
TEST_F(KcerPkcs12ValidatorTest, RsaSuccess) {
  KeyData key_data;
  bssl::UniquePtr<STACK_OF(X509)> certs;
  ASSERT_NO_FATAL_FAILURE(GetData("client.p12", key_data, certs));

  std::vector<CertData> certs_data;
  Pkcs12ReaderStatusCode prepare_certs_status = ValidateAndPrepareCertData(
      cert_cache_, pkcs12_reader_, std::move(certs), key_data, certs_data);
  EXPECT_EQ(prepare_certs_status, Pkcs12ReaderStatusCode::kSuccess);
  ASSERT_EQ(certs_data.size(), 1u);
  // The file doesn't have the nickname set and there are no other certs to copy
  // it from, so the subject name should be used.
  EXPECT_EQ(certs_data[0].nickname, "testusercert");
}

// Test that ValidateAndPrepareCertData() returns success on validating a
// correct EC PKCS#12 data and chooses the correct nickname.
TEST_F(KcerPkcs12ValidatorTest, EcSuccess) {
  KeyData key_data;
  bssl::UniquePtr<STACK_OF(X509)> certs;
  ASSERT_NO_FATAL_FAILURE(GetData("client_with_ec_key.p12", key_data, certs));

  std::vector<CertData> certs_data;
  Pkcs12ReaderStatusCode prepare_certs_status = ValidateAndPrepareCertData(
      cert_cache_, pkcs12_reader_, std::move(certs), key_data, certs_data);
  EXPECT_EQ(prepare_certs_status, Pkcs12ReaderStatusCode::kSuccess);
  ASSERT_EQ(certs_data.size(), 1u);
  // There are no other certs to copy the nickname from, the file has the
  // nickname set and it should be used.
  EXPECT_EQ(certs_data[0].nickname, "serverkey");
}

// Test that ValidateAndPrepareCertData() correctly fails when the key and the
// certs are not related.
TEST_F(KcerPkcs12ValidatorTest, UnrelatedCert) {
  KeyData key_data_1;
  bssl::UniquePtr<STACK_OF(X509)> certs_1;
  ASSERT_NO_FATAL_FAILURE(GetData("client.p12", key_data_1, certs_1));

  KeyData key_data_2;
  bssl::UniquePtr<STACK_OF(X509)> certs_2;
  ASSERT_NO_FATAL_FAILURE(
      GetData("client_with_ec_key.p12", key_data_2, certs_2));

  std::vector<CertData> certs_data;
  Pkcs12ReaderStatusCode prepare_certs_status;

  // `certs_1` and `key_data_2` don't match and should fail.
  prepare_certs_status = ValidateAndPrepareCertData(
      cert_cache_, pkcs12_reader_, std::move(certs_1), key_data_2, certs_data);
  EXPECT_EQ(prepare_certs_status,
            Pkcs12ReaderStatusCode::kPkcs12NoValidCertificatesFound);

  // `certs_2` and `key_data_1` don't match and should fail.
  prepare_certs_status = ValidateAndPrepareCertData(
      cert_cache_, pkcs12_reader_, std::move(certs_2), key_data_1, certs_data);
  EXPECT_EQ(prepare_certs_status,
            Pkcs12ReaderStatusCode::kPkcs12NoValidCertificatesFound);
}

// Test that ValidateAndPrepareCertData() skips already imported keys and
// correctly fails if there's nothing new to import.
TEST_F(KcerPkcs12ValidatorTest, CertExists) {
  KeyData key_data;
  bssl::UniquePtr<STACK_OF(X509)> certs;
  ASSERT_NO_FATAL_FAILURE(GetData("client.p12", key_data, certs));

  X509* cert = sk_X509_value(certs.get(), 0);
  ASSERT_TRUE(cert);

  scoped_refptr<const Cert> kcer_cert = MakeKcerCertFromBsslCert("name", cert);
  cert_cache_ = CertCache(std::vector{kcer_cert});

  std::vector<CertData> certs_data;
  Pkcs12ReaderStatusCode prepare_certs_status = ValidateAndPrepareCertData(
      cert_cache_, pkcs12_reader_, std::move(certs), key_data, certs_data);
  EXPECT_EQ(prepare_certs_status, Pkcs12ReaderStatusCode::kAlreadyExists);
}

// Test that ValidateAndPrepareCertData() takes the nickname from an existing
// cert when it has the same subject name as the one for import. (This is
// implemented for backwards compatibility with NSS, potentially doesn't have to
// stay this way long term.)
TEST_F(KcerPkcs12ValidatorTest, NicknameFromExistingCert) {
  KeyData key_data;
  bssl::UniquePtr<STACK_OF(X509)> certs;
  ASSERT_NO_FATAL_FAILURE(GetData("client.p12", key_data, certs));

  X509* cert = sk_X509_value(certs.get(), 0);
  ASSERT_TRUE(cert);

  base::span<const uint8_t> subject_der;
  Pkcs12ReaderStatusCode get_subject_der_result =
      pkcs12_reader_.GetSubjectNameDer(cert, subject_der);
  ASSERT_EQ(get_subject_der_result, Pkcs12ReaderStatusCode::kSuccess);

  std::vector<std::unique_ptr<net::CertBuilder>> cert_builders =
      net::CertBuilder::CreateSimpleChain(/*chain_length=*/1);
  cert_builders[0]->SetSubjectTLV(subject_der);

  const char kNickname[] = "nickname123";
  scoped_refptr<const Cert> kcer_cert =
      MakeKcerCert(kNickname, cert_builders[0]->GetX509Certificate());
  cert_cache_ = CertCache(std::vector{kcer_cert});

  std::vector<CertData> certs_data;
  Pkcs12ReaderStatusCode prepare_certs_status = ValidateAndPrepareCertData(
      cert_cache_, pkcs12_reader_, std::move(certs), key_data, certs_data);
  EXPECT_EQ(prepare_certs_status, Pkcs12ReaderStatusCode::kSuccess);
  ASSERT_EQ(certs_data.size(), 1u);
  EXPECT_EQ(certs_data[0].nickname, kNickname);
}

// Test that ValidateAndPrepareCertData() will generate a unique nickname when
// the default option is already taken.
TEST_F(KcerPkcs12ValidatorTest, UniqueNickname) {
  KeyData key_data;
  bssl::UniquePtr<STACK_OF(X509)> certs;
  ASSERT_NO_FATAL_FAILURE(GetData("client.p12", key_data, certs));

  X509* cert = sk_X509_value(certs.get(), 0);
  ASSERT_TRUE(cert);

  std::vector<std::unique_ptr<net::CertBuilder>> cert_builders =
      net::CertBuilder::CreateSimpleChain(/*chain_length=*/1);

  const char kNickname[] = "testusercert";
  scoped_refptr<const Cert> kcer_cert =
      MakeKcerCert(kNickname, cert_builders[0]->GetX509Certificate());
  cert_cache_ = CertCache(std::vector{kcer_cert});

  std::vector<CertData> certs_data;
  Pkcs12ReaderStatusCode prepare_certs_status = ValidateAndPrepareCertData(
      cert_cache_, pkcs12_reader_, std::move(certs), key_data, certs_data);
  EXPECT_EQ(prepare_certs_status, Pkcs12ReaderStatusCode::kSuccess);
  ASSERT_EQ(certs_data.size(), 1u);
  EXPECT_EQ(certs_data[0].nickname, std::string(kNickname) + " 1");
}

}  // namespace
}  // namespace kcer::internal
