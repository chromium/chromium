// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/enterprise/connectors/analysis/file_transfer_analysis_delegate.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/connectors/analysis/files_request_handler.h"
#include "chrome/browser/enterprise/connectors/analysis/source_destination_test_util.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/test/fake_files_request_handler.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/test_extension_event_observer.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr char kUserName[] = "test@chromium.org";

constexpr char kDmToken[] = "dm_token";

constexpr char16_t kUserJustification[] = u"User justification";

base::TimeDelta kResponseDelay = base::Seconds(0);

storage::FileSystemURL GetEmptyTestSrcUrl() {
  return storage::FileSystemURL();
}
storage::FileSystemURL GetEmptyTestDestUrl() {
  return storage::FileSystemURL();
}

constexpr char kBlockingScansForDlpAndMalware[] = R"(
{
  "service_provider": "google",
  "enable": [
    {
      "source_destination_list": [
        {
          "sources": [{
            "file_system_type": "*"
          }],
          "destinations": [{
            "file_system_type": "*"
          }]
        }
      ],
      "tags": ["dlp", "malware"]
    }
  ],
  "block_until_verdict": 1,
  "block_large_files": 0
})";

constexpr char kBlockingScansForDlpAndMalwareReportOnly[] = R"(
{
  "service_provider": "google",
  "enable": [
    {
      "source_destination_list": [
        {
          "sources": [{
            "file_system_type": "*"
          }],
          "destinations": [{
            "file_system_type": "*"
          }]
        }
      ],
      "tags": ["dlp", "malware"]
    }
  ],
  "block_until_verdict": 0,
  "block_large_files": 0
})";

constexpr char kBlockingScansForDlp[] = R"(
{
  "service_provider": "google",
  "enable": [
    {
      "source_destination_list": [
        {
          "sources": [{
            "file_system_type": "*"
          }],
          "destinations": [{
            "file_system_type": "*"
          }]
        }
      ],
      "tags": ["dlp"]
    }
  ],
  "block_until_verdict": 1
})";

constexpr char kBlockingScansForMalware[] = R"(
{
  "service_provider": "google",
  "enable": [
    {
      "source_destination_list": [
        {
          "sources": [{
            "file_system_type": "*"
          }],
          "destinations": [{
            "file_system_type": "*"
          }]
        }
      ],
      "tags": ["malware"]
    }
  ],
  "block_until_verdict": 1
})";

constexpr char kNothingEnabled[] = R"({ "service_provider": "google" })";

const std::set<std::string>* DocMimeTypes() {
  static std::set<std::string> set = {
      "application/msword", "text/plain",
      // The 50 MB file can result in no mimetype being found.
      ""};
  return &set;
}

const std::set<std::string>* ZipMimeTypes() {
  static std::set<std::string> set = {"application/zip",
                                      "application/x-zip-compressed"};
  return &set;
}

class ScopedSetDMToken {
 public:
  explicit ScopedSetDMToken(const policy::DMToken& dm_token) {
    SetDMTokenForTesting(dm_token);
  }
  ~ScopedSetDMToken() {
    SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
  }
};

using VolumeInfo = SourceDestinationTestingHelper::VolumeInfo;

constexpr std::initializer_list<VolumeInfo> kVolumeInfos{
    {file_manager::VOLUME_TYPE_TESTING, std::nullopt, "TESTING"},
    {file_manager::VOLUME_TYPE_GOOGLE_DRIVE, std::nullopt, "GOOGLE_DRIVE"},
    {file_manager::VOLUME_TYPE_DOWNLOADS_DIRECTORY, std::nullopt, "MY_FILES"},
    {file_manager::VOLUME_TYPE_REMOVABLE_DISK_PARTITION, std::nullopt,
     "REMOVABLE"},
    {file_manager::VOLUME_TYPE_MOUNTED_ARCHIVE_FILE, std::nullopt, "TESTING"},
    {file_manager::VOLUME_TYPE_PROVIDED, std::nullopt, "PROVIDED"},
    {file_manager::VOLUME_TYPE_MTP, std::nullopt, "DEVICE_MEDIA_STORAGE"},
    {file_manager::VOLUME_TYPE_MEDIA_VIEW, std::nullopt, "ARC"},
    {file_manager::VOLUME_TYPE_CROSTINI, std::nullopt, "CROSTINI"},
    {file_manager::VOLUME_TYPE_ANDROID_FILES, std::nullopt, "ARC"},
    {file_manager::VOLUME_TYPE_DOCUMENTS_PROVIDER, std::nullopt, "ARC"},
    {file_manager::VOLUME_TYPE_SMB, std::nullopt, "SMB"},
    {file_manager::VOLUME_TYPE_SYSTEM_INTERNAL, std::nullopt, "UNKNOWN"},
    {file_manager::VOLUME_TYPE_GUEST_OS, guest_os::VmType::TERMINA, "CROSTINI"},
    {file_manager::VOLUME_TYPE_GUEST_OS, guest_os::VmType::PLUGIN_VM,
     "PLUGIN_VM"},
    {file_manager::VOLUME_TYPE_GUEST_OS, guest_os::VmType::BOREALIS,
     "BOREALIS"},
    {file_manager::VOLUME_TYPE_GUEST_OS, guest_os::VmType::BRUSCHETTA,
     "BRUSCHETTA"},
    {file_manager::VOLUME_TYPE_GUEST_OS, guest_os::VmType::UNKNOWN,
     "UNKNOWN_VM"},
    {file_manager::VOLUME_TYPE_GUEST_OS, std::nullopt, "UNKNOWN_VM"},
    {file_manager::VOLUME_TYPE_GUEST_OS, guest_os::VmType::ARCVM, "ARC"},
};

VolumeInfo GetAnyOtherVolume(const VolumeInfo& volume) {
  for (const auto& volume_i : kVolumeInfos) {
    if (std::string(volume_i.fs_config_string) !=
        std::string(volume.fs_config_string)) {
      return volume_i;
    }
  }
  return {};
}

class BaseTest : public testing::Test {
 public:
  BaseTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");

    source_destination_testing_helper_ =
        std::make_unique<SourceDestinationTestingHelper>(profile_,
                                                         kVolumeInfos);

    file_system_context_ = storage::CreateFileSystemContextForTesting(
        nullptr, source_destination_testing_helper_->GetTempDirPath());
  }

  ~BaseTest() override {
    // This deletion has to happen before source_destination_testing_helper_ is
    // destroyed.
    profile_manager_.DeleteAllTestingProfiles();
  }

  storage::FileSystemURL PathToFileSystemURL(base::FilePath path) {
    return storage::FileSystemURL::CreateForTest(
        kTestStorageKey, storage::kFileSystemTypeLocal, path);
  }

  storage::FileSystemURL GetTestFileSystemURLForVolume(VolumeInfo volume_info) {
    return source_destination_testing_helper_->GetTestFileSystemURLForVolume(
        volume_info);
  }

  Profile* profile() { return profile_; }

  void ValidateIsEnabled(const storage::FileSystemURL& src_url,
                         const storage::FileSystemURL& dest_url,
                         bool expect_dlp,
                         bool expect_malware) {
    auto settings = FileTransferAnalysisDelegate::IsEnabledVec(
        profile(), {src_url}, dest_url);
    ASSERT_EQ(expect_dlp || expect_malware, !settings.empty());
    if (expect_dlp || expect_malware) {
      ASSERT_EQ(settings.size(), 1u);
      ASSERT_TRUE(settings[0].has_value());
      EXPECT_EQ(expect_dlp, settings[0].value().tags.count("dlp"));
      EXPECT_EQ(expect_malware, settings[0].value().tags.count("malware"));
    }
  }

  void ValidateIsEnabled(VolumeInfo src_volume_info,
                         VolumeInfo dest_volume_info,
                         bool expect_dlp,
                         bool expect_malware) {
    ValidateIsEnabled(GetTestFileSystemURLForVolume(src_volume_info),
                      GetTestFileSystemURLForVolume(dest_volume_info),
                      expect_dlp, expect_malware);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome://abc");
  std::unique_ptr<SourceDestinationTestingHelper>
      source_destination_testing_helper_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
};

enum PrefState {
  NO_PREF,
  NOTHING_ENABLED_PREF,
  DLP_PREF,
  MALWARE_PREF,
  DLP_MALWARE_PREF,
};

