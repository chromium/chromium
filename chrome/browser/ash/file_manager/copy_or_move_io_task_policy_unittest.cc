// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/rand_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/dlp/test/mock_dlp_files_controller_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "chrome/browser/enterprise/connectors/analysis/mock_file_transfer_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/analysis/source_destination_test_util.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::Property;
using ::testing::Return;

namespace file_manager {
namespace io_task {

namespace {

const size_t kTestFileSize = 32;

constexpr char kBlockingScansForDlpAndMalware[] = R"(
{
  "service_provider": "google",
  "enable": [
    {
      "source_destination_list": [
        {
          "sources": [{
            "file_system_type": "PROVIDED"
          }],
          "destinations": [{
            "file_system_type": "*"
          }]
        }
      ],
      "tags": ["dlp", "malware"]
    }
  ],
  "block_until_verdict": 1
})";

constexpr std::initializer_list<
    enterprise_connectors::SourceDestinationTestingHelper::VolumeInfo>
    kVolumeInfos{
        {file_manager::VOLUME_TYPE_PROVIDED, absl::nullopt, "PROVIDED"},
        {file_manager::VOLUME_TYPE_GOOGLE_DRIVE, absl::nullopt, "GOOGLE_DRIVE"},
        {file_manager::VOLUME_TYPE_DOWNLOADS_DIRECTORY, absl::nullopt,
         "MY_FILES"},
    };

constexpr char kEmailId[] = "test@example.com";
constexpr char kGaiaId[] = "12345";

struct FileInfo {
  std::string file_contents;
  storage::FileSystemURL source_url;
  storage::FileSystemURL expected_output_url;
};

MATCHER_P(EntryStatusUrls, matcher, "") {
  std::vector<storage::FileSystemURL> urls;
  for (const auto& status : arg) {
    urls.push_back(status.url);
  }
  return testing::ExplainMatchResult(matcher, urls, result_listener);
}

MATCHER_P(EntryStatusErrors, matcher, "") {
  std::vector<absl::optional<base::File::Error>> errors;
  for (const auto& status : arg) {
    errors.push_back(status.error);
  }
  return testing::ExplainMatchResult(matcher, errors, result_listener);
}

void ExpectFileContents(base::FilePath path, std::string expected) {
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(path, &contents));
  EXPECT_EQ(expected, contents);
}

// Creates a new VolumeManager for tests.
// By default, VolumeManager KeyedService is null for testing.
std::unique_ptr<KeyedService> BuildVolumeManager(
    ash::disks::FakeDiskMountManager* disk_mount_manager,
    content::BrowserContext* context) {
  return std::make_unique<file_manager::VolumeManager>(
      Profile::FromBrowserContext(context),
      nullptr /* drive_integration_service */,
      nullptr /* power_manager_client */, disk_mount_manager,
      nullptr /* file_system_provider_service */,
      file_manager::VolumeManager::GetMtpStorageInfoCallback());
}

}  // namespace

