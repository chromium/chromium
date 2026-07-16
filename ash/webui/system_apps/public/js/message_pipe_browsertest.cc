// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/web_applications/test/js_library_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

using MessagePipeBrowserTest = JsLibraryTest;

IN_PROC_BROWSER_TEST_F(MessagePipeBrowserTest, All) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://system-app-test/test_data/"
                      "message_pipe_browsertest_trusted.html")));
  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(web_contents);

  ASSERT_TRUE(
      ExecJs(web_contents->GetPrimaryMainFrame(),
             "import('//webui-test/mocha.js')"
             ".then(() => import('//webui-test/mocha_adapter_simple.js'))"
             ".then(() => import('//system-app-test/message_pipe_test.js'));"));
  ASSERT_TRUE(RunTestOnWebContents(web_contents, "message_pipe_test.js",
                                   "mocha.run()",
                                   /*skip_test_loader=*/false));
}
