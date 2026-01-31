// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/startup/startup_launch_manager.h"

#include <memory>
#include <optional>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/startup/startup_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/auto_launch_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

using auto_launch_util::StartupLaunchMode;
using base::test::TaskEnvironment;

namespace {

class TestStartupLaunchManager : public StartupLaunchManager {
 public:
  explicit TestStartupLaunchManager(BrowserProcess* browser_process)
      : StartupLaunchManager(browser_process) {}
  MOCK_METHOD1(UpdateLaunchOnStartup,
               void(std::optional<StartupLaunchMode> startup_mode));
};

}  // namespace

class StartupLaunchManagerTestBase : public testing::Test {
 public:
  explicit StartupLaunchManagerTestBase(
      bool enable_foreground_launch_feature,
      std::optional<features::LaunchOnStartupDefaultPreference>
          default_preference) {
    if (enable_foreground_launch_feature) {
      CHECK(default_preference.has_value());
      std::string trial_group_param_string;

      switch (*default_preference) {
        case features::LaunchOnStartupDefaultPreference::kDisabled:
          trial_group_param_string = "disabled";
          break;
        case features::LaunchOnStartupDefaultPreference::kEnabled:
          trial_group_param_string = "enabled";
          break;
      }

      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kLaunchOnStartup,
          {
              {
                  features::kLaunchOnStartupModeParam.name,
                  "foreground",
              },
              {
                  features::kLaunchOnStartupDefaultPreferenceParam.name,
                  trial_group_param_string,
              },
          });
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kLaunchOnStartup);
    }

    CHECK_EQ(enable_foreground_launch_feature,
             features::IsForegroundLaunchEnabled());

    // Override factory for creating StartupLaunchManager to return the mocked
    // manager.
    scoped_override_ =
        GlobalFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
            base::BindRepeating([](BrowserProcess& browser_process) {
              return std::make_unique<TestStartupLaunchManager>(
                  &browser_process);
            }));

#if BUILDFLAG(IS_WIN)
    // Setup the test with this pref reset to default.
    g_browser_process->local_state()->ClearPref(
        prefs::kForegroundLaunchOnLogin);
#endif  // BUILDFLAG(IS_WIN)

    // Construct StartupLaunchManager with mocked override.
    TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
        /*profile_manager=*/false);

    // Release startup launch manager's own lock.
    launch_on_startup_manager()->CommitLaunchOnStartupState();
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

  TestStartupLaunchManager* launch_on_startup_manager() {
    return static_cast<TestStartupLaunchManager*>(
        StartupLaunchManager::From(g_browser_process));
  }

 protected:
  TaskEnvironment task_environment_{TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  ui::UserDataFactory::ScopedOverride scoped_override_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class StartupLaunchManagerTest : public StartupLaunchManagerTestBase {
 public:
  StartupLaunchManagerTest()
      : StartupLaunchManagerTestBase(
            /*enable_foreground_launch_feature=*/false,
            /*default_preference=*/std::nullopt) {}
};

TEST_F(StartupLaunchManagerTest, RegisterBackgroundLaunchOnStartup) {
  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();
  StartupLaunchManager::Client extensions_startup_launch_client =
      StartupLaunchManager::Client(StartupLaunchReason::kExtensions);

  EXPECT_CALL(*launch_manager,
              UpdateLaunchOnStartup({StartupLaunchMode::kBackground}))
      .Times(testing::Exactly(1));
  extensions_startup_launch_client.SetLaunchOnStartup(true);
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // Registering the same reason shouldn't update the registry keys again
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup(testing::_))
      .Times(testing::Exactly(0));
  extensions_startup_launch_client.SetLaunchOnStartup(true);
  testing::Mock::VerifyAndClearExpectations(launch_manager);
}

