// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_no_webview_contents_manager.h"

#include <memory>

#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_test_tab_added_waiter.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {

class GlicNoWebviewContentsManagerBrowserTest : public GlicBrowserTest {
 public:
  GlicNoWebviewContentsManagerBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicNoWebview,
                              pwc::mojom::features::kPrivilegedWebContents},
        /*disabled_features=*/{});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       IneligibleAccountHelpOpensTab) {
  GlicNoWebviewContentsManager manager(GetProfile(),
                                       /*initially_hidden=*/false);
  GlicTestTabAddedWaiter waiter(GetProfile());
  manager.GetOverlayPageHandlerForTesting()->OnIneligibleAccountHelpClicked();

  tabs::TabInterface* new_tab = waiter.Wait();
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(new_tab->GetContents()->GetVisibleURL(),
            GURL(features::kGlicIneligibleAccountHelpUrl.Get()));
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       LocationMismatchHelpOpensTab) {
  GlicNoWebviewContentsManager manager(GetProfile(),
                                       /*initially_hidden=*/false);
  GlicTestTabAddedWaiter waiter(GetProfile());
  manager.GetOverlayPageHandlerForTesting()->OnLocationMismatchHelpClicked();

  tabs::TabInterface* new_tab = waiter.Wait();
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(new_tab->GetContents()->GetVisibleURL(),
            GURL(features::kGlicLocationMismatchHelpUrl.Get()));
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       DisabledByAdminLinkOpensTabAndRecordsMetric) {
  GlicNoWebviewContentsManager manager(GetProfile(),
                                       /*initially_hidden=*/false);
  base::UserActionTester user_action_tester;
  GlicTestTabAddedWaiter waiter(GetProfile());
  manager.GetOverlayPageHandlerForTesting()->OnDisabledByAdminLinkClicked();

  tabs::TabInterface* new_tab = waiter.Wait();
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(new_tab->GetContents()->GetVisibleURL(),
            GURL(features::kGlicCaaLinkUrl.Get()));
  EXPECT_EQ(
      user_action_tester.GetActionCount("Glic.DisabledByAdminPanelLinkClicked"),
      1);
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       OverlayNotCreatedOnWarming) {
  GlicNoWebviewContentsManager manager(GetProfile(),
                                       /*initially_hidden=*/true);
  EXPECT_NE(manager.guest_contents(), nullptr);
  EXPECT_EQ(manager.overlay_contents(), nullptr);
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       OverlayNotCreatedOnErrorDuringWarming) {
  GlicNoWebviewContentsManager manager(GetProfile(),
                                       /*initially_hidden=*/true);
  EXPECT_FALSE(manager.ShouldReloadOnShow());
  manager.SetErrorState(mojom::ErrorPanelType::kError);
  EXPECT_TRUE(manager.ShouldReloadOnShow());
  EXPECT_EQ(manager.overlay_contents(), nullptr);

  // Deterministic error panels (like sign-in) should not trigger a reload.
  manager.SetErrorState(mojom::ErrorPanelType::kSignIn);
  EXPECT_FALSE(manager.ShouldReloadOnShow());
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       OverlayCreatedOnlyWhenShown) {
  GlicNoWebviewContentsManager manager(GetProfile(),
                                       /*initially_hidden=*/true);
  EXPECT_EQ(manager.state(),
            GlicNoWebviewContentsManager::DisplayState::kWarming);
  EXPECT_EQ(manager.overlay_contents(), nullptr);
  EXPECT_EQ(manager.active_web_contents(), nullptr);

  // Hidden visibility does not create the overlay.
  manager.SetVisibility(content::Visibility::HIDDEN);
  EXPECT_EQ(manager.overlay_contents(), nullptr);
  EXPECT_EQ(manager.active_web_contents(), nullptr);

  // Visible visibility creates the overlay and transitions to kShowingOverlay.
  manager.SetVisibility(content::Visibility::VISIBLE);
  EXPECT_EQ(manager.state(),
            GlicNoWebviewContentsManager::DisplayState::kShowingOverlay);
  EXPECT_NE(manager.overlay_contents(), nullptr);
  EXPECT_EQ(manager.active_web_contents(), manager.overlay_contents());

  // Transitioning back to hidden starts the 100ms deletion timer.
  manager.SetVisibility(content::Visibility::HIDDEN);
  EXPECT_EQ(manager.state(),
            GlicNoWebviewContentsManager::DisplayState::kWarming);
  EXPECT_EQ(manager.active_web_contents(), nullptr);
  EXPECT_TRUE(manager.overlay_deletion_timer_for_testing().IsRunning());
  EXPECT_NE(manager.overlay_contents(), nullptr);

  // Becoming visible again cancels the deletion timer and preserves the
  // overlay.
  manager.SetVisibility(content::Visibility::VISIBLE);
  EXPECT_FALSE(manager.overlay_deletion_timer_for_testing().IsRunning());
  EXPECT_NE(manager.overlay_contents(), nullptr);
  EXPECT_EQ(manager.active_web_contents(), manager.overlay_contents());
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       GuestErrorShowsGuestAndReloadsOnShow) {
  GlicNoWebviewContentsManager manager(GetProfile(),
                                       /*initially_hidden=*/false);
  manager.SetVisibility(content::Visibility::VISIBLE);
  EXPECT_EQ(manager.state(),
            GlicNoWebviewContentsManager::DisplayState::kShowingOverlay);

  // Navigate guest to a /sorry/ CAPTCHA or error page.
  manager.OnGuestNavigated(GURL("https://gemini.google.com/sorry/index"),
                           /*is_api_allowed=*/false,
                           mojom::GuestPageType::kGuestError,
                           /*is_initial_commit=*/false);

  // Guest contents should be shown so user can view/solve the CAPTCHA.
  EXPECT_EQ(manager.state(),
            GlicNoWebviewContentsManager::DisplayState::kShowingGuest);
  EXPECT_EQ(manager.active_web_contents(), manager.guest_contents());
  // Should reload on next show to recover from error state.
  EXPECT_TRUE(manager.ShouldReloadOnShow());
}

// TODO: Add end-to-end close button, overlay destruction on guest ready,
// and retry/reload tests in the next commit once Host routes to
// GlicNoWebviewContentsManager.

}  // namespace glic
