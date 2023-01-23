// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_installer_impl.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/ash/bruschetta/bruschetta_download_client.h"
#include "chrome/browser/ash/bruschetta/bruschetta_installer.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chrome/browser/download/background_download_service_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"
#include "components/download/public/background_service/test/test_download_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bruschetta {

// Defined in bruschetta_installer_impl.cc
extern const char kInstallResultMetric[];

namespace {

using testing::_;
using testing::AnyNumber;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Sequence;

// Total number of stopping points in ::ExpectStopOnStepN
constexpr int kMaxSteps = 23;

const char kVmName[] = "vm-name";
const char kVmConfigId[] = "test-config-id";
const char kVmConfigName[] = "test vm config";
const char kVmConfigUrl[] = "https://example.com/";
const char kVmConfigHash[] =
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
const char kBadHash[] =
    "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210";

class MockObserver : public BruschettaInstaller::Observer {
 public:
  MOCK_METHOD(void,
              StateChanged,
              (BruschettaInstaller::State state),
              (override));
  MOCK_METHOD(void, Error, (BruschettaInstallResult), (override));
};

class BruschettaInstallerTest : public testing::TestWithParam<int>,
                                protected guest_os::FakeVmServicesHelper {
 public:
  BruschettaInstallerTest() = default;
  BruschettaInstallerTest(const BruschettaInstallerTest&) = delete;
  BruschettaInstallerTest& operator=(const BruschettaInstallerTest&) = delete;
  ~BruschettaInstallerTest() override = default;

 protected:
  void BuildPrefValues() {
    base::Value::Dict vtpm;
    vtpm.Set(prefs::kPolicyVTPMEnabledKey, true);
    vtpm.Set(prefs::kPolicyVTPMUpdateActionKey,
             static_cast<int>(
                 prefs::PolicyUpdateAction::FORCE_SHUTDOWN_IF_MORE_RESTRICTED));
    base::Value::Dict image;
    image.Set(prefs::kPolicyURLKey, kVmConfigUrl);
    image.Set(prefs::kPolicyHashKey, kVmConfigHash);
    base::Value::Dict config;
    config.Set(prefs::kPolicyEnabledKey,
               static_cast<int>(prefs::PolicyEnabledState::INSTALL_ALLOWED));
    config.Set(prefs::kPolicyNameKey, kVmConfigName);
    config.Set(prefs::kPolicyVTPMKey, vtpm.Clone());
    config.Set(prefs::kPolicyImageKey, image.Clone());
    config.Set(prefs::kPolicyUefiKey, image.Clone());
    config.Set(prefs::kPolicyPflashKey, image.Clone());
    prefs_installable_.Set(kVmConfigId, config.Clone());

    config.Set(prefs::kPolicyEnabledKey,
               static_cast<int>(prefs::PolicyEnabledState::RUN_ALLOWED));
    config.Set(prefs::kPolicyNameKey, kVmConfigName);
    config.Set(prefs::kPolicyVTPMKey, vtpm.Clone());
    prefs_not_installable_.Set(kVmConfigId, std::move(config));
  }

  void SetUp() override {
    BuildPrefValues();

    BackgroundDownloadServiceFactory::GetInstance()->SetTestingFactory(
        profile_.GetProfileKey(), base::BindRepeating([](SimpleFactoryKey*) {
          return base::WrapUnique<KeyedService>(
              new download::test::TestDownloadService());
        }));

    download_service_ = static_cast<download::test::TestDownloadService*>(
        BackgroundDownloadServiceFactory::GetForKey(profile_.GetProfileKey()));
    download_service_->SetIsReady(true);
    download_service_->set_client(&download_client_);

    ASSERT_TRUE(base::CreateDirectory(profile_.GetPath().Append("Downloads")));

    ash::disks::DiskMountManager::InitializeForTesting(&disk_mount_manager_);

    BruschettaServiceFactory::EnableForTesting(&profile_);

    installer_ = std::make_unique<BruschettaInstallerImpl>(
        &profile_, base::BindOnce(&BruschettaInstallerTest::CloseCallback,
                                  base::Unretained(this)));

    installer_->AddObserver(&observer_);
  }

  void TearDown() override { ash::disks::DiskMountManager::Shutdown(); }

  // All these methods return anonymous lambdas because gmock doesn't accept
  // base::OnceCallbacks as callable objects in expectations.
  auto CancelCallback() {
    return [this]() { installer_->Cancel(); };
  }

  auto StopCallback() {
    return [this]() { run_loop_.Quit(); };
  }

  auto PrefsCallback(const base::Value::Dict& value) {
    return [this, &value]() {
      profile_.GetPrefs()->SetDict(prefs::kBruschettaVMConfiguration,
                                   value.Clone());
    };
  }

  auto DlcCallback(std::string error) {
    return
        [this, error]() { FakeDlcserviceClient()->set_install_error(error); };
  }

  auto DownloadErrorCallback(bool fail_at_start) {
    return [this, fail_at_start]() {
      download_service_->SetFailedDownload(
          installer_->GetDownloadGuid().AsLowercaseString(), fail_at_start);
    };
  }

  auto DownloadBadHashCallback() {
    return [this]() {
      download_service_->SetHash256(kBadHash);
      download_service_->SetFailedDownload("", false);
    };
  }

  auto DownloadSuccessCallback() {
    return [this]() {
      base::FilePath path;
      base::CreateTemporaryFile(&path);
      download_service_->SetHash256(kVmConfigHash);
      download_service_->SetFilePath(path);
      download_service_->SetFailedDownload("", false);
    };
  }

  auto DiskImageCallback(
      absl::optional<vm_tools::concierge::DiskImageStatus> value) {
    return [this, value]() {
      if (value.has_value()) {
        vm_tools::concierge::CreateDiskImageResponse response;
        response.set_status(*value);
        FakeConciergeClient()->set_create_disk_image_response(
            std::move(response));
      } else {
        FakeConciergeClient()->set_create_disk_image_response(absl::nullopt);
      }
    };
  }

  auto StartVmCallback(absl::optional<bool> success) {
    return [this, success]() {
      if (success.has_value()) {
        vm_tools::concierge::StartVmResponse response;
        response.set_success(*success);
        FakeConciergeClient()->set_start_vm_response(std::move(response));
      } else {
        FakeConciergeClient()->set_start_vm_response(absl::nullopt);
      }
    };
  }

  void ErrorExpectation(Sequence seq) {
    EXPECT_CALL(observer_, Error)
        .Times(1)
        .InSequence(seq)
        .WillOnce(StopCallback());
  }

  template <typename T, typename F>
  void MakeErrorPoint(T& expectation, Sequence seq, F callback) {
    expectation.WillOnce(InvokeWithoutArgs(callback));
    ErrorExpectation(seq);
  }

  template <typename T, typename F, typename G>
  void MakeErrorPoint(T& expectation, Sequence seq, F callback1, G callback2) {
    expectation.WillOnce(
        DoAll(InvokeWithoutArgs(callback1), InvokeWithoutArgs(callback2)));
    ErrorExpectation(seq);
  }

  // Generate expectations and actions for a test that runs the install and
  // stops at the nth point where stopping is possible, returning true if the
  // stop is due to an error and false if the stop is a cancel. Passing in
  // kMaxSteps means letting the install run to completion. If out_reuslt is
  // passed in, will set it to the expected result (as reported to the observer
  // + metrics).
  //
  // Takes an optional Sequence to order these expectations before/after other
  // things.
  bool ExpectStopOnStepN(int n,
                         Sequence seq = {},
                         BruschettaInstallResult* out_result = nullptr) {
    // Policy check step
    {
      if (out_result) {
        *out_result = BruschettaInstallResult::kInstallationProhibited;
      }
      auto& expectation =
          EXPECT_CALL(observer_,
                      StateChanged(BruschettaInstaller::State::kInstallStarted))
              .Times(1)
              .InSequence(seq);
      if (!n--) {
        MakeErrorPoint(expectation, seq, PrefsCallback(prefs_not_installable_));
        return true;
      }

      expectation.WillOnce(
          InvokeWithoutArgs(PrefsCallback(prefs_installable_)));
    }

    // DLC install step
    {
      if (out_result) {
        *out_result = BruschettaInstallResult::kDlcInstallError;
      }
      auto& expectation =
          EXPECT_CALL(observer_,
                      StateChanged(BruschettaInstaller::State::kDlcInstall))
              .Times(1)
              .InSequence(seq);

      if (!n--) {
        expectation.WillOnce(CancelCallback());
        return false;
      }
      if (!n--) {
        MakeErrorPoint(expectation, seq, DlcCallback("Install Error"));
        return true;
      }

      expectation.WillOnce(
          InvokeWithoutArgs(DlcCallback(dlcservice::kErrorNone)));
    }

    // Firmware image download step
    {
      if (out_result) {
        *out_result = BruschettaInstallResult::kDownloadError;
      }
      auto& expectation =
          EXPECT_CALL(
              observer_,
              StateChanged(BruschettaInstaller::State::kFirmwareDownload))
              .Times(1)
              .InSequence(seq);

      if (!n--) {
        expectation.WillOnce(CancelCallback());
        return false;
      }
      if (!n--) {
        MakeErrorPoint(expectation, seq, DownloadErrorCallback(true));
        return true;
      }
      if (!n--) {
        MakeErrorPoint(expectation, seq, DownloadErrorCallback(false));
        return true;
      }
      if (out_result) {
        *out_result = BruschettaInstallResult::kInvalidFirmware;
      }
      if (!n--) {
        MakeErrorPoint(expectation, seq, DownloadBadHashCallback());
        return true;
      }

      expectation.WillOnce(InvokeWithoutArgs(DownloadSuccessCallback()));
    }

    // Boot disk download step
    {
      if (out_result) {
        *out_result = BruschettaInstallResult::kDownloadError;
      }
      auto& expectation =
          EXPECT_CALL(
              observer_,
              StateChanged(BruschettaInstaller::State::kBootDiskDownload))
              .Times(1)
              .InSequence(seq);

      if (!n--) {
        expectation.WillOnce(CancelCallback());
        return false;
      }
      if (!n--) {
        MakeErrorPoint(expectation, seq, DownloadErrorCallback(true));
        return true;
      }
      if (!n--) {
        MakeErrorPoint(expectation, seq, DownloadErrorCallback(false));
        return true;
      }
      if (out_result) {
        *out_result = BruschettaInstallResult::kInvalidBootDisk;
      }
      if (!n--) {
        MakeErrorPoint(expectation, seq, DownloadBadHashCallback());
        return true;
      }

      expectation.WillOnce(InvokeWithoutArgs(DownloadSuccessCallback()));
    }

    // pflash download step
    {
      if (out_result) {
        *out_result = BruschettaInstallResult::kDownloadError;
      }
      auto& expectation =
          EXPECT_CALL(observer_,
                      StateChanged(BruschettaInstaller::State::kPflashDownload))
              .Times(1)
              .InSequence(seq);

      if (!n--) {
        expectation.WillOnce(CancelCallback());
        return false;
      }
      if (!n--) {
        MakeErrorPoint(expectation, seq, DownloadErrorCallback(true));
        return true;
      }
      if (!n--) {
        MakeErrorPoint(expectation, seq, DownloadErrorCallback(false));
        return true;
      }
      if (out_result) {
        *out_result = BruschettaInstallResult::kInvalidPflash;
      }
      if (!n--) {
        MakeErrorPoint(expectation, seq, DownloadBadHashCallback());
        return true;
      }

      expectation.WillOnce(InvokeWithoutArgs(DownloadSuccessCallback()));
    }

    // Open files step
    {
      if (out_result) {
        *out_result = BruschettaInstallResult::kUnableToOpenImages;
      }
      auto& expectation =
          EXPECT_CALL(observer_,
                      StateChanged(BruschettaInstaller::State::kOpenFiles))
              .Times(1)
              .InSequence(seq);

      if (!n--) {
        expectation.WillOnce(CancelCallback());
        return false;
      }
    }

    // Create VM disk step
    {
      if (out_result) {
        *out_result = BruschettaInstallResult::kCreateDiskError;
      }
      auto& expectation =
          EXPECT_CALL(observer_,
                      StateChanged(BruschettaInstaller::State::kCreateVmDisk))
              .Times(1)
              .InSequence(seq);

      if (!n--) {
        expectation.WillOnce(CancelCallback());
        return false;
      }
      if (!n--) {
        MakeErrorPoint(expectation, seq, DiskImageCallback(absl::nullopt));
        return true;
      }
      if (!n--) {
        MakeErrorPoint(
            expectation, seq,
            DiskImageCallback(
                vm_tools::concierge::DiskImageStatus::DISK_STATUS_FAILED));
        return true;
      }

      expectation.WillOnce(InvokeWithoutArgs(DiskImageCallback(
          vm_tools::concierge::DiskImageStatus::DISK_STATUS_CREATED)));
    }

    // Start VM step
    {
      if (out_result) {
        *out_result = BruschettaInstallResult::kInstallationProhibited;
      }
      auto& expectation =
          EXPECT_CALL(observer_,
                      StateChanged(BruschettaInstaller::State::kStartVm))
              .Times(1)
              .InSequence(seq);

      if (!n--) {
        expectation.WillOnce(CancelCallback());
        return false;
      }
      if (!n--) {
        MakeErrorPoint(expectation, seq, PrefsCallback(prefs_not_installable_));
        return true;
      }
      if (out_result) {
        *out_result = BruschettaInstallResult::kStartVmFailed;
      }
      if (!n--) {
        MakeErrorPoint(expectation, seq, StartVmCallback(absl::nullopt));
        return true;
      }
      if (!n--) {
        MakeErrorPoint(expectation, seq, StartVmCallback(false));
        return true;
      }

      expectation.WillOnce(InvokeWithoutArgs(StartVmCallback(true)));
    }

    // Open terminal step
    EXPECT_CALL(observer_,
                StateChanged(BruschettaInstaller::State::kLaunchTerminal))
        .Times(1)
        .InSequence(seq);
    // Dialog closes after this without further action from us

    // Make sure all input steps other then kMaxSteps got handled earlier.
    if (n != 0) {
      ADD_FAILURE() << "Steps input is too high, try setting kMaxSteps to "
                    << kMaxSteps - n;
    }

    // Like a cancellation, finishing the install closes the installer.
    return false;
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::RunLoop run_loop_, run_loop_2_;

  base::Value::Dict prefs_installable_, prefs_not_installable_;

  TestingProfile profile_;
  std::unique_ptr<BruschettaInstaller> installer_;

  MockObserver observer_;
  // Pointer owned by DiskMountManager
  ash::disks::MockDiskMountManager& disk_mount_manager_{
      *new ash::disks::MockDiskMountManager};

  download::test::TestDownloadService* download_service_;
  BruschettaDownloadClient download_client_{&profile_};
  bool destroy_installer_on_completion_ = true;
  base::HistogramTester histogram_tester_;

 private:
  // Called when the installer exists, suitable for base::BindOnce.
  void CloseCallback() {
    if (destroy_installer_on_completion_) {
      // Delete the installer object after it requests closure so we can tell if
      // it does anything afterwards that assumes it hasn't been deleted yet.
      installer_.reset();
    }
    run_loop_.Quit();
    run_loop_2_.Quit();
  }
};

TEST_F(BruschettaInstallerTest, CloseOnPrompt) {
  installer_->Cancel();
  run_loop_.Run();

  EXPECT_FALSE(installer_);
}

TEST_F(BruschettaInstallerTest, InstallSuccess) {
  ExpectStopOnStepN(kMaxSteps);

  installer_->Install(kVmName, kVmConfigId);
  run_loop_.Run();

  histogram_tester_.ExpectBucketCount(kInstallResultMetric,
                                      BruschettaInstallResult::kSuccess, 1);
  EXPECT_FALSE(installer_);
}

TEST_F(BruschettaInstallerTest, TwoInstalls) {
  ExpectStopOnStepN(kMaxSteps);

  installer_->Install(kVmName, kVmConfigId);
  installer_->Install(kVmName, kVmConfigId);
  run_loop_.Run();

  EXPECT_FALSE(installer_);
}

TEST_F(BruschettaInstallerTest, MultipleCancelsNoOp) {
  destroy_installer_on_completion_ = false;
  // Should be safe to call cancel multiple times.
  installer_->Cancel();
  installer_->Cancel();
  run_loop_.Run();
  installer_->Cancel();
  installer_->Cancel();
  run_loop_2_.Run();
}

TEST_P(BruschettaInstallerTest, StopDuringInstall) {
  BruschettaInstallResult expected_result;
  bool is_error = ExpectStopOnStepN(GetParam(), {}, &expected_result);

  installer_->Install(kVmName, kVmConfigId);
  run_loop_.Run();

  if (is_error) {
    // Installer should remain open in error state, tell it to close.
    EXPECT_TRUE(installer_);
    installer_->Cancel();
    run_loop_2_.Run();

    histogram_tester_.ExpectBucketCount(kInstallResultMetric, expected_result,
                                        1);
  }
  EXPECT_FALSE(installer_);
}

TEST_P(BruschettaInstallerTest, ErrorAndRetry) {
  Sequence seq;

  bool is_error = ExpectStopOnStepN(GetParam(), seq);
  if (is_error) {
    ExpectStopOnStepN(kMaxSteps, seq);
  }

  installer_->Install(kVmName, kVmConfigId);
  run_loop_.Run();

  if (!is_error) {
    return;
  }

  // Installer should remain open in error state, retry the install.
  EXPECT_TRUE(installer_);

  installer_->Install(kVmName, kVmConfigId);
  run_loop_2_.Run();

  EXPECT_FALSE(installer_);
}

INSTANTIATE_TEST_SUITE_P(All,
                         BruschettaInstallerTest,
                         testing::Range(0, kMaxSteps));

TEST_F(BruschettaInstallerTest, AllStepsTested) {
  // Meta-test to check that kMaxSteps is set correctly.

  // We generate a lot of gmock expectations just to ignore them, and taking
  // stack traces for all of them is really slow, so disable stack traces for
  // this test.
  ::testing::FLAGS_gtest_stack_trace_depth = 0;

  absl::optional<int> new_max_steps;

  for (int i = 0; i < 1000; i++) {
    testing::TestPartResultArray failures;
    testing::ScopedFakeTestPartResultReporter test_reporter{
        testing::ScopedFakeTestPartResultReporter::
            INTERCEPT_ONLY_CURRENT_THREAD,
        &failures};

    ExpectStopOnStepN(i);

    if (failures.size() > 0) {
      new_max_steps = i - 1;
      break;
    }
  }
  if (new_max_steps.has_value()) {
    if (*new_max_steps != kMaxSteps) {
      ADD_FAILURE() << "kMaxSteps needs to be updated. Try setting it to "
                    << *new_max_steps;
    }
  } else {
    ADD_FAILURE() << "Was unable to find the correct value for kMaxSteps";
  }

  // Force the expectations we just set to be checked here so we can ignore all
  // the failures.
  {
    testing::TestPartResultArray failures;
    testing::ScopedFakeTestPartResultReporter test_reporter{
        testing::ScopedFakeTestPartResultReporter::
            INTERCEPT_ONLY_CURRENT_THREAD,
        &failures};

    testing::Mock::VerifyAndClearExpectations(&observer_);
    testing::Mock::VerifyAndClearExpectations(&disk_mount_manager_);
  }
}

}  // namespace

}  // namespace bruschetta