TEST_F(StartupLaunchManagerTest, UnregisterBackgroundLaunchOnStartup) {
  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();

  StartupLaunchManager::Client extensions_startup_launch_client =
      StartupLaunchManager::Client(StartupLaunchReason::kExtensions);
  StartupLaunchManager::Client glic_startup_launch_client =
      StartupLaunchManager::Client(StartupLaunchReason::kGlic);

  EXPECT_CALL(*launch_manager,
              UpdateLaunchOnStartup({StartupLaunchMode::kBackground}))
      .Times(testing::Exactly(1));
  extensions_startup_launch_client.SetLaunchOnStartup(true);
  glic_startup_launch_client.SetLaunchOnStartup(false);
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // Glic never registered with the manager so unregistering it shouldn't affect
  // whether chrome should launch on startup.
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup(testing::_))
      .Times(testing::Exactly(0));
  glic_startup_launch_client.SetLaunchOnStartup(false);
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // The launch manager shouldn't launch on start up anymore after extensions is
  // unregistered.
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup({std::nullopt}))
      .Times(testing::Exactly(1));
  extensions_startup_launch_client.SetLaunchOnStartup(false);
}

TEST_F(StartupLaunchManagerTest, RegisterMultipleBackgroundLaunchReasons) {
  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();

  StartupLaunchManager::Client extensions_startup_launch_client =
      StartupLaunchManager::Client(StartupLaunchReason::kExtensions);
  StartupLaunchManager::Client glic_startup_launch_client =
      StartupLaunchManager::Client(StartupLaunchReason::kGlic);

  EXPECT_CALL(*launch_manager,
              UpdateLaunchOnStartup({StartupLaunchMode::kBackground}))
      .Times(testing::Exactly(1));
  extensions_startup_launch_client.SetLaunchOnStartup(true);
  glic_startup_launch_client.SetLaunchOnStartup(true);
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // The launch manager should continue to launch on start up because glic is
  // still registered.
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup(testing::_))
      .Times(testing::Exactly(0));
  extensions_startup_launch_client.SetLaunchOnStartup(false);
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // Unregister glic so the launch manager shouldn't launch on startup anymore.
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup({std::nullopt}))
      .Times(testing::Exactly(1));
  glic_startup_launch_client.SetLaunchOnStartup(false);
  testing::Mock::VerifyAndClearExpectations(launch_manager);
}

TEST_F(StartupLaunchManagerTest, WaitForAllClientsToInit) {
  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();

  StartupLaunchManager::Client extensions_startup_launch_client =
      StartupLaunchManager::Client(StartupLaunchReason::kExtensions);
  StartupLaunchManager::Client glic_startup_launch_client =
      StartupLaunchManager::Client(StartupLaunchReason::kGlic);

  // The first client being initialized should not update the registry.
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup(testing::_))
      .Times(testing::Exactly(0));
  extensions_startup_launch_client.SetLaunchOnStartup(true);
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // Registry should be updated now as all clients have registered once.
  EXPECT_CALL(*launch_manager,
              UpdateLaunchOnStartup({StartupLaunchMode::kBackground}))
      .Times(testing::Exactly(1));
  glic_startup_launch_client.SetLaunchOnStartup(true);
  testing::Mock::VerifyAndClearExpectations(launch_manager);
}

TEST_F(StartupLaunchManagerTest, ForceReleaseLocks) {
  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();

  StartupLaunchManager::Client extensions_startup_launch_client =
      StartupLaunchManager::Client(StartupLaunchReason::kExtensions);
  StartupLaunchManager::Client glic_startup_launch_client =
      StartupLaunchManager::Client(StartupLaunchReason::kGlic);

  // The first client being initialized should not update the registry.
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup(testing::_))
      .Times(testing::Exactly(0));
  extensions_startup_launch_client.SetLaunchOnStartup(true);
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // Forcing lock release should update the registry.
  EXPECT_CALL(*launch_manager,
              UpdateLaunchOnStartup({StartupLaunchMode::kBackground}))
      .Times(testing::Exactly(1));
  task_environment_.FastForwardBy(base::Minutes(1));
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // Client updates should still keep functioning.
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup({std::nullopt}))
      .Times(testing::Exactly(1));
  extensions_startup_launch_client.SetLaunchOnStartup(false);
  testing::Mock::VerifyAndClearExpectations(launch_manager);
}