using TestingTuple = std::tuple</*Token valid*/ bool,
                                /*Pref State*/ PrefState,
                                /*Enable unrelated Pref*/ bool>;

static auto testingTupleToString = [](const auto& info) {
  // Can use info.param here to generate the test suffix
  std::string name;
  auto [token_valid, pref_state, unrelated_pref] = info.param;
  if (!token_valid) {
    name += "TokenInvalid";
  }
  switch (pref_state) {
    case NO_PREF:
      name += "NoPref";
      break;
    case NOTHING_ENABLED_PREF:
      name += "NotEnabledPref";
      break;
    case DLP_PREF:
      name += "DLPPref";
      break;
    case MALWARE_PREF:
      name += "MalwarePref";
      break;
    case DLP_MALWARE_PREF:
      name += "DLPMalwarePref";
      break;
  }
  if (unrelated_pref) {
    name += "WithUnrelatedPref";
  }
  return name;
};

}  // namespace

class FileTransferAnalysisDelegateIsEnabledTest
    : public BaseTest,
      public ::testing::WithParamInterface<TestingTuple> {
 protected:
  bool GetTokenValid() { return std::get<0>(GetParam()); }
  PrefState GetPrefState() { return std::get<1>(GetParam()); }
  bool GetUnrelatedPrefEnabled() { return std::get<2>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(,
                         FileTransferAnalysisDelegateIsEnabledTest,
                         testing::Combine(testing::Bool(),
                                          testing::Values(NO_PREF,
                                                          NOTHING_ENABLED_PREF,
                                                          DLP_PREF,
                                                          MALWARE_PREF,
                                                          DLP_MALWARE_PREF),
                                          testing::Bool()),
                         testingTupleToString);

TEST_P(FileTransferAnalysisDelegateIsEnabledTest, Enabled) {
  ScopedSetDMToken scoped_dm_token(
      GetTokenValid() ? policy::DMToken::CreateValidToken(kDmToken)
                      : policy::DMToken::CreateInvalidToken());
  switch (GetPrefState()) {
    case NO_PREF:
      break;
    case NOTHING_ENABLED_PREF:
      enterprise_connectors::test::SetAnalysisConnector(
          profile_->GetPrefs(), FILE_TRANSFER, kNothingEnabled);
      break;
    case DLP_PREF:
      enterprise_connectors::test::SetAnalysisConnector(
          profile_->GetPrefs(), FILE_TRANSFER, kBlockingScansForDlp);
      break;
    case MALWARE_PREF:
      enterprise_connectors::test::SetAnalysisConnector(
          profile_->GetPrefs(), FILE_TRANSFER, kBlockingScansForMalware);
      break;
    case DLP_MALWARE_PREF:
      enterprise_connectors::test::SetAnalysisConnector(
          profile_->GetPrefs(), FILE_TRANSFER, kBlockingScansForDlpAndMalware);
      break;
  }
  if (GetUnrelatedPrefEnabled()) {
    // Set for wrong policy (FILE_DOWNLOADED instead of FILE_TRANSFER)!
    enterprise_connectors::test::SetAnalysisConnector(
        profile_->GetPrefs(), FILE_DOWNLOADED, kBlockingScansForDlpAndMalware);
  }

  auto settings = FileTransferAnalysisDelegate::IsEnabledVec(
      profile(), {GetEmptyTestSrcUrl()}, GetEmptyTestDestUrl());

  if (!GetTokenValid() || GetPrefState() == NO_PREF ||
      GetPrefState() == NOTHING_ENABLED_PREF) {
    EXPECT_TRUE(settings.empty());
  } else {
    ASSERT_EQ(settings.size(), 1u);
    ASSERT_TRUE(settings[0].has_value());
    if (GetPrefState() == DLP_PREF || GetPrefState() == DLP_MALWARE_PREF) {
      EXPECT_TRUE(settings[0].value().tags.count("dlp"));
    }
    if (GetPrefState() == MALWARE_PREF || GetPrefState() == DLP_MALWARE_PREF) {
      EXPECT_TRUE(settings[0].value().tags.count("malware"));
    }
  }
}

using FileTransferAnalysisDelegateIsEnabledTestSameFileSystem = BaseTest;

// Test for FileSystemURL that's not registered with a volume.
TEST_F(FileTransferAnalysisDelegateIsEnabledTestSameFileSystem,
       DlpMalwareDisabledForSameFileSystem) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), FILE_TRANSFER, kBlockingScansForDlpAndMalware);

  auto settings = FileTransferAnalysisDelegate::IsEnabledVec(
      profile(),
      {PathToFileSystemURL(
          source_destination_testing_helper_->GetTempDirPath())},
      PathToFileSystemURL(
          source_destination_testing_helper_->GetTempDirPath()));

  EXPECT_TRUE(settings.empty());
}

using FileTransferAnalysisDelegateIsEnabledTestMultiple = BaseTest;

// Test using multiple source urls.
TEST_F(FileTransferAnalysisDelegateIsEnabledTestMultiple, Test) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));

  enterprise_connectors::test::SetAnalysisConnector(profile_->GetPrefs(),
                                                    FILE_TRANSFER,
                                                    R"({
          "service_provider": "google",
          "enable": [
            {
              "source_destination_list": [
                {
                  "sources": [{
                    "file_system_type": "REMOVABLE"
                  },
                  {
                    "file_system_type": "CROSTINI"
                  }
                  ],
                  "destinations": [{
                    "file_system_type": "ANY"
                  }]
                }
              ],
              "tags": ["malware"]
            }
          ],
          "block_until_verdict": 1
        })");

  std::vector<VolumeInfo> volume_infos{kVolumeInfos};

  std::vector<storage::FileSystemURL> source_urls;
  for (auto&& volume_info : volume_infos) {
    source_urls.push_back(GetTestFileSystemURLForVolume(volume_info));
  }

  storage::FileSystemURL dest_url =
      GetTestFileSystemURLForVolume(*kVolumeInfos.begin());

  auto settings = FileTransferAnalysisDelegate::IsEnabledVec(
      profile(), source_urls, dest_url);
  ASSERT_EQ(settings.size(), source_urls.size());
  for (size_t i = 0; i < volume_infos.size(); ++i) {
    std::string source_string = volume_infos[i].fs_config_string;
    bool should_be_enabled =
        source_string == "REMOVABLE" || source_string == "CROSTINI";
    EXPECT_EQ(should_be_enabled, settings[i].has_value());
    if (settings[i].has_value()) {
      EXPECT_FALSE(settings[i].value().tags.count("dlp"));
      EXPECT_TRUE(settings[i].value().tags.count("malware"));
    }
  }
}

class FileTransferAnalysisDelegateIsEnabledParamTest
    : public BaseTest,
      public testing::WithParamInterface<VolumeInfo> {};

TEST_P(FileTransferAnalysisDelegateIsEnabledParamTest,
       DlpAndMalwareDisabledForSameVolume) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));

  VolumeInfo source_volume = GetParam();

  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), FILE_TRANSFER, kBlockingScansForDlpAndMalware);

  VolumeInfo dest_volume = GetParam();

  ValidateIsEnabled(source_volume, dest_volume,
                    /*dlp*/ false,
                    /*malware*/ false);
  ValidateIsEnabled(dest_volume, source_volume,
                    /*dlp*/ false,
                    /*malware*/ false);
}

TEST_P(FileTransferAnalysisDelegateIsEnabledParamTest,
       DlpDisabledByPatternInSource) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));

  VolumeInfo source_volume = GetParam();

  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), FILE_TRANSFER,
      base::StringPrintf(R"(
        {
          "service_provider": "google",
          "enable": [
            {
              "source_destination_list": [
                {
                  "sources": [{
                    "file_system_type": "*"
                  }],
                  "destinations": [{
                    "file_system_type": "*"
                  }]
                }
              ],
              "tags": ["dlp"]
            }
          ],
          "disable": [
            {
              "source_destination_list": [
                {
                  "sources": [{
                    "file_system_type": "%s"
                  }],
                  "destinations": [{
                    "file_system_type": "*"
                  }]
                }
              ],
              "tags": ["dlp"]
            }
          ],
          "block_until_verdict": 1
        })",
                         source_volume.fs_config_string));

  VolumeInfo dest_volume = GetAnyOtherVolume(source_volume);

  ValidateIsEnabled(source_volume, dest_volume,
                    /*dlp*/ false,
                    /*malware*/ false);
  ValidateIsEnabled(dest_volume, source_volume,
                    /*dlp*/ true,
                    /*malware*/ false);
}

