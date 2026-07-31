// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/test/test_future.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/default_browser/setters/default_browser_visual_guided_setter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace default_browser {

using DefaultBrowserVisualGuidedSetterBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(DefaultBrowserVisualGuidedSetterBrowserTest, Execute) {
  DefaultBrowserVisualGuidedSetter setter(*browser()->GetProfile());
  base::test::TestFuture<DefaultBrowserState> future;

  int initial_tab_count = browser()->tab_strip_model()->count();

  setter.Execute(future.GetCallback(), /*params*/ {});

  EXPECT_EQ(browser()->tab_strip_model()->count(), initial_tab_count + 1);
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(active_contents->GetVisibleURL(),
            GetDefaultBrowserVisualGuideURL());

  EXPECT_FALSE(future.IsReady());

  // Closing the guided setter tab completes the flow.
  browser()->tab_strip_model()->CloseWebContentsAt(
      browser()->tab_strip_model()->active_index(),
      TabCloseTypes::CLOSE_USER_GESTURE);

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(future.Get(), DefaultBrowserState::NOT_DEFAULT);
}

}  // namespace default_browser
#endif  // BUILDFLAG(IS_WIN)
