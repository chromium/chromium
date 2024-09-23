// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_installer.h"

#include <stdint.h>
#include <string.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_drive_image_download_service.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_image_download_client.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_installer_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_metrics_util.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/ash/components/dbus/spaced/fake_spaced_client.h"
#include "chromeos/ash/components/dbus/vm_plugin_dispatcher/vm_plugin_dispatcher_client.h"
#include "components/account_id/account_id.h"
#include "components/download/public/background_service/test/test_download_service.h"
#include "components/drive/service/dummy_drive_service.h"
#include "components/drive/service/fake_drive_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/api_error_codes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

namespace {

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::DoubleEq;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::StrictMock;

using FailureReason = PluginVmInstaller::FailureReason;
using InstallingState = PluginVmInstaller::InstallingState;

const char kProfileName[] = "p1";
const char kUrl[] = "http://example.com";
const char kDriveUrl[] = "https://drive.google.com/open?id=fakedriveid";
const char kDriveId[] = "fakedriveid";
const char kDriveUrl2[] = "https://drive.google.com/open?id=nonexistantdriveid";
const char kDriveContentType[] = "application/zip";
const char kPluginVmImageFile[] = "plugin_vm_image_file_1.zip";
const char kContent[] = "This is zipped content.";
const char kHash[] =
    "c80344fd4a0e9ee9f803f64edb3ea3ed8b11fe300869817e8fd50898d0663c35";
const char kHashUppercase[] =
    "C80344FD4A0E9EE9F803F64EDB3EA3ED8B11FE300869817E8FD50898D0663C35";
const char kHash2[] =
    "02f06421ae27144aacdc598aebcd345a5e2e634405e8578300173628fe1574bd";
// File size set in test_download_service.
const int64_t kDefaultRequiredFreeDiskSpaceGB = 20LL;
const int kRequiredFreeDiskSpaceGB = 40;
const int64_t kBytesPerGigabyte = 1024 * 1024 * 1024;

constexpr char kFailureReasonHistogram[] = "PluginVm.SetupFailureReason";

}  // namespace

class MockObserver : public PluginVmInstaller::Observer {
 public:
  MOCK_METHOD1(OnStateUpdated, void(InstallingState));
  MOCK_METHOD1(OnProgressUpdated, void(double));
  MOCK_METHOD2(OnDownloadProgressUpdated,
               void(uint64_t bytes_downloaded, int64_t content_length));
  MOCK_METHOD0(OnVmExists, void());
  MOCK_METHOD0(OnCreated, void());
  MOCK_METHOD0(OnImported, void());
  MOCK_METHOD1(OnError, void(FailureReason));
  MOCK_METHOD0(OnCancelFinished, void());
};

// We are inheriting from DummyDriveService instead of DriveServiceInterface
// here since we are only interested in a couple of methods and don't need to
// define the rest.
class SimpleFakeDriveService : public drive::DummyDriveService {
 public:
  using DownloadActionCallback = google_apis::DownloadActionCallback;
  using GetContentCallback = google_apis::GetContentCallback;
  using ProgressCallback = google_apis::ProgressCallback;

  void RunDownloadActionCallback(google_apis::ApiErrorCode error,
                                 const base::FilePath& temp_file) {
    std::move(download_action_callback_).Run(error, temp_file);
  }

  void RunGetContentCallback(google_apis::ApiErrorCode error,
                             std::unique_ptr<std::string> content) {
    get_content_callback_.Run(error, std::move(content),
                              !get_content_callback_called_);
    get_content_callback_called_ = true;
  }

  void RunProgressCallback(int64_t progress, int64_t total) {
    progress_callback_.Run(progress, total);
  }

  bool cancel_callback_called() const { return cancel_callback_called_; }

