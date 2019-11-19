// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "chrome/browser/spellchecker/test/spellcheck_panel_browsertest_helper.h"
#endif

namespace {

class SpellCheckMacViewBrowserTest : public InProcessBrowserTest {
 public:
  SpellCheckMacViewBrowserTest() {}
};

#if BUILDFLAG(ENABLE_SPELLCHECK)
IN_PROC_BROWSER_TEST_F(SpellCheckMacViewBrowserTest, SpellCheckPanelVisible) {
  spellcheck::SpellCheckPanelBrowserTestHelper test_helper;

  ASSERT_TRUE(embedded_test_server()->Start());
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL("/title1.html"));

  SEL show_guess_panel = NSSelectorFromString(@"showGuessPanel:");
  [web_contents->GetRenderWidgetHostView()->GetNativeView().GetNativeNSView()
      performSelector:show_guess_panel];
  test_helper.RunUntilBind();
  spellcheck::SpellCheckMockPanelHost* host =
      test_helper.GetSpellCheckMockPanelHostForProcess(
          web_contents->GetMainFrame()->GetProcess());
  EXPECT_TRUE(host->SpellingPanelVisible());
}
#endif

}  // namespace
