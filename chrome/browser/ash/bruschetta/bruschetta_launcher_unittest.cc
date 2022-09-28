// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_launcher.h"

#include <memory>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
    launcher_ = std::make_unique<BruschettaLauncher>("vm_name", &profile_);

    // We set up all our mocks to succeed, then failing tests explicitly break
    // the one thing they want to check the failure mode of.
    ASSERT_TRUE(CreateTestBios());
    vm_tools::concierge::StartVmResponse response;
    response.set_success(true);
    response.set_status(vm_tools::concierge::VmStatus::VM_STATUS_RUNNING);
    FakeConciergeClient()->set_start_vm_response(std::move(response));

    guest_os::GuestId id{guest_os::VmType::UNKNOWN, "vm_name", "penguin"};
    guest_os::GuestOsSessionTracker::GetForProfile(&profile_)
        ->AddGuestForTesting(id, guest_os::GuestInfo(id, 30, {}, {}, {}, {}));
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

  bool CreateTestBios() {
    auto downloads_folder = profile_.GetPath().Append("Downloads");
    bios_path_ = downloads_folder.Append("bios");
    base::File::Error error;
    bool result = base::CreateDirectoryAndGetError(downloads_folder, &error);
    if (!result) {
      LOG(ERROR) << "Error creating downloads folder: " << error;
      return false;
    }
    return base::WriteFile(bios_path_, "");
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::RunLoop run_loop_;
  TestingProfile profile_;
  base::FilePath bios_path_;
  std::unique_ptr<BruschettaLauncher> launcher_;
};

// Try to launch, but DLC service returns an error.
TEST_F(BruschettaLauncherTest, LaunchDlcFailure) {
  BruschettaResult result;
  FakeDlcserviceClient()->set_install_error("Error installing");

  launcher_->EnsureRunning(StoreResultThenQuitRunLoop(&result));
  run_loop_.Run();

  ASSERT_EQ(result, BruschettaResult::kDlcInstallError);
}

// Try to launch, but BIOS file doesn't exist.
TEST_F(BruschettaLauncherTest, LaunchBiosNotAccessible) {
  BruschettaResult result;
  ASSERT_TRUE(base::DeleteFile(bios_path_));

  launcher_->EnsureRunning(StoreResultThenQuitRunLoop(&result));
  run_loop_.Run();

  ASSERT_EQ(result, BruschettaResult::kBiosNotAccessible);
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
  signal.set_name("vm_name");
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
}

}  // namespace bruschetta