  // DriveServiceInterface override.
  google_apis::CancelCallbackOnce DownloadFile(
      const base::FilePath& /*cache_path*/,
      const std::string& /*resource_id*/,
      DownloadActionCallback download_action_callback,
      const GetContentCallback& get_content_callback,
      ProgressCallback progress_callback) override {
    download_action_callback_ = std::move(download_action_callback);
    get_content_callback_ = get_content_callback;
    progress_callback_ = std::move(progress_callback);

    // It is safe to use base::Unretained as this object will not get deleted
    // before the end of the test.
    return base::BindOnce(&SimpleFakeDriveService::CancelCallback,
                          base::Unretained(this));
  }

 private:
  void CancelCallback() { cancel_callback_called_ = true; }

  bool cancel_callback_called_{false};

  DownloadActionCallback download_action_callback_;
  GetContentCallback get_content_callback_;
  ProgressCallback progress_callback_;

  bool get_content_callback_called_{false};
};

class PluginVmInstallerTestBase : public testing::Test {
 public:
  PluginVmInstallerTestBase() = default;

  PluginVmInstallerTestBase(const PluginVmInstallerTestBase&) = delete;
  PluginVmInstallerTestBase& operator=(const PluginVmInstallerTestBase&) =
      delete;

  ~PluginVmInstallerTestBase() override = default;

 protected:
  void SetUp() override {
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    ash::DebugDaemonClient::InitializeFake();
    ash::VmPluginDispatcherClient::InitializeFake();

    ASSERT_TRUE(profiles_dir_.CreateUniqueTempDir());
    CreateProfile();
    plugin_vm_test_helper_ =
        std::make_unique<PluginVmTestHelper>(profile_.get());
    plugin_vm_test_helper_->AllowPluginVm();
    // Sets new PluginVmImage pref for this test.
    SetPluginVmImagePref(kUrl, kHash);

    installer_ = PluginVmInstallerFactory::GetForProfile(profile_.get());
    observer_ = std::make_unique<StrictMock<MockObserver>>();
    installer_->SetObserver(observer_.get());
    installer_->SkipLicenseCheckForTesting();
    installer_->SetFreeDiskSpaceForTesting(std::numeric_limits<int64_t>::max());
    installer_->SetDownloadedImageForTesting(CreateZipFile());

    SetDefaultExpectations();

    fake_concierge_client_ = ash::FakeConciergeClient::Get();

    ash::FakeSpacedClient::InitializeFake();
  }

  void TearDown() override {
    observer_.reset();
    plugin_vm_test_helper_.reset();
    profile_.reset();
    observer_.reset();

    ash::VmPluginDispatcherClient::Shutdown();
    ash::DebugDaemonClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::FakeSpacedClient::Shutdown();
  }

  void SetPluginVmImagePref(std::string url, std::string hash) {
    ScopedDictPrefUpdate update(profile_->GetPrefs(), prefs::kPluginVmImage);
    base::Value::Dict& plugin_vm_image = update.Get();
    plugin_vm_image.Set("url", url);
    plugin_vm_image.Set("hash", hash);
  }

  void SetRequiredFreeDiskSpaceGBPref(int required_free_disk_space) {
    profile_->GetPrefs()->SetInteger(prefs::kPluginVmRequiredFreeDiskSpaceGB,
                                     required_free_disk_space);
  }

  base::FilePath CreateZipFile() {
    base::FilePath zip_file_path =
        profile_->GetPath().AppendASCII(kPluginVmImageFile);
    base::WriteFile(zip_file_path, kContent);
    return zip_file_path;
  }

  void VerifyExpectations() {
    Mock::VerifyAndClearExpectations(observer_.get());
    SetDefaultExpectations();
  }

  // Set expectations for observer events up to and including |end_state|.
  void ExpectObserverEventsUntil(InstallingState end_state) {
    InstallingState states[] = {
        InstallingState::kCheckingLicense,
        InstallingState::kCheckingForExistingVm,
        InstallingState::kCheckingDiskSpace,
        InstallingState::kDownloadingDlc,
        InstallingState::kStartingDispatcher,
        InstallingState::kDownloadingImage,
        InstallingState::kImporting,
    };

    for (InstallingState state : states) {
      EXPECT_CALL(*observer_, OnStateUpdated(state));
      if (state == end_state) {
        return;
      }
    }

    NOTREACHED_IN_MIGRATION();
  }