class CopyOrMoveIOTaskWithScansTest
    : public testing::TestWithParam<OperationType> {
 public:
  static std::string ParamToString(
      const ::testing::TestParamInfo<ParamType>& info) {
    OperationType operation_type = info.param;
    std::string name;
    name += operation_type == OperationType::kCopy ? "Copy" : "Move";
    return name;
  }

 protected:
  OperationType GetOperationType() { return GetParam(); }

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test-profile");

    scoped_feature_list_.InitWithFeatures(
        {features::kFileTransferEnterpriseConnector}, {});

    // Set a device management token. It is required to enable scanning.
    // Without it, FileTransferAnalysisDelegate::IsEnabled() always
    // returns absl::nullopt.
    SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));

    // Set the analysis connector (enterprise_connectors) for FILE_TRANSFER.
    // It is also required for FileTransferAnalysisDelegate::IsEnabled() to
    // return a meaningful result.
    enterprise_connectors::test::SetAnalysisConnector(
        profile_->GetPrefs(), enterprise_connectors::FILE_TRANSFER,
        kBlockingScansForDlpAndMalware);

    source_destination_testing_helper_ =
        std::make_unique<enterprise_connectors::SourceDestinationTestingHelper>(
            profile_, kVolumeInfos);

    file_system_context_ = storage::CreateFileSystemContextForTesting(
        nullptr, source_destination_testing_helper_->GetTempDirPath());

    enterprise_connectors::FileTransferAnalysisDelegate::SetFactorForTesting(
        base::BindRepeating(
            [](base::RepeatingCallback<void(
                   enterprise_connectors::MockFileTransferAnalysisDelegate*,
                   const storage::FileSystemURL& source_url)>
                   mock_setup_callback,
               safe_browsing::DeepScanAccessPoint access_point,
               storage::FileSystemURL source_url,
               storage::FileSystemURL destination_url, Profile* profile,
               storage::FileSystemContext* file_system_context,
               enterprise_connectors::AnalysisSettings settings)
                -> std::unique_ptr<
                    enterprise_connectors::FileTransferAnalysisDelegate> {
              auto delegate = std::make_unique<::testing::StrictMock<
                  enterprise_connectors::MockFileTransferAnalysisDelegate>>(
                  access_point, source_url, destination_url, profile,
                  file_system_context, std::move(settings));

              mock_setup_callback.Run(delegate.get(), source_url);

              return delegate;
            },
            base::BindRepeating(&CopyOrMoveIOTaskWithScansTest::SetupMock,
                                base::Unretained(this))));
  }

  void TearDown() override {
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
  }

  // Setup the expectations of the mock.
  // This function uses the stored expectations from the
  // `scanning_expectations_` map.
  void SetupMock(
      enterprise_connectors::MockFileTransferAnalysisDelegate* delegate,
      const storage::FileSystemURL& source_url) {
    if (auto iter = scanning_expectations_.find(source_url);
        iter != scanning_expectations_.end()) {
      EXPECT_CALL(*delegate, UploadData(_))
          .WillOnce(
              [](base::OnceClosure callback) { std::move(callback).Run(); });

      EXPECT_CALL(*delegate, GetAnalysisResultAfterScan(source_url))
          .WillOnce(Return(iter->second));
      return;
    }

    if (auto iter = directory_scanning_expectations_.find(source_url);
        iter != directory_scanning_expectations_.end()) {
      // Scan for directory detected.
      EXPECT_CALL(*delegate, UploadData(_))
          .WillOnce(
              [](base::OnceClosure callback) { std::move(callback).Run(); });

      for (auto&& scanning_expectation : scanning_expectations_) {
        // Note: We're using IsParent here, so this doesn't support recursive
        // scanning!
        // If the current directory is a parent of the expectation, set an
        // expectation.
        if (iter->IsParent(scanning_expectation.first)) {
          EXPECT_CALL(*delegate,
                      GetAnalysisResultAfterScan(scanning_expectation.first))
              .WillOnce(Return(scanning_expectation.second));
        }
      }
      return;
    }

    // Expect no scans if we set no expectation.
    EXPECT_CALL(*delegate, UploadData(_)).Times(0);
    EXPECT_CALL(*delegate, GetAnalysisResultAfterScan(source_url)).Times(0);
  }

  // Expect a scan for the file specified using `file_info`.
  // The scan will return the specified `result'.
  void ExpectScan(FileInfo file_info,
                  enterprise_connectors::FileTransferAnalysisDelegate::
                      FileTransferAnalysisResult result) {
    ASSERT_EQ(scanning_expectations_.count(file_info.source_url), 0u);
    scanning_expectations_[file_info.source_url] = result;
  }

  // Expect a scan for the directory specified using `file_info`.
  void ExpectDirectoryScan(FileInfo file_info) {
    ASSERT_EQ(directory_scanning_expectations_.count(file_info.source_url), 0u);
    directory_scanning_expectations_.insert(file_info.source_url);
  }

  storage::FileSystemURL GetSourceFileSystemURLForEnabledVolume(
      const std::string& component) {
    return source_destination_testing_helper_->GetTestFileSystemURLForVolume(
        std::data(kVolumeInfos)[0], component);
  }

  storage::FileSystemURL GetSourceFileSystemURLForDisabledVolume(
      const std::string& component) {
    return source_destination_testing_helper_->GetTestFileSystemURLForVolume(
        std::data(kVolumeInfos)[1], component);
  }

  storage::FileSystemURL GetDestinationFileSystemURL(
      const std::string& component) {
    return source_destination_testing_helper_->GetTestFileSystemURLForVolume(
        std::data(kVolumeInfos)[2], component);
  }

  // Creates one file.
  // If `on_enabled_fs` is true, the created file lies on a file system, for
  // which scanning is enabled.
  // If `on_enabled_fs` is false, the created file lies on a file system, for
  // which scanning is disabled.
  FileInfo SetupFile(bool on_enabled_fs, std::string file_name) {
    auto file_contents = base::RandBytesAsString(kTestFileSize);
    storage::FileSystemURL url =
        on_enabled_fs ? GetSourceFileSystemURLForEnabledVolume(file_name)
                      : GetSourceFileSystemURLForDisabledVolume(file_name);
    EXPECT_TRUE(base::WriteFile(url.path(), file_contents));

    return {file_contents, url, GetDestinationFileSystemURL(file_name)};
  }

  std::vector<storage::FileSystemURL> GetExpectedOutputUrlsFromFileInfos(
      const std::vector<FileInfo>& file_infos) {
    std::vector<storage::FileSystemURL> expected_output_urls;
    for (auto&& file : file_infos) {
      expected_output_urls.push_back(file.expected_output_url);
    }
    return expected_output_urls;
  }

  std::vector<storage::FileSystemURL> GetSourceUrlsFromFileInfos(
      const std::vector<FileInfo>& file_infos) {
    std::vector<storage::FileSystemURL> source_urls;
    for (auto&& file : file_infos) {
      source_urls.push_back(file.source_url);
    }
    return source_urls;
  }

  auto GetBaseMatcher(const std::vector<FileInfo>& file_infos,
                      storage::FileSystemURL dest,
                      size_t total_num_files) {
    std::vector<storage::FileSystemURL> source_urls;
    for (auto&& file : file_infos) {
      source_urls.push_back(file.source_url);
    }
    return AllOf(
        Field(&ProgressStatus::type, GetOperationType()),
        Field(&ProgressStatus::sources,
              EntryStatusUrls(GetSourceUrlsFromFileInfos(file_infos))),
        Property(&ProgressStatus::GetDestinationFolder, dest),
        Field(&ProgressStatus::total_bytes, total_num_files * kTestFileSize));
  }

  // The progress callback may be called any number of times, this expectation
  // catches extra calls.
  void ExpectExtraProgressCallbackCalls(
      base::MockRepeatingCallback<void(const ProgressStatus&)>&
          progress_callback,
      const std::vector<FileInfo>& file_infos,
      const storage::FileSystemURL& dest,
      absl::optional<size_t> total_num_files = absl::nullopt) {
    EXPECT_CALL(
        progress_callback,
        Run(AllOf(Field(&ProgressStatus::state, State::kInProgress),
                  GetBaseMatcher(file_infos, dest,
                                 total_num_files.value_or(file_infos.size())))))
        .Times(AnyNumber());
  }

  // Expect the specified number of scanning callback calls.
  // `num_calls` has to be either 0 or 1.
  void ExpectScanningCallbackCall(
      base::MockRepeatingCallback<void(const ProgressStatus&)>&
          progress_callback,
      const std::vector<FileInfo>& file_infos,
      const storage::FileSystemURL& dest,
      size_t num_calls) {
    ASSERT_TRUE(num_calls == 0 || num_calls == 1) << "num_calls=" << num_calls;

    // For this call, `total_bytes` are not yet set!
    EXPECT_CALL(
        progress_callback,
        Run(AllOf(
            Field(&ProgressStatus::state, State::kScanning),
            Field(&ProgressStatus::type, GetOperationType()),
            Field(&ProgressStatus::sources,
                  EntryStatusUrls(GetSourceUrlsFromFileInfos(file_infos))),
            Property(&ProgressStatus::GetDestinationFolder, dest),
            Field(&ProgressStatus::total_bytes, 0))))
        .Times(num_calls);
  }

  // Expect a progress callback call for the specified files.
  // `file_infos` should include all files for which the transfer was initiated.
  // `expected_output_errors` should hold the errors of the files that should
  // have been progressed when the call is expected.
  void ExpectProgressCallbackCall(
      base::MockRepeatingCallback<void(const ProgressStatus&)>&
          progress_callback,
      const std::vector<FileInfo>& file_infos,
      const storage::FileSystemURL& dest,
      const std::vector<absl::optional<base::File::Error>>&
          expected_output_errors) {
    // Progress callback may be called any number of times. This expectation
    // catches extra calls.

    size_t processed_num_files = expected_output_errors.size();
    size_t total_num_files = file_infos.size();

    // ExpectProgressCallbackCall should not be called for the last file.
    ASSERT_LT(processed_num_files, total_num_files);

    // Expected source errors should contain nullopt entries for every entry,
    // even not yet processed ones.
    auto expected_source_errors = expected_output_errors;
    expected_source_errors.resize(file_infos.size(), absl::nullopt);

    // The expected output urls should only be populated for already processed
    // files, so we shrink them here to the appropriate size.
    std::vector<storage::FileSystemURL> partial_expected_output_urls =
        GetExpectedOutputUrlsFromFileInfos(file_infos);
    partial_expected_output_urls.resize(processed_num_files);

    EXPECT_CALL(
        progress_callback,
        Run(AllOf(
            Field(&ProgressStatus::state, State::kInProgress),
            Field(&ProgressStatus::bytes_transferred,
                  processed_num_files * kTestFileSize),
            Field(&ProgressStatus::sources,
                  EntryStatusErrors(ElementsAreArray(expected_source_errors))),
            Field(&ProgressStatus::outputs,
                  EntryStatusUrls(partial_expected_output_urls)),
            Field(&ProgressStatus::outputs,
                  EntryStatusErrors(ElementsAreArray(expected_output_errors))),
            GetBaseMatcher(file_infos, dest, total_num_files))))
        .Times(AtLeast(1));
  }

  // Expect a completion callback call for the specified files.
  // `file_infos` should include all transferred files.
  // `expected_errors` should hold the errors of all files.
  void ExpectCompletionCallbackCall(
      base::MockOnceCallback<void(ProgressStatus)>& complete_callback,
      const std::vector<FileInfo>& file_infos,
      const storage::FileSystemURL& dest,
      const std::vector<absl::optional<base::File::Error>>& expected_errors,
      base::RepeatingClosure quit_closure,
      absl::optional<size_t> maybe_total_num_files = absl::nullopt,
      absl::optional<size_t> maybe_num_blocked_files = absl::nullopt) {
    size_t total_num_files = maybe_total_num_files.value_or(file_infos.size());
    ASSERT_EQ(expected_errors.size(), file_infos.size());
    // We should get one complete callback when the copy/move finishes.
    bool has_error = std::any_of(
        expected_errors.begin(), expected_errors.end(),
        [](const auto& element) {
          return element.has_value() && element.value() != base::File::FILE_OK;
        });

    absl::optional<PolicyError> policy_error = absl::nullopt;
    if (maybe_num_blocked_files.has_value()) {
      policy_error =
          PolicyError(PolicyErrorType::kDlp, maybe_num_blocked_files.value());
    }

    EXPECT_CALL(
        complete_callback,
        Run(AllOf(Field(&ProgressStatus::state,
                        has_error ? State::kError : State::kSuccess),
                  Field(&ProgressStatus::bytes_transferred,
                        total_num_files * kTestFileSize),
                  Field(&ProgressStatus::sources,
                        EntryStatusErrors(ElementsAreArray(expected_errors))),
                  Field(&ProgressStatus::outputs,
                        EntryStatusUrls(
                            GetExpectedOutputUrlsFromFileInfos(file_infos))),
                  Field(&ProgressStatus::outputs,
                        EntryStatusErrors(ElementsAreArray(expected_errors))),
                  Field(&ProgressStatus::policy_error, policy_error),
                  GetBaseMatcher(file_infos, dest, total_num_files))))
        .WillOnce(RunClosure(quit_closure));
  }

  void VerifyFileWasNotTransferred(FileInfo file_info) {
    // If there was an error, the file wasn't copied or moved.
    // The source should still exist.
    ExpectFileContents(file_info.source_url.path(), file_info.file_contents);
    // The destination should not exist.
    EXPECT_FALSE(base::PathExists(file_info.expected_output_url.path()));
  }

  void VerifyFileWasTransferred(FileInfo file_info) {
    if (GetOperationType() == OperationType::kCopy) {
      // For a copy, the source should still be valid.
      ExpectFileContents(file_info.source_url.path(), file_info.file_contents);
    } else {
      // For a move operation, the source should be deleted.
      EXPECT_FALSE(base::PathExists(file_info.source_url.path()));
    }
    // If there's no error, the destination should always exist.
    ExpectFileContents(file_info.expected_output_url.path(),
                       file_info.file_contents);
  }

  void VerifyDirectoryWasTransferred(FileInfo file_info) {
    if (GetOperationType() == OperationType::kCopy) {
      // For a copy, the source should still be valid.
      EXPECT_TRUE(base::PathExists(file_info.source_url.path()));
    } else {
      // For a move operation, the source should be deleted.
      EXPECT_FALSE(base::PathExists(file_info.source_url.path()));
    }
    // If there's no error, the destination should always exist.
    EXPECT_TRUE(base::PathExists(file_info.expected_output_url.path()));
  }

  // The directory should exist at source and destination if there was an
  // error when transferring contained files.
  void VerifyDirectoryExistsAtSourceAndDestination(FileInfo file_info) {
    EXPECT_TRUE(base::PathExists(file_info.source_url.path()));
    EXPECT_TRUE(base::PathExists(file_info.expected_output_url.path()));
  }

  std::unique_ptr<enterprise_connectors::SourceDestinationTestingHelper>
      source_destination_testing_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  std::map<storage::FileSystemURL,
           enterprise_connectors::FileTransferAnalysisDelegate::
               FileTransferAnalysisResult,
           storage::FileSystemURL::Comparator>
      scanning_expectations_;
  storage::FileSystemURLSet directory_scanning_expectations_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
};

