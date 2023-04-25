// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"

class CalculatorBrowserTest : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(CalculatorBrowserTest, Model) {
  base::FilePath test_file;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_file);
  test_file =
      test_file.AppendASCII("extensions/calculator_app/tests/automatic.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           net::FilePathToFileURL(test_file)));

  ASSERT_EQ(true, content::EvalJs(
                      browser()->tab_strip_model()->GetActiveWebContents(),
                      "window.runTests().success"));
}