  // Helper functions for starting and progressing the installer.

  void RunUntil(InstallingState state) {
    base::RunLoop run_loop;
    ON_CALL(*observer_, OnStateUpdated(state))
        .WillByDefault(RunClosure(run_loop.QuitClosure()));
    run_loop.Run();
  }

  void StartAndRunUntil(InstallingState state) {
    installer_->Start();
    RunUntil(state);
  }

  void StartAndRunToCompletion() {
    installer_->Start();
    task_environment_.RunUntilIdle();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<PluginVmTestHelper> plugin_vm_test_helper_;
  raw_ptr<PluginVmInstaller, DanglingUntriaged> installer_;
  std::unique_ptr<MockObserver> observer_;

  // A pointer to a singleton object which is valid until
  // ConciergeClient::Shutdown() is called.
  raw_ptr<ash::FakeConciergeClient, DanglingUntriaged> fake_concierge_client_;
  ash::FakeDlcserviceClient fake_dlcservice_client_;

 private:
  void CreateProfile() {
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(kProfileName);
    profile_builder.SetPath(profiles_dir_.GetPath().AppendASCII(kProfileName));
    std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(pref_service->registry());
    profile_builder.SetPrefService(std::move(pref_service));
    profile_ = profile_builder.Build();
  }

  void SetDefaultExpectations() {
    // Suppress progress updates.
    EXPECT_CALL(*observer_, OnProgressUpdated(_)).Times(AnyNumber());
  }

  base::ScopedTempDir profiles_dir_;
};

class PluginVmInstallerDownloadServiceTest : public PluginVmInstallerTestBase {
 public:
  PluginVmInstallerDownloadServiceTest() = default;

  PluginVmInstallerDownloadServiceTest(
      const PluginVmInstallerDownloadServiceTest&) = delete;
  PluginVmInstallerDownloadServiceTest& operator=(
      const PluginVmInstallerDownloadServiceTest&) = delete;

  ~PluginVmInstallerDownloadServiceTest() override = default;

 protected:
  void SetUp() override {
    PluginVmInstallerTestBase::SetUp();

    download_service_ = std::make_unique<download::test::TestDownloadService>();
    download_service_->SetIsReady(true);
    download_service_->SetHash256(kHash);
    client_ = std::make_unique<PluginVmImageDownloadClient>(profile_.get());
    download_service_->set_client(client_.get());
    installer_->SetDownloadServiceForTesting(download_service_.get());
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    histogram_tester_.reset();
    download_service_.reset();
    client_.reset();

    PluginVmInstallerTestBase::TearDown();
  }

  std::unique_ptr<download::test::TestDownloadService> download_service_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  std::unique_ptr<PluginVmImageDownloadClient> client_;
};

class PluginVmInstallerDriveTest : public PluginVmInstallerTestBase {
 public:
  PluginVmInstallerDriveTest() = default;

  PluginVmInstallerDriveTest(const PluginVmInstallerDriveTest&) = delete;
  PluginVmInstallerDriveTest& operator=(const PluginVmInstallerDriveTest&) =
      delete;

  ~PluginVmInstallerDriveTest() override = default;

 protected:
  void SetUp() override {
    PluginVmInstallerTestBase::SetUp();

    google_apis::ApiErrorCode error = google_apis::OTHER_ERROR;
    std::unique_ptr<google_apis::FileResource> entry;
    auto fake_drive_service = std::make_unique<drive::FakeDriveService>();
    // We will need to access this object for some tests in the future.
    fake_drive_service_ = fake_drive_service.get();
    fake_drive_service->AddNewFileWithResourceId(
        kDriveId, kDriveContentType, kContent,
        "",  // parent_resource_id
        kPluginVmImageFile,
        true,  // shared_with_me
        base::BindLambdaForTesting(
            [&](google_apis::ApiErrorCode drive_error,
                std::unique_ptr<google_apis::FileResource> drive_entry) {
              error = drive_error;
              entry = std::move(drive_entry);
            }));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(google_apis::HTTP_CREATED, error);
    ASSERT_TRUE(entry);

    auto drive_download_service =
        std::make_unique<PluginVmDriveImageDownloadService>(installer_,
                                                            profile_.get());
    // We will need to access this object for some tests in the future.
    drive_download_service_ = drive_download_service.get();

    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    drive_download_service->SetDownloadDirectoryForTesting(temp_dir.Take());
    drive_download_service->SetDriveServiceForTesting(
        std::move(fake_drive_service));

    installer_->SetDriveDownloadServiceForTesting(
        std::move(drive_download_service));
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    histogram_tester_.reset();

    PluginVmInstallerTestBase::TearDown();
  }

