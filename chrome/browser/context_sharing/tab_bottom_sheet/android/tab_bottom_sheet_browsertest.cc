// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/tab_bottom_sheet_test_feature.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace context_sharing {

class TabBottomSheetBrowserTest : public PlatformBrowserTest {
 public:
  TabBottomSheetBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(chrome::android::kTabBottomSheet);
  }
  ~TabBottomSheetBrowserTest() override = default;

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabBottomSheetBrowserTest, ShowAndClose) {
  // 1. Get the active tab.
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);
  tabs::TabInterface* tab = tab_list->GetActiveTab();
  ASSERT_TRUE(tab);

  // 3. Create a WebContents for the bottom sheet.
  std::unique_ptr<content::WebContents> bottom_sheet_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  ASSERT_TRUE(bottom_sheet_contents);

  // 2. Create the test feature.
  TabBottomSheetTestFeature feature(tab);

  feature.SetWebContents(bottom_sheet_contents.get());

  // 4. Show the bottom sheet.
  base::RunLoop run_loop;
  feature.set_on_opened_callback(run_loop.QuitClosure());

  bool shown = feature.Show(/*animate=*/false, /*starts_expanded=*/true);
  ASSERT_TRUE(shown);

  // Wait for OnOpened.
  run_loop.Run();
  EXPECT_TRUE(feature.was_opened());
  EXPECT_TRUE(feature.is_expanded());

  // 5. Close the bottom sheet.
  base::RunLoop close_run_loop;
  feature.set_on_closed_callback(close_run_loop.QuitClosure());

  feature.Close(/*animate=*/false);

  // Wait for OnClosed.
  close_run_loop.Run();
  EXPECT_TRUE(feature.was_closed());
}

IN_PROC_BROWSER_TEST_F(TabBottomSheetBrowserTest, MultipleShows) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);
  tabs::TabInterface* tab = tab_list->GetActiveTab();
  ASSERT_TRUE(tab);

  std::unique_ptr<content::WebContents> contents1 =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  std::unique_ptr<content::WebContents> contents2 =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));

  // First show
  {
    TabBottomSheetTestFeature feature(tab);
    feature.SetWebContents(contents1.get());
    base::RunLoop run_loop1;
    feature.set_on_opened_callback(run_loop1.QuitClosure());
    ASSERT_TRUE(feature.Show(/*animate=*/false, /*starts_expanded=*/true));
    run_loop1.Run();
    EXPECT_TRUE(feature.was_opened());

    // Close
    base::RunLoop close_run_loop1;
    feature.set_on_closed_callback(close_run_loop1.QuitClosure());
    feature.Close(/*animate=*/false);
    close_run_loop1.Run();
    EXPECT_TRUE(feature.was_closed());
  }

  // Second show - using a NEW feature instance to respect single-use contract
  // of CoBrowseViews in Java.
  {
    TabBottomSheetTestFeature feature2(tab);
    feature2.SetWebContents(contents2.get());
    base::RunLoop run_loop2;
    feature2.set_on_opened_callback(run_loop2.QuitClosure());
    ASSERT_TRUE(feature2.Show(/*animate=*/false, /*starts_expanded=*/true));
    run_loop2.Run();
    EXPECT_TRUE(feature2.was_opened());

    // Close at the end to avoid test pollution!
    base::RunLoop close_run_loop2;
    feature2.set_on_closed_callback(close_run_loop2.QuitClosure());
    feature2.Close(/*animate=*/false);
    close_run_loop2.Run();
    EXPECT_TRUE(feature2.was_closed());
  }
}

IN_PROC_BROWSER_TEST_F(TabBottomSheetBrowserTest, CloseWhenNotShowing) {
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_TRUE(tab_list);
  tabs::TabInterface* tab = tab_list->GetActiveTab();
  ASSERT_TRUE(tab);

  TabBottomSheetTestFeature feature(tab);

  // The sheet was never shown, so calling Close is a no-op and will not trigger
  // the callback since the delegate in Java is null.
  feature.Close(/*animate=*/false);

  // Verify that the callback was not triggered.
  EXPECT_FALSE(feature.was_closed());
}

}  // namespace context_sharing
