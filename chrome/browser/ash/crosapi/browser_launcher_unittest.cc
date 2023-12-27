// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_launcher.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "chromeos/startup/startup_switches.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

class BrowserLauncherTest : public testing::Test {
 public:
  BrowserLauncherTest() = default;

 protected:
  base::CommandLine CreateCommandLine() {
    // We'll use a process just does nothing for 30 seconds, which is long
    // enough to stably exercise the test cases we have.
    return base::CommandLine({"/bin/sleep", "30"});
  }

  BrowserLauncher* browser_launcher() { return &browser_launcher_; }

 private:
  base::test::TaskEnvironment task_environment_;

  BrowserLauncher browser_launcher_;
};

TEST_F(BrowserLauncherTest, InitializeCommandLine) {
  // These arguments are just set for unit test and means nothing.
  const base::FilePath test_path = base::FilePath({"/path/to/chrome"});
  BrowserLauncher::LaunchParamsFromBackground params;
  bool launching_at_login_screen = true;

  std::optional<int> startup_fd = 3;
  std::optional<int> postlogin_data_fd = 4;

  std::string_view channel_flag_value = "3";

  base::CommandLine command_line =
      browser_launcher()->InitializeParametersForTesting(
          test_path, params, launching_at_login_screen, startup_fd,
          postlogin_data_fd, channel_flag_value);

  EXPECT_EQ(command_line.GetProgram(), test_path);

  EXPECT_EQ(command_line.GetSwitchValueASCII(switches::kLoggingLevel), "");

  EXPECT_EQ(
      command_line.GetSwitchValueASCII(chromeos::switches::kCrosStartupDataFD),
      base::NumberToString(startup_fd.value()));
  EXPECT_EQ(command_line.GetSwitchValueASCII(
                chromeos::switches::kCrosPostLoginDataFD),
            base::NumberToString(postlogin_data_fd.value()));

  EXPECT_EQ(command_line.GetSwitchValueASCII(kCrosapiMojoPlatformChannelHandle),
            channel_flag_value);
}

TEST_F(BrowserLauncherTest, LaunchAndTriggerTerminate) {
  browser_launcher()->LaunchProcessForTesting(CreateCommandLine(),
                                              base::LaunchOptionsForTest());
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
  browser_launcher()->LaunchProcessForTesting(CreateCommandLine(),
                                              base::LaunchOptionsForTest());
  ASSERT_TRUE(browser_launcher()->IsProcessValid());
  base::test::TestFuture<void> future;
  browser_launcher()->EnsureProcessTerminated(future.GetCallback(),
                                              base::Seconds(5));
  EXPECT_FALSE(browser_launcher()->IsProcessValid());
}
}  // namespace crosapi
