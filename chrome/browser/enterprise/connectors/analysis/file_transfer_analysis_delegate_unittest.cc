// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/file_transfer_analysis_delegate.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "ash/components/disks/disk_mount_manager.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/analysis/fake_files_request_handler.h"
#include "chrome/browser/enterprise/connectors/analysis/files_request_handler.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr char kDmToken[] = "dm_token";
base::TimeDelta kResponseDelay = base::Seconds(0);

storage::FileSystemURL GetEmptyTestSrcUrl() {
  return storage::FileSystemURL();
}
storage::FileSystemURL GetEmptyTestDestUrl() {
  return storage::FileSystemURL();
}

// TODO(crbug.com/1339194): replace url_list with source_destination_list.
constexpr char kBlockingScansForDlpAndMalware[] = R"(
{
  "service_provider": "google",
  "enable": [
    {
      "url_list": [
        "*"
      ],
      "tags": ["dlp", "malware"]
    }
  ],
  "block_until_verdict": 1
})";

constexpr char kBlockingScansForDlp[] = R"(
{
  "service_provider": "google",
  "enable": [
    {
      "url_list": [
        "*"
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
      "url_list": [
        "*"
      ],
      "tags": ["malware"]
    }
  ],
  "block_until_verdict": 1
})";

constexpr char kNothingEnabled[] = R"({ "service_provider": "google" })";

class ScopedSetDMToken {
 public:
  explicit ScopedSetDMToken(const policy::DMToken& dm_token) {
    SetDMTokenForTesting(dm_token);
  }
  ~ScopedSetDMToken() {
    SetDMTokenForTesting(policy::DMToken::CreateEmptyTokenForTesting());
  }
};

struct VolumeInfo {
  file_manager::VolumeType type;
  absl::optional<guest_os::VmType> vm_type;
  std::string fs_config_string;
};

class BaseTest : public testing::Test {
 public:
  BaseTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");

    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindLambdaForTesting([](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<file_manager::VolumeManager>(
                  Profile::FromBrowserContext(context), nullptr, nullptr,
                  ash::disks::DiskMountManager::GetInstance(), nullptr,
                  file_manager::VolumeManager::GetMtpStorageInfoCallback()));
        }));

    // Takes ownership of `disk_mount_manager_`, but Shutdown() must be called.
    ash::disks::DiskMountManager::InitializeForTesting(
        new file_manager::FakeDiskMountManager);

    // Register volumes.
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    file_system_context_ = storage::CreateFileSystemContextForTesting(
        nullptr, temp_dir_.GetPath());
  }

  ~BaseTest() override {
    profile_manager_.DeleteAllTestingProfiles();
    ash::disks::DiskMountManager::Shutdown();
  }

  void EnableFeatures() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures({kEnterpriseConnectorsEnabled}, {});
  }

  void DisableFeatures() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures({}, {kEnterpriseConnectorsEnabled});
  }

  storage::FileSystemURL PathToFileSystemURL(base::FilePath path) {
    return storage::FileSystemURL::CreateForTest(
        kTestStorageKey, storage::kFileSystemTypeLocal, path);
  }

  Profile* profile() { return profile_; }

  void RunUntilDone() { run_loop_.Run(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_dir_;
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome://abc");
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  base::RunLoop run_loop_;
};

enum PrefState {
  NO_PREF,
  NOTHING_ENABLED_PREF,
  DLP_PREF,
  MALWARE_PREF,
  DLP_MALWARE_PREF,
};

using TestingTuple = std::tuple</*Feature enabled*/ bool,
                                /*Token valid*/ bool,
                                /*Pref State*/ PrefState,
                                /*Enable unrelated Pref*/ bool>;

static auto testingTupleToString = [](const auto& info) {
  // Can use info.param here to generate the test suffix
  std::string name;
  auto [feature_enabled, token_valid, pref_state, unrelated_pref] = info.param;
  if (!feature_enabled) {
    name += "NoFeature";
  }
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
  bool GetFeatureEnabled() { return std::get<0>(GetParam()); }
  bool GetTokenValid() { return std::get<1>(GetParam()); }
  PrefState GetPrefState() { return std::get<2>(GetParam()); }
  bool GetUnrelatedPrefEnabled() { return std::get<3>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(,
                         FileTransferAnalysisDelegateIsEnabledTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Values(NO_PREF,
                                                          NOTHING_ENABLED_PREF,
                                                          DLP_PREF,
                                                          MALWARE_PREF,
                                                          DLP_MALWARE_PREF),
                                          testing::Bool()),
                         testingTupleToString);

TEST_P(FileTransferAnalysisDelegateIsEnabledTest, Enabled) {
  if (GetFeatureEnabled()) {
    EnableFeatures();
  } else {
    DisableFeatures();
  }
  ScopedSetDMToken scoped_dm_token(
      GetTokenValid() ? policy::DMToken::CreateValidTokenForTesting(kDmToken)
                      : policy::DMToken::CreateInvalidTokenForTesting());
  switch (GetPrefState()) {
    case NO_PREF:
      break;
    case NOTHING_ENABLED_PREF:
      safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_TRANSFER,
                                          kNothingEnabled);
      break;
    case DLP_PREF:
      safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_TRANSFER,
                                          kBlockingScansForDlp);
      break;
    case MALWARE_PREF:
      safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_TRANSFER,
                                          kBlockingScansForMalware);
      break;
    case DLP_MALWARE_PREF:
      safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_TRANSFER,
                                          kBlockingScansForDlpAndMalware);
      break;
  }
  if (GetUnrelatedPrefEnabled()) {
    // Set for wrong policy (FILE_DOWNLOADED instead of FILE_TRANSFER)!
    safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_DOWNLOADED,
                                        kBlockingScansForDlpAndMalware);
  }

  auto settings = FileTransferAnalysisDelegate::IsEnabled(
      profile(), GetEmptyTestSrcUrl(), GetEmptyTestDestUrl());

  if (!GetFeatureEnabled() || !GetTokenValid() || GetPrefState() == NO_PREF ||
      GetPrefState() == NOTHING_ENABLED_PREF) {
    EXPECT_FALSE(settings.has_value());
  } else {
    ASSERT_TRUE(settings.has_value());
    if (GetPrefState() == DLP_PREF || GetPrefState() == DLP_MALWARE_PREF) {
      EXPECT_TRUE(settings.value().tags.count("dlp"));
    }
    if (GetPrefState() == MALWARE_PREF || GetPrefState() == DLP_MALWARE_PREF) {
      EXPECT_TRUE(settings.value().tags.count("malware"));
    }
  }
}