  SimpleFakeDriveService* SetUpSimpleFakeDriveService() {
    auto fake_drive_service = std::make_unique<SimpleFakeDriveService>();
    SimpleFakeDriveService* fake_drive_service_ptr = fake_drive_service.get();

    drive_download_service_->SetDriveServiceForTesting(
        std::move(fake_drive_service));

    return fake_drive_service_ptr;
  }

  raw_ptr<PluginVmDriveImageDownloadService, DanglingUntriaged>
      drive_download_service_;
  raw_ptr<drive::FakeDriveService, DanglingUntriaged> fake_drive_service_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(PluginVmInstallerDownloadServiceTest, ProgressUpdates) {
  SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);

  // Override default expectation so unexpected calls will fail the test.
  EXPECT_CALL(*observer_, OnProgressUpdated(_)).Times(0);

  EXPECT_CALL(*observer_, OnProgressUpdated(DoubleEq(0.01)));
  EXPECT_CALL(*observer_, OnProgressUpdated(DoubleEq(0.45)));
  EXPECT_CALL(*observer_, OnProgressUpdated(DoubleEq(0.725)));

  ExpectObserverEventsUntil(InstallingState::kImporting);
  EXPECT_CALL(*observer_, OnImported());
  StartAndRunToCompletion();
}

TEST_F(PluginVmInstallerDownloadServiceTest, InsufficientDisk) {
  installer_->SetFreeDiskSpaceForTesting(
      kDefaultRequiredFreeDiskSpaceGB * kBytesPerGigabyte - 1);
  ExpectObserverEventsUntil(InstallingState::kCheckingDiskSpace);
  EXPECT_CALL(*observer_, OnError(FailureReason::INSUFFICIENT_DISK_SPACE));
  StartAndRunToCompletion();
  histogram_tester_->ExpectUniqueSample(
      kFailureReasonHistogram, FailureReason::INSUFFICIENT_DISK_SPACE, 1);
  histogram_tester_->ExpectUniqueSample(kPluginVmSetupResultHistogram,
                                        PluginVmSetupResult::kError, 1);
}

TEST_F(PluginVmInstallerDownloadServiceTest, InsufficientDiskWhenSetInPolicy) {
  SetRequiredFreeDiskSpaceGBPref(kRequiredFreeDiskSpaceGB);
  int64_t requred_free_disk_space_bytes =
      kRequiredFreeDiskSpaceGB * kBytesPerGigabyte;
  installer_->SetFreeDiskSpaceForTesting(requred_free_disk_space_bytes - 1);
  ExpectObserverEventsUntil(InstallingState::kCheckingDiskSpace);
  EXPECT_CALL(*observer_, OnError(FailureReason::INSUFFICIENT_DISK_SPACE));
  StartAndRunToCompletion();
  histogram_tester_->ExpectUniqueSample(
      kFailureReasonHistogram, FailureReason::INSUFFICIENT_DISK_SPACE, 1);
}

TEST_F(PluginVmInstallerDownloadServiceTest, VmExists) {
  // This flow works even if the image url is not set.
  SetPluginVmImagePref("", kHash);

  vm_tools::concierge::ListVmDisksResponse list_vm_disks_response;
  list_vm_disks_response.set_success(true);
  auto* image = list_vm_disks_response.add_images();
  image->set_name(kPluginVmName);
  image->set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_PLUGINVM);
  fake_concierge_client_->set_list_vm_disks_response(list_vm_disks_response);

