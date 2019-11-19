// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace {

const char kEnabledGroupName[] = "Enabled";
const char kDisabledGroupName[] = "Disabled";

// Create 20 ONE_TIME_RANDOMIZED test trials to make sure they persist their
// state correctly between runs. We use 20 trials instead of just a couple so
// that if there's a failure, it likely won't be too flaky since the chance
// of all 20 randomly getting the same state is very low.
const int kNumTestTrials = 20;

}  // namespace

// This test verifies the functionality of the --force-fieldtrials flag.
// It spans multiple browser launches (using the PRE_ construct) and checks
// that forcing two trials affects only those trials, but not other ones. For
// the other trials, it checks that they get the same state between sessions
// as they are ONE_TIME_RANDOMIZED.
class ForceFieldTrialsBrowserTest : public InProcessBrowserTest,
                                    public testing::WithParamInterface<bool> {
 public:
  ForceFieldTrialsBrowserTest() : metrics_consent_(GetParam()) {}
  ~ForceFieldTrialsBrowserTest() override = default;

  std::string GetTestTrialName(int trial_number) {
    return base::StringPrintf("_TestTrial_%d", trial_number);
  }

  // Creates a 50/50 trial with ONE_TIME_RANDOMIZED consistency.
  void CreateFiftyPercentTrial(const std::string& trial_name) {
    auto* trial = base::FieldTrialList::FactoryGetFieldTrial(
        trial_name, 100, "Default", base::FieldTrial::ONE_TIME_RANDOMIZED,
        nullptr);
    trial->AppendGroup(kEnabledGroupName, 50);
    trial->AppendGroup(kDisabledGroupName, 50);
  }

  base::FilePath GetTestFilePath(int trial_number) {
    base::FilePath user_data_dir;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
    std::string file_name =
        "ForceFieldTrialsBrowserTest" + GetTestTrialName(trial_number);
    return user_data_dir.AppendASCII(file_name);
  }

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Force the first trial to "Enabled" and the second to "Disabled".
    command_line->AppendSwitchASCII(
        switches::kForceFieldTrials,
        GetTestTrialName(1) + "/Enabled/" + GetTestTrialName(2) + "/Disabled");
  }

  void SetUp() override {
    // Make metrics reporting work same as in Chrome branded builds, for test
    // consistency between Chromium and Chrome builds.
    ChromeMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
        true);
    // Based on GetParam(), either enable or disable metrics reporting.
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &metrics_consent_);
    InProcessBrowserTest::SetUp();
  }

 private:
  bool metrics_consent_;

  DISALLOW_COPY_AND_ASSIGN(ForceFieldTrialsBrowserTest);
};

IN_PROC_BROWSER_TEST_P(ForceFieldTrialsBrowserTest, PRE_PRE_ForceTrials) {
  // Empty test to bypass first-run provisional client id logic in
  // metrics_state_manager.cc before running the other two tests.
}

// The "PRE_" prefix makes this test run before the corresponding named test.
// This allows us to do a test spanning two Chrome launches and check the
// consistency of behavior between them.
IN_PROC_BROWSER_TEST_P(ForceFieldTrialsBrowserTest, PRE_ForceTrials) {
  // Create twenty one-time randomized field trials. Note: Loop is 1-indexed so
  // that we get trial names from "_TestTrial_1" to "_TestTrial_20".
  for (int i = 1; i <= kNumTestTrials; i++)
    CreateFiftyPercentTrial(GetTestTrialName(i));

  // The first two trials has been forced via SetUpCommandLine() above.
  EXPECT_EQ(kEnabledGroupName,
            base::FieldTrialList::FindFullName(GetTestTrialName(1)));
  EXPECT_EQ(kDisabledGroupName,
            base::FieldTrialList::FindFullName(GetTestTrialName(2)));

  // Save the other trials' group names to separate files, so we can check them
  // in the ForceTrials test below.
  for (int i = 3; i <= kNumTestTrials; i++) {
    const std::string trial_group =
        base::FieldTrialList::FindFullName(GetTestTrialName(i));
    EXPECT_TRUE(trial_group == kEnabledGroupName ||
                trial_group == kDisabledGroupName);

    base::ScopedAllowBlockingForTesting allow_blocking;
    int bytes_to_write = base::checked_cast<int>(trial_group.length());
    int bytes_written = base::WriteFile(GetTestFilePath(i), trial_group.c_str(),
                                        bytes_to_write);
    ASSERT_EQ(bytes_to_write, bytes_written);
  }
}

IN_PROC_BROWSER_TEST_P(ForceFieldTrialsBrowserTest, ForceTrials) {
#if defined(OS_CHROMEOS)
  // TODO(asvitkine): This test fails on Linux Chrome OS bots. Since it passes
  // on proper Chrome OS bots, and Linux Chrome OS is not an end user
  // configuration, disable there for now. Would be good to understand the
  // problem at some point, though. crbug.com/947132
  if (!base::SysInfo::IsRunningOnChromeOS())
    return;
#endif  // defined(OS_CHROMEOS)

  // Create twenty one-time randomized field trials. Note: Loop is 1-indexed so
  // that we get trial names from "_TestTrial_1" to "_TestTrial_20". Since they
  // are one-time randomized and there was a PRE_ test that created them earlier
  // they should get the same groups as before.
  for (int i = 1; i <= kNumTestTrials; i++)
    CreateFiftyPercentTrial(GetTestTrialName(i));

  // The first two trials has been forced via SetUpCommandLine() above.
  EXPECT_EQ(kEnabledGroupName,
            base::FieldTrialList::FindFullName(GetTestTrialName(1)));
  EXPECT_EQ(kDisabledGroupName,
            base::FieldTrialList::FindFullName(GetTestTrialName(2)));

  // Check that the other trials have expected state.
  for (int i = 3; i <= kNumTestTrials; i++) {
    const std::string trial_group =
        base::FieldTrialList::FindFullName(GetTestTrialName(i));
    EXPECT_TRUE(trial_group == kEnabledGroupName ||
                trial_group == kDisabledGroupName);

    // Read the trial group name that was saved by PRE_ForceTrials from the
    // corresponding test file.
    base::ScopedAllowBlockingForTesting allow_blocking;
    char data[256];
    int bytes_read = ReadFile(GetTestFilePath(i), data, sizeof(data));
    ASSERT_NE(-1, bytes_read);

    // Verify that the trial has the same group as last run and was unaffected
    // by the --force-fieldtrials= switch.
    EXPECT_EQ(std::string(data, bytes_read), trial_group)
        << GetTestTrialName(i);
  }
}

INSTANTIATE_TEST_SUITE_P(ForceFieldTrialsBrowserTests,
                         ForceFieldTrialsBrowserTest,
                         testing::Bool());
