// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base_paths.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "chrome/browser/platform_util_internal.h"  // nogncheck (crbug.com/335727004)
#include "chrome/browser/shortcuts/create_shortcut_for_current_web_contents_task.h"
#include "chrome/browser/shortcuts/shortcut_creator.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/image/image_skia.h"

namespace shortcuts {

namespace {

constexpr char kPageWithIcons[] = "/shortcuts/page_icons.html";

class ShortcutCreationBrowserTest : public InProcessBrowserTest {
 public:
  ShortcutCreationBrowserTest() {
    platform_util::internal::DisableShellOperationsForTesting();
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  base::test::ScopedFeatureList feature_list_{features::kShortcutsNotApps};
  base::ScopedPathOverride desktop_{base::DIR_USER_DESKTOP};
};

IN_PROC_BROWSER_TEST_F(ShortcutCreationBrowserTest,
                       CannotStackTasksSameWebContents) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL(kPageWithIcons)));

  base::HistogramTester histogram_tester;

  // Stall the first creation flow to prevent it from proceeding.
  base::test::TestFuture<
      const gfx::ImageSkia&, std::u16string,
      CreateShortcutForCurrentWebContentsTask::ShortcutsDialogResultCallback>
      dialog_callback;
  base::test::TestFuture<bool> first_callback;
  CreateShortcutForCurrentWebContentsTask::CreateAndStart(
      *web_contents(), dialog_callback.GetCallback(),
      first_callback.GetCallback());

  // Verify the first create shortcut task is blocked.
  EXPECT_FALSE(dialog_callback.IsReady());
  EXPECT_FALSE(first_callback.IsReady());

  // Start a new task, verify that does not end up starting.
  base::test::TestFuture<
      const gfx::ImageSkia&, std::u16string,
      CreateShortcutForCurrentWebContentsTask::ShortcutsDialogResultCallback>
      dialog_callback_second;
  base::test::TestFuture<bool> final_callback;
  CreateShortcutForCurrentWebContentsTask::CreateAndStart(
      *web_contents(), dialog_callback_second.GetCallback(),
      final_callback.GetCallback());
  EXPECT_FALSE(final_callback.Get<bool>());
  EXPECT_FALSE(dialog_callback_second.IsReady());
  histogram_tester.ExpectBucketCount(
      "Shortcuts.CreationTask.Result",
      ShortcutCreationTaskResult::kTaskAlreadyRunning, 1);
}

#if BUILDFLAG(IS_LINUX)
#define MAYBE_WebContentsNavigatedAway DISABLED_WebContentsNavigatedAway
#else
#define MAYBE_WebContentsNavigatedAway WebContentsNavigatedAway
#endif  // BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_F(ShortcutCreationBrowserTest,
                       MAYBE_WebContentsNavigatedAway) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL(kPageWithIcons)));

  base::HistogramTester histogram_tester;

  // Stall the creation flow to prevent it from proceeding.
  base::test::TestFuture<
      const gfx::ImageSkia&, std::u16string,
      CreateShortcutForCurrentWebContentsTask::ShortcutsDialogResultCallback>
      dialog_callback;
  base::test::TestFuture<bool> first_callback;
  CreateShortcutForCurrentWebContentsTask::CreateAndStart(
      *web_contents(), dialog_callback.GetCallback(),
      first_callback.GetCallback());

  // Verify the first create shortcut task is blocked.
  EXPECT_FALSE(dialog_callback.IsReady());
  EXPECT_FALSE(first_callback.IsReady());

  // Navigate to a separate page.
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL(url::kAboutBlankURL), /*number_of_navigations=*/1);
  ASSERT_TRUE(first_callback.Wait());
  EXPECT_FALSE(first_callback.Get<bool>());
  histogram_tester.ExpectBucketCount(
      "Shortcuts.CreationTask.Result",
      ShortcutCreationTaskResult::kPageInvalidated, 1);

  // Verify task gets destroyed after exiting due to errors.
  EXPECT_THAT(CreateShortcutForCurrentWebContentsTask::GetForCurrentDocument(
                  web_contents()->GetPrimaryMainFrame()),
              testing::IsNull());
}

IN_PROC_BROWSER_TEST_F(ShortcutCreationBrowserTest, VisibilityChangeStopsTask) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL(kPageWithIcons)));

  base::HistogramTester histogram_tester;

  // Stall the creation flow to prevent it from proceeding.
  base::test::TestFuture<
      const gfx::ImageSkia&, std::u16string,
      CreateShortcutForCurrentWebContentsTask::ShortcutsDialogResultCallback>
      dialog_callback;
  base::test::TestFuture<bool> first_callback;
  CreateShortcutForCurrentWebContentsTask::CreateAndStart(
      *web_contents(), dialog_callback.GetCallback(),
      first_callback.GetCallback());

  // Verify the first create shortcut task is blocked.
  EXPECT_FALSE(dialog_callback.IsReady());
  EXPECT_FALSE(first_callback.IsReady());

  // Open a new tab.
  chrome::NewTab(browser());
  ASSERT_TRUE(first_callback.Wait());
  EXPECT_FALSE(first_callback.Get<bool>());
  histogram_tester.ExpectBucketCount(
      "Shortcuts.CreationTask.Result",
      ShortcutCreationTaskResult::kPageInvalidated, 1);
}

}  // namespace
}  // namespace shortcuts
