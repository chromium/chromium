// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/clipboard_toast_tracker.h"

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/widget/widget.h"

namespace enterprise_data_protection {

using ClipboardToastTrackerBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ClipboardToastTrackerBrowserTest,
                       MaybeShowCopyToastShowsAndRecordsAudit) {
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  auto* toast_controller = ToastController::From(browser());
  ASSERT_TRUE(toast_controller);
  EXPECT_FALSE(toast_controller->IsShowingToast());

  auto* tracker = ClipboardToastTracker::GetForProfile(browser()->GetProfile());
  ASSERT_TRUE(tracker);

  MaybeShowCopyToast(browser()->GetProfile(), web_contents,
                     CopyToastType::kAudit);

  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_EQ(toast_controller->GetCurrentToastId(),
            ToastId::kEnterpriseCopyAudit);
  EXPECT_FALSE(tracker->ShouldShowToast(CopyToastType::kAudit));
  EXPECT_TRUE(tracker->ShouldShowToast(CopyToastType::kKeptInManagedChrome));
}

IN_PROC_BROWSER_TEST_F(ClipboardToastTrackerBrowserTest,
                       MaybeShowCopyToastShowsAndRecordsKeptInManagedChrome) {
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  auto* toast_controller = ToastController::From(browser());
  ASSERT_TRUE(toast_controller);
  EXPECT_FALSE(toast_controller->IsShowingToast());

  auto* tracker = ClipboardToastTracker::GetForProfile(browser()->GetProfile());
  ASSERT_TRUE(tracker);

  MaybeShowCopyToast(browser()->GetProfile(), web_contents,
                     CopyToastType::kKeptInManagedChrome);

  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_EQ(toast_controller->GetCurrentToastId(),
            ToastId::kEnterpriseCopyKeptInManagedChrome);
  EXPECT_TRUE(tracker->ShouldShowToast(CopyToastType::kAudit));
  EXPECT_FALSE(tracker->ShouldShowToast(CopyToastType::kKeptInManagedChrome));
}

IN_PROC_BROWSER_TEST_F(ClipboardToastTrackerBrowserTest,
                       ToastNotShownTwiceInSingleSession) {
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  auto* toast_controller = ToastController::From(browser());
  ASSERT_TRUE(toast_controller);
  EXPECT_FALSE(toast_controller->IsShowingToast());

  auto* tracker = ClipboardToastTracker::GetForProfile(browser()->GetProfile());
  ASSERT_TRUE(tracker);

  // 1. Show the toast for the first time.
  MaybeShowCopyToast(browser()->GetProfile(), web_contents,
                     CopyToastType::kAudit);
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_EQ(toast_controller->GetCurrentToastId(),
            ToastId::kEnterpriseCopyAudit);

  // 2. Dismiss the toast widget so that the UI is clean again.
  toast_controller->GetToastWidgetForTesting()->CloseNow();
  EXPECT_FALSE(toast_controller->IsShowingToast());

  // 3. Attempt to show the same toast type again during the same active
  // session.
  MaybeShowCopyToast(browser()->GetProfile(), web_contents,
                     CopyToastType::kAudit);

  // 4. Verify that the toast is NOT shown a second time.
  EXPECT_FALSE(toast_controller->IsShowingToast());
}

}  // namespace enterprise_data_protection
