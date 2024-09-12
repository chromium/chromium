// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/client_cert_identity_kcer.h"

#include "ash/components/kcer/kcer.h"
#include "ash/components/kcer/kcer_nss/test_utils.h"
#include "base/test/test_future.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace kcer {

class ClientCertIdentityKcerTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::UI,
      content::BrowserTaskEnvironment::REAL_IO_THREAD};

  crypto::ScopedTestNSSDB nss_db_;
  TestKcerHolder kcer_holder_{/*user_slot=*/nss_db_.slot(),
                              /*device_slot=*/nullptr};
};

// Test that an SSL private key can be successfully acquired and used for
// signing.
TEST_F(ClientCertIdentityKcerTest, Success) {
  base::expected<KeyAndCert, Error> cert_and_key = ImportTestKeyAndCert(
      kcer_holder_.GetKcer(), Token::kUser, "client_1.key", "client_1.pem");
  ASSERT_TRUE(cert_and_key.has_value());

  ClientCertIdentityKcer identity(kcer_holder_.GetKcer(), cert_and_key->cert);

  base::test::TestFuture<scoped_refptr<net::SSLPrivateKey>> acquire_waiter;
  identity.AcquirePrivateKey(acquire_waiter.GetCallback());

  scoped_refptr<net::SSLPrivateKey> key = acquire_waiter.Get();
  ASSERT_NE(key, nullptr);

  std::vector<uint8_t> data_to_sign{1, 2, 3, 4, 5, 6, 7, 8};

  base::test::TestFuture<net::Error, const std::vector<uint8_t>&> sign_waiter;
  key->Sign(SSL_SIGN_RSA_PKCS1_SHA256, data_to_sign, sign_waiter.GetCallback());
  EXPECT_EQ(sign_waiter.Get<net::Error>(), net::OK);

  EXPECT_TRUE(VerifySignature(
      SigningScheme::kRsaPkcs1Sha256, cert_and_key->key.GetSpki(),
      DataToSign(std::move(data_to_sign)), Signature(sign_waiter.Get<1>()),
      /*strict=*/true));
}

// Test that ClientCertIdentityKcer correctly fails to acquire a key when Kcer
// fails to find the cert.
TEST_F(ClientCertIdentityKcerTest, CertDoesNotExistFailure) {
  scoped_refptr<const Cert> cert =
      base::MakeRefCounted<Cert>(Token::kUser, Pkcs11Id({1, 2, 3}), "nickname",
                                 /*x509_cert=*/nullptr);

  ClientCertIdentityKcer identity(kcer_holder_.GetKcer(), cert);

  base::test::TestFuture<scoped_refptr<net::SSLPrivateKey>> result_waiter;
  identity.AcquirePrivateKey(result_waiter.GetCallback());
  EXPECT_EQ(result_waiter.Get(), nullptr);
}

// Test that ClientCertIdentityKcer correctly fails to acquire a key when the
// cert is null.
TEST_F(ClientCertIdentityKcerTest, CertIsNullFailure) {
  ClientCertIdentityKcer identity(kcer_holder_.GetKcer(), nullptr);

  base::test::TestFuture<scoped_refptr<net::SSLPrivateKey>> result_waiter;
  identity.AcquirePrivateKey(result_waiter.GetCallback());
  EXPECT_EQ(result_waiter.Get(), nullptr);
}

// Test that ClientCertIdentityKcer correctly fails to acquire a key when Kcer
// is null.
TEST_F(ClientCertIdentityKcerTest, KcerIsNullFailure) {
  base::expected<KeyAndCert, Error> cert_and_key = ImportTestKeyAndCert(
      kcer_holder_.GetKcer(), Token::kUser, "client_1.key", "client_1.pem");
  ASSERT_TRUE(cert_and_key.has_value());

  ClientCertIdentityKcer identity(nullptr, cert_and_key->cert);

  base::test::TestFuture<scoped_refptr<net::SSLPrivateKey>> result_waiter;
  identity.AcquirePrivateKey(result_waiter.GetCallback());
  EXPECT_EQ(result_waiter.Get(), nullptr);
}

}  // namespace kcer
