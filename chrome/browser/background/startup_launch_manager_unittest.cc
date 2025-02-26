// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/startup_launch_manager.h"

#include <memory>

#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class TestStartupLaunchManager : public StartupLaunchManager {
 public:
  TestStartupLaunchManager() = default;
  MOCK_METHOD1(UpdateLaunchOnStartup, void(bool should_launch_on_startup));
};
}  // namespace

class StartupLaunchManagerTest : public testing::Test {
 public:
  void SetUp() override {
    launch_on_startup_manager_ = std::make_unique<TestStartupLaunchManager>();
  }

  TestStartupLaunchManager* launch_on_startup_manager() {
    return launch_on_startup_manager_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestStartupLaunchManager> launch_on_startup_manager_;
};

TEST_F(StartupLaunchManagerTest, RegisterLaunchOnStartup) {
  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup(true))
      .Times(testing::Exactly(1));
  launch_manager->RegisterLaunchOnStartup(StartupLaunchReason::kExtensions);
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // Registering the same reason shouldn't update the registry keys again
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup(testing::_))
      .Times(testing::Exactly(0));
  launch_manager->RegisterLaunchOnStartup(StartupLaunchReason::kExtensions);
  testing::Mock::VerifyAndClearExpectations(launch_manager);
}

TEST_F(StartupLaunchManagerTest, UnregisterLaunchOnStartup) {
  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup(true))
      .Times(testing::Exactly(1));
  launch_manager->RegisterLaunchOnStartup(StartupLaunchReason::kExtensions);
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // Glic never registered with the manager so unregistering it shouldn't affect
  // whether chrome should launch on startup.
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup(testing::_))
      .Times(testing::Exactly(0));
  launch_manager->UnregisterLaunchOnStartup(StartupLaunchReason::kGlic);
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // The launch manager shouldn't launch on start up anymore after extensions is
  // unregistered.
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup(false))
      .Times(testing::Exactly(1));
  launch_manager->UnregisterLaunchOnStartup(StartupLaunchReason::kExtensions);
}

TEST_F(StartupLaunchManagerTest, RegisterMultipleReasons) {
  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup(true))
      .Times(testing::Exactly(1));
  launch_manager->RegisterLaunchOnStartup(StartupLaunchReason::kExtensions);
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup(testing::_))
      .Times(testing::Exactly(0));
  launch_manager->RegisterLaunchOnStartup(StartupLaunchReason::kGlic);
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // The launch manager should continue to launch on start up because glic is
  // still registered.
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup(testing::_))
      .Times(testing::Exactly(0));
  launch_manager->UnregisterLaunchOnStartup(StartupLaunchReason::kExtensions);
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // Unregister glic so the launch manager shouldn't launch on startup anymore.
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup(false))
      .Times(testing::Exactly(1));
  launch_manager->UnregisterLaunchOnStartup(StartupLaunchReason::kGlic);
  testing::Mock::VerifyAndClearExpectations(launch_manager);
}
