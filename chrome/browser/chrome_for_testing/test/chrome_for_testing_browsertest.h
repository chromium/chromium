// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_FOR_TESTING_TEST_CHROME_FOR_TESTING_BROWSERTEST_H_
#define CHROME_BROWSER_CHROME_FOR_TESTING_TEST_CHROME_FOR_TESTING_BROWSERTEST_H_

#include <string>
#include <string_view>

#include "base/files/scoped_temp_dir.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_for_testing {

class ChromeForTestingBrowserTest : public InProcessBrowserTest {
 public:
  ChromeForTestingBrowserTest();

  void SetUpCommandLine(base::CommandLine* command_line) override;

 protected:
  // Override to return Chrome for Testing config JSON.
  virtual std::string GetConfigJson();

  void AppendConfigSwitch(base::CommandLine* command_line);

  std::string FormatConfigJsonBoolean(std::string_view option, bool value);

  base::ScopedTempDir temp_dir_;
};

}  // namespace chrome_for_testing

#endif  // CHROME_BROWSER_CHROME_FOR_TESTING_TEST_CHROME_FOR_TESTING_BROWSERTEST_H_