TEST_P(CopyOrMoveIOTaskWithScansTest, BlockSingleFileUsingResultBlocked) {
  // Create file.
  auto file = SetupFile(/*on_enabled_fs=*/true, "file.txt");
  auto dest = GetDestinationFileSystemURL("");

  // Block the file using RESULT_BLOCK.
  ExpectScan(
      file,
      enterprise_connectors::FileTransferAnalysisDelegate::RESULT_BLOCKED);

  base::RunLoop run_loop;

  // Setup the expected callbacks.
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  ExpectExtraProgressCallbackCalls(progress_callback, {file}, dest);
  ExpectScanningCallbackCall(progress_callback, {file}, dest, 1);

  ExpectCompletionCallbackCall(
      complete_callback, {file}, dest, {base::File::FILE_ERROR_SECURITY},
      run_loop.QuitClosure(), /*maybe_total_num_files=*/absl::nullopt,
      /*maybe_num_blocked_files=*/1);

  // Start the copy/move.
  CopyOrMoveIOTask task(GetOperationType(), GetSourceUrlsFromFileInfos({file}),
                        dest, profile_, file_system_context_);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  // Wait for the copy/move to be completed.
  run_loop.Run();

  VerifyFileWasNotTransferred(file);
}

TEST_P(CopyOrMoveIOTaskWithScansTest, BlockSingleFileUsingResultUnknown) {
  // Create the file.
  auto file = SetupFile(/*on_enabled_fs=*/true, "file.txt");
  auto dest = GetDestinationFileSystemURL("");

  // Block the file using RESULT_UNKNOWN.
  ExpectScan(
      file,
      enterprise_connectors::FileTransferAnalysisDelegate::RESULT_UNKNOWN);

  base::RunLoop run_loop;

  // Setup the expected callbacks.
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  ExpectExtraProgressCallbackCalls(progress_callback, {file}, dest);
  ExpectScanningCallbackCall(progress_callback, {file}, dest, 1);

  ExpectCompletionCallbackCall(
      complete_callback, {file}, dest, {base::File::FILE_ERROR_SECURITY},
      run_loop.QuitClosure(), /*maybe_total_num_files=*/absl::nullopt,
      /*maybe_num_blocked_files=*/1);

  // Start the copy/move.
  CopyOrMoveIOTask task(GetOperationType(), GetSourceUrlsFromFileInfos({file}),
                        dest, profile_, file_system_context_);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  // Wait for the copy/move to be completed.
  run_loop.Run();

  VerifyFileWasNotTransferred(file);
}