TEST_F(StartupLaunchManagerTest, WaitForStartupLaunchManagerToInit) {
  // Destroy the current startup launch manager instance and recreate it.
  TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
      /*profile_manager=*/false);

  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();

  StartupLaunchManager::Client extensions_startup_launch_client =
      StartupLaunchManager::Client(StartupLaunchReason::kExtensions);

  // Startup launch manager should not update registry since it is not
  // initialized yet.
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup(testing::_))
      .Times(testing::Exactly(0));
  extensions_startup_launch_client.SetLaunchOnStartup(true);
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // Startup launch manager should update registry once initialized.
  EXPECT_CALL(*launch_manager,
              UpdateLaunchOnStartup({StartupLaunchMode::kBackground}))
      .Times(testing::Exactly(1));
  launch_manager->CommitLaunchOnStartupState();
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // Client updates should now write to registry without waiting.
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup({std::nullopt}))
      .Times(testing::Exactly(1));
  extensions_startup_launch_client.SetLaunchOnStartup(false);
  testing::Mock::VerifyAndClearExpectations(launch_manager);
}
#if BUILDFLAG(IS_WIN)
class StartupLaunchManagerForegroundLaunchOptInTest
    : public StartupLaunchManagerTestBase {
 public:
  StartupLaunchManagerForegroundLaunchOptInTest()
      : StartupLaunchManagerTestBase(
            /*enable_foreground_launch_feature=*/true,
            /*default_preference=*/features::LaunchOnStartupDefaultPreference::
                kDisabled) {}
};

TEST_F(StartupLaunchManagerForegroundLaunchOptInTest,
       StartupWithNoUserSetPref) {
  // If starting up without a user-set pref value, we should not enable
  // foreground launch by default.
  TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
      /*profile_manager=*/false);

  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup({std::nullopt}))
      .Times(testing::Exactly(1));

  launch_manager->CommitLaunchOnStartupState();

  testing::Mock::VerifyAndClearExpectations(launch_manager);
}

TEST_F(StartupLaunchManagerForegroundLaunchOptInTest,
       EnableForegroundLaunchPreStartup) {
  // Recreate StartupLaunchManager with pref enabled to simulate Chrome starting
  // with pref enabled.
  TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();

  g_browser_process->local_state()->SetBoolean(prefs::kForegroundLaunchOnLogin,
                                               true);

  TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
      /*profile_manager=*/false);

  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();
  EXPECT_CALL(*launch_manager,
              UpdateLaunchOnStartup({StartupLaunchMode::kForeground}))
      .Times(testing::Exactly(1));

  launch_manager->CommitLaunchOnStartupState();

  testing::Mock::VerifyAndClearExpectations(launch_manager);
}

TEST_F(StartupLaunchManagerForegroundLaunchOptInTest,
       EnableForegroundLaunchPostStartup) {
  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();
  EXPECT_CALL(*launch_manager,
              UpdateLaunchOnStartup({StartupLaunchMode::kForeground}))
      .Times(testing::Exactly(1));

  g_browser_process->local_state()->SetBoolean(prefs::kForegroundLaunchOnLogin,
                                               true);

  testing::Mock::VerifyAndClearExpectations(launch_manager);
}

TEST_F(StartupLaunchManagerForegroundLaunchOptInTest,
       UnregisterForegroundReason) {
  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();

  EXPECT_CALL(*launch_manager,
              UpdateLaunchOnStartup({StartupLaunchMode::kForeground}))
      .Times(testing::Exactly(1));
  g_browser_process->local_state()->SetBoolean(prefs::kForegroundLaunchOnLogin,
                                               true);
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // Disabling foreground launch when nothing else is registered should disabled
  // launch on startup entirely.
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup({std::nullopt}))
      .Times(testing::Exactly(1));
  g_browser_process->local_state()->SetBoolean(prefs::kForegroundLaunchOnLogin,
                                               false);
  testing::Mock::VerifyAndClearExpectations(launch_manager);
}

