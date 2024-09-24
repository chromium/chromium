// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"

#include "base/hash/sha1.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_unittest_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/common/file_type_policies_test_util.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "net/cert/x509_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

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
  std::string hashed = base::HexEncode(base::SHA1Hash(
      net::x509_util::CryptoBufferAsSpan(issuer_cert->cert_buffer())));
  std::string cert_base = "cert/" + hashed;

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

#if BUILDFLAG(FULL_SAFE_BROWSING)
TEST(DownloadProtectionUtilTest, ShouldSendDangerousDownloadReport) {
  content::BrowserTaskEnvironment task_environment;
  GURL download_url("https://example.com");
  auto setup = [&](Profile* profile,
                   NiceMock<download::MockDownloadItem>* item) {
    content::DownloadItemUtils::AttachInfoForTesting(item, profile,
                                                     /*web_contents=*/nullptr);
    DownloadProtectionService::SetDownloadProtectionData(
        item, "download_token", ClientDownloadResponse::DANGEROUS_HOST,
        ClientDownloadResponse::TailoredVerdict());
    SetSafeBrowsingState(profile->GetPrefs(),
                         SafeBrowsingState::STANDARD_PROTECTION);

    ON_CALL(*item, GetURL).WillByDefault(ReturnRef(download_url));
    ON_CALL(*item, GetDangerType)
        .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST));
    ON_CALL(*item, IsDangerous).WillByDefault(Return(true));
  };
  {
    // Report should be sent.
    TestingProfile profile;
    NiceMock<download::MockDownloadItem> download_item;
    setup(&profile, &download_item);
    EXPECT_TRUE(ShouldSendDangerousDownloadReport(
        &download_item,
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_WARNING));
  }
  {
    // Report should not be sent because Safe Browsing is disabled.
    TestingProfile profile;
    NiceMock<download::MockDownloadItem> download_item;
    setup(&profile, &download_item);
    SetSafeBrowsingState(profile.GetPrefs(),
                         SafeBrowsingState::NO_SAFE_BROWSING);
    EXPECT_FALSE(ShouldSendDangerousDownloadReport(
        &download_item,
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_WARNING));
  }
  {
    // Report should not be sent because this report type should only be sent
    // when extended reporting is enabled.
    TestingProfile profile;
    NiceMock<download::MockDownloadItem> download_item;
    setup(&profile, &download_item);
    SetExtendedReportingPrefForTests(profile.GetPrefs(), false);
    EXPECT_FALSE(ShouldSendDangerousDownloadReport(
        &download_item,
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_OPENED));
  }
  {
    // Report should not be sent because this is an off-the-record profile.
    TestingProfile profile;
    TestingProfile::Builder profile_builder;
    TestingProfile* otr_profile = profile_builder.BuildIncognito(&profile);
    NiceMock<download::MockDownloadItem> download_item;
    setup(otr_profile, &download_item);
    EXPECT_FALSE(ShouldSendDangerousDownloadReport(
        &download_item,
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_WARNING));
  }
  {
    // Report should not be sent because the URL is empty.
    TestingProfile profile;
    NiceMock<download::MockDownloadItem> download_item;
    setup(&profile, &download_item);
    GURL empty_url("");
    ON_CALL(download_item, GetURL).WillByDefault(ReturnRef(empty_url));
    EXPECT_FALSE(ShouldSendDangerousDownloadReport(
        &download_item,
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_WARNING));
  }
  {
    // Report should not be sent because the download is not dangerous.
    TestingProfile profile;
    NiceMock<download::MockDownloadItem> download_item;
    setup(&profile, &download_item);
    ON_CALL(download_item, IsDangerous).WillByDefault(Return(false));
    EXPECT_FALSE(ShouldSendDangerousDownloadReport(
        &download_item,
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_WARNING));
  }
  {
    // Report should be sent because it was dangerous and is now validated by
    // user.
    TestingProfile profile;
    NiceMock<download::MockDownloadItem> download_item;
    setup(&profile, &download_item);
    ON_CALL(download_item, IsDangerous).WillByDefault(Return(false));
    ON_CALL(download_item, GetDangerType)
        .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED));
    EXPECT_TRUE(ShouldSendDangerousDownloadReport(
        &download_item,
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_WARNING));
  }
  {
    // Report should be sent because it is under async local password scanning.
    TestingProfile profile;
    NiceMock<download::MockDownloadItem> download_item;
    setup(&profile, &download_item);
    ON_CALL(download_item, IsDangerous).WillByDefault(Return(false));
    ON_CALL(download_item, GetDangerType)
        .WillByDefault(Return(
            download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING));
    // Async scanning may be triggered when the response is safe.
    DownloadProtectionService::SetDownloadProtectionData(
        &download_item, "download_token", ClientDownloadResponse::SAFE,
        ClientDownloadResponse::TailoredVerdict());
    EXPECT_TRUE(ShouldSendDangerousDownloadReport(
        &download_item,
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_RECOVERY));
  }
  {
    // Report should be sent because it is under deep scanning.
    TestingProfile profile;
    NiceMock<download::MockDownloadItem> download_item;
    setup(&profile, &download_item);
    ON_CALL(download_item, IsDangerous).WillByDefault(Return(false));
    ON_CALL(download_item, GetDangerType)
        .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING));
    // Async scanning may be triggered when the response is safe.
    DownloadProtectionService::SetDownloadProtectionData(
        &download_item, "download_token", ClientDownloadResponse::SAFE,
        ClientDownloadResponse::TailoredVerdict());
    EXPECT_TRUE(ShouldSendDangerousDownloadReport(
        &download_item,
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_RECOVERY));
  }
  {
    // Report should not be sent because there is no token.
    TestingProfile profile;
    NiceMock<download::MockDownloadItem> download_item;
    setup(&profile, &download_item);
    DownloadProtectionService::SetDownloadProtectionData(
        &download_item, "", ClientDownloadResponse::DANGEROUS_HOST,
        ClientDownloadResponse::TailoredVerdict());
    EXPECT_FALSE(ShouldSendDangerousDownloadReport(
        &download_item,
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_WARNING));
  }
  {
    // Report should not be sent because ClientDownloadResponse is SAFE.
    TestingProfile profile;
    NiceMock<download::MockDownloadItem> download_item;
    setup(&profile, &download_item);
    DownloadProtectionService::SetDownloadProtectionData(
        &download_item, "download_token", ClientDownloadResponse::SAFE,
        ClientDownloadResponse::TailoredVerdict());
    EXPECT_FALSE(ShouldSendDangerousDownloadReport(
        &download_item,
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_WARNING));
  }
  {
    // Report should be sent because DANGEROUS_URL doesn't have token or unsafe
    // verdict.
    TestingProfile profile;
    NiceMock<download::MockDownloadItem> download_item;
    setup(&profile, &download_item);
    ON_CALL(download_item, GetDangerType)
        .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL));
    DownloadProtectionService::SetDownloadProtectionData(
        &download_item, "", ClientDownloadResponse::SAFE,
        ClientDownloadResponse::TailoredVerdict());
    EXPECT_TRUE(ShouldSendDangerousDownloadReport(
        &download_item,
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_WARNING));
  }
}
#endif

}  // namespace safe_browsing
