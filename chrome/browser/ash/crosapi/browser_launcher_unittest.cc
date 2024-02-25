// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_launcher.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/startup/startup_switches.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

class BrowserLauncherTest : public testing::Test {
 public:
  BrowserLauncherTest() = default;

 protected:
  BrowserLauncher* browser_launcher() { return &browser_launcher_; }

 private:
  base::test::TaskEnvironment task_environment_;

  BrowserLauncher browser_launcher_;
};

TEST_F(BrowserLauncherTest, AdditionalParametersForLaunchParams) {
  BrowserLauncher::LaunchParamsFromBackground params;
  params.lacros_additional_args.emplace_back("--switch1");
  params.lacros_additional_args.emplace_back("--switch2=value2");

  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ash::switches::kLacrosChromeAdditionalArgs, "--foo####--switch3=value3");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ash::switches::kLacrosChromeAdditionalEnv, "foo1=bar2####switch4=value4");

  BrowserLauncher::LaunchParams parameters(
      base::CommandLine({"/bin/sleep", "30"}), base::LaunchOptionsForTest());
  browser_launcher()->SetUpAdditionalParametersForTesting(params, parameters);

  EXPECT_TRUE(parameters.command_line.HasSwitch("switch1"));
  EXPECT_TRUE(parameters.command_line.HasSwitch("foo"));
  EXPECT_EQ(parameters.command_line.GetSwitchValueASCII("switch2"), "value2");
  EXPECT_EQ(parameters.command_line.GetSwitchValueASCII("switch3"), "value3");

  EXPECT_EQ(parameters.options.environment["foo1"], "bar2");
  EXPECT_EQ(parameters.options.environment["switch4"], "value4");

  EXPECT_EQ(parameters.command_line.GetSwitches().size(), 4u);
  EXPECT_EQ(parameters.options.environment.size(), 2u);
}

TEST_F(BrowserLauncherTest, WithoutAdditionalParametersForCommandLine) {
  BrowserLauncher::LaunchParamsFromBackground params;
  base::test::ScopedCommandLine scoped_command_line;
  BrowserLauncher::LaunchParams parameters(
      base::CommandLine({"/bin/sleep", "30"}), base::LaunchOptionsForTest());
  parameters.command_line.RemoveSwitch(
      ash::switches::kLacrosChromeAdditionalArgs);
  browser_launcher()->SetUpAdditionalParametersForTesting(params, parameters);
  EXPECT_EQ(parameters.command_line.GetSwitches().size(), 0u);
  EXPECT_EQ(parameters.options.environment.size(), 0u);
}

TEST_F(BrowserLauncherTest, LaunchAndTriggerTerminate) {
  // We'll use a process just does nothing for 30 seconds, which is long
  // enough to stably exercise the test cases we have.
  BrowserLauncher::LaunchParams parameters(
      base::CommandLine({"/bin/sleep", "30"}), base::LaunchOptionsForTest());
  browser_launcher()->LaunchProcessForTesting(parameters);
  EXPECT_TRUE(browser_launcher()->IsProcessValid());
  EXPECT_TRUE(browser_launcher()->TriggerTerminate(/*exit_code=*/0));
  int exit_code;
  EXPECT_TRUE(
      browser_launcher()->GetProcessForTesting().WaitForExit(&exit_code));
  // -1 is expected as an `exit_code` because it is compulsorily terminated by
  // signal.
  EXPECT_EQ(exit_code, -1);

  // TODO(mayukoaiba): We should reset process in order to check by
  // EXPECT_FALSE(browser_launcher()->IsProcessValid()) whether
  // "TriggerTerminate" works properly.
}

TEST_F(BrowserLauncherTest, TerminateOnBackground) {
  // We'll use a process just does nothing for 30 seconds, which is long
  // enough to stably exercise the test cases we have.
  BrowserLauncher::LaunchParams parameters(
      base::CommandLine({"/bin/sleep", "30"}), base::LaunchOptionsForTest());
  browser_launcher()->LaunchProcessForTesting(parameters);
  ASSERT_TRUE(browser_launcher()->IsProcessValid());
  base::test::TestFuture<void> future;
  browser_launcher()->EnsureProcessTerminated(future.GetCallback(),
                                              base::Seconds(5));
  EXPECT_FALSE(browser_launcher()->IsProcessValid());
}
}  // namespace crosapi
