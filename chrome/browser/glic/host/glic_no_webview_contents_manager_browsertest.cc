// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_no_webview_contents_manager.h"

#include <memory>
#include <string_view>

#include "base/strings/stringprintf.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
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

namespace {

GlicNoWebviewContentsManager* GetNoWebviewContentsManager(
    GlicInstanceImpl* instance) {
  if (!instance) {
    return nullptr;
  }
  return static_cast<GlicNoWebviewContentsManager*>(
      instance->host().contents_manager());
}

void ClickOverlayElement(content::WebContents* overlay_contents,
                         std::string_view query_selector) {
  ASSERT_TRUE(overlay_contents);
  EXPECT_TRUE(content::WaitForLoadStop(overlay_contents));
  content::ExecuteScriptAsync(overlay_contents,
                              base::StringPrintf(
                                  R"(
        const start = Date.now();
        const check = () => {
          const el = document.querySelector('%s');
          if (el && !el.hidden) {
            el.click();
          } else if (Date.now() - start <= 5000) {
            setTimeout(check, 50);
          }
        };
        check();
      )",
                                  std::string(query_selector).c_str()));
}

}  // namespace

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

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       IneligibleAccountHelpClickOpensTab) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  auto* manager = GetNoWebviewContentsManager(instance);
  ASSERT_TRUE(manager);
  ASSERT_TRUE(manager->overlay_contents());

  manager->SetErrorState(mojom::ErrorPanelType::kIneligibleAccount);

  GlicTestTabAddedWaiter waiter(GetProfile());
  ClickOverlayElement(manager->overlay_contents(),
                      "#ineligibleAccountHelpButton");

  tabs::TabInterface* new_tab = waiter.Wait();
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(new_tab->GetContents()->GetVisibleURL(),
            GURL(features::kGlicIneligibleAccountHelpUrl.Get()));
  EXPECT_OK(WaitForGlicClose(instance));
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       LocationMismatchHelpClickOpensTab) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  auto* manager = GetNoWebviewContentsManager(instance);
  ASSERT_TRUE(manager);
  ASSERT_TRUE(manager->overlay_contents());

  manager->SetErrorState(mojom::ErrorPanelType::kLocationMismatch);

  GlicTestTabAddedWaiter waiter(GetProfile());
  ClickOverlayElement(manager->overlay_contents(),
                      "#locationMismatchHelpButton");

  tabs::TabInterface* new_tab = waiter.Wait();
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(new_tab->GetContents()->GetVisibleURL(),
            GURL(features::kGlicLocationMismatchHelpUrl.Get()));
  EXPECT_OK(WaitForGlicClose(instance));
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       DisabledByAdminLinkClickOpensTabAndRecordsMetric) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  auto* manager = GetNoWebviewContentsManager(instance);
  ASSERT_TRUE(manager);
  ASSERT_TRUE(manager->overlay_contents());

  manager->SetErrorState(mojom::ErrorPanelType::kDisabledByAdminWithLink);

  base::UserActionTester user_action_tester;
  GlicTestTabAddedWaiter waiter(GetProfile());
  ClickOverlayElement(manager->overlay_contents(), "#disabledByAdminPanel a");

  tabs::TabInterface* new_tab = waiter.Wait();
  ASSERT_TRUE(new_tab);
  EXPECT_EQ(new_tab->GetContents()->GetVisibleURL(),
            GURL(features::kGlicCaaLinkUrl.Get()));
  EXPECT_EQ(
      user_action_tester.GetActionCount("Glic.DisabledByAdminPanelLinkClicked"),
      1);
  EXPECT_OK(WaitForGlicClose(instance));
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       CloseButtonClickClosesPanel) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  auto* manager = GetNoWebviewContentsManager(instance);
  ASSERT_TRUE(manager);
  ASSERT_TRUE(manager->overlay_contents());

  ClickOverlayElement(manager->overlay_contents(),
                      "#loadingPanel .close-button");
  EXPECT_OK(WaitForGlicClose(instance));
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       DisabledByAdminCloseButtonClickClosesPanel) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  auto* manager = GetNoWebviewContentsManager(instance);
  ASSERT_TRUE(manager);
  ASSERT_TRUE(manager->overlay_contents());

  manager->SetErrorState(mojom::ErrorPanelType::kDisabledByAdmin);

  ClickOverlayElement(manager->overlay_contents(),
                      "#disabledByAdminCloseButton");
  EXPECT_OK(WaitForGlicClose(instance));
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       RetryButtonClickTriggersReload) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  auto* manager = GetNoWebviewContentsManager(instance);
  ASSERT_TRUE(manager);
  ASSERT_TRUE(manager->overlay_contents());

  manager->SetErrorState(mojom::ErrorPanelType::kOffline);

  ClickOverlayElement(manager->overlay_contents(), "#retry");
}

}  // namespace glic