TEST_P(FileTransferAnalysisDelegateIsEnabledParamTest,
       DlpDisabledByPatternInDestination) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));

  VolumeInfo dest_volume = GetParam();

  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), FILE_TRANSFER,
      base::StringPrintf(R"(
        {
          "service_provider": "google",
          "enable": [
            {
              "source_destination_list": [
                {
                  "sources": [{
                    "file_system_type": "*"
                  }],
                  "destinations": [{
                    "file_system_type": "*"
                  }]
                }
              ],
              "tags": ["dlp"]
            }
          ],
          "disable": [
            {
              "source_destination_list": [
                {
                  "sources": [{
                    "file_system_type": "*"
                  }],
                  "destinations": [{
                    "file_system_type": "%s"
                  }]
                }
              ],
              "tags": ["dlp"]
            }
          ],
          "block_until_verdict": 1
        })",
                         dest_volume.fs_config_string));

  VolumeInfo source_volume = GetAnyOtherVolume(dest_volume);

  ValidateIsEnabled(source_volume, dest_volume,
                    /*dlp*/ false,
                    /*malware*/ false);
  ValidateIsEnabled(dest_volume, source_volume,
                    /*dlp*/ true,
                    /*malware*/ false);
}

TEST_P(FileTransferAnalysisDelegateIsEnabledParamTest,
       MalwareEnabledWithPatternInSource) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));

  VolumeInfo source_volume = GetParam();

  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), FILE_TRANSFER,
      base::StringPrintf(
          R"({
          "service_provider": "google",
          "enable": [
            {
              "source_destination_list": [
                {
                  "sources": [{
                    "file_system_type": "%s"
                  }],
                  "destinations": [{
                    "file_system_type": "ANY"
                  }]
                }
              ],
              "tags": ["malware"]
            }
          ],
          "block_until_verdict": 1
        })",
          source_volume.fs_config_string));

  VolumeInfo dest_volume = GetAnyOtherVolume(source_volume);

  ValidateIsEnabled(source_volume, dest_volume,
                    /*dlp*/ false,
                    /*malware*/ true);
  ValidateIsEnabled(dest_volume, source_volume,
                    /*dlp*/ false,
                    /*malware*/ false);
}

TEST_P(FileTransferAnalysisDelegateIsEnabledParamTest,
       MalwareEnabledWithPatternsInDestination) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));

  VolumeInfo dest_volume = GetParam();

  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), FILE_TRANSFER,
      base::StringPrintf(
          R"({
          "service_provider": "google",
          "enable": [
            {
              "source_destination_list": [
                {
                  "sources": [{
                    "file_system_type": "ANY"
                  }],
                  "destinations": [{
                    "file_system_type": "%s"
                  }]
                }
              ],
              "tags": ["malware"]
            }
          ],
          "block_until_verdict": 1
        })",
          dest_volume.fs_config_string));

  VolumeInfo source_volume = GetAnyOtherVolume(dest_volume);

  ValidateIsEnabled(source_volume, dest_volume,
                    /*dlp*/ false,
                    /*malware*/ true);
  ValidateIsEnabled(dest_volume, source_volume,
                    /*dlp*/ false,
                    /*malware*/ false);
}

INSTANTIATE_TEST_SUITE_P(,
                         FileTransferAnalysisDelegateIsEnabledParamTest,
                         testing::ValuesIn(kVolumeInfos));

class FileTransferAnalysisDelegateAuditOnlyTest : public BaseTest {
 public:
  FileTransferAnalysisDelegateAuditOnlyTest() = default;

 protected:
  void SetUp() override {
    BaseTest::SetUp();

    enterprise_connectors::test::SetAnalysisConnector(
        profile_->GetPrefs(), FILE_TRANSFER, kBlockingScansForDlpAndMalware);

    // Setup reporting:
    test::SetOnSecurityEventReporting(profile()->GetPrefs(),
                                      /*enabled*/ true,
                                      /*enabled_event_names*/ {},
                                      /*enabled_opt_in_events*/ {},
                                      /*machine_scope*/ false);
    cloud_policy_client_ = std::make_unique<policy::MockCloudPolicyClient>();
    cloud_policy_client_->SetDMToken(kDmToken);
    extensions::SafeBrowsingPrivateEventRouterFactory::GetInstance()
        ->SetTestingFactory(
            profile_, base::BindRepeating(
                          &safe_browsing::BuildSafeBrowsingPrivateEventRouter));
    RealtimeReportingClientFactory::GetInstance()->SetTestingFactory(
        profile_,
        base::BindRepeating(&safe_browsing::BuildRealtimeReportingClient));
    RealtimeReportingClientFactory::GetForProfile(profile())
        ->SetBrowserCloudPolicyClientForTesting(cloud_policy_client_.get());
    identity_test_environment_ =
        std::make_unique<signin::IdentityTestEnvironment>();
    identity_test_environment_->MakePrimaryAccountAvailable(
        kUserName, signin::ConsentLevel::kSync);
    RealtimeReportingClientFactory::GetForProfile(profile())
        ->SetIdentityManagerForTesting(
            identity_test_environment_->identity_manager());

    FilesRequestHandler::SetFactoryForTesting(base::BindRepeating(
        &test::FakeFilesRequestHandler::Create,
        base::BindRepeating(
            &FileTransferAnalysisDelegateAuditOnlyTest::FakeFileUploadCallback,
            base::Unretained(this))));

    source_directory_url_ =
        source_destination_testing_helper_->GetTestFileSystemURLForVolume(
            /*VolumeInfo*/ kSourceVolumeInfo, "source");
    ASSERT_TRUE(base::CreateDirectory(source_directory_url_.path()));
    destination_directory_url_ =
        source_destination_testing_helper_->GetTestFileSystemURLForVolume(
            /*VolumeInfo*/ kDestinationVolumeInfo, "destination");
    ASSERT_TRUE(base::CreateDirectory(destination_directory_url_.path()));
  }

  void TearDown() override {
    // Needs to be called before destructor of cloud_policy_client_.
    RealtimeReportingClientFactory::GetForProfile(profile())
        ->SetBrowserCloudPolicyClientForTesting(nullptr);

    BaseTest::TearDown();
  }

  void ScanUpload(const storage::FileSystemURL& source_url,
                  const storage::FileSystemURL& destination_url) {
    source_url_ = source_url;
    destination_url_ = destination_url;
    // The access point is only used for metrics, so its value doesn't affect
    // the tests in this file and can always be the same.
    file_transfer_analysis_delegate_ = FileTransferAnalysisDelegate::Create(
        safe_browsing::DeepScanAccessPoint::FILE_TRANSFER, source_url,
        destination_url, profile_, file_system_context_.get(), GetSettings());

    base::test::TestFuture<void> future;
    file_transfer_analysis_delegate_->UploadData(future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  AnalysisSettings GetSettings() {
    auto* service = ConnectorsServiceFactory::GetForBrowserContext(profile());
    // If the corresponding Connector policy isn't set, no scans can be
    // performed.
    EXPECT_TRUE(service);
    EXPECT_TRUE(service->IsConnectorEnabled(AnalysisConnector::FILE_TRANSFER));

    // Get settings.
    auto settings = service->GetAnalysisSettings(
        GetEmptyTestSrcUrl(), GetEmptyTestDestUrl(),
        AnalysisConnector::FILE_TRANSFER);
    EXPECT_TRUE(settings.has_value());
    return std::move(settings.value());
  }

  void SetDLPResponse(ContentAnalysisResponse response) {
    dlp_response_ = std::move(response);
  }

  void PathFailsDeepScan(base::FilePath path,
                         ContentAnalysisResponse response) {
    failures_.insert({std::move(path), std::move(response)});
  }

  void FakeFileUploadCallback(
      safe_browsing::BinaryUploadService::Result result,
      const base::FilePath& path,
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request,
      test::FakeFilesRequestHandler::FakeFileRequestCallback callback) {
    EXPECT_FALSE(path.empty());
    EXPECT_EQ(request->device_token(), kDmToken);

    EXPECT_EQ(request->content_analysis_request().request_data().source(),
              kSourceVolumeInfo.fs_config_string);
    EXPECT_EQ(request->content_analysis_request().request_data().destination(),
              kDestinationVolumeInfo.fs_config_string);

    // Simulate a response.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), path,
                       safe_browsing::BinaryUploadService::Result::SUCCESS,
                       ConnectorStatusCallback(path)),
        kResponseDelay);
  }

