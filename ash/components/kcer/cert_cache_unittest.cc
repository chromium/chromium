// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/cert_cache.h"

#include <memory>

#include "ash/components/kcer/kcer.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_builder.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace kcer::internal {
namespace {

std::unique_ptr<net::CertBuilder> MakeCertBuilder() {
  return net::CertBuilder::FromFile(
      net::GetTestCertsDirectory().AppendASCII("ok_cert.pem"), nullptr);
}

scoped_refptr<const Cert> MakeKcerCert(
    scoped_refptr<net::X509Certificate> cert) {
  // CertCache only cares about the `cert`, other fields are can be anything.
  return base::MakeRefCounted<Cert>(Token::kUser, Pkcs11Id(),
                                    /*nickname=*/std::string(),
                                    std::move(cert));
}

base::span<const uint8_t> AsSpan(scoped_refptr<net::X509Certificate> cert) {
  return net::x509_util::CryptoBufferAsSpan(cert->cert_buffer());
}

// Test that an empty cache doesn't find an unrelated certificate and doesn't
// return any certs.
TEST(KcerCertCacheTest, EmptyCacheThenCertNotFound) {
  std::unique_ptr<net::CertBuilder> builder = MakeCertBuilder();
  scoped_refptr<net::X509Certificate> cert = builder->GetX509Certificate();

  CertCache empty_cache;

  EXPECT_EQ(empty_cache.FindCert(AsSpan(cert)), nullptr);
  EXPECT_EQ(empty_cache.GetAllCerts().size(), 0u);
}

// Test that a cache with one cert can find and return that cert, but not an
// unrelated one.
TEST(KcerCertCacheTest, OneCert) {
  std::unique_ptr<net::CertBuilder> builder_0 = MakeCertBuilder();
  std::unique_ptr<net::CertBuilder> builder_1 = MakeCertBuilder();

  scoped_refptr<net::X509Certificate> cert_0 = builder_0->GetX509Certificate();
  scoped_refptr<const Cert> kcer_cert_0 =
      MakeKcerCert(builder_0->GetX509Certificate());

  scoped_refptr<net::X509Certificate> cert_1 = builder_1->GetX509Certificate();

  std::vector<scoped_refptr<const Cert>> certs({kcer_cert_0});
  CertCache cache(certs);

  EXPECT_EQ(cache.FindCert(AsSpan(cert_0)), kcer_cert_0);
  EXPECT_EQ(cache.FindCert(AsSpan(cert_1)), nullptr);
  EXPECT_THAT(cache.GetAllCerts(), testing::ElementsAre(kcer_cert_0));
}

// Test that CertCache can hold, find and return multiple certs.
TEST(KcerCertCacheTest, MultipleCerts) {
  std::unique_ptr<net::CertBuilder> builder_0 = MakeCertBuilder();
  std::unique_ptr<net::CertBuilder> builder_1 = MakeCertBuilder();
  std::unique_ptr<net::CertBuilder> builder_2 = MakeCertBuilder();
  std::unique_ptr<net::CertBuilder> builder_3 = MakeCertBuilder();

  scoped_refptr<net::X509Certificate> cert_0 =
      builder_0->GetX509Certificate().get();
  scoped_refptr<const Cert> kcer_cert_0 =
      MakeKcerCert(builder_0->GetX509Certificate());

  scoped_refptr<net::X509Certificate> cert_1 =
      builder_1->GetX509Certificate().get();
  scoped_refptr<const Cert> kcer_cert_1 =
      MakeKcerCert(builder_1->GetX509Certificate());

  scoped_refptr<net::X509Certificate> cert_2 =
      builder_2->GetX509Certificate().get();
  scoped_refptr<const Cert> kcer_cert_2 =
      MakeKcerCert(builder_2->GetX509Certificate());

  scoped_refptr<net::X509Certificate> cert_3 =
      builder_3->GetX509Certificate().get();
  scoped_refptr<const Cert> kcer_cert_3 =
      MakeKcerCert(builder_3->GetX509Certificate());

  // Add a lot of duplicates in different order to exercise the comparator.
  std::vector<scoped_refptr<const Cert>> certs(
      {kcer_cert_0, kcer_cert_1, kcer_cert_2, kcer_cert_3, kcer_cert_3,
       kcer_cert_2, kcer_cert_1, kcer_cert_0, kcer_cert_0, kcer_cert_2,
       kcer_cert_3, kcer_cert_1});
  CertCache cache(certs);

  EXPECT_EQ(cache.FindCert(AsSpan(cert_0)), kcer_cert_0);
  EXPECT_EQ(cache.FindCert(AsSpan(cert_1)), kcer_cert_1);
  EXPECT_EQ(cache.FindCert(AsSpan(cert_2)), kcer_cert_2);
  EXPECT_EQ(cache.FindCert(AsSpan(cert_3)), kcer_cert_3);
  EXPECT_THAT(cache.GetAllCerts(),
              testing::UnorderedElementsAre(kcer_cert_0, kcer_cert_1,
                                            kcer_cert_2, kcer_cert_3));
}

}  // namespace
}  // namespace kcer::internal
