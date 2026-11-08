// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_toast.h"

#include <memory>
#include <string>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "url/gurl.h"

namespace glic {
namespace {

// Use Glic string resource IDs that are referenced in Desktop production code
// (chrome.dll) so they are retained when enable_resource_allowlist_generation
// is true on official builds.
constexpr int kFirstTitleId = IDS_GLIC_TASK_WAITING_FOR_CONSENT_TITLE;
constexpr int kFirstDescriptionId = IDS_GLIC_TASK_WAITING_FOR_CONSENT_SUBTITLE;
constexpr int kSecondTitleId = IDS_GLIC_ACTIVITY_TITLE_GEMINI;
constexpr int kSecondDescriptionId = IDS_GLIC_ACTIVITY_WORKING;

}  // namespace

class GlicToastBrowserTest : public InProcessBrowserTest {
 public:
  GlicToastBrowserTest() = default;
  ~GlicToastBrowserTest() override = default;

 protected:
  tabs::TabInterface* GetActiveTabInterface() {
    return browser()->GetActiveTabInterface();
  }

  ToastController* GetToastController() {
    return ToastController::MaybeGetForTabInterface(GetActiveTabInterface());
  }
};

IN_PROC_BROWSER_TEST_F(GlicToastBrowserTest, ShowWithTitleAndDescription) {
  tabs::TabInterface* tab = GetActiveTabInterface();
  ASSERT_TRUE(tab);

  std::unique_ptr<GlicToast> toast =
      GlicToast::Show(*tab, kFirstTitleId, kFirstDescriptionId);
  ASSERT_TRUE(toast);
  EXPECT_TRUE(toast->IsShowing());

  ToastController* const controller = GetToastController();
  ASSERT_TRUE(controller);
  EXPECT_TRUE(controller->IsShowingToast());
  EXPECT_EQ(controller->GetCurrentToastId(), ToastId::kGlic);

  toasts::ToastView* const toast_view = controller->GetToastViewForTesting();
  ASSERT_TRUE(toast_view);
  const std::u16string expected_text = l10n_util::GetStringFUTF16(
      IDS_GLIC_TOAST_TITLE_AND_DESCRIPTION,
      l10n_util::GetStringUTF16(kFirstTitleId),
      l10n_util::GetStringUTF16(kFirstDescriptionId));
  EXPECT_EQ(toast_view->label_for_testing()->GetText(), expected_text);
}

IN_PROC_BROWSER_TEST_F(GlicToastBrowserTest, ShowWithTitleOnly) {
  tabs::TabInterface* tab = GetActiveTabInterface();
  ASSERT_TRUE(tab);

  std::unique_ptr<GlicToast> toast = GlicToast::Show(*tab, kFirstTitleId);
  ASSERT_TRUE(toast);
  EXPECT_TRUE(toast->IsShowing());

  ToastController* const controller = GetToastController();
  ASSERT_TRUE(controller);
  EXPECT_TRUE(controller->IsShowingToast());
  EXPECT_EQ(controller->GetCurrentToastId(), ToastId::kGlic);

  toasts::ToastView* const toast_view = controller->GetToastViewForTesting();
  ASSERT_TRUE(toast_view);
  EXPECT_EQ(toast_view->label_for_testing()->GetText(),
            l10n_util::GetStringUTF16(kFirstTitleId));
}

IN_PROC_BROWSER_TEST_F(GlicToastBrowserTest, DestroyBeforeExpiryIsSafe) {
  tabs::TabInterface* tab = GetActiveTabInterface();
  ASSERT_TRUE(tab);

  std::unique_ptr<GlicToast> toast =
      GlicToast::Show(*tab, kFirstTitleId, kFirstDescriptionId);
  ASSERT_TRUE(toast);
  EXPECT_TRUE(toast->IsShowing());

  ToastController* const controller = GetToastController();
  ASSERT_TRUE(controller);
  EXPECT_TRUE(controller->IsShowingToast());

  base::test::TestFuture<void> toast_destroyed;
  auto subscription = controller->RegisterOnWidgetDestroyed(
      base::BindLambdaForTesting([&](ToastId id) {
        if (id == ToastId::kGlic) {
          toast_destroyed.SetValue();
        }
      }));

  // Destroying the GlicToast object does not immediately close Desktop Views
  // toasts, but allows them to expire safely on their own.
  toast.reset();
  EXPECT_TRUE(controller->IsShowingToast());

  // Trigger the auto-dismiss timer to close the toast.
  controller->GetToastCloseTimerForTesting()->FireNow();

  EXPECT_TRUE(toast_destroyed.Wait());
  EXPECT_FALSE(controller->IsShowingToast());
}

IN_PROC_BROWSER_TEST_F(GlicToastBrowserTest, AutoDismissSetsIsShowingFalse) {
  tabs::TabInterface* tab = GetActiveTabInterface();
  ASSERT_TRUE(tab);

  std::unique_ptr<GlicToast> toast =
      GlicToast::Show(*tab, kFirstTitleId, kFirstDescriptionId);
  ASSERT_TRUE(toast);
  EXPECT_TRUE(toast->IsShowing());

  ToastController* const controller = GetToastController();
  ASSERT_TRUE(controller);
  EXPECT_TRUE(controller->IsShowingToast());

  base::test::TestFuture<void> toast_destroyed;
  auto subscription = controller->RegisterOnWidgetDestroyed(
      base::BindLambdaForTesting([&](ToastId id) {
        if (id == ToastId::kGlic) {
          toast_destroyed.SetValue();
        }
      }));

  // Trigger the auto-dismiss timer.
  controller->GetToastCloseTimerForTesting()->FireNow();

  EXPECT_TRUE(toast_destroyed.Wait());
  EXPECT_FALSE(controller->IsShowingToast());
  EXPECT_FALSE(toast->IsShowing());
}

IN_PROC_BROWSER_TEST_F(GlicToastBrowserTest,
                       ShowOnBackgroundTabReturnsNullptr) {
  TabListInterface* const tab_list = TabListInterface::From(browser());
  ASSERT_TRUE(tab_list);
  tabs::TabInterface* const background_tab =
      tab_list->OpenTab(GURL("about:blank"), 1, /*foreground=*/false);
  ASSERT_TRUE(background_tab);
  EXPECT_FALSE(background_tab->IsActivated());

  std::unique_ptr<GlicToast> toast =
      GlicToast::Show(*background_tab, kFirstTitleId);
  EXPECT_EQ(toast, nullptr);

  ToastController* const controller = GetToastController();
  ASSERT_TRUE(controller);
  EXPECT_FALSE(controller->IsShowingToast());
}

IN_PROC_BROWSER_TEST_F(GlicToastBrowserTest,
                       ReassignWhileShowingPreemptsAndTransitions) {
  tabs::TabInterface* tab = GetActiveTabInterface();
  ASSERT_TRUE(tab);

  base::HistogramTester histogram_tester;

  std::unique_ptr<GlicToast> toast =
      GlicToast::Show(*tab, kFirstTitleId, kFirstDescriptionId);
  ASSERT_TRUE(toast);
  EXPECT_TRUE(toast->IsShowing());

  ToastController* const controller = GetToastController();
  ASSERT_TRUE(controller);
  EXPECT_TRUE(controller->IsShowingToast());

  base::test::TestFuture<void> first_toast_destroyed;
  auto subscription = controller->RegisterOnWidgetDestroyed(
      base::BindLambdaForTesting([&](ToastId id) {
        if (id == ToastId::kGlic) {
          first_toast_destroyed.SetValue();
        }
      }));

  // Reassigning `toast` creates the new GlicToast before destroying the old
  // one. The first toast should be closed with kPreempted (not
  // kFeatureDismiss), and the new toast should become queued in
  // next_toast_params_.
  toast = GlicToast::Show(*tab, kSecondTitleId, kSecondDescriptionId);
  ASSERT_TRUE(toast);
  EXPECT_TRUE(toast->IsShowing());

  // Old toast was preempted, not feature-dismissed.
  EXPECT_EQ(
      histogram_tester.GetBucketCount(
          "Toast.Glic.Dismissed", toasts::ToastCloseReason::kFeatureDismiss),
      0);
  EXPECT_EQ(histogram_tester.GetBucketCount(
                "Toast.Glic.Dismissed", toasts::ToastCloseReason::kPreempted),
            1);

  // Wait for the first toast widget to finish its fade-out and be destroyed.
  EXPECT_TRUE(first_toast_destroyed.Wait());

  // The new toast should now be showing.
  EXPECT_TRUE(controller->IsShowingToast());
  EXPECT_TRUE(toast->IsShowing());
  toasts::ToastView* const toast_view = controller->GetToastViewForTesting();
  ASSERT_TRUE(toast_view);
  const std::u16string expected_text = l10n_util::GetStringFUTF16(
      IDS_GLIC_TOAST_TITLE_AND_DESCRIPTION,
      l10n_util::GetStringUTF16(kSecondTitleId),
      l10n_util::GetStringUTF16(kSecondDescriptionId));
  EXPECT_EQ(toast_view->label_for_testing()->GetText(), expected_text);

  // Clean up the second toast via its auto-dismiss timer.
  base::test::TestFuture<void> second_toast_destroyed;
  subscription = controller->RegisterOnWidgetDestroyed(
      base::BindLambdaForTesting([&](ToastId id) {
        if (id == ToastId::kGlic) {
          second_toast_destroyed.SetValue();
        }
      }));
  controller->GetToastCloseTimerForTesting()->FireNow();
  EXPECT_TRUE(second_toast_destroyed.Wait());
  EXPECT_FALSE(controller->IsShowingToast());
}

}  // namespace glic
