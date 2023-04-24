// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/command_line.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/variations/variations_switches.h"
#include "content/public/test/browser_test.h"

class FieldTrialsOnOffBrowserTest : public InProcessBrowserTest,
                                    public testing::WithParamInterface<bool> {
 public:
  FieldTrialsOnOffBrowserTest() = default;
  FieldTrialsOnOffBrowserTest(const FieldTrialsOnOffBrowserTest&) = delete;
  FieldTrialsOnOffBrowserTest& operator=(const FieldTrialsOnOffBrowserTest&) =
      delete;
  ~FieldTrialsOnOffBrowserTest() override = default;

  void SetUp() override {
    // Enable only one of --{enable,disable}-field-trial-config switches.
    std::vector<std::string> switches = {
        variations::switches::kDisableFieldTrialTestingConfig,
        variations::switches::kEnableFieldTrialTestingConfig};
    int append = GetParam();
    int remove = !append;
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (!command_line->HasSwitch(switches[append])) {
      command_line->AppendSwitch(switches[append]);
    }
    if (command_line->HasSwitch(switches[remove])) {
      command_line->RemoveSwitch(switches[remove]);
    }
    InProcessBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_P(FieldTrialsOnOffBrowserTest, Test) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL initial_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
}

INSTANTIATE_TEST_SUITE_P(All,
                         FieldTrialsOnOffBrowserTest,
                         testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "On" : "Off";
                         });