  ContentAnalysisResponse ConnectorStatusCallback(const base::FilePath& path) {
    // The path succeeds if it is not in the `failures_` maps.
    auto it = failures_.find(path);
    ContentAnalysisResponse response =
        it != failures_.end()
            ? it->second
            : test::FakeContentAnalysisDelegate::SuccessfulResponse([this]() {
                std::set<std::string> tags;
                if (!dlp_response_.has_value()) {
                  tags.insert("dlp");
                }
                tags.insert("malware");
                return tags;
              }());

    if (dlp_response_.has_value()) {
      *response.add_results() = dlp_response_.value().results(0);
      if (dlp_response_.value().has_request_token()) {
        response.set_request_token(dlp_response_.value().request_token());
      }
    }

    return response;
  }

  [[nodiscard]] std::vector<base::FilePath> CreateFilesForTest(
      const std::vector<base::FilePath::StringType>& file_names,
      const base::FilePath prefix_path) {
    const std::string content = "content";
    std::vector<base::FilePath> paths;
    for (const auto& file_name : file_names) {
      base::FilePath path = prefix_path.Append(file_name);
      if (!base::DirectoryExists(path.DirName())) {
        base::CreateDirectory(path.DirName());
      }
      base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
      file.WriteAtCurrentPos(base::as_byte_span(content));
      paths.emplace_back(path);
    }
    return paths;
  }

  policy::MockCloudPolicyClient* cloud_policy_client() {
    return cloud_policy_client_.get();
  }

 protected:
  std::unique_ptr<FileTransferAnalysisDelegate>
      file_transfer_analysis_delegate_;

  storage::FileSystemURL source_directory_url_;
  storage::FileSystemURL destination_directory_url_;
  VolumeInfo kSourceVolumeInfo{file_manager::VOLUME_TYPE_DOWNLOADS_DIRECTORY,
                               std::nullopt, "MY_FILES"};
  VolumeInfo kDestinationVolumeInfo{
      file_manager::VOLUME_TYPE_REMOVABLE_DISK_PARTITION, std::nullopt,
      "REMOVABLE"};

 private:
  // Used to test reporting.
  std::unique_ptr<policy::MockCloudPolicyClient> cloud_policy_client_;
  // Needed to check username in reports.
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_environment_;

  ScopedSetDMToken scoped_dm_token_{
      policy::DMToken::CreateValidToken(kDmToken)};

  // Paths in this map will be consider to have failed deep scan checks.
  // The actual failure response is given for each path.
  std::map<base::FilePath, ContentAnalysisResponse> failures_;

  // DLP response to ovewrite in the callback if present.
  std::optional<ContentAnalysisResponse> dlp_response_ = std::nullopt;

