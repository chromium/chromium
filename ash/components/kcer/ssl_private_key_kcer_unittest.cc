// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/ssl_private_key_kcer.h"

#include "ash/components/kcer/kcer.h"
#include "ash/components/kcer/kcer_nss/test_utils.h"
#include "base/test/test_future.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace kcer {

class SSLPrivateKeyKcerTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::UI,
      content::BrowserTaskEnvironment::REAL_IO_THREAD};

  crypto::ScopedTestNSSDB nss_db_;
  TestKcerHolder kcer_holder_{/*user_slot=*/nss_db_.slot(),
                              /*device_slot=*/nullptr};
  std::vector<uint8_t> data_to_sign_{1, 2, 3, 4, 5, 6, 7, 8};
};

// Test that SSLPrivateKeyKcer can successfully sign data.
TEST_F(SSLPrivateKeyKcerTest, SignSuccess) {
  base::expected<KeyAndCert, Error> cert_and_key = ImportTestKeyAndCert(
      kcer_holder_.GetKcer(), Token::kUser, "client_1.key", "client_1.pem");
  ASSERT_TRUE(cert_and_key.has_value());

  auto key = base::MakeRefCounted<SSLPrivateKeyKcer>(
      kcer_holder_.GetKcer(), cert_and_key->cert, KeyType::kRsa,
      /*supported_schemes=*/
      base::flat_set<SigningScheme>({SigningScheme::kRsaPkcs1Sha256}));

  base::test::TestFuture<net::Error, const std::vector<uint8_t>&> sign_waiter;
  key->Sign(SSL_SIGN_RSA_PKCS1_SHA256, data_to_sign_,
            sign_waiter.GetCallback());
  EXPECT_EQ(sign_waiter.Get<net::Error>(), net::OK);

  EXPECT_TRUE(VerifySignature(
      SigningScheme::kRsaPkcs1Sha256, cert_and_key->key.GetSpki(),
      DataToSign(std::move(data_to_sign_)), Signature(sign_waiter.Get<1>()),
      /*strict=*/true));
}

// Test that SSLPrivateKeyKcerTest correctly fails to sign data when Kcer
// fails to find the cert.
TEST_F(SSLPrivateKeyKcerTest, SignFailure) {
  scoped_refptr<const Cert> cert =
      base::MakeRefCounted<Cert>(Token::kUser, Pkcs11Id({1, 2, 3}), "nickname",
                                 /*x509_cert=*/nullptr);
  auto key = base::MakeRefCounted<SSLPrivateKeyKcer>(
      kcer_holder_.GetKcer(), cert, KeyType::kRsa,
      /*supported_schemes=*/
      base::flat_set<SigningScheme>({SigningScheme::kRsaPkcs1Sha256}));

  base::test::TestFuture<net::Error, const std::vector<uint8_t>&> sign_waiter;
  key->Sign(SSL_SIGN_RSA_PKCS1_SHA256, data_to_sign_,
            sign_waiter.GetCallback());
  EXPECT_EQ(sign_waiter.Get<net::Error>(),
            net::Error::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED);
}

// Test that SSLPrivateKeyKcerTest correctly fails to sign data when the
// cert is null.
TEST_F(SSLPrivateKeyKcerTest, CertIsNullFailure) {
  auto key = base::MakeRefCounted<SSLPrivateKeyKcer>(
      kcer_holder_.GetKcer(), /*cert=*/nullptr, KeyType::kRsa,
      /*supported_schemes=*/
      base::flat_set<SigningScheme>({SigningScheme::kRsaPkcs1Sha256}));

  base::test::TestFuture<net::Error, const std::vector<uint8_t>&> sign_waiter;
  key->Sign(SSL_SIGN_RSA_PKCS1_SHA256, data_to_sign_,
            sign_waiter.GetCallback());
  EXPECT_EQ(sign_waiter.Get<net::Error>(), net::Error::ERR_UNEXPECTED);
}

// Test that SSLPrivateKeyKcerTest correctly fails to sign data when the
// Kcer is null.
TEST_F(SSLPrivateKeyKcerTest, KcerIsNullFailure) {
  base::expected<KeyAndCert, Error> cert_and_key = ImportTestKeyAndCert(
      kcer_holder_.GetKcer(), Token::kUser, "client_1.key", "client_1.pem");
  ASSERT_TRUE(cert_and_key.has_value());

  auto key = base::MakeRefCounted<SSLPrivateKeyKcer>(
      /*kcer=*/nullptr, cert_and_key->cert, KeyType::kRsa,
      /*supported_schemes=*/
      base::flat_set<SigningScheme>({SigningScheme::kRsaPkcs1Sha256}));

  base::test::TestFuture<net::Error, const std::vector<uint8_t>&> sign_waiter;
  key->Sign(SSL_SIGN_RSA_PKCS1_SHA256, data_to_sign_,
            sign_waiter.GetCallback());
  EXPECT_EQ(sign_waiter.Get<net::Error>(), net::Error::ERR_CONTEXT_SHUT_DOWN);
}

}  // namespace kcer
