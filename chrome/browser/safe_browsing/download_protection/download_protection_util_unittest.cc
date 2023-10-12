// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"

#include "base/hash/sha1.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_unittest_util.h"
#include "components/safe_browsing/content/common/file_type_policies_test_util.h"
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
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_path));
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

TEST(DownloadProtectionUtilTest, HigherWeightArchivesSelectedFirst) {
  safe_browsing::FileTypePoliciesTestOverlay scoped_dangerous;
  {
    // Setup fake file-type config so that this test is not dependent on the
    // actual policy values.
    auto fake_config = std::make_unique<DownloadFileTypeConfig>();
    fake_config->set_max_archived_binaries_to_report(10);
    fake_config->mutable_default_file_type()
        ->add_platform_settings()
        ->set_file_weight(0);
    DownloadFileType* msi = fake_config->add_file_types();
    msi->set_extension("msi");
    msi->add_platform_settings()->set_file_weight(1);

    scoped_dangerous.SwapConfig(fake_config);
  }

  ClientDownloadRequest::ArchivedBinary zip;
  zip.set_file_path("a.zip");
  zip.set_is_archive(true);

  ClientDownloadRequest::ArchivedBinary msi;
  msi.set_file_path("a.msi");
  msi.set_is_executable(true);

  google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>
      binaries;
  *binaries.Add() = zip;
  *binaries.Add() = zip;
  *binaries.Add() = msi;

  google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>
      selected_binaries = SelectArchiveEntries(binaries);

  // Selecting a single deepest entry leads to just one zip in front of the
  // higher-weight files. So we expect this order.
  ASSERT_EQ(selected_binaries.size(), 3);
  EXPECT_EQ(selected_binaries[0].file_path(), "a.zip");
  EXPECT_EQ(selected_binaries[1].file_path(), "a.msi");
  EXPECT_EQ(selected_binaries[2].file_path(), "a.zip");
}

TEST(DownloadProtectionUtilTest, EncryptedFileSelected) {
  safe_browsing::FileTypePoliciesTestOverlay scoped_dangerous;
  {
    // Setup fake file-type config so that this test is not dependent on the
    // actual policy values.
    auto fake_config = std::make_unique<DownloadFileTypeConfig>();
    fake_config->set_max_archived_binaries_to_report(10);
    fake_config->mutable_default_file_type()->add_platform_settings();
    scoped_dangerous.SwapConfig(fake_config);
  }

  ClientDownloadRequest::ArchivedBinary zip;
  zip.set_file_path("a.zip");
  zip.set_is_archive(true);

  ClientDownloadRequest::ArchivedBinary encrypted;
  encrypted.set_file_path("encrypted.dll");
  encrypted.set_is_executable(true);
  encrypted.set_is_encrypted(true);

  google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>
      binaries;
  *binaries.Add() = zip;
  *binaries.Add() = encrypted;

  google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>
      selected_binaries = SelectArchiveEntries(binaries);

  ASSERT_EQ(selected_binaries.size(), 2);
  EXPECT_EQ(selected_binaries[0].file_path(), "encrypted.dll");
  EXPECT_EQ(selected_binaries[1].file_path(), "a.zip");
}

TEST(DownloadProtectionUtilTest, OnlyOneEncryptedFilePrioritized) {
  safe_browsing::FileTypePoliciesTestOverlay scoped_dangerous;
  {
    // Setup fake file-type config so that this test is not dependent on the
    // actual policy values.
    auto fake_config = std::make_unique<DownloadFileTypeConfig>();
    fake_config->set_max_archived_binaries_to_report(10);
    fake_config->mutable_default_file_type()->add_platform_settings();
    scoped_dangerous.SwapConfig(fake_config);
  }

  ClientDownloadRequest::ArchivedBinary exe;
  exe.set_file_path("evil.exe");
  exe.set_is_archive(true);

  ClientDownloadRequest::ArchivedBinary encrypted;
  encrypted.set_file_path("encrypted.dll");
  encrypted.set_is_executable(true);
  encrypted.set_is_encrypted(true);

  google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>
      binaries;
  *binaries.Add() = exe;
  *binaries.Add() = encrypted;

  encrypted.set_file_path("other_encrypted.dll");
  *binaries.Add() = encrypted;

  google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>
      selected_binaries = SelectArchiveEntries(binaries);

  // Only one encrypted DLL is prioritized over the more relevant exe.
  ASSERT_EQ(selected_binaries.size(), 3);
  EXPECT_EQ(selected_binaries[0].file_path(), "encrypted.dll");
  EXPECT_EQ(selected_binaries[1].file_path(), "evil.exe");
  EXPECT_EQ(selected_binaries[2].file_path(), "other_encrypted.dll");
}