  // URLs to verify source and destination.
  storage::FileSystemURL source_url_;
  storage::FileSystemURL destination_url_;
};

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest, InvalidPath) {
  storage::FileSystemURL source_url = GetEmptyTestSrcUrl();
  storage::FileSystemURL destination_url = GetEmptyTestDestUrl();

  test::EventReportValidator validator(cloud_policy_client());
  validator.ExpectNoReport();

  ScanUpload(source_url, destination_url);

  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(source_url)
          .IsUnknown());
  // Checks that there was an early return.
  EXPECT_FALSE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest, NonExistingFile) {
  storage::FileSystemURL source_url = PathToFileSystemURL(
      source_directory_url_.path().Append("does_not_exist"));

  test::EventReportValidator validator(cloud_policy_client());
  validator.ExpectNoReport();

  ScanUpload(source_url, destination_directory_url_);

  // Directories should always be unknown!
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(source_url)
          .IsUnknown());
  // Checks that there was an early return.
  EXPECT_FALSE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest, EmptyDirectory) {
  test::EventReportValidator validator(cloud_policy_client());
  validator.ExpectNoReport();

  ScanUpload(source_directory_url_, destination_directory_url_);

  // Directories should always be unknown!
  EXPECT_TRUE(file_transfer_analysis_delegate_
                  ->GetAnalysisResultAfterScan(source_directory_url_)
                  .IsUnknown());
  // Checks that there was an early return.
  EXPECT_FALSE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest, SingleFileAllowed) {
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc")}, source_directory_url_.path());

  storage::FileSystemURL source_url = PathToFileSystemURL(paths[0]);

  test::EventReportValidator validator(cloud_policy_client());
  validator.ExpectNoReport();

  ScanUpload(source_url, destination_directory_url_);

  EXPECT_TRUE(file_transfer_analysis_delegate_
                  ->GetAnalysisResultAfterScan(source_directory_url_)
                  .IsUnknown());
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(source_url)
          .IsAllowed());
  // Checks that some scanning was performed.
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest, SingleFileBlockedDlp) {
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc")}, source_directory_url_.path());

  // Mark all files and text with failed scans.
  std::string scan_id = "scan_id";
  ContentAnalysisResponse response =
      test::FakeContentAnalysisDelegate::DlpResponse(
          ContentAnalysisResponse::Result::SUCCESS, "rule",
          TriggeredRule::BLOCK);
  response.set_request_token(scan_id);

  SetDLPResponse(response);

  storage::FileSystemURL source_url = PathToFileSystemURL(paths[0]);

  // Check reporting.
  test::EventReportValidator validator(cloud_policy_client());
  validator.ExpectSensitiveDataEvent(
      /*url*/ "",
      /*tab_url*/ "",
      /*source*/ kSourceVolumeInfo.fs_config_string,
      /*destination*/ kDestinationVolumeInfo.fs_config_string,
      /*filename*/ "foo.doc",
      // printf "content" | sha256sum  |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "ED7002B439E9AC845F22357D822BAC1444730FBDB6016D3EC9432297B9EC9F73",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileTransfer,
      /*dlp_verdict*/ response.results()[0],
      /*mimetype*/ DocMimeTypes(),
      /*size*/ std::string("content").size(),
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ scan_id,
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  ScanUpload(source_url, destination_directory_url_);

  EXPECT_TRUE(file_transfer_analysis_delegate_
                  ->GetAnalysisResultAfterScan(source_directory_url_)
                  .IsUnknown());
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(source_url)
          .IsBlocked());
  // Checks that some scanning was performed.
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest, SingleFileWarnDlp) {
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc")}, source_directory_url_.path());

  // Mark all files and text with failed scans.
  std::string scan_id = "scan_id";
  ContentAnalysisResponse response =
      test::FakeContentAnalysisDelegate::DlpResponse(
          ContentAnalysisResponse::Result::SUCCESS, "rule",
          TriggeredRule::WARN);
  response.set_request_token(scan_id);

  SetDLPResponse(response);

  storage::FileSystemURL source_url = PathToFileSystemURL(paths[0]);

  {
    // Check reporting.
    test::EventReportValidator validator(cloud_policy_client());
    validator.ExpectSensitiveDataEvent(
        /*url*/ "",
        /*tab_url*/ "",
        /*source*/ kSourceVolumeInfo.fs_config_string,
        /*destination*/ kDestinationVolumeInfo.fs_config_string,
        /*filename*/ "foo.doc",
        // printf "content" | sha256sum  |  tr '[:lower:]' '[:upper:]'
        /*sha*/
        "ED7002B439E9AC845F22357D822BAC1444730FBDB6016D3EC9432297B9EC9F73",
        /*trigger*/
        extensions::SafeBrowsingPrivateEventRouter::kTriggerFileTransfer,
        /*dlp_verdict*/ response.results()[0],
        /*mimetype*/ DocMimeTypes(),
        /*size*/ std::string("content").size(),
        /*result*/
        safe_browsing::EventResultToString(safe_browsing::EventResult::WARNED),
        /*username*/ kUserName,
        /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
        /*scan_id*/ scan_id,
        /*content_transfer_method*/ std::nullopt,
        /*user_justification*/ std::nullopt);

    ScanUpload(source_url, destination_directory_url_);
  }

  EXPECT_TRUE(file_transfer_analysis_delegate_
                  ->GetAnalysisResultAfterScan(source_directory_url_)
                  .IsUnknown());

  // AnalysisResult should be blocked as the warning isn't bypassed.
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(source_url)
          .IsBlocked());
  // Checks that some scanning was performed.
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());

  auto warned_files = file_transfer_analysis_delegate_->GetWarnedFiles();
  ASSERT_EQ(warned_files.size(), 1ul);
  EXPECT_EQ(paths[0], warned_files[0].path());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest, SingleFileWarnDlpBypassed) {
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc")}, source_directory_url_.path());

  // Mark all files and text with failed scans.
  std::string scan_id = "scan_id";
  ContentAnalysisResponse response =
      test::FakeContentAnalysisDelegate::DlpResponse(
          ContentAnalysisResponse::Result::SUCCESS, "rule",
          TriggeredRule::WARN);
  response.set_request_token(scan_id);

  SetDLPResponse(response);

  storage::FileSystemURL source_url = PathToFileSystemURL(paths[0]);

  {
    // Check reporting.
    test::EventReportValidator validator(cloud_policy_client());
    validator.ExpectSensitiveDataEvent(
        /*url*/ "",
        /*tab_url*/ "",
        /*source*/ kSourceVolumeInfo.fs_config_string,
        /*destination*/ kDestinationVolumeInfo.fs_config_string,
        /*filename*/ "foo.doc",
        // printf "content" | sha256sum  |  tr '[:lower:]' '[:upper:]'
        /*sha*/
        "ED7002B439E9AC845F22357D822BAC1444730FBDB6016D3EC9432297B9EC9F73",
        /*trigger*/
        extensions::SafeBrowsingPrivateEventRouter::kTriggerFileTransfer,
        /*dlp_verdict*/ response.results()[0],
        /*mimetype*/ DocMimeTypes(),
        /*size*/ std::string("content").size(),
        /*result*/
        safe_browsing::EventResultToString(safe_browsing::EventResult::WARNED),
        /*username*/ kUserName,
        /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
        /*scan_id*/ scan_id,
        /*content_transfer_method*/ std::nullopt,
        /*user_justification*/ std::nullopt);

    ScanUpload(source_url, destination_directory_url_);
  }

  EXPECT_TRUE(file_transfer_analysis_delegate_
                  ->GetAnalysisResultAfterScan(source_directory_url_)
                  .IsUnknown());

  // Checks that some scanning was performed.
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());

  auto warned_files = file_transfer_analysis_delegate_->GetWarnedFiles();
  ASSERT_EQ(warned_files.size(), 1ul);
  EXPECT_EQ(paths[0], warned_files[0].path());

  {
    // Check reporting of bypass.
    test::EventReportValidator validator(cloud_policy_client());
    validator.ExpectSensitiveDataEvent(
        /*url*/ "",
        /*tab_url*/ "",
        /*source*/ kSourceVolumeInfo.fs_config_string,
        /*destination*/ kDestinationVolumeInfo.fs_config_string,
        /*filename*/ "foo.doc",
        // printf "content" | sha256sum  |  tr '[:lower:]' '[:upper:]'
        /*sha*/
        "ED7002B439E9AC845F22357D822BAC1444730FBDB6016D3EC9432297B9EC9F73",
        /*trigger*/
        extensions::SafeBrowsingPrivateEventRouter::kTriggerFileTransfer,
        /*dlp_verdict*/ response.results()[0],
        /*mimetype*/ DocMimeTypes(),
        /*size*/ std::string("content").size(),
        /*result*/
        safe_browsing::EventResultToString(
            safe_browsing::EventResult::BYPASSED),
        /*username*/ kUserName,
        /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
        /*scan_id*/ scan_id,
        /*content_transfer_method*/ std::nullopt,
        /*user_justification*/ kUserJustification);

    file_transfer_analysis_delegate_->BypassWarnings(kUserJustification);
  }
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest, CustomWarningSettingsUnset) {
  // By default the custom warning message and the learn more URL are not set,
  // and a user justification is not required to bypass a warning.
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc")}, source_directory_url_.path());

  // Mark all files and text with failed scans.
  std::string scan_id = "scan_id";
  ContentAnalysisResponse response =
      test::FakeContentAnalysisDelegate::DlpResponse(
          ContentAnalysisResponse::Result::SUCCESS, "rule",
          TriggeredRule::WARN);
  response.set_request_token(scan_id);

  SetDLPResponse(response);

  storage::FileSystemURL source_url = PathToFileSystemURL(paths[0]);
  ScanUpload(source_url, destination_directory_url_);

  ASSERT_EQ(
      file_transfer_analysis_delegate_->BypassRequiresJustification(kDlpTag),
      false);
  ASSERT_FALSE(
      file_transfer_analysis_delegate_->GetCustomMessage(kDlpTag).has_value());
  ASSERT_FALSE(file_transfer_analysis_delegate_->GetCustomLearnMoreUrl(kDlpTag)
                   .has_value());

  ASSERT_EQ(file_transfer_analysis_delegate_->BypassRequiresJustification(
                kMalwareTag),
            false);
  ASSERT_FALSE(file_transfer_analysis_delegate_->GetCustomMessage(kMalwareTag)
                   .has_value());
  ASSERT_FALSE(
      file_transfer_analysis_delegate_->GetCustomLearnMoreUrl(kMalwareTag)
          .has_value());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest, CustomWarningSettingsSet) {
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc")}, source_directory_url_.path());

  // Setup a policy that sets the custom warning message, the learn more URL,
  // and requires a user justification to bypass warnings.
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), enterprise_connectors::FILE_TRANSFER,
      R"(
        {
          "service_provider": "google",
          "enable": [
            {
              "source_destination_list": [
                {
                  "sources": [{
                    "file_system_type": "*"
                  }],
                  "destinations": [{
                    "file_system_type": "*"
                  }]
                }
              ],
              "tags": ["dlp", "malware"]
            }
          ],
          "block_until_verdict": 1,
          "custom_messages" : [
            {
              "learn_more_url": "https://learnmore-dlp.com",
              "message": "Custom message dlp",
              "tag": "dlp"
            }, {
              "learn_more_url": "https://learnmore-malware.com",
              "message": "Custom message malware",
              "tag": "malware"
            }
          ],
          "require_justification_tags": [
            "dlp",
            "malware"
          ]
        }
      )");

  // Mark all files and text with failed scans.
  std::string scan_id = "scan_id";
  ContentAnalysisResponse response =
      test::FakeContentAnalysisDelegate::DlpResponse(
          ContentAnalysisResponse::Result::SUCCESS, "rule",
          TriggeredRule::WARN);
  response.set_request_token(scan_id);

  SetDLPResponse(response);

  storage::FileSystemURL source_url = PathToFileSystemURL(paths[0]);
  ScanUpload(source_url, destination_directory_url_);

  ASSERT_EQ(
      file_transfer_analysis_delegate_->BypassRequiresJustification(kDlpTag),
      true);
  ASSERT_EQ(file_transfer_analysis_delegate_->GetCustomMessage(kDlpTag),
            u"Custom message dlp");
  ASSERT_EQ(file_transfer_analysis_delegate_->GetCustomLearnMoreUrl(kDlpTag),
            std::optional<GURL>("https://learnmore-dlp.com"));

  ASSERT_EQ(file_transfer_analysis_delegate_->BypassRequiresJustification(
                kMalwareTag),
            true);
  ASSERT_EQ(file_transfer_analysis_delegate_->GetCustomMessage(kMalwareTag),
            u"Custom message malware");
  ASSERT_EQ(
      file_transfer_analysis_delegate_->GetCustomLearnMoreUrl(kMalwareTag),
      std::optional<GURL>("https://learnmore-malware.com"));

  const std::string wrong_tag = "wrong-tag";
  ASSERT_EQ(
      file_transfer_analysis_delegate_->BypassRequiresJustification(wrong_tag),
      false);
  ASSERT_FALSE(file_transfer_analysis_delegate_->GetCustomMessage(wrong_tag)
                   .has_value());
  ASSERT_FALSE(
      file_transfer_analysis_delegate_->GetCustomLearnMoreUrl(wrong_tag)
          .has_value());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest,
       SingleFileBlockedDlpReportOnly) {
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), FILE_TRANSFER,
      kBlockingScansForDlpAndMalwareReportOnly);

  // For report-only mode, the destination is scanned, because we perform the
  // scan after a transfer. So we create the file at the destination.
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc")}, destination_directory_url_.path());

  // Mark all files and text with failed scans.
  std::string scan_id = "scan_id";
  ContentAnalysisResponse response =
      test::FakeContentAnalysisDelegate::DlpResponse(
          ContentAnalysisResponse::Result::SUCCESS, "rule",
          TriggeredRule::BLOCK);
  response.set_request_token(scan_id);

  SetDLPResponse(response);

  storage::FileSystemURL source_url = PathToFileSystemURL(
      source_directory_url_.path().Append(FILE_PATH_LITERAL("foo.doc")));
  storage::FileSystemURL destination_url = PathToFileSystemURL(paths[0]);

  // Check reporting.
  test::EventReportValidator validator(cloud_policy_client());
  validator.ExpectSensitiveDataEvent(
      /*url*/ "",
      /*tab_url*/ "",
      /*source*/ kSourceVolumeInfo.fs_config_string,
      /*destination*/ kDestinationVolumeInfo.fs_config_string,
      /*filename*/ "foo.doc",
      // printf "content" | sha256sum  |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "ED7002B439E9AC845F22357D822BAC1444730FBDB6016D3EC9432297B9EC9F73",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileTransfer,
      /*dlp_verdict*/ response.results()[0],
      /*mimetype*/ DocMimeTypes(),
      /*size*/ std::string("content").size(),
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::ALLOWED),
      /*username*/ kUserName,
      /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ scan_id,
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  ScanUpload(source_url, destination_url);

  // No checks for GetAnalysisResultAfterScan, because it's not allowed to be
  // called for report-only mode.

  // Checks that some scanning was performed.
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest, SingleFileBlockedMalware) {
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc")}, source_directory_url_.path());

  // Mark all files and text with failed scans.
  std::string scan_id = "scan_id";
  ContentAnalysisResponse response =
      test::FakeContentAnalysisDelegate::MalwareResponse(TriggeredRule::BLOCK);

  // Setting the rule_name is required for a correct value of thread_type in the
  // report.
  response.mutable_results()
      ->at(0)
      .mutable_triggered_rules()
      ->at(0)
      .set_rule_name("malware");
  response.set_request_token(scan_id);
  PathFailsDeepScan(paths[0], response);

  storage::FileSystemURL source_url = PathToFileSystemURL(paths[0]);

  // Check reporting.
  test::EventReportValidator validator(cloud_policy_client());
  validator.ExpectDangerousDeepScanningResult(
      /*url*/ "",
      /*tab_url*/ "",
      /*source*/ kSourceVolumeInfo.fs_config_string,
      /*destination*/ kDestinationVolumeInfo.fs_config_string,
      /*filename*/ "foo.doc",
      // printf "content" | sha256sum  |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "ED7002B439E9AC845F22357D822BAC1444730FBDB6016D3EC9432297B9EC9F73",
      /*thread_type*/ "DANGEROUS",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileTransfer,
      /*mimetype*/ DocMimeTypes(),
      /*size*/ std::string("content").size(),
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ scan_id);

  ScanUpload(source_url, destination_directory_url_);

  EXPECT_TRUE(file_transfer_analysis_delegate_
                  ->GetAnalysisResultAfterScan(source_directory_url_)
                  .IsUnknown());
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(source_url)
          .IsBlocked());
  // Checks that some scanning was performed.
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest, SingleFileAllowedEncryptedd) {
  // UtilityThreadHelper needed to verify that the file is encrypted.
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII("encrypted.zip");
  base::FilePath path = source_directory_url_.path().Append("encrypted.zip");
  base::CopyFile(test_zip, path);

  // Mark all files and text with successful scans.
  std::string scan_id = "scan_id";
  ContentAnalysisResponse response =
      test::FakeContentAnalysisDelegate::DlpResponse(
          ContentAnalysisResponse::Result::SUCCESS, "rule",
          TriggeredRule::REPORT_ONLY);
  response.set_request_token(scan_id);

  SetDLPResponse(response);

  storage::FileSystemURL source_url = PathToFileSystemURL(path);

  // Check reporting.
  test::EventReportValidator validator(cloud_policy_client());
  // When resumable upload is in use and the policy does not block encrypted
  // files by default, the file's metadata is uploaded for scanning.
  validator.ExpectSensitiveDataEvent(
      /*url*/ "",
      /*tab_url*/ "",
      /*source*/ kSourceVolumeInfo.fs_config_string,
      /*destination*/ kDestinationVolumeInfo.fs_config_string,
      /*filename*/ "encrypted.zip",
      // printf "content" | sha256sum  |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "701FCEA8B2112FFAB257A8A8DFD3382ABCF047689AB028D42903E3B3AA488D9A",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileTransfer,
      /*dlp_verdict*/ response.results()[0],
      /*mimetype*/ ZipMimeTypes(),
      /*size*/ 20015,
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::ALLOWED),
      /*username*/ kUserName,
      /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ scan_id,
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  ScanUpload(source_url, destination_directory_url_);

  EXPECT_TRUE(file_transfer_analysis_delegate_
                  ->GetAnalysisResultAfterScan(source_directory_url_)
                  .IsUnknown());
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(source_url)
          .IsAllowed());
  // Checks that some scanning was performed.
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest,
       DirectoryWithSingleFileAllowed) {
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc")}, source_directory_url_.path());

  storage::FileSystemURL source_url = PathToFileSystemURL(paths[0]);

  test::EventReportValidator validator(cloud_policy_client());
  validator.ExpectNoReport();

  ScanUpload(source_directory_url_, destination_directory_url_);

  EXPECT_TRUE(file_transfer_analysis_delegate_
                  ->GetAnalysisResultAfterScan(source_directory_url_)
                  .IsUnknown());
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(source_url)
          .IsAllowed());
  // Checks that some scanning was performed.
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest,
       DirectoryWithSingleFileBlocked) {
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc")}, source_directory_url_.path());

  // Mark all files and text with failed scans.
  std::string scan_id = "scan_id";
  ContentAnalysisResponse response =
      test::FakeContentAnalysisDelegate::DlpResponse(
          ContentAnalysisResponse::Result::SUCCESS, "rule",
          TriggeredRule::BLOCK);
  response.set_request_token(scan_id);
  SetDLPResponse(response);

  // Check reporting.
  test::EventReportValidator validator(cloud_policy_client());
  validator.ExpectSensitiveDataEvent(
      /*url*/ "",
      /*tab_url*/ "",
      /*source*/ kSourceVolumeInfo.fs_config_string,
      /*destination*/ kDestinationVolumeInfo.fs_config_string,
      /*filename*/ "foo.doc",
      // printf "content" | sha256sum  |  tr '[:lower:]' '[:upper:]'
      /*sha*/
      "ED7002B439E9AC845F22357D822BAC1444730FBDB6016D3EC9432297B9EC9F73",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileTransfer,
      /*dlp_verdict*/ response.results()[0],
      /*mimetype*/ DocMimeTypes(),
      /*size*/ std::string("content").size(),
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ scan_id,
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  ScanUpload(source_directory_url_, destination_directory_url_);

  EXPECT_TRUE(file_transfer_analysis_delegate_
                  ->GetAnalysisResultAfterScan(source_directory_url_)
                  .IsUnknown());
  EXPECT_TRUE(file_transfer_analysis_delegate_
                  ->GetAnalysisResultAfterScan(PathToFileSystemURL(paths[0]))
                  .IsBlocked());
  // Checks that some scanning was performed.
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest,
       DirectoryWithMultipleFilesAllAllowed) {
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc"), FILE_PATH_LITERAL("baa.doc"),
       FILE_PATH_LITERAL("blub.doc")},
      source_directory_url_.path());

  test::EventReportValidator validator(cloud_policy_client());
  validator.ExpectNoReport();

  ScanUpload(source_directory_url_, destination_directory_url_);

  EXPECT_TRUE(file_transfer_analysis_delegate_
                  ->GetAnalysisResultAfterScan(source_directory_url_)
                  .IsUnknown());
  for (const auto& path : paths) {
    EXPECT_TRUE(file_transfer_analysis_delegate_
                    ->GetAnalysisResultAfterScan(PathToFileSystemURL(path))
                    .IsAllowed());
  }
  // Checks that some scanning was performed.
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest,
       DirectoryWithMultipleFilesAllBlocked) {
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc"), FILE_PATH_LITERAL("baa.doc"),
       FILE_PATH_LITERAL("blub.doc")},
      source_directory_url_.path());

  // Mark all files and text with failed scans.
  std::string scan_id = "scan_id";
  ContentAnalysisResponse response =
      test::FakeContentAnalysisDelegate::DlpResponse(
          ContentAnalysisResponse::Result::SUCCESS, "rule",
          TriggeredRule::BLOCK);
  response.set_request_token(scan_id);
  SetDLPResponse(response);

  storage::FileSystemURL source_url = PathToFileSystemURL(paths[0]);

  test::EventReportValidator validator(cloud_policy_client());
  validator.ExpectSensitiveDataEvents(
      /*url*/ "",
      /*tab_url*/ "",
      /*source*/ kSourceVolumeInfo.fs_config_string,
      /*destination*/ kDestinationVolumeInfo.fs_config_string,
      /*filenames*/ {"foo.doc", "baa.doc", "blub.doc"},
      // printf "content" | sha256sum  |  tr '[:lower:]' '[:upper:]'
      /*sha256s*/
      {"ED7002B439E9AC845F22357D822BAC1444730FBDB6016D3EC9432297B9EC9F73",
       "ED7002B439E9AC845F22357D822BAC1444730FBDB6016D3EC9432297B9EC9F73",
       "ED7002B439E9AC845F22357D822BAC1444730FBDB6016D3EC9432297B9EC9F73"},
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileTransfer,
      /*dlp_verdicts*/
      {response.results()[0], response.results()[0], response.results()[0]},
      /*mimetype*/ DocMimeTypes(),
      /*size*/ std::string("content").size(),
      /*result*/
      {safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
       safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
       safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED)},
      /*username*/ kUserName,
      /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
      /*scan_ids*/ {scan_id, scan_id, scan_id},
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  ScanUpload(source_directory_url_, destination_directory_url_);

  EXPECT_TRUE(file_transfer_analysis_delegate_
                  ->GetAnalysisResultAfterScan(source_directory_url_)
                  .IsUnknown());
  for (const auto& path : paths) {
    EXPECT_TRUE(file_transfer_analysis_delegate_
                    ->GetAnalysisResultAfterScan(PathToFileSystemURL(path))
                    .IsBlocked());
  }
  // Checks that some scanning was performed.
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest,
       DirectoryWithMultipleFilesSomeBlocked) {
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("good1.doc"), FILE_PATH_LITERAL("good2.doc"),
       FILE_PATH_LITERAL("bad1.doc"), FILE_PATH_LITERAL("bad2.doc"),
       FILE_PATH_LITERAL("a_good1.doc")},
      source_directory_url_.path());

  std::string scan_id = "scan_id";
  ContentAnalysisResponse::Result result;
  // Mark all files and text with failed scans.
  for (const auto& path : paths) {
    if (path.value().find("bad") != std::string::npos) {
      ContentAnalysisResponse response =
          test::FakeContentAnalysisDelegate::DlpResponse(
              ContentAnalysisResponse::Result::SUCCESS, "rule",
              TriggeredRule::BLOCK);
      response.set_request_token(scan_id);
      PathFailsDeepScan(path, response);
      result = response.results()[0];
    }
  }

  storage::FileSystemURL source_url = PathToFileSystemURL(paths[0]);

  test::EventReportValidator validator(cloud_policy_client());
  validator.ExpectSensitiveDataEvents(
      /*url*/ "",
      /*tab_url*/ "",
      /*source*/ kSourceVolumeInfo.fs_config_string,
      /*destination*/ kDestinationVolumeInfo.fs_config_string,
      /*filenames*/ {"bad1.doc", "bad2.doc"},
      // printf "content" | sha256sum  |  tr '[:lower:]' '[:upper:]'
      /*sha256s*/
      {"ED7002B439E9AC845F22357D822BAC1444730FBDB6016D3EC9432297B9EC9F73",
       "ED7002B439E9AC845F22357D822BAC1444730FBDB6016D3EC9432297B9EC9F73"},
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileTransfer,
      /*dlp_verdicts*/
      {result, result},
      /*mimetype*/ DocMimeTypes(),
      /*size*/ std::string("content").size(),
      /*result*/
      {safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
       safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED)},
      /*username*/ kUserName,
      /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
      /*scan_ids*/ {scan_id, scan_id},
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  ScanUpload(source_directory_url_, destination_directory_url_);

  EXPECT_TRUE(file_transfer_analysis_delegate_
                  ->GetAnalysisResultAfterScan(source_directory_url_)
                  .IsUnknown());
  for (const auto& path : paths) {
    if (path.value().find("bad") != std::string::npos) {
      EXPECT_TRUE(file_transfer_analysis_delegate_
                      ->GetAnalysisResultAfterScan(PathToFileSystemURL(path))
                      .IsBlocked());
    } else {
      EXPECT_TRUE(file_transfer_analysis_delegate_
                      ->GetAnalysisResultAfterScan(PathToFileSystemURL(path))
                      .IsAllowed());
    }
  }

  // Checks that some scanning was performed.
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest, DirectoryTreeSomeBlocked) {
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("good1.doc"), FILE_PATH_LITERAL("good2.doc"),
       FILE_PATH_LITERAL("bad1.doc"), FILE_PATH_LITERAL("bad2.doc"),
       FILE_PATH_LITERAL("a_good1.doc"), FILE_PATH_LITERAL("a/good1.doc"),
       FILE_PATH_LITERAL("a/a_good1.doc"), FILE_PATH_LITERAL("a/e/bad2b.doc"),
       FILE_PATH_LITERAL("a/e/a_good1.doc"),
       FILE_PATH_LITERAL("a/e/a_bad1.doc"), FILE_PATH_LITERAL("b/good2.doc"),
       FILE_PATH_LITERAL("b/bad1b.doc")},
      source_directory_url_.path());

  std::vector<std::string> expected_filenames;
  std::vector<std::string> expected_shas;
  std::vector<ContentAnalysisResponse::Result> expected_dlp_verdicts;
  std::vector<std::string> expected_results;
  std::vector<std::string> expected_scan_ids;

  // Mark all files and text with failed scans.
  for (size_t i = 0; i < paths.size(); ++i) {
    auto&& path = paths[i];
    if (path.value().find("bad") != std::string::npos) {
      ContentAnalysisResponse response =
          test::FakeContentAnalysisDelegate::DlpResponse(
              ContentAnalysisResponse::Result::SUCCESS,
              base::StrCat({"rule", base::NumberToString(i)}),
              TriggeredRule::BLOCK);
      std::string request_token =
          base::StrCat({"scan_id", base::NumberToString(i)});
      response.set_request_token(request_token);
      PathFailsDeepScan(path, response);

      expected_filenames.push_back(path.BaseName().AsUTF8Unsafe());
      expected_shas.push_back(
          "ED7002B439E9AC845F22357D822BAC1444730FBDB6016D3EC9432297B9EC9F73");
      expected_dlp_verdicts.push_back(response.results()[0]);
      expected_results.push_back(safe_browsing::EventResultToString(
          safe_browsing::EventResult::BLOCKED));
      expected_scan_ids.push_back(request_token);
    }
  }

  storage::FileSystemURL source_url = PathToFileSystemURL(paths[0]);

  test::EventReportValidator validator(cloud_policy_client());
  validator.ExpectSensitiveDataEvents(
      /*url*/ "",
      /*tab_url*/ "",
      /*source*/ kSourceVolumeInfo.fs_config_string,
      /*destination*/ kDestinationVolumeInfo.fs_config_string,
      /*filenames*/ expected_filenames,
      // printf "content" | sha256sum  |  tr '[:lower:]' '[:upper:]'
      /*sha256s*/
      expected_shas,
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileTransfer,
      /*dlp_verdicts*/
      expected_dlp_verdicts,
      /*mimetype*/ DocMimeTypes(),
      /*size*/ std::string("content").size(),
      /*result*/
      expected_results,
      /*username*/ kUserName,
      /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
      /*scan_ids*/ expected_scan_ids,
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  ScanUpload(source_directory_url_, destination_directory_url_);

  EXPECT_TRUE(file_transfer_analysis_delegate_
                  ->GetAnalysisResultAfterScan(source_directory_url_)
                  .IsUnknown());
  for (const auto& path : paths) {
    if (path.value().find("bad") != std::string::npos) {
      EXPECT_TRUE(file_transfer_analysis_delegate_
                      ->GetAnalysisResultAfterScan(PathToFileSystemURL(path))
                      .IsBlocked());
    } else {
      EXPECT_TRUE(file_transfer_analysis_delegate_
                      ->GetAnalysisResultAfterScan(PathToFileSystemURL(path))
                      .IsAllowed());
    }
  }
  // Checks that some scanning was performed.
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest,
       DirectoryTreeSomeBlockedSomeWarned) {
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("good1.doc"), FILE_PATH_LITERAL("good2.doc"),
       FILE_PATH_LITERAL("bad1.doc"), FILE_PATH_LITERAL("bad2.doc"),
       FILE_PATH_LITERAL("warn1.doc"), FILE_PATH_LITERAL("warn2.doc"),
       FILE_PATH_LITERAL("a_good1.doc"), FILE_PATH_LITERAL("a/good1.doc"),
       FILE_PATH_LITERAL("a/a_good1.doc"), FILE_PATH_LITERAL("a/e/bad2b.doc"),
       FILE_PATH_LITERAL("a/e/a_good1.doc"),
       FILE_PATH_LITERAL("a/e/a_bad1.doc"),
       FILE_PATH_LITERAL("a/e/a_warn1.doc"),
       FILE_PATH_LITERAL("a/e/a_warn2.doc"), FILE_PATH_LITERAL("b/good2.doc"),
       FILE_PATH_LITERAL("b/bad1b.doc")},
      source_directory_url_.path());

  std::vector<storage::FileSystemURL> expected_warned_files;

  {
    std::vector<std::string> expected_filenames;
    std::vector<std::string> expected_shas;
    std::vector<ContentAnalysisResponse::Result> expected_dlp_verdicts;
    std::vector<std::string> expected_results;
    std::vector<std::string> expected_scan_ids;

    // Mark all files and text with failed scans.
    for (size_t i = 0; i < paths.size(); ++i) {
      auto&& path = paths[i];
      bool should_block = path.value().find("bad") != std::string::npos;
      bool should_warn = path.value().find("warn") != std::string::npos;
      if (should_block || should_warn) {
        ContentAnalysisResponse response =
            test::FakeContentAnalysisDelegate::DlpResponse(
                ContentAnalysisResponse::Result::SUCCESS,
                base::StrCat({"rule", base::NumberToString(i)}),
                should_block ? TriggeredRule::BLOCK : TriggeredRule::WARN);
        std::string request_token =
            base::StrCat({"scan_id", base::NumberToString(i)});
        response.set_request_token(request_token);
        PathFailsDeepScan(path, response);

        expected_filenames.push_back(path.BaseName().AsUTF8Unsafe());
        expected_shas.push_back(
            "ED7002B439E9AC845F22357D822BAC1444730FBDB6016D3EC9432297B9EC9F73");
        expected_dlp_verdicts.push_back(response.results()[0]);
        if (should_block) {
          expected_results.push_back(safe_browsing::EventResultToString(
              safe_browsing::EventResult::BLOCKED));
        } else {
          ASSERT_TRUE(should_warn);
          expected_results.push_back(safe_browsing::EventResultToString(
              safe_browsing::EventResult::WARNED));
        }
        expected_scan_ids.push_back(request_token);
      }

      if (should_warn) {
        expected_warned_files.emplace_back(PathToFileSystemURL(path));
      }
    }

    test::EventReportValidator validator(cloud_policy_client());
    validator.ExpectSensitiveDataEvents(
        /*url*/ "",
        /*tab_url*/ "",
        /*source*/ kSourceVolumeInfo.fs_config_string,
        /*destination*/ kDestinationVolumeInfo.fs_config_string,
        /*filenames*/ expected_filenames,
        // printf "content" | sha256sum  |  tr '[:lower:]' '[:upper:]'
        /*sha256s*/
        expected_shas,
        /*trigger*/
        extensions::SafeBrowsingPrivateEventRouter::kTriggerFileTransfer,
        /*dlp_verdicts*/
        expected_dlp_verdicts,
        /*mimetype*/ DocMimeTypes(),
        /*size*/ std::string("content").size(),
        /*result*/
        expected_results,
        /*username*/ kUserName,
        /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
        /*scan_ids*/ expected_scan_ids,
        /*content_transfer_method*/ std::nullopt,
        /*user_justification*/ std::nullopt);

    ScanUpload(source_directory_url_, destination_directory_url_);
  }

  auto warned_files = file_transfer_analysis_delegate_->GetWarnedFiles();
  EXPECT_THAT(warned_files,
              ::testing::UnorderedElementsAreArray(expected_warned_files));

  EXPECT_TRUE(file_transfer_analysis_delegate_
                  ->GetAnalysisResultAfterScan(source_directory_url_)
                  .IsUnknown());
  for (const auto& path : paths) {
    bool should_block = path.value().find("bad") != std::string::npos;
    bool should_warn = path.value().find("warn") != std::string::npos;
    if (should_block || should_warn) {
      EXPECT_TRUE(file_transfer_analysis_delegate_
                      ->GetAnalysisResultAfterScan(PathToFileSystemURL(path))
                      .IsBlocked());
    } else {
      EXPECT_TRUE(file_transfer_analysis_delegate_
                      ->GetAnalysisResultAfterScan(PathToFileSystemURL(path))
                      .IsAllowed());
    }
  }

  {
    std::vector<std::string> expected_filenames;
    std::vector<std::string> expected_shas;
    std::vector<ContentAnalysisResponse::Result> expected_dlp_verdicts;
    std::vector<std::string> expected_results;
    std::vector<std::string> expected_scan_ids;

    // Mark all files and text with failed scans.
    for (size_t i = 0; i < paths.size(); ++i) {
      auto&& path = paths[i];
      bool should_warn = path.value().find("warn") != std::string::npos;
      if (should_warn) {
        ContentAnalysisResponse response =
            test::FakeContentAnalysisDelegate::DlpResponse(
                ContentAnalysisResponse::Result::SUCCESS,
                base::StrCat({"rule", base::NumberToString(i)}),
                TriggeredRule::WARN);
        std::string request_token =
            base::StrCat({"scan_id", base::NumberToString(i)});
        response.set_request_token(request_token);

        expected_filenames.push_back(path.BaseName().AsUTF8Unsafe());
        expected_shas.push_back(
            "ED7002B439E9AC845F22357D822BAC1444730FBDB6016D3EC9432297B9EC9F73");
        expected_dlp_verdicts.push_back(response.results()[0]);

        expected_results.push_back(safe_browsing::EventResultToString(
            safe_browsing::EventResult::BYPASSED));

        expected_scan_ids.push_back(request_token);
      }
    }

    test::EventReportValidator validator(cloud_policy_client());
    validator.ExpectSensitiveDataEvents(
        /*url*/ "",
        /*tab_url*/ "",
        /*source*/ kSourceVolumeInfo.fs_config_string,
        /*destination*/ kDestinationVolumeInfo.fs_config_string,
        /*filenames*/ expected_filenames,
        // printf "content" | sha256sum  |  tr '[:lower:]' '[:upper:]'
        /*sha256s*/
        expected_shas,
        /*trigger*/
        extensions::SafeBrowsingPrivateEventRouter::kTriggerFileTransfer,
        /*dlp_verdicts*/
        expected_dlp_verdicts,
        /*mimetype*/ DocMimeTypes(),
        /*size*/ std::string("content").size(),
        /*result*/
        expected_results,
        /*username*/ kUserName,
        /*profile_identifier*/ profile_->GetPath().AsUTF8Unsafe(),
        /*scan_ids*/ expected_scan_ids,
        /*content_transfer_method*/ std::nullopt,
        /*user_justification*/ kUserJustification);

    file_transfer_analysis_delegate_->BypassWarnings(kUserJustification);
  }

  // Should now no longer block bypassed files.
  EXPECT_TRUE(file_transfer_analysis_delegate_
                  ->GetAnalysisResultAfterScan(source_directory_url_)
                  .IsUnknown());
  for (const auto& path : paths) {
    bool should_block = path.value().find("bad") != std::string::npos;
    if (should_block) {
      EXPECT_TRUE(file_transfer_analysis_delegate_
                      ->GetAnalysisResultAfterScan(PathToFileSystemURL(path))
                      .IsBlocked());
    } else {
      EXPECT_TRUE(file_transfer_analysis_delegate_
                      ->GetAnalysisResultAfterScan(PathToFileSystemURL(path))
                      .IsAllowed());
    }
  }

  // Checks that some scanning was performed.
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

}  // namespace enterprise_connectors