TEST_F(StartupLaunchManagerForegroundLaunchOptInTest,
       AddingForegroundReasonAfterBackgroundReason) {
  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();

  EXPECT_CALL(*launch_manager,
              UpdateLaunchOnStartup({StartupLaunchMode::kBackground}))
      .Times(testing::Exactly(1));
  StartupLaunchManager::Client extensions_startup_launch_client =
      StartupLaunchManager::Client(StartupLaunchReason::kExtensions);
  extensions_startup_launch_client.SetLaunchOnStartup(true);
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // Adding foreground reason after background reason should update launch on
  // startup to be foreground mode.
  EXPECT_CALL(*launch_manager,
              UpdateLaunchOnStartup({StartupLaunchMode::kForeground}))
      .Times(testing::Exactly(1));
  g_browser_process->local_state()->SetBoolean(prefs::kForegroundLaunchOnLogin,
                                               true);
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // Removing foreground reason should now set it back to background mode.
  EXPECT_CALL(*launch_manager,
              UpdateLaunchOnStartup({StartupLaunchMode::kBackground}))
      .Times(testing::Exactly(1));
  g_browser_process->local_state()->SetBoolean(prefs::kForegroundLaunchOnLogin,
                                               false);
  testing::Mock::VerifyAndClearExpectations(launch_manager);
}

TEST_F(StartupLaunchManagerForegroundLaunchOptInTest,
       AddingBackgroundReasonAfterForegroundReason) {
  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();
  StartupLaunchManager::Client extensions_startup_launch_client =
      StartupLaunchManager::Client(StartupLaunchReason::kExtensions);
  // This will release the acquired lock.
  extensions_startup_launch_client.SetLaunchOnStartup(false);

  EXPECT_CALL(*launch_manager,
              UpdateLaunchOnStartup({StartupLaunchMode::kForeground}))
      .Times(testing::Exactly(1));
  g_browser_process->local_state()->SetBoolean(prefs::kForegroundLaunchOnLogin,
                                               true);
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // Adding background reason now should be a no-op.
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup(testing::_))
      .Times(testing::Exactly(0));
  extensions_startup_launch_client.SetLaunchOnStartup(true);
  testing::Mock::VerifyAndClearExpectations(launch_manager);

  // Removing background reason should still be a no-op.
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup(testing::_))
      .Times(testing::Exactly(0));
  extensions_startup_launch_client.SetLaunchOnStartup(false);
  testing::Mock::VerifyAndClearExpectations(launch_manager);
}

class StartupLaunchManagerForegroundLaunchOptOutTest
    : public StartupLaunchManagerTestBase {
 public:
  StartupLaunchManagerForegroundLaunchOptOutTest()
      : StartupLaunchManagerTestBase(
            /*enable_foreground_launch_feature=*/true,
            /*default_preference=*/features::LaunchOnStartupDefaultPreference::
                kEnabled) {}
};

TEST_F(StartupLaunchManagerForegroundLaunchOptOutTest,
       StartupWithNoUserSetPref) {
  // If starting up without a user-set pref value, we should enable foreground
  // launch by default.
  TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
      /*profile_manager=*/false);

  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();
  EXPECT_CALL(*launch_manager,
              UpdateLaunchOnStartup({StartupLaunchMode::kForeground}))
      .Times(testing::Exactly(1));

  launch_manager->CommitLaunchOnStartupState();

  testing::Mock::VerifyAndClearExpectations(launch_manager);
}

TEST_F(StartupLaunchManagerForegroundLaunchOptOutTest,
       DisableForegroundLaunchPreStartup) {
  // Recreate StartupLaunchManager with pref disabled to simulate Chrome
  // starting with pref disabled.
  TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();

  g_browser_process->local_state()->SetBoolean(prefs::kForegroundLaunchOnLogin,
                                               false);

  TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
      /*profile_manager=*/false);

  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup({std::nullopt}))
      .Times(testing::Exactly(1));

  launch_manager->CommitLaunchOnStartupState();

  testing::Mock::VerifyAndClearExpectations(launch_manager);
}

TEST_F(StartupLaunchManagerForegroundLaunchOptOutTest,
       DisableForegroundLaunchPostStartup) {
  TestStartupLaunchManager* const launch_manager = launch_on_startup_manager();
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup({std::nullopt}))
      .Times(testing::Exactly(1));

  g_browser_process->local_state()->SetBoolean(prefs::kForegroundLaunchOnLogin,
                                               false);

  testing::Mock::VerifyAndClearExpectations(launch_manager);
}

#endif  // BUILDFLAG(IS_WIN)
