// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/startup/startup_launch_manager.h"

#include <memory>
#include <optional>

#include "base/test/task_environment.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/installer/util/auto_launch_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

using auto_launch_util::StartupLaunchMode;

namespace {

class TestStartupLaunchManager : public StartupLaunchManager {
 public:
  explicit TestStartupLaunchManager(BrowserProcess* browser_process)
      : StartupLaunchManager(browser_process) {}
  MOCK_METHOD1(UpdateLaunchOnStartup,
               void(std::optional<StartupLaunchMode> startup_mode));
};
}  // namespace

class StartupLaunchManagerTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_override_ =
        GlobalFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
            base::BindRepeating([](BrowserProcess& browser_process) {
              return std::make_unique<TestStartupLaunchManager>(
                  &browser_process);
            }));

    // Initialize StartupLaunchManager as it is not created yet.
    TestingBrowserProcess::GetGlobal()->CreateGlobalFeaturesForTesting();
  }

  TestStartupLaunchManager* launch_on_startup_manager() {
    return static_cast<TestStartupLaunchManager*>(
        StartupLaunchManager::From(g_browser_process));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  ui::UserDataFactory::ScopedOverride scoped_override_;
};

TEST_F(StartupLaunchManagerTest, RegisterLaunchOnStartup) {
  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();
  EXPECT_CALL(*launch_manager,
              UpdateLaunchOnStartup({StartupLaunchMode::kBackground}))
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
  EXPECT_CALL(*launch_manager,
              UpdateLaunchOnStartup({StartupLaunchMode::kBackground}))
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
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup({std::nullopt}))
      .Times(testing::Exactly(1));
  launch_manager->UnregisterLaunchOnStartup(StartupLaunchReason::kExtensions);
}

TEST_F(StartupLaunchManagerTest, RegisterMultipleReasons) {
  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();
  EXPECT_CALL(*launch_manager,
              UpdateLaunchOnStartup({StartupLaunchMode::kBackground}))
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
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup({std::nullopt}))
      .Times(testing::Exactly(1));
  launch_manager->UnregisterLaunchOnStartup(StartupLaunchReason::kGlic);
  testing::Mock::VerifyAndClearExpectations(launch_manager);
}