using FileTransferAnalysisDelegateIsEnabledTestSameFileSystem = BaseTest;

TEST_F(FileTransferAnalysisDelegateIsEnabledTestSameFileSystem,
       DlpMalwareDisabledForSameFileSystem) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_TRANSFER,
                                      kBlockingScansForDlpAndMalware);

  auto settings = FileTransferAnalysisDelegate::IsEnabled(
      profile(), PathToFileSystemURL(temp_dir_.GetPath()),
      PathToFileSystemURL(temp_dir_.GetPath()));

  EXPECT_FALSE(settings.has_value());
}

class FileTransferAnalysisDelegateAuditOnlyTest : public BaseTest {
 public:
  FileTransferAnalysisDelegateAuditOnlyTest() = default;

 protected:
  void SetUp() override {
    BaseTest::SetUp();

    EnableFeatures();
    safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_TRANSFER,
                                        kBlockingScansForDlpAndMalware);

    FilesRequestHandler::SetFactoryForTesting(base::BindRepeating(
        &FakeFilesRequestHandler::Create,
        base::BindRepeating(
            &FileTransferAnalysisDelegateAuditOnlyTest::FakeFileUploadCallback,
            base::Unretained(this))));

    source_directory_url_ =
        PathToFileSystemURL(temp_dir_.GetPath().Append("source"));
    ASSERT_TRUE(base::CreateDirectory(source_directory_url_.path()));
    destination_directory_url_ =
        PathToFileSystemURL(temp_dir_.GetPath().Append("destination"));
    ASSERT_TRUE(base::CreateDirectory(destination_directory_url_.path()));
  }

  void ScanUpload(const storage::FileSystemURL& source_url,
                  const storage::FileSystemURL& destination_url) {
    // The access point is only used for metrics, so its value doesn't affect
    // the tests in this file and can always be the same.
    file_transfer_analysis_delegate_ =
        std::make_unique<FileTransferAnalysisDelegate>(
            safe_browsing::DeepScanAccessPoint::FILE_TRANSFER, source_url,
            destination_url, profile_, file_system_context_.get(),
            GetSettings(), run_loop_.QuitClosure());

    file_transfer_analysis_delegate_->UploadData();
    RunUntilDone();
  }

  enterprise_connectors::AnalysisSettings GetSettings() {
    auto* service =
        enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
            profile());
    // If the corresponding Connector policy isn't set, no scans can be
    // performed.
    EXPECT_TRUE(service);
    EXPECT_TRUE(service->IsConnectorEnabled(AnalysisConnector::FILE_TRANSFER));

    // TODO(crbug.com/1339194): Use file system urls here!
    auto settings =
        service->GetAnalysisSettings(GURL(), AnalysisConnector::FILE_TRANSFER);
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
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request) {
    EXPECT_FALSE(path.empty());
    EXPECT_EQ(request->device_token(), kDmToken);
    // Simulate a response.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &FilesRequestHandler::FileRequestCallbackForTesting,
            base::Unretained(file_transfer_analysis_delegate_
                                 ->GetFilesRequestHandlerForTesting()),
            path, safe_browsing::BinaryUploadService::Result::SUCCESS,
            ConnectorStatusCallback(path)),
        kResponseDelay);
  }

  ContentAnalysisResponse ConnectorStatusCallback(const base::FilePath& path) {
    // The path succeeds if it is not in the `failures_` maps.
    auto it = failures_.find(path);
    ContentAnalysisResponse response =
        it != failures_.end()
            ? it->second
            : FakeContentAnalysisDelegate::SuccessfulResponse([this]() {
                std::set<std::string> tags;
                if (!dlp_response_.has_value())
                  tags.insert("dlp");
                tags.insert("malware");
                return tags;
              }());

    if (dlp_response_.has_value()) {
      *response.add_results() = dlp_response_.value().results(0);
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
      file.WriteAtCurrentPos(content.data(), content.size());
      paths.emplace_back(path);
    }
    return paths;
  }

 protected:
  std::unique_ptr<FileTransferAnalysisDelegate>
      file_transfer_analysis_delegate_;

  storage::FileSystemURL source_directory_url_;
  storage::FileSystemURL destination_directory_url_;

 private:
  ScopedSetDMToken scoped_dm_token_{
      policy::DMToken::CreateValidTokenForTesting(kDmToken)};

  // Paths in this map will be consider to have failed deep scan checks.
  // The actual failure response is given for each path.
  std::map<base::FilePath, ContentAnalysisResponse> failures_;

  // DLP response to ovewrite in the callback if present.
  absl::optional<ContentAnalysisResponse> dlp_response_ = absl::nullopt;
};

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest, InvalidPath) {
  storage::FileSystemURL source_url = GetEmptyTestSrcUrl();
  storage::FileSystemURL destination_url = GetEmptyTestDestUrl();
  ScanUpload(source_url, destination_url);

  EXPECT_EQ(
      FileTransferAnalysisDelegate::RESULT_UNKNOWN,
      file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(source_url));
  // Checks that there was an early return.
  EXPECT_FALSE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest, NonExistingFile) {
  storage::FileSystemURL source_url = PathToFileSystemURL(
      source_directory_url_.path().Append("does_not_exist"));

  ScanUpload(source_url, destination_directory_url_);

  // Directories should always be unknown!
  EXPECT_EQ(
      FileTransferAnalysisDelegate::RESULT_UNKNOWN,
      file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(source_url));
  // Checks that there was an early return.
  EXPECT_FALSE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest, EmptyDirectory) {
  ScanUpload(source_directory_url_, destination_directory_url_);

  // Directories should always be unknown!
  EXPECT_EQ(FileTransferAnalysisDelegate::RESULT_UNKNOWN,
            file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(
                source_directory_url_));
  // Checks that there was an early return.
  EXPECT_FALSE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest, SingleFileAllowed) {
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc")}, source_directory_url_.path());

  storage::FileSystemURL source_url = PathToFileSystemURL(paths[0]);

  ScanUpload(source_url, destination_directory_url_);

  EXPECT_EQ(FileTransferAnalysisDelegate::RESULT_UNKNOWN,
            file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(
                source_directory_url_));
  EXPECT_EQ(
      FileTransferAnalysisDelegate::RESULT_ALLOWED,
      file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(source_url));
  // Checks that some scanning was performed.
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest, SingleFileBlocked) {
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc")}, source_directory_url_.path());

  // Mark all files and text with failed scans.
  SetDLPResponse(FakeContentAnalysisDelegate::DlpResponse(
      ContentAnalysisResponse::Result::SUCCESS, "rule", TriggeredRule::BLOCK));

  storage::FileSystemURL source_url = PathToFileSystemURL(paths[0]);

  ScanUpload(source_url, destination_directory_url_);

  EXPECT_EQ(FileTransferAnalysisDelegate::RESULT_UNKNOWN,
            file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(
                source_directory_url_));
  EXPECT_EQ(
      FileTransferAnalysisDelegate::RESULT_BLOCKED,
      file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(source_url));
  // Checks that some scanning was performed.
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest,
       DirectoryWithSingleFileAllowed) {
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc")}, source_directory_url_.path());

  storage::FileSystemURL source_url = PathToFileSystemURL(paths[0]);

  ScanUpload(source_directory_url_, destination_directory_url_);

  EXPECT_EQ(FileTransferAnalysisDelegate::RESULT_UNKNOWN,
            file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(
                source_directory_url_));
  EXPECT_EQ(
      FileTransferAnalysisDelegate::RESULT_ALLOWED,
      file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(source_url));
  // Checks that some scanning was performed.
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

TEST_F(FileTransferAnalysisDelegateAuditOnlyTest,
       DirectoryWithSingleFileBlocked) {
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc")}, source_directory_url_.path());

  // Mark all files and text with failed scans.
  SetDLPResponse(FakeContentAnalysisDelegate::DlpResponse(
      ContentAnalysisResponse::Result::SUCCESS, "rule", TriggeredRule::BLOCK));

  ScanUpload(source_directory_url_, destination_directory_url_);

  EXPECT_EQ(FileTransferAnalysisDelegate::RESULT_UNKNOWN,
            file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(
                source_directory_url_));
  EXPECT_EQ(FileTransferAnalysisDelegate::RESULT_BLOCKED,
            file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(
                PathToFileSystemURL(paths[0])));
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

  ScanUpload(source_directory_url_, destination_directory_url_);

  EXPECT_EQ(FileTransferAnalysisDelegate::RESULT_UNKNOWN,
            file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(
                source_directory_url_));
  for (const auto& path : paths) {
    EXPECT_EQ(FileTransferAnalysisDelegate::RESULT_ALLOWED,
              file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(
                  PathToFileSystemURL(path)));
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
  SetDLPResponse(FakeContentAnalysisDelegate::DlpResponse(
      ContentAnalysisResponse::Result::SUCCESS, "rule", TriggeredRule::BLOCK));

  storage::FileSystemURL source_url = PathToFileSystemURL(paths[0]);

  ScanUpload(source_directory_url_, destination_directory_url_);

  EXPECT_EQ(FileTransferAnalysisDelegate::RESULT_UNKNOWN,
            file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(
                source_directory_url_));
  for (const auto& path : paths) {
    EXPECT_EQ(FileTransferAnalysisDelegate::RESULT_BLOCKED,
              file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(
                  PathToFileSystemURL(path)));
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

  // Mark all files and text with failed scans.
  for (const auto& path : paths) {
    if (path.value().find("bad") != std::string::npos) {
      PathFailsDeepScan(path, FakeContentAnalysisDelegate::DlpResponse(
                                  ContentAnalysisResponse::Result::SUCCESS,
                                  "rule", TriggeredRule::BLOCK));
    }
  }

  storage::FileSystemURL source_url = PathToFileSystemURL(paths[0]);

  ScanUpload(source_directory_url_, destination_directory_url_);

  EXPECT_EQ(FileTransferAnalysisDelegate::RESULT_UNKNOWN,
            file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(
                source_directory_url_));
  for (const auto& path : paths) {
    if (path.value().find("bad") != std::string::npos) {
      EXPECT_EQ(FileTransferAnalysisDelegate::RESULT_BLOCKED,
                file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(
                    PathToFileSystemURL(path)));
    } else {
      EXPECT_EQ(FileTransferAnalysisDelegate::RESULT_ALLOWED,
                file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(
                    PathToFileSystemURL(path)));
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
       FILE_PATH_LITERAL("a/a_good1.doc"), FILE_PATH_LITERAL("a/e/bad2.doc"),
       FILE_PATH_LITERAL("a/e/a_good1.doc"),
       FILE_PATH_LITERAL("a/e/a_bad1.doc"), FILE_PATH_LITERAL("b/good2.doc"),
       FILE_PATH_LITERAL("b/bad1.doc")},
      source_directory_url_.path());

  // Mark all files and text with failed scans.
  for (const auto& path : paths) {
    if (path.value().find("bad") != std::string::npos) {
      PathFailsDeepScan(path, FakeContentAnalysisDelegate::DlpResponse(
                                  ContentAnalysisResponse::Result::SUCCESS,
                                  "rule", TriggeredRule::BLOCK));
    }
  }

  storage::FileSystemURL source_url = PathToFileSystemURL(paths[0]);

  ScanUpload(source_directory_url_, destination_directory_url_);

  EXPECT_EQ(FileTransferAnalysisDelegate::RESULT_UNKNOWN,
            file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(
                source_directory_url_));
  for (const auto& path : paths) {
    if (path.value().find("bad") != std::string::npos) {
      EXPECT_EQ(FileTransferAnalysisDelegate::RESULT_BLOCKED,
                file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(
                    PathToFileSystemURL(path)));
    } else {
      EXPECT_EQ(FileTransferAnalysisDelegate::RESULT_ALLOWED,
                file_transfer_analysis_delegate_->GetAnalysisResultAfterScan(
                    PathToFileSystemURL(path)));
    }
  }

  // Checks that some scanning was performed.
  EXPECT_TRUE(
      file_transfer_analysis_delegate_->GetFilesRequestHandlerForTesting());
}

}  // namespace enterprise_connectors
