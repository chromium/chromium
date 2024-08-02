// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/ash/policy/dlp/test/mock_dlp_files_controller_ash.h"
#include "chrome/browser/ash/policy/dlp/test/mock_files_policy_notification_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "chrome/browser/enterprise/connectors/analysis/mock_file_transfer_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/analysis/source_destination_test_util.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
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
using ::testing::UnorderedElementsAreArray;

namespace file_manager::io_task {

namespace {

const size_t kTestFileSize = 32;
const int kTaskId = 1;
base::TimeDelta kResponseDelay = base::Seconds(0);

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
        {file_manager::VOLUME_TYPE_PROVIDED, std::nullopt, "PROVIDED"},
        {file_manager::VOLUME_TYPE_GOOGLE_DRIVE, std::nullopt, "GOOGLE_DRIVE"},
        {file_manager::VOLUME_TYPE_DOWNLOADS_DIRECTORY, std::nullopt,
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
  std::vector<std::optional<base::File::Error>> errors;
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
    : public testing::TestWithParam<std::tuple<OperationType,
                                               /*new_policy_ux*/ bool,
                                               /*new_file_transfer_ux*/ bool>> {
 public:
  static std::string ParamToString(
      const ::testing::TestParamInfo<ParamType>& info) {
    auto [operation_type, new_policy_ux, new_file_transfer_ux] = info.param;
    std::string name;
    name += operation_type == OperationType::kCopy ? "Copy" : "Move";
    if (new_policy_ux) {
      name += "PolicyUX";
    }
    if (new_file_transfer_ux) {
      name += "ConnectorsUX";
    }
    return name;
  }

 protected:
  OperationType GetOperationType() { return std::get<0>(GetParam()); }

  bool UseNewPolicyUI() { return std::get<1>(GetParam()); }
  bool UseNewConnectorsUI() { return std::get<2>(GetParam()); }

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test-profile");

    std::vector<base::test::FeatureRef> enabled_features{
        features::kFileTransferEnterpriseConnector};
    std::vector<base::test::FeatureRef> disabled_features;

    if (UseNewPolicyUI()) {
      enabled_features.push_back(features::kNewFilesPolicyUX);
    } else {
      disabled_features.push_back(features::kNewFilesPolicyUX);
    }

    if (UseNewConnectorsUI()) {
      enabled_features.push_back(features::kFileTransferEnterpriseConnectorUI);
    } else {
      disabled_features.push_back(features::kFileTransferEnterpriseConnectorUI);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    // Set a device management token. It is required to enable scanning.
    // Without it, FileTransferAnalysisDelegate::IsEnabled() always
    // returns std::nullopt.
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

    if (UseNewConnectorsUI() || UseNewPolicyUI()) {
      // Set FilesPolicyNotificationManager.
      policy::FilesPolicyNotificationManagerFactory::GetInstance()
          ->SetTestingFactory(
              profile_.get(),
              base::BindRepeating(&CopyOrMoveIOTaskWithScansTest::
                                      SetFilesPolicyNotificationManager,
                                  base::Unretained(this)));
      // Initialize the FPNM, s.t., it exists before starting the test. This is
      // needed to set expectations.
      ASSERT_TRUE(
          policy::FilesPolicyNotificationManagerFactory::GetForBrowserContext(
              profile_.get()));
    }
  }

  std::unique_ptr<KeyedService> SetFilesPolicyNotificationManager(
      content::BrowserContext* context) {
    auto fpnm = std::make_unique<
        testing::StrictMock<policy::MockFilesPolicyNotificationManager>>(
        profile_.get());
    fpnm_ = fpnm.get();

    return fpnm;
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

      if (UseNewConnectorsUI()) {
        if (warned_files_.count(source_url) != 0) {
          EXPECT_CALL(*delegate, GetWarnedFiles())
              .WillOnce(
                  Return(std::vector<storage::FileSystemURL>{source_url}));
        } else {
          EXPECT_CALL(*delegate, GetWarnedFiles())
              .WillOnce(Return(std::vector<storage::FileSystemURL>{}));
        }
      }

      if (iter->second.has_value()) {
        EXPECT_CALL(*delegate, GetAnalysisResultAfterScan(source_url))
            .WillOnce(Return(iter->second.value()));
      }
      return;
    }

    if (auto iter = directory_scanning_expectations_.find(source_url);
        iter != directory_scanning_expectations_.end()) {
      // Scan for directory detected.
      EXPECT_CALL(*delegate, UploadData(_))
          .WillOnce(
              [](base::OnceClosure callback) { std::move(callback).Run(); });

      // Setup warned files call if the new UI is used.
      if (UseNewConnectorsUI()) {
        std::vector<storage::FileSystemURL> warned_files;
        for (auto&& warned_file : warned_files_) {
          // Note: We're using IsParent here, so this doesn't support recursive
          // scanning!
          // If the current directory is a parent of the expectation, set an
          // expectation.
          if (iter->IsParent(warned_file)) {
            warned_files.push_back(warned_file);
          }
        }

        EXPECT_CALL(*delegate, GetWarnedFiles()).WillOnce(Return(warned_files));
      }

      for (auto&& scanning_expectation : scanning_expectations_) {
        // Note: We're using IsParent here, so this doesn't support recursive
        // scanning!
        // If the current directory is a parent of the expectation, set an
        // expectation.
        if (iter->IsParent(scanning_expectation.first) &&
            scanning_expectation.second.has_value()) {
          EXPECT_CALL(*delegate,
                      GetAnalysisResultAfterScan(scanning_expectation.first))
              .WillOnce(Return(scanning_expectation.second.value()));
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
  void SetFileTransferAnalysisResult(
      FileInfo file_info,
      enterprise_connectors::FileTransferAnalysisDelegate::
          FileTransferAnalysisResult result) {
    ASSERT_EQ(scanning_expectations_.count(file_info.source_url), 0u);
    scanning_expectations_[file_info.source_url] = result;
  }

  // Expect a scan for the file specified using `file_info` but don't expect the
  // result of this file to be checked using `GetAnalysisResultAfterScan()`. Use
  // this when writing a test that doesn't proceed on a warning.
  void ExpectScanWithoutCheckingResult(FileInfo file_info) {
    ASSERT_EQ(scanning_expectations_.count(file_info.source_url), 0u);
    scanning_expectations_[file_info.source_url] = std::nullopt;
  }

  // Mark the passed file as warned file.
  // The FileTransferAnalysisDelegate `delegate` is mocked to return files
  // marked as warned files through the `GetWarnedFiles()` call.
  void SetFileHasWarning(FileInfo file_info) {
    ASSERT_EQ(warned_files_.count(file_info.source_url), 0u);
    warned_files_.insert(file_info.source_url);
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
      std::optional<size_t> total_num_files = std::nullopt) {
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
      const std::vector<std::optional<base::File::Error>>&
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
    expected_source_errors.resize(file_infos.size(), std::nullopt);

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
      const std::vector<std::optional<base::File::Error>>& expected_errors,
      base::RepeatingClosure quit_closure,
      std::optional<size_t> maybe_total_num_files = std::nullopt,
      std::vector<std::optional<PolicyError>> maybe_policy_errors = {
          std::nullopt}) {
    size_t total_num_files = maybe_total_num_files.value_or(file_infos.size());
    ASSERT_EQ(expected_errors.size(), file_infos.size());
    // We should get one complete callback when the copy/move finishes.
    bool has_error = std::any_of(
        expected_errors.begin(), expected_errors.end(),
        [](const auto& element) {
          return element.has_value() && element.value() != base::File::FILE_OK;
        });

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
                  Field(&ProgressStatus::policy_error,
                        testing::AnyOfArray(maybe_policy_errors)),
                  GetBaseMatcher(file_infos, dest, total_num_files))))
        .WillOnce(RunClosure(quit_closure));
  }

  void ExpectFPNMBlockedFiles(std::vector<FileInfo> files,
                              policy::FilesPolicyDialog::BlockReason reason) {
    std::vector<base::FilePath> blocked_files;
    for (auto&& file : files) {
      blocked_files.push_back(file.source_url.path());
    }

    EXPECT_CALL(*fpnm_, SetConnectorsBlockedFiles(
                            /*task_id=*/{kTaskId},
                            GetOperationType() == OperationType::kCopy
                                ? policy::dlp::FileAction::kCopy
                                : policy::dlp::FileAction::kMove,
                            reason,
                            policy::FilesPolicyDialog::Info::Error(
                                reason, blocked_files)));
  }

  void ExpectFPNMFilesWarningDialogAndProceed(CopyOrMoveIOTask& task) {
    std::vector<base::FilePath> warned_files;
    for (auto&& file : warned_files_) {
      warned_files.push_back(file.path());
    }

    // Enterprise connectors file transfer currently only considers warning mode
    // for sensitive data.
    auto dialog_info = policy::FilesPolicyDialog::Info::Warn(
        policy::FilesPolicyDialog::BlockReason::
            kEnterpriseConnectorsSensitiveData,
        warned_files);

    EXPECT_CALL(*fpnm_, ShowConnectorsWarning(
                            /*callback=*/_,
                            /*task_id=*/{kTaskId},
                            GetOperationType() == OperationType::kCopy
                                ? policy::dlp::FileAction::kCopy
                                : policy::dlp::FileAction::kMove,
                            std::move(dialog_info)))
        .WillOnce([&](auto&&... args) {
          warning_callback_ = std::move(std::get<0>(std::tie(args...)));

          base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
              FROM_HERE,
              base::BindOnce(&CopyOrMoveIOTaskWithScansTest::ProceedWarning,
                             base::Unretained(this), std::ref(task)),
              kResponseDelay);
        });
  }

  void ExpectFPNMFilesWarningDialogAndCancel(
      base::RepeatingClosure cancelCallback) {
    std::vector<base::FilePath> warned_files;
    for (auto&& file : warned_files_) {
      warned_files.push_back(file.path());
    }

    // Enterprise connectors file transfer currently only considers warning mode
    // for sensitive data.
    auto dialog_settings = policy::FilesPolicyDialog::Info::Warn(
        policy::FilesPolicyDialog::BlockReason::
            kEnterpriseConnectorsSensitiveData,
        warned_files);

    EXPECT_CALL(*fpnm_, ShowConnectorsWarning(
                            /*callback=*/_,
                            /*task_id=*/{kTaskId},
                            GetOperationType() == OperationType::kCopy
                                ? policy::dlp::FileAction::kCopy
                                : policy::dlp::FileAction::kMove,
                            std::move(dialog_settings)))
        .WillOnce([&, cancelCallback](auto&&... args) {
          warning_callback_ = std::move(std::get<0>(std::tie(args...)));

          base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
              FROM_HERE, cancelCallback, kResponseDelay);
        });
  }