TEST_P(CopyOrMoveIOTaskWithScansTest, AllowSingleFileUsingResultAllowed) {
  // Create the file.
  auto file = SetupFile(/*on_enabled_fs=*/true, "file.txt");
  auto dest = GetDestinationFileSystemURL("");

  // Allow the file using RESULT_ALLOWED.
  ExpectScan(
      file,
      enterprise_connectors::FileTransferAnalysisDelegate::RESULT_ALLOWED);

  base::RunLoop run_loop;

  // Setup the expected callbacks.
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  ExpectExtraProgressCallbackCalls(progress_callback, {file}, dest);
  ExpectScanningCallbackCall(progress_callback, {file}, dest, 1);

  ExpectCompletionCallbackCall(complete_callback, {file}, dest,
                               {base::File::FILE_OK}, run_loop.QuitClosure());

  // Start the copy/move.
  CopyOrMoveIOTask task(GetOperationType(), GetSourceUrlsFromFileInfos({file}),
                        dest, profile_, file_system_context_);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  // Wait for the copy/move to be completed.
  run_loop.Run();

  VerifyFileWasTransferred(file);
}

TEST_P(CopyOrMoveIOTaskWithScansTest, SingleFileOnDisabledFileSystem) {
  // Create the file.
  auto file = SetupFile(/*on_enabled_fs=*/false, "file.txt");
  auto dest = GetDestinationFileSystemURL("");

  // We don't expect any scan to happen, so we don't set any expectation.

  base::RunLoop run_loop;

  // Setup the expected callbacks.
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  ExpectExtraProgressCallbackCalls(progress_callback, {file}, dest);
  ExpectScanningCallbackCall(progress_callback, {file}, dest, 0);

  ExpectCompletionCallbackCall(complete_callback, {file}, dest,
                               {base::File::FILE_OK}, run_loop.QuitClosure());

  // Start the copy/move.
  CopyOrMoveIOTask task(GetOperationType(), GetSourceUrlsFromFileInfos({file}),
                        dest, profile_, file_system_context_);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  // Wait for the copy/move to be completed.
  run_loop.Run();

  VerifyFileWasTransferred(file);
}

