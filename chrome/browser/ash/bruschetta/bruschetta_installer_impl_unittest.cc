// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_installer_impl.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_amount_of_physical_memory_override.h"
#include "base/values.h"
#include "chrome/browser/ash/bruschetta/bruschetta_download.h"
#include "chrome/browser/ash/bruschetta/bruschetta_installer.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
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
constexpr int kMaxSteps = 26;

// Total number of stopping points in ::ExpectStopOnStepN when we don't install
// a pflash file.
constexpr int kMaxStepsNoPflash = kMaxSteps - 7;

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

class StubDownload : public BruschettaDownload {
 public:
  StubDownload(base::FilePath path, std::string hash)
      : path_(std::move(path)), hash_(std::move(hash)) {}
  ~StubDownload() override = default;
  void StartDownload(
      Profile* profile,
      GURL url,
      base::OnceCallback<void(base::FilePath, std::string)> callback) override {
    std::move(callback).Run(path_, hash_);
  }
  base::FilePath path_;
  std::string hash_;
};

class BruschettaInstallerTest : public testing::TestWithParam<int>,
                                protected guest_os::FakeVmServicesHelper {
 public:
  BruschettaInstallerTest() : fake_20gb_memory(20ULL * 1024 * 1024) {}
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
    base::Value::List oem_strings;
    oem_strings.Append("OEM string");

    base::Value::Dict config;

    config.Set(prefs::kPolicyEnabledKey,
               static_cast<int>(prefs::PolicyEnabledState::RUN_ALLOWED));
    config.Set(prefs::kPolicyNameKey, kVmConfigName);
    config.Set(prefs::kPolicyVTPMKey, vtpm.Clone());
    config.Set(prefs::kPolicyOEMStringsKey, oem_strings.Clone());
    prefs_not_installable_.Set(kVmConfigId, config.Clone());

    config.Set(prefs::kPolicyEnabledKey,
               static_cast<int>(prefs::PolicyEnabledState::INSTALL_ALLOWED));
    config.Set(prefs::kPolicyImageKey, image.Clone());
    prefs_installable_no_pflash_.Set(kVmConfigId, config.Clone());

    config.Set(prefs::kPolicyPflashKey, image.Clone());
    prefs_installable_.Set(kVmConfigId, config.Clone());
  }

  void SetUp() override {
    ash::AttestationClient::InitializeFake();
    ash::system::StatisticsProvider::SetTestProvider(
        &fake_statistics_provider_);
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kAttestedDeviceIdKey, "my:cool:ADID");

    BuildPrefValues();

    ASSERT_TRUE(base::CreateDirectory(
        profile_.GetPath().Append("MyFiles").Append("Downloads")));

    ash::disks::DiskMountManager::InitializeForTesting(&*disk_mount_manager_);

    installer_ = std::make_unique<BruschettaInstallerImpl>(
        &profile_, base::BindOnce(&BruschettaInstallerTest::CloseCallback,
                                  base::Unretained(this)));

    installer_->AddObserver(&observer_);
    ConfigureDownloadFactory(base::FilePath(), "");
  }

  // Configures the Bruschetta installer to use a fake downloader which
  // immediately completes returning |path| and |hash|.
  void ConfigureDownloadFactory(base::FilePath path, std::string hash) {
    installer_->SetDownloadFactoryForTesting(base::BindLambdaForTesting(
        [path = std::move(path), hash = std::move(hash)]() {
          std::unique_ptr<BruschettaDownload> d =
              std::make_unique<StubDownload>(std::move(path), std::move(hash));
          return d;
        }));
  }

  void TearDown() override {
    CheckVmRegistration();
    ash::disks::DiskMountManager::Shutdown();
    ash::AttestationClient::Shutdown();
  }

  void CheckVmRegistration() {
    const auto& running_vms = BruschettaServiceFactory::GetForProfile(&profile_)
                                  ->GetRunningVmsForTesting();
    if (expect_vm_registered_) {
      auto it = running_vms.find(kVmName);
      EXPECT_NE(it, running_vms.end());
      EXPECT_TRUE(it->second.vtpm_enabled);
    } else {
      EXPECT_FALSE(running_vms.contains(kVmName));
    }
  }

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
    return [this]() { ConfigureDownloadFactory(base::FilePath(), ""); };
  }

  auto DownloadBadHashCallback() {
    return [this]() {
      base::FilePath path;
      base::CreateTemporaryFile(&path);
      ConfigureDownloadFactory(path, kBadHash);
    };
  }

  auto DownloadSuccessCallback() {
    return [this]() {
      base::FilePath path;
      base::CreateTemporaryFile(&path);
      ConfigureDownloadFactory(path, kVmConfigHash);
    };
  }

  auto DiskImageCallback(
      std::optional<vm_tools::concierge::DiskImageStatus> value) {
    return [this, value]() {
      if (value.has_value()) {
        vm_tools::concierge::CreateDiskImageResponse response;
        response.set_status(*value);
        FakeConciergeClient()->set_create_disk_image_response(
            std::move(response));
      } else {
        FakeConciergeClient()->set_create_disk_image_response(std::nullopt);
      }
    };
  }

  auto InstallPflashCallback(std::optional<bool> success) {
    return [this, success]() {
      if (success.has_value()) {
        vm_tools::concierge::InstallPflashResponse response;
        response.set_success(*success);
        FakeConciergeClient()->set_install_pflash_response(std::move(response));
      } else {
        FakeConciergeClient()->set_install_pflash_response(std::nullopt);
      }
    };
  }

  auto ClearVekCallback(bool success) {
    return [success]() {
      ash::AttestationClient::Get()->GetTestInterface()->set_delete_keys_status(
          success ? attestation::STATUS_SUCCESS
                  : attestation::STATUS_INVALID_PARAMETER);
    };
  }

  auto StartVmCallback(std::optional<bool> success) {
    return [this, success]() {
      if (success.has_value()) {
        vm_tools::concierge::StartVmResponse response;
        response.set_success(*success);
        FakeConciergeClient()->set_start_vm_response(std::move(response));
        this->expect_vm_registered_ = *success;
      } else {
        FakeConciergeClient()->set_start_vm_response(std::nullopt);
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
  // kMaxSteps means letting the install run to completion. If out_result is
  // passed in, will set it to the expected result (as reported to the observer
  // + metrics).
  //
  // Takes an optional Sequence to order these expectations before/after other
  // things.
  bool ExpectStopOnStepN(int n,
                         Sequence seq = {},
                         BruschettaInstallResult* out_result = nullptr,
                         bool use_pflash = true) {
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

      if (use_pflash) {
        expectation.WillOnce(
            InvokeWithoutArgs(PrefsCallback(prefs_installable_)));
      } else {
        expectation.WillOnce(
            InvokeWithoutArgs(PrefsCallback(prefs_installable_no_pflash_)));
      }
    }

    // Tools DLC install step
    {
      if (out_result) {
        *out_result = BruschettaInstallResult::kToolsDlcUnknownError;
      }
      auto& expectation =
          EXPECT_CALL(
              observer_,
              StateChanged(BruschettaInstaller::State::kToolsDlcInstall))
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

    // UEFI DLC install step
    {
      if (out_result) {
        *out_result = BruschettaInstallResult::kFirmwareDlcUnknownError;
      }
      auto& expectation =
          EXPECT_CALL(
              observer_,
              StateChanged(BruschettaInstaller::State::kFirmwareDlcInstall))
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

      if (use_pflash) {
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
        MakeErrorPoint(expectation, seq, DiskImageCallback(std::nullopt));
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

    // Install pflash file step
    {
      if (out_result) {
        *out_result = BruschettaInstallResult::kInstallPflashError;
      }
      auto& expectation =
          EXPECT_CALL(observer_,
                      StateChanged(BruschettaInstaller::State::kInstallPflash))
              .Times(1)
              .InSequence(seq);

      if (use_pflash) {
        if (!n--) {
          expectation.WillOnce(CancelCallback());
          return false;
        }
        if (!n--) {
          MakeErrorPoint(expectation, seq, InstallPflashCallback(std::nullopt));
          return true;
        }
        if (!n--) {
          MakeErrorPoint(expectation, seq, InstallPflashCallback(false));
          return true;
        }

        expectation.WillOnce(InvokeWithoutArgs(InstallPflashCallback(true)));
      }
    }

    // Clear vEK step
    {
      if (out_result) {
        *out_result = BruschettaInstallResult::kClearVekFailed;
      }
      auto& expectation =
          EXPECT_CALL(observer_,
                      StateChanged(BruschettaInstaller::State::kClearVek))
              .Times(1)
              .InSequence(seq);

      if (!n--) {
        expectation.WillOnce(CancelCallback());
        return false;
      }
      if (!n--) {
        MakeErrorPoint(expectation, seq, ClearVekCallback(false));
        return true;
      }

      expectation.WillOnce(InvokeWithoutArgs(ClearVekCallback(true)));
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
        MakeErrorPoint(expectation, seq, StartVmCallback(std::nullopt));
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

  base::Value::Dict prefs_installable_no_pflash_, prefs_installable_,
      prefs_not_installable_;

  TestingProfile profile_;
  std::unique_ptr<BruschettaInstallerImpl> installer_;

  MockObserver observer_;
  // Pointer owned by DiskMountManager
  const raw_ref<ash::disks::MockDiskMountManager, DanglingUntriaged>
      disk_mount_manager_{*new ash::disks::MockDiskMountManager};

  bool destroy_installer_on_completion_ = true;
  base::HistogramTester histogram_tester_;

  bool expect_vm_registered_ = false;
  ash::system::FakeStatisticsProvider fake_statistics_provider_;
  base::test::ScopedAmountOfPhysicalMemoryOverride fake_20gb_memory;

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

TEST_F(BruschettaInstallerTest, InstallSuccessNoPflash) {
  ExpectStopOnStepN(kMaxStepsNoPflash, {}, nullptr, false);

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

TEST_P(BruschettaInstallerTest, StopDuringInstallNoPflash) {
  if (GetParam() > kMaxStepsNoPflash) {
    GTEST_SKIP();
  }

  BruschettaInstallResult expected_result;
  bool is_error = ExpectStopOnStepN(GetParam(), {}, &expected_result, false);

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
  GTEST_FLAG_SET(stack_trace_depth, 0);

  std::optional<int> new_max_steps;

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
    testing::Mock::VerifyAndClearExpectations(&*disk_mount_manager_);
  }
}

}  // namespace

}  // namespace bruschetta
