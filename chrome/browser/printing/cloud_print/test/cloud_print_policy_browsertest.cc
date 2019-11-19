// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/result_codes.h"

// These tests don't apply to the Mac version; see GetCommandLineForRelaunch
// for details.
#if defined(OS_MACOSX)
#error This test file should not be part of the Mac build.
#endif

namespace {

class CloudPrintPolicyTest : public InProcessBrowserTest {
 public:
  CloudPrintPolicyTest() {}
};

IN_PROC_BROWSER_TEST_F(CloudPrintPolicyTest, NormalPassedFlag) {
  base::FilePath test_file_path = ui_test_utils::GetTestFilePath(
      base::FilePath(), base::FilePath().AppendASCII("empty.html"));
  base::CommandLine new_command_line(GetCommandLineForRelaunch());
  new_command_line.AppendArgPath(test_file_path);

  ui_test_utils::TabAddedWaiter tab_add(browser());

  base::Process process =
      base::LaunchProcess(new_command_line, base::LaunchOptionsForTest());
  EXPECT_TRUE(process.IsValid());

  tab_add.Wait();

  int exit_code = -100;
  bool exited = process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                               &exit_code);
  EXPECT_TRUE(exited);
  EXPECT_EQ(chrome::RESULT_CODE_NORMAL_EXIT_PROCESS_NOTIFIED, exit_code);
}

}  // namespace
