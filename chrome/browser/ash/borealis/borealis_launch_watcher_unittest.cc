// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_launch_watcher.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace borealis {
namespace {

using CallbackFactory =
    StrictCallbackFactory<void(absl::optional<std::string>)>;

class BorealisLaunchWatcherTest : public testing::Test,
                                  protected guest_os::FakeVmServicesHelper {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  // This test doesn't actually need the profile for anything meaningful,
  // beyond hashing, so it is safe for it to be nullptr.
  raw_ptr<Profile, ExperimentalAsh> profile_ = nullptr;
};

TEST_F(BorealisLaunchWatcherTest, VmStartsCallbackRan) {
  CallbackFactory callback_expectation;
  BorealisLaunchWatcher watcher(profile_, "FooVm");
  vm_tools::cicerone::ContainerStartedSignal signal;
  signal.set_owner_id(ash::ProfileHelper::GetUserIdHashFromProfile(profile_));
  signal.set_vm_name("FooVm");
  signal.set_container_name("FooContainer");

  EXPECT_CALL(callback_expectation,
              Call(absl::optional<std::string>("FooContainer")));
  watcher.AwaitLaunch(callback_expectation.BindOnce());
  FakeCiceroneClient()->NotifyContainerStarted(std::move(signal));

  task_environment_.RunUntilIdle();
}

TEST_F(BorealisLaunchWatcherTest, VmTimesOutCallbackRan) {
  CallbackFactory callback_expectation;
  BorealisLaunchWatcher watcher(profile_, "FooVm");
  watcher.SetTimeoutForTesting(base::Milliseconds(0));

  EXPECT_CALL(callback_expectation,
              Call(absl::optional<std::string>(absl::nullopt)));
  watcher.AwaitLaunch(callback_expectation.BindOnce());

  task_environment_.RunUntilIdle();
}

TEST_F(BorealisLaunchWatcherTest, VmAlreadyStartedCallbackRan) {
  CallbackFactory callback_expectation;
  BorealisLaunchWatcher watcher(profile_, "FooVm");
  vm_tools::cicerone::ContainerStartedSignal signal;
  signal.set_owner_id(ash::ProfileHelper::GetUserIdHashFromProfile(profile_));
  signal.set_vm_name("FooVm");
  signal.set_container_name("FooContainer");

  EXPECT_CALL(callback_expectation,
              Call(absl::optional<std::string>("FooContainer")));
  FakeCiceroneClient()->NotifyContainerStarted(std::move(signal));
  watcher.AwaitLaunch(callback_expectation.BindOnce());

  task_environment_.RunUntilIdle();
}

TEST_F(BorealisLaunchWatcherTest, VmStartsMultipleCallbacksRan) {
  CallbackFactory callback_expectation;
  BorealisLaunchWatcher watcher(profile_, "FooVm");
  vm_tools::cicerone::ContainerStartedSignal signal;
  signal.set_owner_id(ash::ProfileHelper::GetUserIdHashFromProfile(profile_));
  signal.set_vm_name("FooVm");
  signal.set_container_name("FooContainer");

  EXPECT_CALL(callback_expectation,
              Call(absl::optional<std::string>("FooContainer")))
      .Times(2);
  watcher.AwaitLaunch(callback_expectation.BindOnce());
  watcher.AwaitLaunch(callback_expectation.BindOnce());
  FakeCiceroneClient()->NotifyContainerStarted(std::move(signal));

  task_environment_.RunUntilIdle();
}

TEST_F(BorealisLaunchWatcherTest, VmTimesOutMultipleCallbacksRan) {
  CallbackFactory callback_expectation;
  BorealisLaunchWatcher watcher(profile_, "FooVm");
  watcher.SetTimeoutForTesting(base::Milliseconds(0));

  EXPECT_CALL(callback_expectation,
              Call(absl::optional<std::string>(absl::nullopt)))
      .Times(2);
  watcher.AwaitLaunch(callback_expectation.BindOnce());
  watcher.AwaitLaunch(callback_expectation.BindOnce());

  task_environment_.RunUntilIdle();
}

TEST_F(BorealisLaunchWatcherTest, OtherVmsStartBorealisTimesOutCallbackRan) {
  CallbackFactory callback_expectation;
  BorealisLaunchWatcher watcher(profile_, "FooVm");
  watcher.SetTimeoutForTesting(base::Milliseconds(0));
  vm_tools::cicerone::ContainerStartedSignal signal1;
  signal1.set_owner_id("not-the-owner");
  signal1.set_vm_name("FooVm");
  vm_tools::cicerone::ContainerStartedSignal signal2;
  signal2.set_owner_id(ash::ProfileHelper::GetUserIdHashFromProfile(profile_));
  signal2.set_vm_name("not-FooVm");

  EXPECT_CALL(callback_expectation,
              Call(absl::optional<std::string>(absl::nullopt)));
  FakeCiceroneClient()->NotifyContainerStarted(std::move(signal1));
  FakeCiceroneClient()->NotifyContainerStarted(std::move(signal2));
  watcher.AwaitLaunch(callback_expectation.BindOnce());

  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace borealis
