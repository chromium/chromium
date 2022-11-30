// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/startup_helper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"

class StartupHelperBrowserTest : public InProcessBrowserTest {
 public:
  StartupHelperBrowserTest() {}

  StartupHelperBrowserTest(const StartupHelperBrowserTest&) = delete;
  StartupHelperBrowserTest& operator=(const StartupHelperBrowserTest&) = delete;

  ~StartupHelperBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kNoStartupWindow);

    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
    test_data_dir_ = test_data_dir_.AppendASCII("extensions");
  }

 protected:
  base::FilePath test_data_dir_;
};

IN_PROC_BROWSER_TEST_F(StartupHelperBrowserTest, ValidateCrx) {
  // A list of crx file paths along with an expected result of valid (true) or
  // invalid (false).
  std::vector<std::pair<base::FilePath, bool> > expectations;
  expectations.push_back(
      std::make_pair(test_data_dir_.AppendASCII("good.crx"), true));
  expectations.push_back(
      std::make_pair(test_data_dir_.AppendASCII("good2.crx"), true));
  expectations.push_back(
      std::make_pair(test_data_dir_.AppendASCII("bad_underscore.crx"), true));
  expectations.push_back(
      std::make_pair(test_data_dir_.AppendASCII("bad_magic.crx"), false));

  for (auto i = expectations.begin(); i != expectations.end(); ++i) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    const base::FilePath& path = i->first;
    command_line.AppendSwitchPath(switches::kValidateCrx, path);

    std::string error;
    extensions::StartupHelper helper;
    base::ScopedAllowBlockingForTesting allow_blocking;
    bool result = helper.ValidateCrx(command_line, &error);
    if (i->second) {
      EXPECT_TRUE(result) << path.LossyDisplayName()
                          << " expected to be valid but wasn't";
    } else {
      EXPECT_FALSE(result) << path.LossyDisplayName()
                           << " expected to be invalid but wasn't";
      EXPECT_FALSE(error.empty()) << "Error message wasn't set for "
                                  << path.LossyDisplayName();
    }
  }
}