  ExpectObserverEventsUntil(InstallingState::kCheckingForExistingVm);
  EXPECT_CALL(*observer_, OnVmExists());
  StartAndRunToCompletion();

  histogram_tester_->ExpectUniqueSample(
      kPluginVmSetupResultHistogram, PluginVmSetupResult::kVmAlreadyExists, 1);
}

TEST_F(PluginVmInstallerDownloadServiceTest, InvalidVmExists) {
  // This flow works even if the image url is not set.
  SetPluginVmImagePref("", kHash);

  vm_tools::concierge::ListVmDisksResponse list_vm_disks_response;
  list_vm_disks_response.set_success(true);
  auto* image = list_vm_disks_response.add_images();
  // Pretend we have a VM with the right name in a wrong location.
  image->set_name(kPluginVmName);
  image->set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
  fake_concierge_client_->set_list_vm_disks_response(list_vm_disks_response);

  ExpectObserverEventsUntil(InstallingState::kCheckingForExistingVm);
  EXPECT_CALL(*observer_, OnError(FailureReason::EXISTING_IMAGE_INVALID));
  StartAndRunToCompletion();

  histogram_tester_->ExpectUniqueSample(
      kFailureReasonHistogram, FailureReason::EXISTING_IMAGE_INVALID, 1);
}

TEST_F(PluginVmInstallerDownloadServiceTest, CancelOnVmExistsCheck) {
  base::RunLoop run_loop;
  ExpectObserverEventsUntil(InstallingState::kCheckingForExistingVm);
  ON_CALL(*observer_, OnStateUpdated(InstallingState::kCheckingForExistingVm))
      .WillByDefault(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(*observer_, OnCancelFinished());

  installer_->Start();
  run_loop.Run();
  installer_->Cancel();
  task_environment_.RunUntilIdle();

  histogram_tester_->ExpectUniqueSample(
      kPluginVmSetupResultHistogram,
      PluginVmSetupResult::kUserCancelledCheckingForExistingVm, 1);
}

TEST_F(PluginVmInstallerDownloadServiceTest, DownloadPluginVmImageParamsTest) {
  SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);

  ExpectObserverEventsUntil(InstallingState::kImporting);
  EXPECT_CALL(*observer_, OnImported());

  StartAndRunUntil(InstallingState::kDownloadingImage);

  std::string guid = installer_->GetCurrentDownloadGuid();
  const std::optional<download::DownloadParams>& params =
      download_service_->GetDownload(guid);
  ASSERT_TRUE(params.has_value());
  EXPECT_EQ(guid, params->guid);
  EXPECT_EQ(download::DownloadClient::PLUGIN_VM_IMAGE, params->client);
  EXPECT_EQ(GURL(kUrl), params->request_params.url);

  // Finishing image processing.
  task_environment_.RunUntilIdle();
}

TEST_F(PluginVmInstallerDownloadServiceTest, OnlyOneImageIsProcessedTest) {
  SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);

  ExpectObserverEventsUntil(InstallingState::kImporting);
  EXPECT_CALL(*observer_, OnImported());

  StartAndRunUntil(InstallingState::kDownloadingImage);

  EXPECT_TRUE(installer_->IsProcessing());

  RunUntil(InstallingState::kImporting);

  EXPECT_TRUE(installer_->IsProcessing());

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(installer_->IsProcessing());

  histogram_tester_->ExpectUniqueSample(kPluginVmSetupResultHistogram,
                                        PluginVmSetupResult::kSuccess, 1);
}

