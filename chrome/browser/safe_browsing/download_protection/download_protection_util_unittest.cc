// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"

#include "base/hash/sha1.h"
#include "base/path_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_unittest_util.h"
#include "net/cert/x509_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using ::testing::ElementsAre;

TEST(DownloadProtectionUtilTest, GetCertificateAllowlistStrings) {
  // We'll pass this cert in as the "issuer", even though it isn't really
  // used to sign the certs below.  GetCertificateAllowlistStirngs doesn't care
  // about this.

  base::FilePath source_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_path));
  base::FilePath testdata_path = source_path.AppendASCII("chrome")
                                     .AppendASCII("test")
                                     .AppendASCII("data")
                                     .AppendASCII("safe_browsing")
                                     .AppendASCII("download_protection");

  scoped_refptr<net::X509Certificate> issuer_cert(
      ReadTestCertificate(testdata_path, "issuer.pem"));
  ASSERT_TRUE(issuer_cert.get());
  std::string hashed = base::SHA1HashString(std::string(
      net::x509_util::CryptoBufferAsStringPiece(issuer_cert->cert_buffer())));
  std::string cert_base =
      "cert/" + base::HexEncode(hashed.data(), hashed.size());

  scoped_refptr<net::X509Certificate> cert(
      ReadTestCertificate(testdata_path, "test_cn.pem"));
  ASSERT_TRUE(cert.get());
  std::vector<std::string> allowlist_strings;
  GetCertificateAllowlistStrings(*cert.get(), *issuer_cert.get(),
                                 &allowlist_strings);
  // This also tests escaping of characters in the certificate attributes.
  EXPECT_THAT(allowlist_strings, ElementsAre(cert_base + "/CN=subject%2F%251"));

  cert = ReadTestCertificate(testdata_path, "test_cn_o.pem");
  ASSERT_TRUE(cert.get());
  allowlist_strings.clear();
  GetCertificateAllowlistStrings(*cert.get(), *issuer_cert.get(),
                                 &allowlist_strings);
  EXPECT_THAT(allowlist_strings, ElementsAre(cert_base + "/CN=subject",
                                             cert_base + "/CN=subject/O=org",
                                             cert_base + "/O=org"));

  cert = ReadTestCertificate(testdata_path, "test_cn_o_ou.pem");
  ASSERT_TRUE(cert.get());
  allowlist_strings.clear();
  GetCertificateAllowlistStrings(*cert.get(), *issuer_cert.get(),
                                 &allowlist_strings);
  EXPECT_THAT(
      allowlist_strings,
      ElementsAre(cert_base + "/CN=subject", cert_base + "/CN=subject/O=org",
                  cert_base + "/CN=subject/O=org/OU=unit",
                  cert_base + "/CN=subject/OU=unit", cert_base + "/O=org",
                  cert_base + "/O=org/OU=unit", cert_base + "/OU=unit"));

  cert = ReadTestCertificate(testdata_path, "test_cn_ou.pem");
  ASSERT_TRUE(cert.get());
  allowlist_strings.clear();
  GetCertificateAllowlistStrings(*cert.get(), *issuer_cert.get(),
                                 &allowlist_strings);
  EXPECT_THAT(allowlist_strings, ElementsAre(cert_base + "/CN=subject",
                                             cert_base + "/CN=subject/OU=unit",
                                             cert_base + "/OU=unit"));

  cert = ReadTestCertificate(testdata_path, "test_o.pem");
  ASSERT_TRUE(cert.get());
  allowlist_strings.clear();
  GetCertificateAllowlistStrings(*cert.get(), *issuer_cert.get(),
                                 &allowlist_strings);
  EXPECT_THAT(allowlist_strings, ElementsAre(cert_base + "/O=org"));

  cert = ReadTestCertificate(testdata_path, "test_o_ou.pem");
  ASSERT_TRUE(cert.get());
  allowlist_strings.clear();
  GetCertificateAllowlistStrings(*cert.get(), *issuer_cert.get(),
                                 &allowlist_strings);
  EXPECT_THAT(allowlist_strings,
              ElementsAre(cert_base + "/O=org", cert_base + "/O=org/OU=unit",
                          cert_base + "/OU=unit"));

  cert = ReadTestCertificate(testdata_path, "test_ou.pem");
  ASSERT_TRUE(cert.get());
  allowlist_strings.clear();
  GetCertificateAllowlistStrings(*cert.get(), *issuer_cert.get(),
                                 &allowlist_strings);
  EXPECT_THAT(allowlist_strings, ElementsAre(cert_base + "/OU=unit"));

  cert = ReadTestCertificate(testdata_path, "test_c.pem");
  ASSERT_TRUE(cert.get());
  allowlist_strings.clear();
  GetCertificateAllowlistStrings(*cert.get(), *issuer_cert.get(),
                                 &allowlist_strings);
  EXPECT_THAT(allowlist_strings, ElementsAre());
}

}  // namespace safe_browsing
