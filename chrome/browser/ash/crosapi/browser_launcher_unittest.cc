// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_launcher.h"

#include "base/command_line.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
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

TEST_F(BrowserLauncherTest, LaunchAndTriggerTerminate) {
  browser_launcher()->LaunchProcess(CreateCommandLine(),
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
  browser_launcher()->LaunchProcess(CreateCommandLine(),
                                    base::LaunchOptionsForTest());
  ASSERT_TRUE(browser_launcher()->IsProcessValid());
  base::test::TestFuture<void> future;
  browser_launcher()->EnsureProcessTerminated(future.GetCallback(),
                                              base::Seconds(5));
  EXPECT_FALSE(browser_launcher()->IsProcessValid());
}
}  // namespace crosapi