TEST(DownloadProtectionUtilTest, DeepestEntrySelected) {
  safe_browsing::FileTypePoliciesTestOverlay scoped_dangerous;
  {
    // Setup fake file-type config so that this test is not dependent on the
    // actual policy values.
    auto fake_config = std::make_unique<DownloadFileTypeConfig>();
    fake_config->set_max_archived_binaries_to_report(10);
    fake_config->mutable_default_file_type()->add_platform_settings();
    scoped_dangerous.SwapConfig(fake_config);
  }

  ClientDownloadRequest::ArchivedBinary zip;
  zip.set_file_path("a.zip");
  zip.set_is_archive(true);

  ClientDownloadRequest::ArchivedBinary deep;
  deep.set_file_path("hidden/in/deep/path/file.exe");
  deep.set_is_executable(true);

  google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>
      binaries;
  *binaries.Add() = zip;
  *binaries.Add() = deep;

  google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>
      selected_binaries = SelectArchiveEntries(binaries);

  ASSERT_EQ(selected_binaries.size(), 2);
  EXPECT_EQ(selected_binaries[0].file_path(), "hidden/in/deep/path/file.exe");
  EXPECT_EQ(selected_binaries[1].file_path(), "a.zip");
}

TEST(DownloadProtectionUtilTest, OnlyOneDeepestEntryPrioritized) {
  safe_browsing::FileTypePoliciesTestOverlay scoped_dangerous;
  {
    // Setup fake file-type config so that this test is not dependent on the
    // actual policy values.
    auto fake_config = std::make_unique<DownloadFileTypeConfig>();
    fake_config->set_max_archived_binaries_to_report(10);
    fake_config->mutable_default_file_type()->add_platform_settings();
    scoped_dangerous.SwapConfig(fake_config);
  }

  ClientDownloadRequest::ArchivedBinary exe;
  exe.set_file_path("evil.exe");
  exe.set_is_executable(true);

  ClientDownloadRequest::ArchivedBinary deep;
  deep.set_file_path("hidden/in/deep/path/random.dll");
  deep.set_is_executable(true);

  google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>
      binaries;
  *binaries.Add() = exe;
  *binaries.Add() = deep;

  deep.set_file_path("hidden/in/deep/path/other.dll");
  *binaries.Add() = deep;

  google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>
      selected_binaries = SelectArchiveEntries(binaries);

  // One deep entry is prioritized over the more relevant entry at the root.
  ASSERT_EQ(selected_binaries.size(), 3);
  EXPECT_EQ(selected_binaries[0].file_path(), "hidden/in/deep/path/random.dll");
  EXPECT_EQ(selected_binaries[1].file_path(), "evil.exe");
  EXPECT_EQ(selected_binaries[2].file_path(), "hidden/in/deep/path/other.dll");
}

TEST(DownloadProtectionUtilTest, NonWildcardEntryDeterministic) {
  safe_browsing::FileTypePoliciesTestOverlay scoped_dangerous;
  {
    // Setup fake file-type config so that this test is not dependent on the
    // actual policy values.
    auto fake_config = std::make_unique<DownloadFileTypeConfig>();
    fake_config->set_max_archived_binaries_to_report(10);
    fake_config->mutable_default_file_type()->add_platform_settings();
    scoped_dangerous.SwapConfig(fake_config);
  }

  google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>
      binaries;
  for (int i = 0; i < 12; i++) {
    ClientDownloadRequest::ArchivedBinary exe;
    exe.set_file_path("evil" + base::NumberToString(i + 1) + ".exe");
    exe.set_is_executable(true);
    *binaries.Add() = exe;
  }

  google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>
      selected_binaries = SelectArchiveEntries(binaries);
  ASSERT_EQ(selected_binaries.size(), 10);
  for (int i = 0; i < 9; i++) {
    EXPECT_EQ(selected_binaries[i + 1].file_path(),
              "evil" + base::NumberToString(i + 1) + ".exe");
  }

  EXPECT_TRUE(selected_binaries[0].file_path() == "evil10.exe" ||
              selected_binaries[0].file_path() == "evil11.exe" ||
              selected_binaries[0].file_path() == "evil12.exe")
      << "Wilcard entry is " << selected_binaries[0].file_path();
}

}  // namespace safe_browsing
