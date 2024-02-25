// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app_shim/app_shim_controller.h"

#include "base/apple/foundation_util.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/mac/app_mode_common.h"
#include "components/variations/variations_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kFeatureOnByDefaultName[] = "AppShimOnByDefault";
BASE_FEATURE(kFeatureOnByDefault,
             kFeatureOnByDefaultName,
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr char kFeatureOffByDefaultName[] = "AppShimOffByDefault";
BASE_FEATURE(kFeatureOffByDefault,
             kFeatureOffByDefaultName,
             base::FEATURE_DISABLED_BY_DEFAULT);

void PersistFeatureState(
    const variations::VariationsCommandLine& feature_state) {
  feature_state.WriteToFile(base::PathService::CheckedGet(chrome::DIR_USER_DATA)
                                .Append(app_mode::kFeatureStateFileName));
}

}  // namespace

class AppShimControllerTest : public testing::Test {
 private:
  base::ScopedPathOverride user_data_dir_override_{chrome::DIR_USER_DATA};
};

TEST_F(AppShimControllerTest, EarlyAccessFeatureAllowList) {
  base::test::ScopedFeatureList clear_feature_list;
  clear_feature_list.InitWithNullFeatureAndFieldTrialLists();

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  AppShimController::PreInitFeatureState(command_line);

  // Reset crash-on-early-access flag.
  base::FeatureList::ResetEarlyFeatureAccessTrackerForTesting();

  // Should not be able to access arbitrary features without getting early
  // access errors.
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_EQ(&kFeatureOnByDefault,
            base::FeatureList::GetEarlyAccessedFeatureForTesting());
  base::FeatureList::ResetEarlyFeatureAccessTrackerForTesting();
}

TEST_F(AppShimControllerTest, FeatureStateFromCommandLine) {
  base::test::ScopedFeatureList clear_feature_list;
  clear_feature_list.InitWithNullFeatureAndFieldTrialLists();

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kEnableFeatures,
                                 kFeatureOffByDefaultName);
  command_line.AppendSwitchASCII(switches::kDisableFeatures,
                                 kFeatureOnByDefaultName);
  AppShimController::PreInitFeatureState(command_line);

  base::FeatureList::GetInstance()->AddEarlyAllowedFeatureForTesting(
      kFeatureOnByDefaultName);
  base::FeatureList::GetInstance()->AddEarlyAllowedFeatureForTesting(
      kFeatureOffByDefaultName);
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_FALSE(base::FeatureList::GetEarlyAccessedFeatureForTesting());
}

TEST_F(AppShimControllerTest, FeatureStateFromFeatureFile) {
  base::test::ScopedFeatureList clear_feature_list;
  clear_feature_list.InitWithNullFeatureAndFieldTrialLists();

  variations::VariationsCommandLine feature_state;
  feature_state.enable_features = kFeatureOffByDefaultName;
  feature_state.disable_features = kFeatureOnByDefaultName;
  PersistFeatureState(feature_state);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  AppShimController::PreInitFeatureState(command_line);

  base::FeatureList::GetInstance()->AddEarlyAllowedFeatureForTesting(
      kFeatureOnByDefaultName);
  base::FeatureList::GetInstance()->AddEarlyAllowedFeatureForTesting(
      kFeatureOffByDefaultName);
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_FALSE(base::FeatureList::GetEarlyAccessedFeatureForTesting());
}

TEST_F(AppShimControllerTest, FeatureStateFromFeatureFileAndCommandLine) {
  base::test::ScopedFeatureList clear_feature_list;
  clear_feature_list.InitWithNullFeatureAndFieldTrialLists();

  variations::VariationsCommandLine feature_state;
  feature_state.enable_features = kFeatureOffByDefaultName;
  feature_state.disable_features = kFeatureOnByDefaultName;
  PersistFeatureState(feature_state);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kDisableFeatures,
                                 kFeatureOffByDefaultName);
  AppShimController::PreInitFeatureState(command_line);

  base::FeatureList::GetInstance()->AddEarlyAllowedFeatureForTesting(
      kFeatureOnByDefaultName);
  base::FeatureList::GetInstance()->AddEarlyAllowedFeatureForTesting(
      kFeatureOffByDefaultName);
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_FALSE(base::FeatureList::GetEarlyAccessedFeatureForTesting());
}

TEST_F(AppShimControllerTest,
       FeatureStateFromFeatureFileIsIgnoredWhenLaunchedByChrome) {
  base::test::ScopedFeatureList clear_feature_list;
  clear_feature_list.InitWithNullFeatureAndFieldTrialLists();

  variations::VariationsCommandLine feature_state;
  feature_state.enable_features = kFeatureOffByDefaultName;
  PersistFeatureState(feature_state);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(app_mode::kLaunchedByChromeProcessId);
  AppShimController::PreInitFeatureState(command_line);

  base::FeatureList::GetInstance()->AddEarlyAllowedFeatureForTesting(
      kFeatureOffByDefaultName);
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_FALSE(base::FeatureList::GetEarlyAccessedFeatureForTesting());
}