TEST_P(CopyOrMoveIOTaskWithScansTest, FilesOnDisabledAndEnabledFileSystems) {
  // Create the files.
  auto enabled_file = SetupFile(/*on_enabled_fs=*/true, "file1.txt");
  auto disabled_file = SetupFile(/*on_enabled_fs=*/false, "file2.txt");

  // Expect a scan for the enabled file and block it.
  ExpectScan(
      enabled_file,
      enterprise_connectors::FileTransferAnalysisDelegate::RESULT_BLOCKED);
  // Don't expect any scan for the file on the disabled file system.

  auto dest = GetDestinationFileSystemURL("");

  base::RunLoop run_loop;

  // Setup the expected callbacks.
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  ExpectExtraProgressCallbackCalls(progress_callback,
                                   {enabled_file, disabled_file}, dest);
  ExpectScanningCallbackCall(progress_callback, {enabled_file, disabled_file},
                             dest, 1);

  ExpectProgressCallbackCall(progress_callback, {enabled_file, disabled_file},
                             dest, {base::File::FILE_ERROR_SECURITY});

  ExpectCompletionCallbackCall(
      complete_callback, {enabled_file, disabled_file}, dest,
      {base::File::FILE_ERROR_SECURITY, base::File::FILE_OK},
      run_loop.QuitClosure(), /*maybe_total_num_files=*/absl::nullopt,
      /*maybe_num_blocked_files=*/1);

  // Start the copy/move.
  CopyOrMoveIOTask task(
      GetOperationType(),
      GetSourceUrlsFromFileInfos({enabled_file, disabled_file}), dest, profile_,
      file_system_context_);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  // Wait for the copy/move to be completed.
  run_loop.Run();

  // Verify the files after the copy/move.
  VerifyFileWasNotTransferred(enabled_file);
  VerifyFileWasTransferred(disabled_file);
}

TEST_P(CopyOrMoveIOTaskWithScansTest, DirectoryTransferBlockAll) {
  // Create directory.
  FileInfo directory{"", GetSourceFileSystemURLForEnabledVolume("folder"),
                     GetDestinationFileSystemURL("folder")};
  ASSERT_TRUE(base::CreateDirectory(directory.source_url.path()));

  // Create the files.
  auto file0 = SetupFile(/*on_enabled_fs=*/true, "folder/file0.txt");
  auto file1 = SetupFile(/*on_enabled_fs=*/true, "folder/file1.txt");

  // Expect a scan for both files and block the transfer.
  ExpectDirectoryScan(directory);
  ExpectScan(
      file0,
      enterprise_connectors::FileTransferAnalysisDelegate::RESULT_BLOCKED);
  ExpectScan(
      file1,
      enterprise_connectors::FileTransferAnalysisDelegate::RESULT_BLOCKED);

  auto dest = GetDestinationFileSystemURL("");

  base::RunLoop run_loop;

  // Setup the expected callbacks.
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  ExpectExtraProgressCallbackCalls(progress_callback, {directory}, dest,
                                   /*total_num_files=*/2);
  ExpectScanningCallbackCall(progress_callback, {directory}, dest, 1);

  // For moves, only the last error is reported. The last step the operation
  // performs is to try to remove the parent directory. This fails with
  // FILE_ERROR_NOT_EMPTY, as there are files that weren't moved.
  auto expected_error = GetOperationType() == OperationType::kCopy
                            ? base::File::FILE_ERROR_SECURITY
                            : base::File::FILE_ERROR_NOT_EMPTY;

  ExpectCompletionCallbackCall(complete_callback, {directory}, dest,
                               {expected_error}, run_loop.QuitClosure(),
                               /*maybe_total_num_files=*/2,
                               /*maybe_num_blocked_files=*/2);

  // Start the copy/move.
  CopyOrMoveIOTask task(GetOperationType(),
                        GetSourceUrlsFromFileInfos({directory}), dest, profile_,
                        file_system_context_);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  // Wait for the copy/move to be completed.
  run_loop.Run();

  // Verify the directory and the files after the copy/move.
  VerifyDirectoryExistsAtSourceAndDestination(directory);
  VerifyFileWasNotTransferred(file0);
  VerifyFileWasNotTransferred(file1);
}

