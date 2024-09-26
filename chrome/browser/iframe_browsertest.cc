// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

class IFrameTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  void NavigateAndVerifyTitle(const char* file, const char* page_title) {
    GURL url = ui_test_utils::GetTestUrl(
        base::FilePath(), base::FilePath().AppendASCII(file));

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_EQ(base::ASCIIToUTF16(page_title),
              browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
  }
};

IN_PROC_BROWSER_TEST_F(IFrameTest, Crash) {
  NavigateAndVerifyTitle("iframe.html", "iframe test");
}

IN_PROC_BROWSER_TEST_F(IFrameTest, InEmptyFrame) {
  NavigateAndVerifyTitle("iframe_in_empty_frame.html", "iframe test");
}

// Test for https://crbug.com/621076. It ensures that file chooser triggered
// by an iframe, which is destroyed before the chooser is closed, does not
// result in a use-after-free condition.
//
// Note: This test is disabled temporarily to track down a memory leak reported
// by the ASan bots. It will be enabled once the root cause is found.
// TODO(crbug.com/40904458): Re-enable this test
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_FileChooserInDestroyedSubframe \
  DISABLED_FileChooserInDestroyedSubframe
#else
#define MAYBE_FileChooserInDestroyedSubframe FileChooserInDestroyedSubframe
#endif
IN_PROC_BROWSER_TEST_F(IFrameTest, MAYBE_FileChooserInDestroyedSubframe) {
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL file_input_url(embedded_test_server()->GetURL("/file_input.html"));

  // Navigate to a page, which contains an iframe, and navigate the iframe
  // to a document containing a file input field.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe.html")));
  NavigateIframeToURL(tab, "test", file_input_url);

  // Invoke the file chooser and remove the iframe from the main document.
  content::RenderFrameHost* frame = ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(frame);
  EXPECT_EQ(frame->GetSiteInstance(),
            tab->GetPrimaryMainFrame()->GetSiteInstance());
  EXPECT_TRUE(ExecJs(frame, "document.getElementById('fileinput').click();"));
  EXPECT_TRUE(ExecJs(tab->GetPrimaryMainFrame(),
                     "document.body.removeChild("
                     "document.querySelectorAll('iframe')[0])"));
  ASSERT_EQ(nullptr, ChildFrameAt(tab->GetPrimaryMainFrame(), 0));

  // On ASan bots, this test should succeed without reporting use-after-free
  // condition.
}
