// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scheduler_config/scheduler_configuration_manager.h"

#include <memory>

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"

namespace ash {

class SchedulerConfigurationManagerTest
    : public testing::Test,
      public SchedulerConfigurationManagerBase::Observer {
 public:
  SchedulerConfigurationManagerTest() {
    SchedulerConfigurationManager::RegisterLocalStatePrefs(
        local_state_.registry());
  }

  // SchedulerConfigurationManagerBase::Observer:
  void OnConfigurationSet(bool success, size_t num_cores_disabled) override {
    ++configuration_set_count_;
  }

  base::test::TaskEnvironment task_environment_;

  FakeDebugDaemonClient debug_daemon_client_;
  TestingPrefServiceSimple local_state_;

  size_t configuration_set_count_ = 0;
};

TEST_F(SchedulerConfigurationManagerTest, Startup) {
  local_state_.SetString(prefs::kSchedulerConfiguration, "initial");
  debug_daemon_client_.SetServiceIsAvailable(false);

  // Manager waits on initialization for service to be available.
  SchedulerConfigurationManager manager(&debug_daemon_client_, &local_state_);
  manager.AddObserver(this);

  task_environment_.RunUntilIdle();
  EXPECT_EQ("", debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(0u, configuration_set_count_);

  // Config changes don't lead to updates while debugd isn't ready.
  local_state_.SetString(prefs::kSchedulerConfiguration, "config");
  task_environment_.RunUntilIdle();
  EXPECT_EQ("", debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(0u, configuration_set_count_);

  // Once the debugd service becomes available, the config gets set.
  debug_daemon_client_.SetServiceIsAvailable(true);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("config", debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(1u, configuration_set_count_);
}

TEST_F(SchedulerConfigurationManagerTest, CommandLineDefault) {
  // In real usage the command line is set before Chrome launches, so set it as
  // such here.
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* cmd_line = scoped_command_line.GetProcessCommandLine();
  cmd_line->AppendSwitchASCII("scheduler-configuration-default",
                              "cmd-line-default");

  // Correct default is used when the command line is setup.
  SchedulerConfigurationManager manager(&debug_daemon_client_, &local_state_);
  manager.AddObserver(this);
  task_environment_.RunUntilIdle();

  EXPECT_EQ("cmd-line-default",
            debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(1u, configuration_set_count_);

  // Change user pref, which should trigger a config change.
  local_state_.SetUserPref(prefs::kSchedulerConfiguration,
                           std::make_unique<base::Value>("user"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("user", debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(2u, configuration_set_count_);

  // Set a policy, which should override the user setting
  local_state_.SetManagedPref(prefs::kSchedulerConfiguration,
                              std::make_unique<base::Value>("policy"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("policy", debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(3u, configuration_set_count_);

  // Dropping the user pref doesn't change anything.
  local_state_.RemoveUserPref(prefs::kSchedulerConfiguration);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("policy", debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(4u, configuration_set_count_);

  // Dropping the policy as well reverts to the cmdline default.
  local_state_.RemoveManagedPref(prefs::kSchedulerConfiguration);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("cmd-line-default",
            debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(5u, configuration_set_count_);
}

TEST_F(SchedulerConfigurationManagerTest, ConfigChange) {
  // Correct default is used when there is no configured value.
  SchedulerConfigurationManager manager(&debug_daemon_client_, &local_state_);
  manager.AddObserver(this);

  task_environment_.RunUntilIdle();
  EXPECT_EQ(debugd::scheduler_configuration::kCoreIsolationScheduler,
            debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(1u, configuration_set_count_);

  // Change user pref, which should trigger a config change.
  local_state_.SetUserPref(prefs::kSchedulerConfiguration,
                           std::make_unique<base::Value>("user"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("user", debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(2u, configuration_set_count_);

  // Set a policy, which should override the user setting
  local_state_.SetManagedPref(prefs::kSchedulerConfiguration,
                              std::make_unique<base::Value>("policy"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ("policy", debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(3u, configuration_set_count_);

  // Dropping the user pref doesn't change anything.
  local_state_.RemoveUserPref(prefs::kSchedulerConfiguration);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("policy", debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(4u, configuration_set_count_);

  // Dropping the policy as well reverts to the default configuration.
  local_state_.RemoveManagedPref(prefs::kSchedulerConfiguration);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(debugd::scheduler_configuration::kCoreIsolationScheduler,
            debug_daemon_client_.scheduler_configuration_name());
  EXPECT_EQ(5u, configuration_set_count_);
}

TEST_F(SchedulerConfigurationManagerTest, FinchDefault) {
  auto feature_list = std::make_unique<base::test::ScopedFeatureList>();
  feature_list->InitAndEnableFeatureWithParameters(
      features::kSchedulerConfiguration, {{"config", "finch"}});

  // Finch parameter selects the default.
  SchedulerConfigurationManager manager(&debug_daemon_client_, &local_state_);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("finch", debug_daemon_client_.scheduler_configuration_name());

  // Config values override finch default.
  local_state_.SetString(prefs::kSchedulerConfiguration, "config");
  task_environment_.RunUntilIdle();
  EXPECT_EQ("config", debug_daemon_client_.scheduler_configuration_name());
}

}  // namespace ash