TEST_F(PluginVmInstallerDownloadServiceTest,
       CanProceedWithANewImageWhenSucceededTest) {
  SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);

  ExpectObserverEventsUntil(InstallingState::kImporting);
  EXPECT_CALL(*observer_, OnImported());
  StartAndRunToCompletion();
  VerifyExpectations();

  EXPECT_FALSE(installer_->IsProcessing());
  ExpectObserverEventsUntil(InstallingState::kImporting);
  EXPECT_CALL(*observer_, OnImported());

  // As it is deleted after successful importing.
  installer_->SetDownloadedImageForTesting(CreateZipFile());
  StartAndRunToCompletion();

  histogram_tester_->ExpectUniqueSample(kPluginVmSetupResultHistogram,
                                        PluginVmSetupResult::kSuccess, 2);
}

TEST_F(PluginVmInstallerDownloadServiceTest,
       CanProceedWithANewImageWhenFailedTest) {
  SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);

  ExpectObserverEventsUntil(InstallingState::kDownloadingImage);
  EXPECT_CALL(*observer_, OnError(FailureReason::DOWNLOAD_FAILED_ABORTED));

  StartAndRunUntil(InstallingState::kDownloadingImage);
  std::string guid = installer_->GetCurrentDownloadGuid();
  download_service_->SetFailedDownload(guid, false);
  task_environment_.RunUntilIdle();
  VerifyExpectations();

  EXPECT_FALSE(installer_->IsProcessing());

  ExpectObserverEventsUntil(InstallingState::kImporting);
  EXPECT_CALL(*observer_, OnImported());

  StartAndRunToCompletion();

  histogram_tester_->ExpectBucketCount(kPluginVmSetupResultHistogram,
                                       PluginVmSetupResult::kError, 1);
  histogram_tester_->ExpectBucketCount(kPluginVmSetupResultHistogram,
                                       PluginVmSetupResult::kSuccess, 1);
}

TEST_F(PluginVmInstallerDownloadServiceTest, CancelledDownloadTest) {
  ExpectObserverEventsUntil(InstallingState::kDownloadingImage);
  EXPECT_CALL(*observer_, OnCancelFinished());

  StartAndRunUntil(InstallingState::kDownloadingImage);
  installer_->Cancel();
  task_environment_.RunUntilIdle();

  histogram_tester_->ExpectTotalCount(kFailureReasonHistogram, 0);
  histogram_tester_->ExpectUniqueSample(
      kPluginVmSetupResultHistogram,
      PluginVmSetupResult::kUserCancelledDownloadingPluginVmImage, 1);
}

TEST_F(PluginVmInstallerDownloadServiceTest, ImportNonExistingImageTest) {
  SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);

  ExpectObserverEventsUntil(InstallingState::kImporting);
  EXPECT_CALL(*observer_, OnError(FailureReason::COULD_NOT_OPEN_IMAGE));

  installer_->SetDownloadedImageForTesting(base::FilePath());
  StartAndRunToCompletion();
}

TEST_F(PluginVmInstallerDownloadServiceTest, ImportFailedOutOfSpaceTest) {
  SetupConciergeForFailedDiskImageImport(
      fake_concierge_client_,
      vm_tools::concierge::DISK_STATUS_NOT_ENOUGH_SPACE);

  ExpectObserverEventsUntil(InstallingState::kImporting);
  EXPECT_CALL(*observer_, OnError(FailureReason::OUT_OF_DISK_SPACE));
  StartAndRunToCompletion();

  histogram_tester_->ExpectBucketCount(kPluginVmSetupResultHistogram,
                                       PluginVmSetupResult::kError, 1);
}

TEST_F(PluginVmInstallerDownloadServiceTest, CancelledImportTest) {
  SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);
  SetupConciergeForCancelDiskImageOperation(fake_concierge_client_,
                                            true /* success */);

  ExpectObserverEventsUntil(InstallingState::kImporting);
  StartAndRunUntil(InstallingState::kImporting);

  EXPECT_CALL(*observer_, OnCancelFinished());
  installer_->Cancel();
  task_environment_.RunUntilIdle();

  histogram_tester_->ExpectUniqueSample(
      kPluginVmSetupResultHistogram,
      PluginVmSetupResult::kUserCancelledImportingPluginVmImage, 1);
}

