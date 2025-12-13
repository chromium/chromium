// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"

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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

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

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION)
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
  // Report type for Android is gated by ESB (non-Incognito).
  {
    TestingProfile profile;
    NiceMock<download::MockDownloadItem> download_item;
    setup(&profile, &download_item);
    SetSafeBrowsingState(profile.GetPrefs(),
                         SafeBrowsingState::NO_SAFE_BROWSING);
    EXPECT_FALSE(ShouldSendDangerousDownloadReport(
        &download_item,
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_WARNING_ANDROID));
  }
  {
    TestingProfile profile;
    NiceMock<download::MockDownloadItem> download_item;
    setup(&profile, &download_item);
    SetSafeBrowsingState(profile.GetPrefs(),
                         SafeBrowsingState::STANDARD_PROTECTION);
    EXPECT_FALSE(ShouldSendDangerousDownloadReport(
        &download_item,
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_WARNING_ANDROID));
  }
  {
    TestingProfile profile;
    NiceMock<download::MockDownloadItem> download_item;
    setup(&profile, &download_item);
    SetSafeBrowsingState(profile.GetPrefs(),
                         SafeBrowsingState::ENHANCED_PROTECTION);
    EXPECT_TRUE(ShouldSendDangerousDownloadReport(
        &download_item,
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_WARNING_ANDROID));
  }
  {
    TestingProfile profile;
    TestingProfile::Builder profile_builder;
    TestingProfile* otr_profile = profile_builder.BuildIncognito(&profile);
    NiceMock<download::MockDownloadItem> download_item;
    setup(otr_profile, &download_item);
    SetSafeBrowsingState(otr_profile->GetPrefs(),
                         SafeBrowsingState::ENHANCED_PROTECTION);
    EXPECT_FALSE(ShouldSendDangerousDownloadReport(
        &download_item,
        ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_WARNING_ANDROID));
  }
}
#endif

TEST(DownloadProtectionUtilTest, IsFiletypeSupportedForFullDownloadProtection) {
  // Set up a fake config that specifies ping types for filetype extensions.
  safe_browsing::FileTypePoliciesTestOverlay file_type_policies;
  auto fake_config = std::make_unique<DownloadFileTypeConfig>();
  fake_config->mutable_default_file_type()->set_ping_setting(
      DownloadFileType::NO_PING);
  std::pair<std::string, DownloadFileType::PingSetting> kPingSettings[] = {
      {"noping", DownloadFileType::NO_PING},
      {"sampledping", DownloadFileType::SAMPLED_PING},
      {"fullping", DownloadFileType::FULL_PING}};
  for (const auto& [extension, ping_setting] : kPingSettings) {
    auto* file_type = fake_config->add_file_types();
    file_type->set_extension(extension);
    file_type->set_ping_setting(ping_setting);
  }
  file_type_policies.SwapConfig(fake_config);

  EXPECT_FALSE(IsFiletypeSupportedForFullDownloadProtection(
      base::FilePath(FILE_PATH_LITERAL("foo.default"))));
  EXPECT_FALSE(IsFiletypeSupportedForFullDownloadProtection(
      base::FilePath(FILE_PATH_LITERAL("foo.noping"))));
  EXPECT_FALSE(IsFiletypeSupportedForFullDownloadProtection(
      base::FilePath(FILE_PATH_LITERAL("foo.sampledping"))));
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(IsFiletypeSupportedForFullDownloadProtection(
      base::FilePath(FILE_PATH_LITERAL("foo.fullping"))));
  EXPECT_TRUE(IsFiletypeSupportedForFullDownloadProtection(
      base::FilePath(FILE_PATH_LITERAL("foo.fUlLpInG"))));
#else
  EXPECT_FALSE(IsFiletypeSupportedForFullDownloadProtection(
      base::FilePath(FILE_PATH_LITERAL("foo.fullping"))));
  // Android hard-codes that only APK files are supported.
  EXPECT_TRUE(IsFiletypeSupportedForFullDownloadProtection(
      base::FilePath(FILE_PATH_LITERAL("foo.apk"))));
  EXPECT_TRUE(IsFiletypeSupportedForFullDownloadProtection(
      base::FilePath(FILE_PATH_LITERAL("foo.APK"))));
#endif
}

}  // namespace safe_browsing