TEST_F(AppShimControllerTest, FinalizeFeatureState) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::Thread io_thread("CrAppShimIO");
  io_thread.Start();
  auto io_thread_runner = io_thread.task_runner();

  base::test::ScopedFeatureList clear_feature_list;
  clear_feature_list.InitWithNullFeatureAndFieldTrialLists();

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kEnableFeatures,
                                 kFeatureOffByDefaultName);
  AppShimController::PreInitFeatureState(command_line);

  base::FeatureList::GetInstance()->AddEarlyAllowedFeatureForTesting(
      kFeatureOffByDefaultName);
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_FALSE(base::FeatureList::GetEarlyAccessedFeatureForTesting());

  variations::VariationsCommandLine feature_state;
  feature_state.enable_features = "";
  feature_state.disable_features = base::JoinString(
      {kFeatureOnByDefaultName, kFeatureOffByDefaultName}, ",");
  AppShimController::FinalizeFeatureState(std::move(feature_state),
                                          io_thread_runner);
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureOnByDefault));
  EXPECT_FALSE(base::FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_FALSE(base::FeatureList::GetEarlyAccessedFeatureForTesting());

  // Verify that AppShimController did not leave the io thread blocked.
  base::RunLoop run_loop;
  io_thread_runner->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(AppShimControllerTest, FinalizeFeatureStateWithFieldTrials) {
  static constexpr char kTrialName[] = "TrialName";
  static constexpr char kTrialGroup1Name[] = "Group1";
  static constexpr char kTrialGroup2Name[] = "Group2";
  static constexpr char kParam1Name[] = "param1";
  static constexpr char kParam2Name[] = "param2";

  base::test::SingleThreadTaskEnvironment task_environment;
  base::Thread io_thread("CrAppShimIO");
  io_thread.Start();
  auto io_thread_runner = io_thread.task_runner();

  base::test::ScopedFeatureList clear_feature_list;
  clear_feature_list.InitWithNullFeatureAndFieldTrialLists();

  static const base::FeatureParam<int> feature_param1{&kFeatureOffByDefault,
                                                      kParam1Name, 0};
  static const base::FeatureParam<int> feature_param2{&kFeatureOffByDefault,
                                                      kParam2Name, 0};

  variations::VariationsCommandLine feature_state_in_file;
  feature_state_in_file.enable_features =
      base::StringPrintf("%s<%s", kFeatureOffByDefaultName, kTrialName);
  feature_state_in_file.disable_features = "";
  feature_state_in_file.field_trial_states =
      base::StringPrintf("%s/%s", kTrialName, kTrialGroup1Name);
  feature_state_in_file.field_trial_params = base::StringPrintf(
      "%s.%s:%s/13,%s.%s:%s/5", kTrialName, kTrialGroup1Name, kParam1Name,
      kTrialName, kTrialGroup2Name, kParam1Name);
  PersistFeatureState(feature_state_in_file);

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(
      variations::switches::kForceFieldTrialParams,
      base::StringPrintf("%s.%s:%s/42", kTrialName, kTrialGroup1Name,
                         kParam2Name));
  AppShimController::PreInitFeatureState(command_line);

  base::FeatureList::GetInstance()->AddEarlyAllowedFeatureForTesting(
      kFeatureOffByDefaultName);
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_FALSE(base::FeatureList::GetEarlyAccessedFeatureForTesting());
  // With both command line and feature state file data the values on the
  // command line should take priority.
  EXPECT_EQ(0, feature_param1.Get());
  EXPECT_EQ(42, feature_param2.Get());

  variations::VariationsCommandLine feature_state;
  feature_state.enable_features =
      base::StringPrintf("%s<%s", kFeatureOffByDefaultName, kTrialName);
  feature_state.disable_features = "";
  feature_state.field_trial_states =
      base::StringPrintf("%s/%s", kTrialName, kTrialGroup2Name);
  feature_state.field_trial_params = base::StringPrintf(
      "%s.%s:%s/2", kTrialName, kTrialGroup2Name, kParam1Name);
  AppShimController::FinalizeFeatureState(feature_state, io_thread_runner);
  EXPECT_TRUE(base::FeatureList::IsEnabled(kFeatureOffByDefault));
  EXPECT_FALSE(base::FeatureList::GetEarlyAccessedFeatureForTesting());
  EXPECT_EQ(2, feature_param1.Get());
  EXPECT_EQ(0, feature_param2.Get());
}