TEST_F(PluginVmInstallerDownloadServiceTest, EmptyPluginVmImageUrlTest) {
  SetPluginVmImagePref("", kHash);
  ExpectObserverEventsUntil(InstallingState::kDownloadingDlc);
  EXPECT_CALL(*observer_, OnError(FailureReason::INVALID_IMAGE_URL));
  StartAndRunToCompletion();

  histogram_tester_->ExpectUniqueSample(kFailureReasonHistogram,
                                        FailureReason::INVALID_IMAGE_URL, 1);
}

TEST_F(PluginVmInstallerDownloadServiceTest, VerifyDownloadTest) {
  EXPECT_FALSE(installer_->VerifyDownload(kHash2));
  EXPECT_TRUE(installer_->VerifyDownload(kHashUppercase));
  EXPECT_TRUE(installer_->VerifyDownload(kHash));
  EXPECT_FALSE(installer_->VerifyDownload(std::string()));
}

TEST_F(PluginVmInstallerDownloadServiceTest, CannotStartIfPluginVmIsDisabled) {
  profile_->ScopedCrosSettingsTestHelper()->SetBoolean(ash::kPluginVmAllowed,
                                                       false);
  EXPECT_EQ(FailureReason::NOT_ALLOWED, installer_->Start());
  task_environment_.RunUntilIdle();
}

TEST_F(PluginVmInstallerDriveTest, InvalidDriveUrlTest) {
  SetPluginVmImagePref(kDriveUrl2, kHash);

  ExpectObserverEventsUntil(InstallingState::kDownloadingImage);
  EXPECT_CALL(*observer_, OnError(FailureReason::INVALID_IMAGE_URL));
  StartAndRunToCompletion();
}

TEST_F(PluginVmInstallerDriveTest, NoConnectionDriveTest) {
  SetPluginVmImagePref(kDriveUrl, kHash);
  fake_drive_service_->set_offline(true);

  ExpectObserverEventsUntil(InstallingState::kDownloadingImage);
  EXPECT_CALL(*observer_, OnError(FailureReason::DOWNLOAD_FAILED_NETWORK));
  StartAndRunToCompletion();
}

TEST_F(PluginVmInstallerDriveTest, WrongHashDriveTest) {
  SetPluginVmImagePref(kDriveUrl, kHash2);

  ExpectObserverEventsUntil(InstallingState::kDownloadingImage);
  EXPECT_CALL(*observer_, OnDownloadProgressUpdated(_, _)).Times(2);
  EXPECT_CALL(*observer_, OnError(FailureReason::HASH_MISMATCH));

  StartAndRunToCompletion();
}

TEST_F(PluginVmInstallerDriveTest, DriveDownloadFailedAfterStartingTest) {
  SetPluginVmImagePref(kDriveUrl, kHash);
  SimpleFakeDriveService* fake_drive_service = SetUpSimpleFakeDriveService();

  ExpectObserverEventsUntil(InstallingState::kDownloadingImage);
  EXPECT_CALL(*observer_, OnDownloadProgressUpdated(5, 100));
  EXPECT_CALL(*observer_, OnDownloadProgressUpdated(10, 100));
  EXPECT_CALL(*observer_, OnError(FailureReason::DOWNLOAD_FAILED_NETWORK));

  StartAndRunToCompletion();

  fake_drive_service->RunGetContentCallback(
      google_apis::HTTP_SUCCESS, std::make_unique<std::string>("Part1"));
  fake_drive_service->RunProgressCallback(5, 100);
  fake_drive_service->RunGetContentCallback(
      google_apis::HTTP_SUCCESS, std::make_unique<std::string>("Part2"));
  fake_drive_service->RunProgressCallback(10, 100);
  fake_drive_service->RunGetContentCallback(google_apis::NO_CONNECTION,
                                            std::unique_ptr<std::string>());
}

