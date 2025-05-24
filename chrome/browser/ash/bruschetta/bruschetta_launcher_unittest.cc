// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"

#include <memory>
#include <optional>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kLaunchHistogram[] = "Bruschetta.LaunchResult";
const char kTestVmName[] = "vm_name";
const char kTestVmConfig[] = "vm_config";
}

namespace bruschetta {

class BruschettaLauncherTest : public testing::Test,
                               protected guest_os::FakeVmServicesHelper {
 public:
  BruschettaLauncherTest() = default;
  BruschettaLauncherTest(const BruschettaLauncherTest&) = delete;
  BruschettaLauncherTest& operator=(const BruschettaLauncherTest&) = delete;
  ~BruschettaLauncherTest() override = default;

 protected:
  void SetUp() override {
    launcher_ = std::make_unique<BruschettaLauncher>(kTestVmName, &profile_);

    // We set up all our mocks to succeed, then failing tests explicitly break
    // the one thing they want to check the failure mode of.
    vm_tools::concierge::StartVmResponse response;
    response.set_success(true);
    response.set_status(vm_tools::concierge::VmStatus::VM_STATUS_RUNNING);
    FakeConciergeClient()->set_start_vm_response(std::move(response));

    guest_os::GuestId id{guest_os::VmType::BRUSCHETTA, kTestVmName, "penguin"};
    guest_os::GuestOsSessionTrackerFactory::GetForProfile(&profile_)
        ->AddGuestForTesting(id, guest_os::GuestInfo(id, 30, {}, {}, {}, {}));

    SetupPrefs();
  }

  void TearDown() override {}

  base::RepeatingCallback<void(BruschettaResult)> StoreResultThenQuitRunLoop(
      BruschettaResult* out_result) {
    return base::BindLambdaForTesting(
        [this, out_result](BruschettaResult result) {
          *out_result = result;
          this->run_loop_.Quit();
        });
  }

  void SetupPrefs() {
    BruschettaServiceFactory::GetForProfile(&profile_)->RegisterInPrefs(
        MakeBruschettaId(kTestVmName), kTestVmConfig);

    base::Value::Dict pref;
    base::Value::Dict config;
    config.Set(prefs::kPolicyEnabledKey,
               static_cast<int>(prefs::PolicyEnabledState::RUN_ALLOWED));
    config.Set(prefs::kPolicyNameKey, "Display Name");

    base::Value::Dict vtpm;
    vtpm.Set(prefs::kPolicyVTPMEnabledKey, true);
    vtpm.Set(prefs::kPolicyVTPMUpdateActionKey,
             static_cast<int>(
                 prefs::PolicyUpdateAction::FORCE_SHUTDOWN_IF_MORE_RESTRICTED));

    config.Set(prefs::kPolicyVTPMKey, std::move(vtpm));

    pref.Set(kTestVmConfig, std::move(config));
    profile_.GetPrefs()->SetDict(prefs::kBruschettaVMConfiguration,
                                 std::move(pref));
  }

  void SetVtpmStatus(bool enabled) {
    ScopedDictPrefUpdate updater(profile_.GetPrefs(),
                                 prefs::kBruschettaVMConfiguration);

    updater.Get()
        .FindDict(kTestVmConfig)
        ->FindDict(prefs::kPolicyVTPMKey)
        ->Set(prefs::kPolicyVTPMEnabledKey, enabled);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::RunLoop run_loop_;
  TestingProfile profile_;
  std::unique_ptr<BruschettaLauncher> launcher_;
  base::HistogramTester histogram_tester_{};
};

// Try to launch, but DLC service returns an error.
TEST_F(BruschettaLauncherTest, LaunchToolsDlcFailure) {
  BruschettaResult result;
  FakeDlcserviceClient()->set_install_errors(base::circular_deque(
      {std::string("Error installing"), std::string(dlcservice::kErrorNone)}));

  launcher_->EnsureRunning(StoreResultThenQuitRunLoop(&result));
  run_loop_.Run();

  ASSERT_EQ(result, BruschettaResult::kDlcInstallError);
  histogram_tester_.ExpectUniqueSample(kLaunchHistogram,
                                       BruschettaResult::kDlcInstallError, 1);

  ASSERT_FALSE(BruschettaServiceFactory::GetForProfile(&profile_)
                   ->GetRunningVmsForTesting()
                   .contains(kTestVmName));
}

// Try to launch, but DLC service returns an error.
TEST_F(BruschettaLauncherTest, LaunchFirmwareDlcFailure) {
  BruschettaResult result;
  FakeDlcserviceClient()->set_install_errors(base::circular_deque(
      {std::string(dlcservice::kErrorNone), std::string("Error installing")}));

  launcher_->EnsureRunning(StoreResultThenQuitRunLoop(&result));
  run_loop_.Run();

  ASSERT_EQ(result, BruschettaResult::kDlcInstallError);
  histogram_tester_.ExpectUniqueSample(kLaunchHistogram,
                                       BruschettaResult::kDlcInstallError, 1);

  ASSERT_FALSE(BruschettaServiceFactory::GetForProfile(&profile_)
                   ->GetRunningVmsForTesting()
                   .contains(kTestVmName));
}

// Try to launch, but StartVm fails.
TEST_F(BruschettaLauncherTest, LaunchStartVmFails) {
  BruschettaResult result;
  vm_tools::concierge::StartVmResponse response;
  response.set_failure_reason("failure reason");
  response.set_success(false);
  response.set_status(vm_tools::concierge::VmStatus::VM_STATUS_FAILURE);
  FakeConciergeClient()->set_start_vm_response(std::move(response));

  launcher_->EnsureRunning(StoreResultThenQuitRunLoop(&result));
  run_loop_.Run();

  ASSERT_EQ(result, BruschettaResult::kStartVmFailed);
  histogram_tester_.ExpectUniqueSample(kLaunchHistogram,
                                       BruschettaResult::kStartVmFailed, 1);

  ASSERT_FALSE(BruschettaServiceFactory::GetForProfile(&profile_)
                   ->GetRunningVmsForTesting()
                   .contains(kTestVmName));
}

// Try to launch, VM already running.
TEST_F(BruschettaLauncherTest, LaunchStartVmSuccess) {
  BruschettaResult result;
  vm_tools::concierge::StartVmResponse response;
  response.set_success(true);
  response.set_status(vm_tools::concierge::VmStatus::VM_STATUS_RUNNING);
  FakeConciergeClient()->set_start_vm_response(std::move(response));

  launcher_->EnsureRunning(StoreResultThenQuitRunLoop(&result));
  run_loop_.Run();

  ASSERT_EQ(result, BruschettaResult::kSuccess);

  // Alpha VMs should have vtpm enabled.
  const auto& running_vms = BruschettaServiceFactory::GetForProfile(&profile_)
                                ->GetRunningVmsForTesting();
  auto it = running_vms.find(kTestVmName);
  ASSERT_NE(it, running_vms.end());
  ASSERT_TRUE(it->second.vtpm_enabled);

  // Run for another few minutes to check that we only get the single success
  // metric and not e.g. a spurious timeout metric as we saw in b/299415527.
  this->task_environment_.FastForwardBy(base::Minutes(5));
  histogram_tester_.ExpectUniqueSample(kLaunchHistogram,
                                       BruschettaResult::kSuccess, 1);
}

// Try to launch, but vm_concierge is not available.
TEST_F(BruschettaLauncherTest, WaitConciergeFails) {
  BruschettaResult result;
  FakeConciergeClient()->set_wait_for_service_to_be_available_response(false);

  launcher_->EnsureRunning(StoreResultThenQuitRunLoop(&result));
  run_loop_.Run();

  ASSERT_EQ(result, BruschettaResult::kConciergeUnavailable);
  histogram_tester_.ExpectUniqueSample(
      kLaunchHistogram, BruschettaResult::kConciergeUnavailable, 1);

  ASSERT_FALSE(BruschettaServiceFactory::GetForProfile(&profile_)
                   ->GetRunningVmsForTesting()
                   .contains(kTestVmName));
}

// Multiple concurrent launch requests are batched into one request.
TEST_F(BruschettaLauncherTest, MultipleLaunchRequestsAreBatched) {
  std::vector<BruschettaResult> results;
  std::vector<BruschettaResult> expected;
  size_t num_concurrent = 3;
  auto callback = base::BindLambdaForTesting(
      [this, &results, num_concurrent](BruschettaResult result) {
        results.push_back(result);
        if (results.size() == num_concurrent) {
          this->run_loop_.Quit();
        }
      });

  for (size_t n = 0; n < num_concurrent; n++) {
    launcher_->EnsureRunning(callback);
    expected.emplace_back(BruschettaResult::kSuccess);
  }
  run_loop_.Run();

  ASSERT_EQ(FakeConciergeClient()->start_vm_call_count(), 1);
  ASSERT_EQ(results, expected);
}

// Multiple non-overlapping launch requests are not batched into one request.
TEST_F(BruschettaLauncherTest, SeparateLaunchRequestsAreNotBatched) {
  int num_repeats = 2;
  BruschettaResult last_result;
  for (int n = 0; n < num_repeats; n++) {
    launcher_->EnsureRunning(StoreResultThenQuitRunLoop(&last_result));
    // Run until we're idle, rather than all the way until the run loop is
    // killed, so we can still run things next time through the loop.
    this->task_environment_.RunUntilIdle();
    ASSERT_EQ(last_result, BruschettaResult::kSuccess);
  }

  ASSERT_EQ(FakeConciergeClient()->start_vm_call_count(), num_repeats);
}

// We should timeout if launch takes too long.
TEST_F(BruschettaLauncherTest, LaunchTimeout) {
  vm_tools::concierge::VmStoppedSignal signal;
  signal.set_name(kTestVmName);
  FakeConciergeClient()->NotifyVmStopped(
      signal);  // Notify stopped to clear the session tracker.

  BruschettaResult last_result = BruschettaResult::kUnknown;
  launcher_->EnsureRunning(StoreResultThenQuitRunLoop(&last_result));
  // Run until we're idle, rather than all the way until the run loop is
  // killed, so we can still run things next time through the loop.
  this->task_environment_.FastForwardBy(base::Minutes(3));
  ASSERT_EQ(last_result, BruschettaResult::kUnknown);  // No result yet.
  this->task_environment_.FastForwardBy(base::Minutes(2));
  ASSERT_EQ(last_result, BruschettaResult::kTimeout);  // Timed out.
  histogram_tester_.ExpectUniqueSample(kLaunchHistogram,
                                       BruschettaResult::kTimeout, 1);

  // The timeout here happens *after* starting the VM, so we still expect it to
  // be registered as running.
  ASSERT_TRUE(BruschettaServiceFactory::GetForProfile(&profile_)
                  ->GetRunningVmsForTesting()
                  .contains(kTestVmName));
}

TEST_F(BruschettaLauncherTest, LaunchBlockedByPolicy) {
  BruschettaResult result;

  // Clear the enterprise policy, which implicitly blocks VMs from running.
  profile_.GetPrefs()->ClearPref(prefs::kBruschettaVMConfiguration);

  launcher_->EnsureRunning(StoreResultThenQuitRunLoop(&result));
  run_loop_.Run();

  ASSERT_EQ(result, BruschettaResult::kForbiddenByPolicy);
  histogram_tester_.ExpectUniqueSample(kLaunchHistogram,
                                       BruschettaResult::kForbiddenByPolicy, 1);

  ASSERT_FALSE(BruschettaServiceFactory::GetForProfile(&profile_)
                   ->GetRunningVmsForTesting()
                   .contains(kTestVmName));
}

TEST_F(BruschettaLauncherTest, VtpmEnabledByPolicy) {
  SetVtpmStatus(true);

  BruschettaResult result;
  vm_tools::concierge::StartVmResponse response;
  response.set_success(true);
  response.set_status(vm_tools::concierge::VmStatus::VM_STATUS_RUNNING);
  FakeConciergeClient()->set_start_vm_response(std::move(response));

  launcher_->EnsureRunning(StoreResultThenQuitRunLoop(&result));
  run_loop_.Run();

  ASSERT_EQ(result, BruschettaResult::kSuccess);
  histogram_tester_.ExpectUniqueSample(kLaunchHistogram,
                                       BruschettaResult::kSuccess, 1);

  const auto& running_vms = BruschettaServiceFactory::GetForProfile(&profile_)
                                ->GetRunningVmsForTesting();
  auto it = running_vms.find(kTestVmName);
  ASSERT_NE(it, running_vms.end());
  ASSERT_TRUE(it->second.vtpm_enabled);
}

TEST_F(BruschettaLauncherTest, VtpmDisabledByPolicy) {
  SetVtpmStatus(false);

  BruschettaResult result;
  vm_tools::concierge::StartVmResponse response;
  response.set_success(true);
  response.set_status(vm_tools::concierge::VmStatus::VM_STATUS_RUNNING);
  FakeConciergeClient()->set_start_vm_response(std::move(response));

  launcher_->EnsureRunning(StoreResultThenQuitRunLoop(&result));
  run_loop_.Run();

  ASSERT_EQ(result, BruschettaResult::kSuccess);
  histogram_tester_.ExpectUniqueSample(kLaunchHistogram,
                                       BruschettaResult::kSuccess, 1);

  const auto& running_vms = BruschettaServiceFactory::GetForProfile(&profile_)
                                ->GetRunningVmsForTesting();
  auto it = running_vms.find(kTestVmName);
  ASSERT_NE(it, running_vms.end());
  ASSERT_FALSE(it->second.vtpm_enabled);
}

}  // namespace bruschetta