TEST_P(CopyOrMoveIOTaskWithScansTest, DirectoryTransferBlockOne) {
  // Create directory.
  FileInfo directory{"", GetSourceFileSystemURLForEnabledVolume("folder"),
                     GetDestinationFileSystemURL("folder")};
  ASSERT_TRUE(base::CreateDirectory(directory.source_url.path()));

  // Create the files.
  auto file0 = SetupFile(/*on_enabled_fs=*/true, "folder/file0.txt");
  auto file1 = SetupFile(/*on_enabled_fs=*/true, "folder/file1.txt");

  // Expect a scan for both files and block the transfer.
  ExpectDirectoryScan(directory);
  ExpectScan(
      file0,
      enterprise_connectors::FileTransferAnalysisDelegate::RESULT_BLOCKED);
  ExpectScan(
      file1,
      enterprise_connectors::FileTransferAnalysisDelegate::RESULT_ALLOWED);

  auto dest = GetDestinationFileSystemURL("");

  base::RunLoop run_loop;

  // Setup the expected callbacks.
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  ExpectExtraProgressCallbackCalls(progress_callback, {directory}, dest,
                                   /*total_num_files=*/2);
  ExpectScanningCallbackCall(progress_callback, {directory}, dest, 1);

  // For moves, only the last error is reported. The last step the operation
  // performs is to try to remove the parent directory. This fails with
  // FILE_ERROR_NOT_EMPTY, as there are files that weren't moved.
  auto expected_error = GetOperationType() == OperationType::kCopy
                            ? base::File::FILE_ERROR_SECURITY
                            : base::File::FILE_ERROR_NOT_EMPTY;

  ExpectCompletionCallbackCall(complete_callback, {directory}, dest,
                               {expected_error}, run_loop.QuitClosure(),
                               /*maybe_total_num_files=*/2,
                               /*maybe_num_blocked_files=*/1);

  // Start the copy/move.
  CopyOrMoveIOTask task(GetOperationType(),
                        GetSourceUrlsFromFileInfos({directory}), dest, profile_,
                        file_system_context_);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  // Wait for the copy/move to be completed.
  run_loop.Run();

  // Verify the directory and the files after the copy/move.
  VerifyDirectoryExistsAtSourceAndDestination(directory);
  VerifyFileWasNotTransferred(file0);
  VerifyFileWasTransferred(file1);
}

TEST_P(CopyOrMoveIOTaskWithScansTest, DirectoryTransferAllowAll) {
  // Create directory.
  FileInfo directory{"", GetSourceFileSystemURLForEnabledVolume("folder"),
                     GetDestinationFileSystemURL("folder")};
  ASSERT_TRUE(base::CreateDirectory(directory.source_url.path()));

  // Create the files.
  auto file0 = SetupFile(/*on_enabled_fs=*/true, "folder/file0.txt");
  auto file1 = SetupFile(/*on_enabled_fs=*/true, "folder/file1.txt");

  // Expect a scan for both files and block the transfer.
  ExpectDirectoryScan(directory);
  ExpectScan(
      file0,
      enterprise_connectors::FileTransferAnalysisDelegate::RESULT_ALLOWED);
  ExpectScan(
      file1,
      enterprise_connectors::FileTransferAnalysisDelegate::RESULT_ALLOWED);

  auto dest = GetDestinationFileSystemURL("");

  base::RunLoop run_loop;

  // Setup the expected callbacks.
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  ExpectExtraProgressCallbackCalls(progress_callback, {directory}, dest,
                                   /*total_num_files=*/2);
  ExpectScanningCallbackCall(progress_callback, {directory}, dest, 1);

  ExpectCompletionCallbackCall(complete_callback, {directory}, dest,
                               {base::File::FILE_OK}, run_loop.QuitClosure(),
                               /*maybe_total_num_files=*/2);

  // Start the copy/move.
  CopyOrMoveIOTask task(GetOperationType(),
                        GetSourceUrlsFromFileInfos({directory}), dest, profile_,
                        file_system_context_);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  // Wait for the copy/move to be completed.
  run_loop.Run();

  // Verify the directory and the files after the copy/move.
  VerifyDirectoryWasTransferred(directory);
  VerifyFileWasTransferred(file0);
  VerifyFileWasTransferred(file1);
}

INSTANTIATE_TEST_SUITE_P(CopyOrMove,
                         CopyOrMoveIOTaskWithScansTest,
                         testing::Values(OperationType::kCopy,
                                         OperationType::kMove),
                         &CopyOrMoveIOTaskWithScansTest::ParamToString);

class CopyOrMoveIOTaskWithDLPTest : public testing::Test {
 public:
  CopyOrMoveIOTaskWithDLPTest(const CopyOrMoveIOTaskWithDLPTest&) = delete;
  CopyOrMoveIOTaskWithDLPTest& operator=(const CopyOrMoveIOTaskWithDLPTest&) =
      delete;
  ~CopyOrMoveIOTaskWithDLPTest() override = default;

 protected:
  CopyOrMoveIOTaskWithDLPTest()
      : profile_(std::make_unique<TestingProfile>()),
        user_manager_(new ash::FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(user_manager_.get())) {}

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager =
        std::make_unique<testing::NiceMock<policy::MockDlpRulesManager>>();
    mock_rules_manager_ = dlp_rules_manager.get();
    ON_CALL(*mock_rules_manager_, IsFilesPolicyEnabled)
        .WillByDefault(testing::Return(true));

    files_controller_ = std::make_unique<
        testing::StrictMock<policy::MockDlpFilesControllerAsh>>(
        *mock_rules_manager_);

    ON_CALL(*mock_rules_manager_, GetDlpFilesController())
        .WillByDefault(::testing::Return(files_controller_.get()));