TEST_F(PluginVmInstallerDriveTest, CancelledDriveDownloadTest) {
  SetPluginVmImagePref(kDriveUrl, kHash);
  SimpleFakeDriveService* fake_drive_service = SetUpSimpleFakeDriveService();

  ExpectObserverEventsUntil(InstallingState::kDownloadingImage);
  EXPECT_CALL(*observer_, OnDownloadProgressUpdated(5, 100));
  EXPECT_CALL(*observer_, OnCancelFinished());

  StartAndRunToCompletion();

  fake_drive_service->RunGetContentCallback(
      google_apis::HTTP_SUCCESS, std::make_unique<std::string>("Part1"));
  fake_drive_service->RunProgressCallback(5, 100);
  installer_->Cancel();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(fake_drive_service->cancel_callback_called());
}

TEST_F(PluginVmInstallerDriveTest, SuccessfulDriveDownloadTest) {
  SetPluginVmImagePref(kDriveUrl, kHash);
  fake_dlcservice_client_.set_install_error(dlcservice::kErrorNone);

  ExpectObserverEventsUntil(InstallingState::kImporting);
  EXPECT_CALL(*observer_, OnDownloadProgressUpdated(_, std::strlen(kContent)))
      .Times(AtLeast(1));
  EXPECT_CALL(*observer_, OnError(_));

  StartAndRunToCompletion();
  histogram_tester_->ExpectUniqueSample(kPluginVmDlcUseResultHistogram,
                                        PluginVmDlcUseResult::kDlcSuccess, 1);
}

TEST_F(PluginVmInstallerDriveTest, InstallingPluginVmDlcNeedReboot) {
  SetPluginVmImagePref(kDriveUrl, kHash);
  fake_dlcservice_client_.set_install_error(dlcservice::kErrorNeedReboot);

  ExpectObserverEventsUntil(InstallingState::kDownloadingDlc);
  EXPECT_CALL(*observer_, OnError(FailureReason::DLC_NEED_REBOOT));

  StartAndRunToCompletion();
  histogram_tester_->ExpectUniqueSample(
      kPluginVmDlcUseResultHistogram, PluginVmDlcUseResult::kNeedRebootDlcError,
      1);
}

TEST_F(PluginVmInstallerDriveTest, InstallingPluginVmDlcNeedSpace) {
  SetPluginVmImagePref(kDriveUrl, kHash);
  fake_dlcservice_client_.set_install_error(dlcservice::kErrorAllocation);

  ExpectObserverEventsUntil(InstallingState::kDownloadingDlc);
  EXPECT_CALL(*observer_, OnError(FailureReason::DLC_NEED_SPACE));

  StartAndRunToCompletion();
  histogram_tester_->ExpectUniqueSample(
      kPluginVmDlcUseResultHistogram, PluginVmDlcUseResult::kNeedSpaceDlcError,
      1);
}

TEST_F(PluginVmInstallerDriveTest, InstallingPluginVmDlcWhenUnsupported) {
  SetPluginVmImagePref(kDriveUrl, kHash);
  fake_dlcservice_client_.set_install_error(dlcservice::kErrorInvalidDlc);

  ExpectObserverEventsUntil(InstallingState::kDownloadingDlc);
  EXPECT_CALL(*observer_, OnError(FailureReason::DLC_UNSUPPORTED));

  StartAndRunToCompletion();
  histogram_tester_->ExpectUniqueSample(kPluginVmDlcUseResultHistogram,
                                        PluginVmDlcUseResult::kInvalidDlcError,
                                        1);
}

TEST_F(PluginVmInstallerDriveTest, InstallingPluginVmDlcWhenNoImageFound) {
  SetPluginVmImagePref(kDriveUrl, kHash);
  fake_dlcservice_client_.set_install_error(dlcservice::kErrorNoImageFound);

  ExpectObserverEventsUntil(InstallingState::kDownloadingDlc);
  EXPECT_CALL(*observer_, OnError(FailureReason::DLC_INTERNAL));

  StartAndRunToCompletion();
  histogram_tester_->ExpectUniqueSample(
      kPluginVmDlcUseResultHistogram,
      PluginVmDlcUseResult::kNoImageFoundDlcError, 1);
}

}  // namespace plugin_vm