  void ProceedWarning(CopyOrMoveIOTask& task) {
    file_manager::io_task::ResumeParams params;
    params.policy_params = file_manager::io_task::PolicyResumeParams(
        policy::Policy::kEnterpriseConnectors);

    EXPECT_CALL(*fpnm_, OnIOTaskResumed(
                            /*task_id=*/{kTaskId}))
        .WillOnce([&]() {
          std::move(warning_callback_)
              .Run(/*user_justification=*/std::nullopt, true);
        });

    task.Resume(params);
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

  void VerifyDirectoryWasNotTransferred(FileInfo file_info) {
    // Directory should exist at source.
    EXPECT_TRUE(base::PathExists(file_info.source_url.path()));

    // Directory should not exist at destination.
    EXPECT_FALSE(base::PathExists(file_info.expected_output_url.path()));
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
           std::optional<enterprise_connectors::FileTransferAnalysisDelegate::
                             FileTransferAnalysisResult>,
           storage::FileSystemURL::Comparator>
      scanning_expectations_;
  std::set<storage::FileSystemURL, storage::FileSystemURL::Comparator>
      warned_files_;
  storage::FileSystemURLSet directory_scanning_expectations_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
  raw_ptr<policy::MockFilesPolicyNotificationManager, DanglingUntriaged> fpnm_;
  policy::WarningWithJustificationCallback warning_callback_;
};

INSTANTIATE_TEST_SUITE_P(
    CopyOrMove,
    CopyOrMoveIOTaskWithScansTest,
    testing::Combine(testing::Values(OperationType::kCopy,
                                     OperationType::kMove),
                     /*new_policy_ux*/ testing::Bool(),
                     /*new_file_transfer_ux*/ testing::Bool()),
    &CopyOrMoveIOTaskWithScansTest::ParamToString);

using CopyOrMoveIOTaskWithScansWarnTest = CopyOrMoveIOTaskWithScansTest;

INSTANTIATE_TEST_SUITE_P(
    CopyOrMove,
    CopyOrMoveIOTaskWithScansWarnTest,
    testing::Combine(testing::Values(OperationType::kCopy,
                                     OperationType::kMove),
                     /*new_policy_ux*/ testing::Bool(),
                     /*new_file_transfer_ux*/ testing::Values(true)),
    &CopyOrMoveIOTaskWithScansTest::ParamToString);

TEST_P(CopyOrMoveIOTaskWithScansTest, BlockSingleFileUsingResultBlocked) {
  // Create file.
  auto file = SetupFile(/*on_enabled_fs=*/true, "file.txt");
  auto dest = GetDestinationFileSystemURL("");

  // Block the file using RESULT_BLOCK.
  SetFileTransferAnalysisResult(
      file, enterprise_connectors::FileTransferAnalysisDelegate::
                FileTransferAnalysisResult::Blocked(
                    enterprise_connectors::FinalContentAnalysisResult::FAILURE,
                    enterprise_connectors::kDlpTag));

  base::RunLoop run_loop;

  // Setup the expected callbacks.
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  ExpectExtraProgressCallbackCalls(progress_callback, {file}, dest);
  ExpectScanningCallbackCall(progress_callback, {file}, dest, 1);

  ExpectCompletionCallbackCall(
      complete_callback, {file}, dest, {base::File::FILE_ERROR_SECURITY},
      run_loop.QuitClosure(), /*maybe_total_num_files=*/std::nullopt,
      /*maybe_policy_errors=*/
      {UseNewPolicyUI() && UseNewConnectorsUI()
           ? std::make_optional(PolicyError(
                 PolicyErrorType::kEnterpriseConnectors, 1, "file.txt"))
           : std::nullopt});

  if (UseNewPolicyUI() && UseNewConnectorsUI()) {
    ExpectFPNMBlockedFiles({file}, policy::FilesPolicyDialog::BlockReason::
                                       kEnterpriseConnectorsSensitiveData);
  }

  // Start the copy/move.
  CopyOrMoveIOTask task(GetOperationType(), GetSourceUrlsFromFileInfos({file}),
                        dest, profile_, file_system_context_);
  task.SetTaskID(kTaskId);
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
  SetFileTransferAnalysisResult(
      file, enterprise_connectors::FileTransferAnalysisDelegate::
                FileTransferAnalysisResult::Unknown());

  base::RunLoop run_loop;

  // Setup the expected callbacks.
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  ExpectExtraProgressCallbackCalls(progress_callback, {file}, dest);
  ExpectScanningCallbackCall(progress_callback, {file}, dest, 1);

  ExpectCompletionCallbackCall(
      complete_callback, {file}, dest, {base::File::FILE_ERROR_SECURITY},
      run_loop.QuitClosure(), /*maybe_total_num_files=*/std::nullopt,
      /*maybe_policy_errors=*/
      {UseNewPolicyUI() && UseNewConnectorsUI()
           ? std::make_optional(PolicyError(
                 PolicyErrorType::kEnterpriseConnectors, 1, "file.txt"))
           : std::nullopt});

  if (UseNewPolicyUI() && UseNewConnectorsUI()) {
    ExpectFPNMBlockedFiles({file}, policy::FilesPolicyDialog::BlockReason::
                                       kEnterpriseConnectorsUnknownScanResult);
  }

  // Start the copy/move.
  CopyOrMoveIOTask task(GetOperationType(), GetSourceUrlsFromFileInfos({file}),
                        dest, profile_, file_system_context_);
  task.SetTaskID(kTaskId);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  // Wait for the copy/move to be completed.
  run_loop.Run();

  VerifyFileWasNotTransferred(file);
}

TEST_P(CopyOrMoveIOTaskWithScansWarnTest, WarnSingleFileProceed) {
  // Create file.
  auto file = SetupFile(/*on_enabled_fs=*/true, "file.txt");
  auto dest = GetDestinationFileSystemURL("");

  // Mark the file to have a warning.
  SetFileHasWarning(file);

  // After proceeding the scan, the result is allowed.
  SetFileTransferAnalysisResult(
      file, enterprise_connectors::FileTransferAnalysisDelegate::
                FileTransferAnalysisResult::Allowed());

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
  task.SetTaskID(kTaskId);

  ExpectFPNMFilesWarningDialogAndProceed(task);

  task.Execute(progress_callback.Get(), complete_callback.Get());
  // Wait for the copy/move to be completed.
  run_loop.Run();

  VerifyFileWasTransferred(file);
}

TEST_P(CopyOrMoveIOTaskWithScansWarnTest, WarnSingleFileCancel) {
  // Create file.
  auto file = SetupFile(/*on_enabled_fs=*/true, "file.txt");
  auto dest = GetDestinationFileSystemURL("");

  // Mark the file to have a warning.
  SetFileHasWarning(file);

  // The file should be scanned, but there should not be a check for its result,
  // as the warning is canceled.
  ExpectScanWithoutCheckingResult(file);

  base::RunLoop run_loop;

  // Setup the expected callbacks.
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  ExpectExtraProgressCallbackCalls(progress_callback, {file}, dest);
  ExpectScanningCallbackCall(progress_callback, {file}, dest, 1);

  // Expect that the completion callback isn't run.
  EXPECT_CALL(complete_callback, Run(_)).Times(0);

  // Start the copy/move.
  CopyOrMoveIOTask task(GetOperationType(), GetSourceUrlsFromFileInfos({file}),
                        dest, profile_, file_system_context_);
  task.SetTaskID(kTaskId);

  // Expect a warning dialog and cancel.
  ExpectFPNMFilesWarningDialogAndCancel(run_loop.QuitClosure());

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
  SetFileTransferAnalysisResult(
      file, enterprise_connectors::FileTransferAnalysisDelegate::
                FileTransferAnalysisResult::Allowed());

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
  SetFileTransferAnalysisResult(
      enabled_file,
      enterprise_connectors::FileTransferAnalysisDelegate::
          FileTransferAnalysisResult::Blocked(
              enterprise_connectors::FinalContentAnalysisResult::FAILURE,
              enterprise_connectors::kDlpTag));
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
      run_loop.QuitClosure(), /*maybe_total_num_files=*/std::nullopt,
      /*maybe_policy_errors=*/
      {UseNewPolicyUI() && UseNewConnectorsUI()
           ? std::make_optional(PolicyError(
                 PolicyErrorType::kEnterpriseConnectors, 1, "file1.txt"))
           : std::nullopt});

  if (UseNewPolicyUI() && UseNewConnectorsUI()) {
    ExpectFPNMBlockedFiles({enabled_file},
                           policy::FilesPolicyDialog::BlockReason::
                               kEnterpriseConnectorsSensitiveData);
  }

  // Start the copy/move.
  CopyOrMoveIOTask task(
      GetOperationType(),
      GetSourceUrlsFromFileInfos({enabled_file, disabled_file}), dest, profile_,
      file_system_context_);
  task.SetTaskID(kTaskId);
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

  // Expect a scan for both files and block the transfer for all files.
  ExpectDirectoryScan(directory);
  SetFileTransferAnalysisResult(
      file0, enterprise_connectors::FileTransferAnalysisDelegate::
                 FileTransferAnalysisResult::Blocked(
                     enterprise_connectors::FinalContentAnalysisResult::FAILURE,
                     enterprise_connectors::kDlpTag));
  SetFileTransferAnalysisResult(
      file1, enterprise_connectors::FileTransferAnalysisDelegate::
                 FileTransferAnalysisResult::Blocked(
                     enterprise_connectors::FinalContentAnalysisResult::FAILURE,
                     enterprise_connectors::kMalwareTag));

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

  std::vector<std::optional<PolicyError>> maybe_policy_errors;
  if (UseNewPolicyUI() && UseNewConnectorsUI()) {
    // Depending on the order of the execution, `file_name` can be different.
    maybe_policy_errors.push_back(
        PolicyError(PolicyErrorType::kEnterpriseConnectors, 2, "file0.txt"));
    maybe_policy_errors.push_back(
        PolicyError(PolicyErrorType::kEnterpriseConnectors, 2, "file1.txt"));
  } else {
    maybe_policy_errors.push_back(std::nullopt);
  }

  ExpectCompletionCallbackCall(complete_callback, {directory}, dest,
                               {expected_error}, run_loop.QuitClosure(),
                               /*maybe_total_num_files=*/2,
                               /*maybe_policy_errors=*/
                               maybe_policy_errors);

  if (UseNewPolicyUI() && UseNewConnectorsUI()) {
    ExpectFPNMBlockedFiles({file0}, policy::FilesPolicyDialog::BlockReason::
                                        kEnterpriseConnectorsSensitiveData);
    ExpectFPNMBlockedFiles(
        {file1},
        policy::FilesPolicyDialog::BlockReason::kEnterpriseConnectorsMalware);
  }

  // Start the copy/move.
  CopyOrMoveIOTask task(GetOperationType(),
                        GetSourceUrlsFromFileInfos({directory}), dest, profile_,
                        file_system_context_);
  task.SetTaskID(kTaskId);
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

  // Expect a scan for both files and block the transfer of one file.
  ExpectDirectoryScan(directory);
  SetFileTransferAnalysisResult(
      file0, enterprise_connectors::FileTransferAnalysisDelegate::
                 FileTransferAnalysisResult::Blocked(
                     enterprise_connectors::FinalContentAnalysisResult::FAILURE,
                     enterprise_connectors::kDlpTag));
  SetFileTransferAnalysisResult(
      file1, enterprise_connectors::FileTransferAnalysisDelegate::
                 FileTransferAnalysisResult::Allowed());

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

  ExpectCompletionCallbackCall(
      complete_callback, {directory}, dest, {expected_error},
      run_loop.QuitClosure(),
      /*maybe_total_num_files=*/2,
      /*maybe_policy_errors=*/
      {UseNewPolicyUI() && UseNewConnectorsUI()
           ? std::make_optional(PolicyError(
                 PolicyErrorType::kEnterpriseConnectors, 1, "file0.txt"))
           : std::nullopt});

  if (UseNewPolicyUI() && UseNewConnectorsUI()) {
    ExpectFPNMBlockedFiles({file0}, policy::FilesPolicyDialog::BlockReason::
                                        kEnterpriseConnectorsSensitiveData);
  }

  // Start the copy/move.
  CopyOrMoveIOTask task(GetOperationType(),
                        GetSourceUrlsFromFileInfos({directory}), dest, profile_,
                        file_system_context_);
  task.SetTaskID(kTaskId);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  // Wait for the copy/move to be completed.
  run_loop.Run();

  // Verify the directory and the files after the copy/move.
  VerifyDirectoryExistsAtSourceAndDestination(directory);
  VerifyFileWasNotTransferred(file0);
  VerifyFileWasTransferred(file1);
}

TEST_P(CopyOrMoveIOTaskWithScansWarnTest,
       DirectoryTransferBlockSomeWarnSomeProceed) {
  // This test can only work for UseNewConnectorsUI() == true.
  ASSERT_TRUE(UseNewConnectorsUI());

  // Create directory.
  FileInfo directory{"", GetSourceFileSystemURLForEnabledVolume("folder"),
                     GetDestinationFileSystemURL("folder")};
  ASSERT_TRUE(base::CreateDirectory(directory.source_url.path()));

  // Create the files.
  auto blocked_file_0 =
      SetupFile(/*on_enabled_fs=*/true, "folder/0_file_blocked.txt");
  auto blocked_file_1 =
      SetupFile(/*on_enabled_fs=*/true, "folder/1_file_blocked.txt");
  auto blocked_file_2 =
      SetupFile(/*on_enabled_fs=*/true, "folder/2_encrypted_file_blocked.txt");
  auto blocked_file_3 =
      SetupFile(/*on_enabled_fs=*/true, "folder/3_large_file_blocked.txt");
  auto allowed_file_0 =
      SetupFile(/*on_enabled_fs=*/true, "folder/4_file_allowed.txt");
  auto allowed_file_1 =
      SetupFile(/*on_enabled_fs=*/true, "folder/5_file_allowed.txt");
  auto warned_file_0 =
      SetupFile(/*on_enabled_fs=*/true, "folder/6_file_warned.txt");
  auto warned_file_1 =
      SetupFile(/*on_enabled_fs=*/true, "folder/7_file_warned.txt");

  // Mark the file to have a warning.
  SetFileHasWarning(warned_file_0);
  SetFileHasWarning(warned_file_1);

  // Expect a scan for both files and block the transfer for some files and warn
  // for other files.
  ExpectDirectoryScan(directory);
  SetFileTransferAnalysisResult(
      blocked_file_0,
      enterprise_connectors::FileTransferAnalysisDelegate::
          FileTransferAnalysisResult::Blocked(
              enterprise_connectors::FinalContentAnalysisResult::FAILURE,
              enterprise_connectors::kDlpTag));
  SetFileTransferAnalysisResult(
      blocked_file_1,
      enterprise_connectors::FileTransferAnalysisDelegate::
          FileTransferAnalysisResult::Blocked(
              enterprise_connectors::FinalContentAnalysisResult::FAILURE,
              enterprise_connectors::kMalwareTag));
  SetFileTransferAnalysisResult(
      blocked_file_2, enterprise_connectors::FileTransferAnalysisDelegate::
                          FileTransferAnalysisResult::Blocked(
                              enterprise_connectors::
                                  FinalContentAnalysisResult::ENCRYPTED_FILES,
                              /*tag=*/std::string()));
  SetFileTransferAnalysisResult(
      blocked_file_3,
      enterprise_connectors::FileTransferAnalysisDelegate::
          FileTransferAnalysisResult::Blocked(
              enterprise_connectors::FinalContentAnalysisResult::LARGE_FILES,
              /*tag=*/std::string()));
  SetFileTransferAnalysisResult(
      allowed_file_0, enterprise_connectors::FileTransferAnalysisDelegate::
                          FileTransferAnalysisResult::Allowed());
  SetFileTransferAnalysisResult(
      allowed_file_1, enterprise_connectors::FileTransferAnalysisDelegate::
                          FileTransferAnalysisResult::Allowed());
  // We proceed on the warning, so the result is allowed for the warned file
  SetFileTransferAnalysisResult(
      warned_file_0, enterprise_connectors::FileTransferAnalysisDelegate::
                         FileTransferAnalysisResult::Allowed());
  SetFileTransferAnalysisResult(
      warned_file_1, enterprise_connectors::FileTransferAnalysisDelegate::
                         FileTransferAnalysisResult::Allowed());

  auto dest = GetDestinationFileSystemURL("");

  base::RunLoop run_loop;

  // Setup the expected callbacks.
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  ExpectExtraProgressCallbackCalls(progress_callback, {directory}, dest,
                                   /*total_num_files=*/8);
  ExpectScanningCallbackCall(progress_callback, {directory}, dest, 1);

  // For moves, only the last error is reported. The last step the operation
  // performs is to try to remove the parent directory. This fails with
  // FILE_ERROR_NOT_EMPTY, as there are files that weren't moved.
  auto expected_error = GetOperationType() == OperationType::kCopy
                            ? base::File::FILE_ERROR_SECURITY
                            : base::File::FILE_ERROR_NOT_EMPTY;

  std::vector<std::optional<PolicyError>> maybe_policy_errors;
  if (UseNewPolicyUI()) {
    // Depending on the order of the execution, `file_name` can be different.
    maybe_policy_errors.push_back(PolicyError(
        PolicyErrorType::kEnterpriseConnectors, 4, "0_file_blocked.txt"));
    maybe_policy_errors.push_back(PolicyError(
        PolicyErrorType::kEnterpriseConnectors, 4, "1_file_blocked.txt"));
    maybe_policy_errors.push_back(
        PolicyError(PolicyErrorType::kEnterpriseConnectors, 4,
                    "2_encrypted_file_blocked.txt"));
    maybe_policy_errors.push_back(PolicyError(
        PolicyErrorType::kEnterpriseConnectors, 4, "3_large_file_blocked.txt"));
  } else {
    maybe_policy_errors.push_back(std::nullopt);
  }

  ExpectCompletionCallbackCall(complete_callback, {directory}, dest,
                               {expected_error}, run_loop.QuitClosure(),
                               /*maybe_total_num_files=*/8,
                               /*maybe_policy_errors=*/
                               maybe_policy_errors);

  // Start the copy/move.
  CopyOrMoveIOTask task(GetOperationType(),
                        GetSourceUrlsFromFileInfos({directory}), dest, profile_,
                        file_system_context_);
  task.SetTaskID(kTaskId);

  // Expect a warning dialog and error dialog about the blocked files
  ExpectFPNMFilesWarningDialogAndProceed(task);

  if (UseNewPolicyUI()) {
    // We expect a call to
    // FilesPolicyNotificationManager::AddConnectorsBlockedFiles for scan result
    // tag.
    ExpectFPNMBlockedFiles({blocked_file_0},
                           policy::FilesPolicyDialog::BlockReason::
                               kEnterpriseConnectorsSensitiveData);
    ExpectFPNMBlockedFiles(
        {blocked_file_1},
        policy::FilesPolicyDialog::BlockReason::kEnterpriseConnectorsMalware);
    ExpectFPNMBlockedFiles({blocked_file_2},
                           policy::FilesPolicyDialog::BlockReason::
                               kEnterpriseConnectorsEncryptedFile);
    ExpectFPNMBlockedFiles(
        {blocked_file_3},
        policy::FilesPolicyDialog::BlockReason::kEnterpriseConnectorsLargeFile);
  }

  task.Execute(progress_callback.Get(), complete_callback.Get());
  // Wait for the copy/move to be completed.
  run_loop.Run();

  // Verify the directory and the files after the copy/move.
  VerifyDirectoryExistsAtSourceAndDestination(directory);
  VerifyFileWasNotTransferred(blocked_file_0);
  VerifyFileWasNotTransferred(blocked_file_1);
  VerifyFileWasNotTransferred(blocked_file_2);
  VerifyFileWasNotTransferred(blocked_file_3);
  VerifyFileWasTransferred(allowed_file_0);
  VerifyFileWasTransferred(allowed_file_1);
  VerifyFileWasTransferred(warned_file_0);
  VerifyFileWasTransferred(warned_file_1);
}

TEST_P(CopyOrMoveIOTaskWithScansWarnTest,
       DirectoryTransferBlockSomeWarnSomeCancel) {
  // Create directory.
  FileInfo directory{"", GetSourceFileSystemURLForEnabledVolume("folder"),
                     GetDestinationFileSystemURL("folder")};
  ASSERT_TRUE(base::CreateDirectory(directory.source_url.path()));

  // Create the files.
  auto blocked_file_0 =
      SetupFile(/*on_enabled_fs=*/true, "folder/0_file_blocked.txt");
  auto blocked_file_1 =
      SetupFile(/*on_enabled_fs=*/true, "folder/1_file_blocked.txt");
  auto allowed_file_0 =
      SetupFile(/*on_enabled_fs=*/true, "folder/2_file_allowed.txt");
  auto allowed_file_1 =
      SetupFile(/*on_enabled_fs=*/true, "folder/3_file_allowed.txt");
  auto warned_file_0 =
      SetupFile(/*on_enabled_fs=*/true, "folder/4_file_warned.txt");
  auto warned_file_1 =
      SetupFile(/*on_enabled_fs=*/true, "folder/5_file_warned.txt");

  // Mark the file to have a warning.
  SetFileHasWarning(warned_file_0);
  SetFileHasWarning(warned_file_1);

  // Expect a scan for both files and block the transfer for some files and warn
  // for other files.
  ExpectDirectoryScan(directory);

  // All files should be scanned, but as the transfer is cancelled, no result
  // should be checked.
  ExpectScanWithoutCheckingResult(blocked_file_0);
  ExpectScanWithoutCheckingResult(blocked_file_1);
  ExpectScanWithoutCheckingResult(allowed_file_0);
  ExpectScanWithoutCheckingResult(allowed_file_1);
  ExpectScanWithoutCheckingResult(warned_file_0);
  ExpectScanWithoutCheckingResult(warned_file_1);

  auto dest = GetDestinationFileSystemURL("");

  base::RunLoop run_loop;

  // Setup the expected callbacks.
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  ExpectExtraProgressCallbackCalls(progress_callback, {directory}, dest,
                                   /*total_num_files=*/6);
  ExpectScanningCallbackCall(progress_callback, {directory}, dest, 1);

  // Expect that the completion callback isn't run.
  EXPECT_CALL(complete_callback, Run(_)).Times(0);

  // Start the copy/move.
  CopyOrMoveIOTask task(GetOperationType(),
                        GetSourceUrlsFromFileInfos({directory}), dest, profile_,
                        file_system_context_);
  task.SetTaskID(kTaskId);

  // Expect a warning dialog and error dialog about the blocked files
  ExpectFPNMFilesWarningDialogAndCancel(run_loop.QuitClosure());

  task.Execute(progress_callback.Get(), complete_callback.Get());
  // Wait for the copy/move to be completed.
  run_loop.Run();

  // Verify the directory and the files after the copy/move.
  VerifyDirectoryWasNotTransferred(directory);
  VerifyFileWasNotTransferred(blocked_file_0);
  VerifyFileWasNotTransferred(blocked_file_1);
  VerifyFileWasNotTransferred(allowed_file_0);
  VerifyFileWasNotTransferred(allowed_file_1);
  VerifyFileWasNotTransferred(warned_file_0);
  VerifyFileWasNotTransferred(warned_file_1);
}

TEST_P(CopyOrMoveIOTaskWithScansTest, DirectoryTransferAllowAll) {
  // Create directory.
  FileInfo directory{"", GetSourceFileSystemURLForEnabledVolume("folder"),
                     GetDestinationFileSystemURL("folder")};
  ASSERT_TRUE(base::CreateDirectory(directory.source_url.path()));

  // Create the files.
  auto file0 = SetupFile(/*on_enabled_fs=*/true, "folder/file0.txt");
  auto file1 = SetupFile(/*on_enabled_fs=*/true, "folder/file1.txt");

  // Expect a scan for both files and allow the transfer for all files.
  ExpectDirectoryScan(directory);
  SetFileTransferAnalysisResult(
      file0, enterprise_connectors::FileTransferAnalysisDelegate::
                 FileTransferAnalysisResult::Allowed());
  SetFileTransferAnalysisResult(
      file1, enterprise_connectors::FileTransferAnalysisDelegate::
                 FileTransferAnalysisResult::Allowed());

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

class CopyOrMoveIOTaskWithDLPTest : public testing::Test {
 public:
  CopyOrMoveIOTaskWithDLPTest(const CopyOrMoveIOTaskWithDLPTest&) = delete;
  CopyOrMoveIOTaskWithDLPTest& operator=(const CopyOrMoveIOTaskWithDLPTest&) =
      delete;
  ~CopyOrMoveIOTaskWithDLPTest() override = default;

 protected:
  CopyOrMoveIOTaskWithDLPTest() = default;

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager =
        std::make_unique<testing::NiceMock<policy::MockDlpRulesManager>>(
            Profile::FromBrowserContext(context));
    mock_rules_manager_ = dlp_rules_manager.get();
    ON_CALL(*mock_rules_manager_, IsFilesPolicyEnabled)
        .WillByDefault(testing::Return(true));

    files_controller_ = std::make_unique<
        testing::StrictMock<policy::MockDlpFilesControllerAsh>>(
        *mock_rules_manager_, profile_.get());

    ON_CALL(*mock_rules_manager_, GetDlpFilesController())
        .WillByDefault(::testing::Return(files_controller_.get()));

    return dlp_rules_manager;
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kNewFilesPolicyUX);

    AccountId account_id = AccountId::FromUserEmailGaiaId(kEmailId, kGaiaId);
    profile_->SetIsNewProfile(true);
    user_manager::User* user =
        fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
            account_id, /*is_affiliated=*/false,
            user_manager::UserType::kRegular, profile_.get());
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
    fake_user_manager_->SimulateUserProfileLoad(account_id);

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

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  ash::disks::FakeDiskMountManager disk_mount_manager_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
  const std::unique_ptr<TestingProfile> profile_{
      std::make_unique<TestingProfile>()};
  raw_ptr<policy::MockDlpRulesManager, DanglingUntriaged> mock_rules_manager_ =
      nullptr;
  std::unique_ptr<policy::MockDlpFilesControllerAsh> files_controller_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
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
      CheckIfTransferAllowed(std::make_optional(task_id), source_urls, dest,
                             /*is_move=*/false, testing::_))
      .WillOnce(
          [source_urls](
              std::optional<file_manager::io_task::IOTaskId> task_id,
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
      CheckIfTransferAllowed(std::make_optional(task_id), source_urls, dest,
                             /*is_move=*/false, testing::_))
      .WillOnce(
          [](std::optional<file_manager::io_task::IOTaskId> task_id,
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
                Field(&ProgressStatus::policy_error, std::nullopt),
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
      CheckIfTransferAllowed(std::make_optional(task_id), source_urls, dest,
                             /*is_move=*/true, testing::_))
      .WillOnce(
          [blocked_file](
              std::optional<file_manager::io_task::IOTaskId> task_id,
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
      CheckIfTransferAllowed(std::make_optional(task_id), source_urls, dest,
                             /*is_move=*/false, testing::_))
      .WillOnce(
          [&task](std::optional<file_manager::io_task::IOTaskId> task_id,
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
                Field(&ProgressStatus::bytes_transferred, 0))));
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
                Field(&ProgressStatus::policy_error, std::nullopt),
                Field(&ProgressStatus::bytes_transferred, 2 * kTestFileSize))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  // Start the copy.
  task.Execute(progress_callback.Get(), complete_callback.Get());
  // Wait for the copy to be completed.
  run_loop.Run();
}

}  // namespace file_manager::io_task