    return dlp_rules_manager;
  }

  void SetUp() override {
    policy::DlpFilesController::SetNewFilesPolicyUXEnabledForTesting(
        /*is_enabled=*/true);

    AccountId account_id = AccountId::FromUserEmailGaiaId(kEmailId, kGaiaId);
    profile_->SetIsNewProfile(true);
    user_manager::User* user =
        user_manager_->AddUserWithAffiliationAndTypeAndProfile(
            account_id, /*is_affiliated=*/false,
            user_manager::USER_TYPE_REGULAR, profile_.get());
    user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                /*browser_restart=*/false,
                                /*is_child=*/false);
    user_manager_->SimulateUserProfileLoad(account_id);

    // DLP Setup.
    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindRepeating(&CopyOrMoveIOTaskWithDLPTest::SetDlpRulesManager,
                            base::Unretained(this)));
    ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
    ASSERT_NE(policy::DlpRulesManagerFactory::GetForPrimaryProfile()
                  ->GetDlpFilesController(),
              nullptr);

    // Define a VolumeManager to associate with the testing profile.
    // disk_mount_manager_ outlives profile_, and therefore outlives the
    // repeating callback.
    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindRepeating(&BuildVolumeManager,
                            base::Unretained(&disk_mount_manager_)));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        nullptr, temp_dir_.GetPath());
  }

  storage::FileSystemURL CreateFileSystemURL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        kTestStorageKey, storage::kFileSystemTypeTest,
        base::FilePath::FromUTF8Unsafe(path));
  }

  content::BrowserTaskEnvironment task_environment_;
  ash::disks::FakeDiskMountManager disk_mount_manager_;
  raw_ptr<policy::MockDlpRulesManager, ExperimentalAsh> mock_rules_manager_ =
      nullptr;
  std::unique_ptr<policy::MockDlpFilesControllerAsh> files_controller_;
  const std::unique_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  raw_ptr<ash::FakeChromeUserManager, ExperimentalAsh> user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome-extension://abc");
};

