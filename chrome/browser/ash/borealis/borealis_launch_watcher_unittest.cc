// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_launch_watcher.h"

#include <memory>

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace borealis {
namespace {

class CallbackForTestingExpectation {
 public:
  base::OnceCallback<void(base::Optional<std::string>)> GetCallback() {
    return base::BindOnce(&CallbackForTestingExpectation::Callback,
                          base::Unretained(this));
  }
  MOCK_METHOD(void, Callback, (base::Optional<std::string>), ());
};

class BorealisLaunchWatcherTest : public testing::Test {
 public:
  BorealisLaunchWatcherTest() = default;
  BorealisLaunchWatcherTest(const BorealisLaunchWatcherTest&) = delete;
  BorealisLaunchWatcherTest& operator=(const BorealisLaunchWatcherTest&) =
      delete;
  ~BorealisLaunchWatcherTest() override = default;

 protected:
  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    fake_cicerone_client_ = static_cast<chromeos::FakeCiceroneClient*>(
        chromeos::DBusThreadManager::Get()->GetCiceroneClient());
  }

  void TearDown() override {
    chromeos::DBusThreadManager::Shutdown();
    profile_.reset();
  }

  // Owned by DBusThreadManager
  chromeos::FakeCiceroneClient* fake_cicerone_client_ = nullptr;

  std::unique_ptr<TestingProfile> profile_;
  content::BrowserTaskEnvironment task_environment_;

 private:
  void CreateProfile() {
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("defaultprofile");
    profile_ = profile_builder.Build();
  }
};

TEST_F(BorealisLaunchWatcherTest, VmStartsCallbackRan) {
  testing::StrictMock<CallbackForTestingExpectation> callback_expectation;
  BorealisLaunchWatcher watcher(profile_.get(), "FooVm");
  vm_tools::cicerone::ContainerStartedSignal signal;
  signal.set_owner_id(
      chromeos::ProfileHelper::GetUserIdHashFromProfile(profile_.get()));
  signal.set_vm_name("FooVm");
  signal.set_container_name("FooContainer");

  EXPECT_CALL(callback_expectation,
              Callback(base::Optional<std::string>("FooContainer")));
  watcher.AwaitLaunch(callback_expectation.GetCallback());
  fake_cicerone_client_->NotifyContainerStarted(std::move(signal));

  task_environment_.RunUntilIdle();
}

TEST_F(BorealisLaunchWatcherTest, VmTimesOutCallbackRan) {
  testing::StrictMock<CallbackForTestingExpectation> callback_expectation;
  BorealisLaunchWatcher watcher(profile_.get(), "FooVm");
  watcher.SetTimeoutForTesting(base::TimeDelta::FromMilliseconds(0));

  EXPECT_CALL(callback_expectation,
              Callback(base::Optional<std::string>(base::nullopt)));
  watcher.AwaitLaunch(callback_expectation.GetCallback());

  task_environment_.RunUntilIdle();
}

TEST_F(BorealisLaunchWatcherTest, VmAlreadyStartedCallbackRan) {
  testing::StrictMock<CallbackForTestingExpectation> callback_expectation;
  BorealisLaunchWatcher watcher(profile_.get(), "FooVm");
  vm_tools::cicerone::ContainerStartedSignal signal;
  signal.set_owner_id(
      chromeos::ProfileHelper::GetUserIdHashFromProfile(profile_.get()));
  signal.set_vm_name("FooVm");
  signal.set_container_name("FooContainer");

  EXPECT_CALL(callback_expectation,
              Callback(base::Optional<std::string>("FooContainer")));
  fake_cicerone_client_->NotifyContainerStarted(std::move(signal));
  watcher.AwaitLaunch(callback_expectation.GetCallback());

  task_environment_.RunUntilIdle();
}

TEST_F(BorealisLaunchWatcherTest, VmStartsMultipleCallbacksRan) {
  testing::StrictMock<CallbackForTestingExpectation> callback_expectation;
  BorealisLaunchWatcher watcher(profile_.get(), "FooVm");
  vm_tools::cicerone::ContainerStartedSignal signal;
  signal.set_owner_id(
      chromeos::ProfileHelper::GetUserIdHashFromProfile(profile_.get()));
  signal.set_vm_name("FooVm");
  signal.set_container_name("FooContainer");

  EXPECT_CALL(callback_expectation,
              Callback(base::Optional<std::string>("FooContainer")))
      .Times(2);
  watcher.AwaitLaunch(callback_expectation.GetCallback());
  watcher.AwaitLaunch(callback_expectation.GetCallback());
  fake_cicerone_client_->NotifyContainerStarted(std::move(signal));

  task_environment_.RunUntilIdle();
}

TEST_F(BorealisLaunchWatcherTest, VmTimesOutMultipleCallbacksRan) {
  testing::StrictMock<CallbackForTestingExpectation> callback_expectation;
  BorealisLaunchWatcher watcher(profile_.get(), "FooVm");
  watcher.SetTimeoutForTesting(base::TimeDelta::FromMilliseconds(0));

  EXPECT_CALL(callback_expectation,
              Callback(base::Optional<std::string>(base::nullopt)))
      .Times(2);
  watcher.AwaitLaunch(callback_expectation.GetCallback());
  watcher.AwaitLaunch(callback_expectation.GetCallback());

  task_environment_.RunUntilIdle();
}

TEST_F(BorealisLaunchWatcherTest, OtherVmsStartBorealisTimesOutCallbackRan) {
  testing::StrictMock<CallbackForTestingExpectation> callback_expectation;
  BorealisLaunchWatcher watcher(profile_.get(), "FooVm");
  watcher.SetTimeoutForTesting(base::TimeDelta::FromMilliseconds(0));
  vm_tools::cicerone::ContainerStartedSignal signal1;
  signal1.set_owner_id("not-the-owner");
  signal1.set_vm_name("FooVm");
  vm_tools::cicerone::ContainerStartedSignal signal2;
  signal2.set_owner_id(
      chromeos::ProfileHelper::GetUserIdHashFromProfile(profile_.get()));
  signal2.set_vm_name("not-FooVm");

  EXPECT_CALL(callback_expectation,
              Callback(base::Optional<std::string>(base::nullopt)));
  fake_cicerone_client_->NotifyContainerStarted(std::move(signal1));
  fake_cicerone_client_->NotifyContainerStarted(std::move(signal2));
  watcher.AwaitLaunch(callback_expectation.GetCallback());

  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace borealis