TEST_F(CopyOrMoveIOTaskWithDLPTest, BlockSingleFile) {
  // Create file.
  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  ASSERT_TRUE(
      base::WriteFile(temp_dir_.GetPath().Append("foo.txt"), foo_contents));
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL("foo.txt"),
  };

  auto dest = CreateFileSystemURL("");

  IOTaskId task_id = 1;

  // Block the file.
  EXPECT_CALL(
      *files_controller_.get(),
      CheckIfTransferAllowed(absl::make_optional(task_id), source_urls, dest,
                             /*is_move=*/false, testing::_))
      .WillOnce(
          [source_urls](
              absl::optional<file_manager::io_task::IOTaskId> task_id,
              const std::vector<storage::FileSystemURL>& transferred_files,
              storage::FileSystemURL destination, bool is_move,
              policy::DlpFilesControllerAsh::CheckIfTransferAllowedCallback
                  result_callback) {
            std::move(result_callback).Run(source_urls);
          });

  base::RunLoop run_loop;

  // Setup the expected callbacks.
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  EXPECT_CALL(
      complete_callback,
      Run(AllOf(
          Field(&ProgressStatus::type, OperationType::kCopy),
          Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
          Property(&ProgressStatus::GetDestinationFolder, dest),
          Field(&ProgressStatus::total_bytes, 1 * kTestFileSize),
          Field(&ProgressStatus::state, State::kError),
          Field(&ProgressStatus::policy_error,
                PolicyError(PolicyErrorType::kDlp, /*blocked_files=*/1)))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  // Start the copy.
  CopyOrMoveIOTask task(OperationType::kCopy, source_urls, dest, profile_.get(),
                        file_system_context_);
  task.SetTaskID(task_id);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  // Wait for the copy to be completed.
  run_loop.Run();
}

TEST_F(CopyOrMoveIOTaskWithDLPTest, AllowSingleFile) {
  // Create file.
  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  ASSERT_TRUE(
      base::WriteFile(temp_dir_.GetPath().Append("foo.txt"), foo_contents));
  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL("foo.txt"),
  };

  auto dest = CreateFileSystemURL("");

  IOTaskId task_id = 1;

  // Allow the file.
  EXPECT_CALL(
      *files_controller_.get(),
      CheckIfTransferAllowed(absl::make_optional(task_id), source_urls, dest,
                             /*is_move=*/false, testing::_))
      .WillOnce(
          [](absl::optional<file_manager::io_task::IOTaskId> task_id,
             const std::vector<storage::FileSystemURL>& transferred_files,
             storage::FileSystemURL destination, bool is_move,
             policy::DlpFilesControllerAsh::CheckIfTransferAllowedCallback
                 result_callback) { std::move(result_callback).Run({}); });

  base::RunLoop run_loop;

  // Setup the expected callbacks.
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  EXPECT_CALL(
      complete_callback,
      Run(AllOf(Field(&ProgressStatus::type, OperationType::kCopy),
                Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
                Property(&ProgressStatus::GetDestinationFolder, dest),
                Field(&ProgressStatus::total_bytes, 1 * kTestFileSize),
                Field(&ProgressStatus::state, State::kSuccess),
                Field(&ProgressStatus::policy_error, absl::nullopt),
                Field(&ProgressStatus::bytes_transferred, 1 * kTestFileSize))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  // Start the copy.
  CopyOrMoveIOTask task(OperationType::kCopy, source_urls, dest, profile_.get(),
                        file_system_context_);
  task.SetTaskID(task_id);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  // Wait for the copy to be completed.
  run_loop.Run();
}

TEST_F(CopyOrMoveIOTaskWithDLPTest, DirectoryTransferBlockOne) {
  ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().Append("folder")));

  std::string foo_contents = base::RandBytesAsString(kTestFileSize);
  std::string bar_contents = base::RandBytesAsString(kTestFileSize);
  ASSERT_TRUE(base::WriteFile(temp_dir_.GetPath().Append("folder/foo.txt"),
                              foo_contents));
  ASSERT_TRUE(base::WriteFile(temp_dir_.GetPath().Append("folder/bar.txt"),
                              bar_contents));
  ASSERT_TRUE(base::CreateDirectory(temp_dir_.GetPath().Append("dest_folder")));

  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL("folder"),
  };
  auto dest = CreateFileSystemURL("dest_folder");
  auto blocked_file = CreateFileSystemURL("folder/bar.txt");

  IOTaskId task_id = 1;

  // Block one file from the folder.
  EXPECT_CALL(
      *files_controller_.get(),
      CheckIfTransferAllowed(absl::make_optional(task_id), source_urls, dest,
                             /*is_move=*/true, testing::_))
      .WillOnce(
          [blocked_file](
              absl::optional<file_manager::io_task::IOTaskId> task_id,
              const std::vector<storage::FileSystemURL>& transferred_files,
              storage::FileSystemURL destination, bool is_move,
              policy::DlpFilesControllerAsh::CheckIfTransferAllowedCallback
                  result_callback) {
            std::move(result_callback).Run({blocked_file});
          });

  base::RunLoop run_loop;

  // Setup the expected callbacks.
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  EXPECT_CALL(
      complete_callback,
      Run(AllOf(
          Field(&ProgressStatus::type, OperationType::kMove),
          Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
          Property(&ProgressStatus::GetDestinationFolder, dest),
          // Field(&ProgressStatus::total_bytes, 2 * kTestFileSize),
          Field(&ProgressStatus::state, State::kError),
          Field(&ProgressStatus::policy_error,
                PolicyError(PolicyErrorType::kDlp, /*blocked_files=*/1)))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  // Start the move.
  CopyOrMoveIOTask task(OperationType::kMove, source_urls, dest, profile_.get(),
                        file_system_context_);
  task.SetTaskID(task_id);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  // Wait for the move to be completed.
  run_loop.Run();
}

TEST_F(CopyOrMoveIOTaskWithDLPTest, WarnMultipleFiles) {
  // Create the files.
  std::string file_contents = base::RandBytesAsString(kTestFileSize);
  ASSERT_TRUE(
      base::WriteFile(temp_dir_.GetPath().Append("file1.txt"), file_contents));
  ASSERT_TRUE(
      base::WriteFile(temp_dir_.GetPath().Append("file2.txt"), file_contents));

  std::vector<storage::FileSystemURL> source_urls = {
      CreateFileSystemURL("file1.txt"),
      CreateFileSystemURL("file2.txt"),
  };

  auto dest = CreateFileSystemURL("");

  IOTaskId task_id = 1;
  CopyOrMoveIOTask task(OperationType::kCopy, source_urls, dest, profile_.get(),
                        file_system_context_);
  task.SetTaskID(task_id);

  // Pause for warning then proceed.
  EXPECT_CALL(
      *files_controller_.get(),
      CheckIfTransferAllowed(absl::make_optional(task_id), source_urls, dest,
                             /*is_move=*/false, testing::_))
      .WillOnce(
          [&task](absl::optional<file_manager::io_task::IOTaskId> task_id,
                  const std::vector<storage::FileSystemURL>& transferred_files,
                  storage::FileSystemURL destination, bool is_move,
                  policy::DlpFilesControllerAsh::CheckIfTransferAllowedCallback
                      result_callback) {
            ASSERT_TRUE(task_id.has_value());
            // Pause the task.
            file_manager::io_task::PauseParams pause_params;
            pause_params.policy_params =
                file_manager::io_task::PolicyPauseParams(
                    policy::Policy::kDlp, /*warning_files_count=*/1);
            task.Pause(std::move(pause_params));
            // Resume the task.
            file_manager::io_task::ResumeParams resume_params;
            resume_params.policy_params =
                file_manager::io_task::PolicyResumeParams(policy::Policy::kDlp);
            task.Resume(std::move(resume_params));

            std::move(result_callback).Run({});
          });

  base::RunLoop run_loop;

  // Setup the expected callbacks.
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  // Task is paused.
  EXPECT_CALL(
      progress_callback,
      Run(AllOf(Field(&ProgressStatus::type, OperationType::kCopy),
                Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
                Property(&ProgressStatus::GetDestinationFolder, dest),
                Field(&ProgressStatus::state, State::kPaused),
                Field(&ProgressStatus::bytes_transferred, 0))));
  // After the task is resumed.
  EXPECT_CALL(
      progress_callback,
      Run(AllOf(Field(&ProgressStatus::type, OperationType::kCopy),
                Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
                Property(&ProgressStatus::GetDestinationFolder, dest),
                Field(&ProgressStatus::state, State::kInProgress),
                Field(&ProgressStatus::bytes_transferred, 1 * kTestFileSize))));
  // Task is completed.
  EXPECT_CALL(
      complete_callback,
      Run(AllOf(Field(&ProgressStatus::type, OperationType::kCopy),
                Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
                Property(&ProgressStatus::GetDestinationFolder, dest),
                Field(&ProgressStatus::total_bytes, 2 * kTestFileSize),
                Field(&ProgressStatus::state, State::kSuccess),
                Field(&ProgressStatus::policy_error, absl::nullopt),
                Field(&ProgressStatus::bytes_transferred, 2 * kTestFileSize))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  // Start the copy.
  task.Execute(progress_callback.Get(), complete_callback.Get());
  // Wait for the copy to be completed.
  run_loop.Run();
}

}  // namespace io_task
}  // namespace file_manager
