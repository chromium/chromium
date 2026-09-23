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
#include "build/build_config.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_test_tab_added_waiter.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/prefs/pref_service.h"
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
  ASSERT_TRUE(content::WaitForLoadStop(overlay_contents));
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
        /*enabled_features=*/{features::kGlicNoWebview},
        /*disabled_features=*/{});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test fixture specifically for tests that interact with the overlay WebUI
// (error panels and loading panel). Uses a static page without Glic client
// scripts so the guest does not bootstrap into a client and trigger premature
// overlay deletion while the test interacts with the overlay.
class GlicNoWebviewOverlayBrowserTest
    : public GlicNoWebviewContentsManagerBrowserTest {
 public:
  GlicNoWebviewOverlayBrowserTest() {
    // Setting a static HTML page prevents the guest from bootstrapping a
    // GlicWebClient, avoiding race conditions where the guest loads quickly
    // and prematurely dismisses the loading panel or deletes the overlay.
    SetGlicPagePath("/title1.html");
  }
};

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       IneligibleAccountHelpOpensTab) {
  GlicNoWebviewContentsManager manager(GetProfile(), &service()->enabling(),
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
  GlicNoWebviewContentsManager manager(GetProfile(), &service()->enabling(),
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
  GlicNoWebviewContentsManager manager(GetProfile(), &service()->enabling(),
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
  GlicNoWebviewContentsManager manager(GetProfile(), &service()->enabling(),
                                       /*initially_hidden=*/true);
  EXPECT_NE(manager.guest_contents(), nullptr);
  EXPECT_EQ(manager.overlay_contents(), nullptr);
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       OverlayNotCreatedOnErrorDuringWarming) {
  GlicNoWebviewContentsManager manager(GetProfile(), &service()->enabling(),
                                       /*initially_hidden=*/true);
  EXPECT_EQ(manager.state(),
            GlicNoWebviewContentsManager::DisplayState::kWarming);
  EXPECT_FALSE(manager.ShouldReloadOnShow());
  manager.SetErrorState(mojom::ErrorPanelType::kError);
  EXPECT_TRUE(manager.ShouldReloadOnShow());
  EXPECT_EQ(manager.overlay_contents(), nullptr);
  EXPECT_EQ(manager.state(),
            GlicNoWebviewContentsManager::DisplayState::kWarming);

  // Deterministic error panels (like sign-in) should not trigger a reload.
  manager.SetErrorState(mojom::ErrorPanelType::kSignIn);
  EXPECT_FALSE(manager.ShouldReloadOnShow());

  // Becoming visible transitions to kShowingOverlay and creates overlay.
  manager.SetVisibility(content::Visibility::VISIBLE);
  EXPECT_EQ(manager.state(),
            GlicNoWebviewContentsManager::DisplayState::kShowingOverlay);
  EXPECT_NE(manager.overlay_contents(), nullptr);
  EXPECT_EQ(manager.active_web_contents(), manager.overlay_contents());
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       OverlayCreatedOnlyWhenShown) {
  GlicNoWebviewContentsManager manager(GetProfile(), &service()->enabling(),
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
  GlicNoWebviewContentsManager manager(GetProfile(), &service()->enabling(),
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
                       PanelOpenRetriesLoadingWhenInErrorState) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));
  PreventDeletionOnClose(instance);
  content::WebContents* initial_guest = instance->host().web_client_contents();
  ASSERT_TRUE(initial_guest);

  auto* manager = GetNoWebviewContentsManager(instance);
  ASSERT_TRUE(manager);
  EXPECT_FALSE(manager->ShouldReloadOnShow());

  // Normal close and re-open should not trigger a reload.
  ASSERT_OK(CloseGlicForTabAndWait(GetTabListInterface()->GetActiveTab()));
  GlicInstanceImpl* reopened_instance = nullptr;
  ASSERT_OK_AND_ASSIGN(reopened_instance, OpenGlicForActiveTab());
  EXPECT_EQ(instance, reopened_instance);
  EXPECT_EQ(initial_guest, instance->host().web_client_contents());

  // Put the manager into a transient error state that requires reload on show.
  manager = GetNoWebviewContentsManager(instance);
  ASSERT_TRUE(manager);
  manager->SetErrorState(mojom::ErrorPanelType::kOffline);
  EXPECT_TRUE(manager->ShouldReloadOnShow());

  // Close the panel while in the error state.
  ASSERT_OK(CloseGlicForTabAndWait(GetTabListInterface()->GetActiveTab()));

  // Re-opening the panel should detect ShouldReloadOnShow() and reload the
  // host.
  ASSERT_OK_AND_ASSIGN(reopened_instance, OpenGlicForActiveTab());
  EXPECT_EQ(instance, reopened_instance);
  ASSERT_OK(WaitForGlicClient(instance));
  EXPECT_OK(
      WaitForWebUiContentsVisibility(instance, content::Visibility::VISIBLE));
  content::WebContents* new_guest = instance->host().web_client_contents();
  EXPECT_TRUE(new_guest);
  EXPECT_NE(initial_guest, new_guest);

  auto* new_manager = GetNoWebviewContentsManager(instance);
  ASSERT_TRUE(new_manager);
  EXPECT_FALSE(new_manager->ShouldReloadOnShow());
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewOverlayBrowserTest,
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

IN_PROC_BROWSER_TEST_F(GlicNoWebviewOverlayBrowserTest,
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

IN_PROC_BROWSER_TEST_F(GlicNoWebviewOverlayBrowserTest,
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

IN_PROC_BROWSER_TEST_F(GlicNoWebviewOverlayBrowserTest,
                       CloseButtonClickClosesPanel) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  auto* manager = GetNoWebviewContentsManager(instance);
  ASSERT_TRUE(manager);
  ASSERT_TRUE(manager->overlay_contents());

  ClickOverlayElement(manager->overlay_contents(),
                      "#loadingPanel .close-button");
  EXPECT_OK(WaitForGlicClose(instance));
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewOverlayBrowserTest,
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

IN_PROC_BROWSER_TEST_F(GlicNoWebviewOverlayBrowserTest,
                       RetryButtonClickTriggersReload) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  auto* manager = GetNoWebviewContentsManager(instance);
  ASSERT_TRUE(manager);
  ASSERT_TRUE(manager->overlay_contents());

  manager->SetErrorState(mojom::ErrorPanelType::kOffline);

  ClickOverlayElement(manager->overlay_contents(), "#retry");
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewOverlayBrowserTest,
                       SignInRequiredShowsSignInPanelAndRecoversOnReauth) {
  // Invalidate account credentials to transition to kSignInRequired.
  InvalidateAccount(GetProfile());
  ASSERT_EQ(GlicEnabling::GetProfileReadyState(GetProfile()),
            mojom::ProfileReadyState::kSignInRequired);

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  auto* manager = GetNoWebviewContentsManager(instance);
  ASSERT_TRUE(manager);
  ASSERT_TRUE(manager->overlay_contents());

  // Manager must display the kSignIn error panel.
  EXPECT_EQ(manager->error_type(), mojom::ErrorPanelType::kSignIn);

  // Clicking the "Verify it's you" button triggers the sign-in flow.
  ClickOverlayElement(manager->overlay_contents(), "#signInButton");

  // User re-authenticates.
  ReauthAccount(GetProfile());
  ASSERT_EQ(GlicEnabling::GetProfileReadyState(GetProfile()),
            mojom::ProfileReadyState::kReady);

  // The error state must be cleared and the manager recovers to loading state.
  ASSERT_OK(WaitForErrorPanelType(std::nullopt));
}

IN_PROC_BROWSER_TEST_F(
    GlicNoWebviewOverlayBrowserTest,
    DisabledByAdminShowsDisabledByAdminPanelAndRecoversOnEnable) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  auto* manager = GetNoWebviewContentsManager(instance);
  ASSERT_TRUE(manager->overlay_contents());

  // Disable Glic policy while open.
  GetProfile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kDisabled));
  ASSERT_EQ(GlicEnabling::GetProfileReadyState(GetProfile()),
            mojom::ProfileReadyState::kDisabledByAdmin);

  // Manager must display the kDisabledByAdmin error panel.
  ASSERT_OK(WaitForErrorPanelType(mojom::ErrorPanelType::kDisabledByAdmin));

  // Re-enable Glic policy.
  GetProfile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::kGeminiSettings,
      std::to_underlying(
          optimization_guide::prefs::GeminiSettingsPolicyState::kEnabled));
  ASSERT_EQ(GlicEnabling::GetProfileReadyState(GetProfile()),
            mojom::ProfileReadyState::kReady);

  // The error state must be cleared and the manager recovers to loading state.
  ASSERT_OK(WaitForErrorPanelType(std::nullopt));
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       DetermineOverlayStateInputs) {
  using OverlayContentsManager =
      GlicNoWebviewContentsManager::OverlayContentsManager;
  // Input 1: Active error state takes precedence over loading/guest readiness.
  auto error_state = OverlayContentsManager::DetermineOverlayState(
      mojom::ErrorPanelType::kSignIn, /*is_guest_ready=*/false,
      mojom::PanelStateKind::kDetached);
  ASSERT_TRUE(error_state && error_state->is_error());
  EXPECT_EQ(error_state->get_error(), mojom::ErrorPanelType::kSignIn);

  // Error also takes precedence even if guest is ready.
  auto error_ready_state = OverlayContentsManager::DetermineOverlayState(
      mojom::ErrorPanelType::kDisabledByAdmin, /*is_guest_ready=*/true,
      mojom::PanelStateKind::kAttached);
  ASSERT_TRUE(error_ready_state && error_ready_state->is_error());
  EXPECT_EQ(error_ready_state->get_error(),
            mojom::ErrorPanelType::kDisabledByAdmin);

  // Input 2: Guest readiness. When guest is ready and no error, returns null.
  auto ready_state = OverlayContentsManager::DetermineOverlayState(
      /*error_type=*/std::nullopt, /*is_guest_ready=*/true,
      mojom::PanelStateKind::kAttached);
  EXPECT_FALSE(ready_state);

  // Input 3: Panel state. When loading and detached, returns kFloating style.
  auto floating_state = OverlayContentsManager::DetermineOverlayState(
      /*error_type=*/std::nullopt, /*is_guest_ready=*/false,
      mojom::PanelStateKind::kDetached);
  ASSERT_TRUE(floating_state && floating_state->is_loading());
  EXPECT_EQ(floating_state->get_loading(), mojom::LoadingStyle::kFloating);

  // When loading and attached to side panel or unattached, returns kSidePanel
  // style.
  auto attached_state = OverlayContentsManager::DetermineOverlayState(
      /*error_type=*/std::nullopt, /*is_guest_ready=*/false,
      mojom::PanelStateKind::kAttached);
  ASSERT_TRUE(attached_state && attached_state->is_loading());
  EXPECT_EQ(attached_state->get_loading(), mojom::LoadingStyle::kSidePanel);

  auto unattached_state = OverlayContentsManager::DetermineOverlayState(
      /*error_type=*/std::nullopt, /*is_guest_ready=*/false,
      /*panel_state_kind=*/std::nullopt);
  ASSERT_TRUE(unattached_state && unattached_state->is_loading());
  EXPECT_EQ(unattached_state->get_loading(), mojom::LoadingStyle::kSidePanel);
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewOverlayBrowserTest,
                       LoadingStyleMatchesPanelState) {
  {
    // Unattached manager defaults to kSidePanel loading style.
    GlicNoWebviewContentsManager manager(GetProfile(), &service()->enabling(),
                                         /*initially_hidden=*/false);
    auto overlay_state = manager.GetOverlayStateForTesting();
    ASSERT_TRUE(overlay_state && overlay_state->is_loading());
    EXPECT_EQ(overlay_state->get_loading(), mojom::LoadingStyle::kSidePanel);
    EXPECT_EQ(manager.GetLoadingStyleForTesting(),
              mojom::LoadingStyle::kSidePanel);
  }

  // Attached to an active side panel instance.
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  auto* attached_manager = GetNoWebviewContentsManager(instance);
  ASSERT_TRUE(attached_manager);
  auto attached_overlay_state = attached_manager->GetOverlayStateForTesting();
  ASSERT_TRUE(attached_overlay_state && attached_overlay_state->is_loading());
  EXPECT_EQ(attached_overlay_state->get_loading(),
            mojom::LoadingStyle::kSidePanel);
  EXPECT_EQ(attached_manager->GetLoadingStyleForTesting(),
            mojom::LoadingStyle::kSidePanel);

#if !BUILDFLAG(IS_ANDROID)
  // Attached to an active detached instance.
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * detached_instance,
                       OpenGlicForActiveTabAndDetach());
  auto* detached_manager = GetNoWebviewContentsManager(detached_instance);
  ASSERT_TRUE(detached_manager);
  auto detached_overlay_state = detached_manager->GetOverlayStateForTesting();
  ASSERT_TRUE(detached_overlay_state && detached_overlay_state->is_loading());
  EXPECT_EQ(detached_overlay_state->get_loading(),
            mojom::LoadingStyle::kFloating);
  EXPECT_EQ(detached_manager->GetLoadingStyleForTesting(),
            mojom::LoadingStyle::kFloating);
#endif
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       TransientErrorClearedWhenGuestBecomesResponsive) {
  GlicNoWebviewContentsManager manager(GetProfile(), &service()->enabling(),
                                       /*initially_hidden=*/false);
  manager.SetVisibility(content::Visibility::VISIBLE);
  EXPECT_EQ(manager.state(),
            GlicNoWebviewContentsManager::DisplayState::kShowingOverlay);

  // Set a transient error (e.g. generic failure or offline).
  manager.SetErrorState(mojom::ErrorPanelType::kError);
  EXPECT_EQ(manager.error_type(), mojom::ErrorPanelType::kError);
  EXPECT_EQ(manager.state(),
            GlicNoWebviewContentsManager::DisplayState::kShowingOverlay);

  // When guest becomes responsive, stale transient error must be cleared and
  // the container swaps to the guest.
  manager.OnWebClientStateChanged(mojom::WebClientState::kResponsive);
  EXPECT_EQ(manager.error_type(), std::nullopt);
  EXPECT_TRUE(manager.guest_ready().get());
  EXPECT_EQ(manager.state(),
            GlicNoWebviewContentsManager::DisplayState::kShowingGuest);
  EXPECT_EQ(manager.active_web_contents(), manager.guest_contents());
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       PolicyErrorNotClearedWhenGuestBecomesResponsive) {
  GlicNoWebviewContentsManager manager(GetProfile(), &service()->enabling(),
                                       /*initially_hidden=*/false);
  manager.SetVisibility(content::Visibility::VISIBLE);

  // Set a policy error (e.g. sign-in required).
  manager.SetErrorState(mojom::ErrorPanelType::kSignIn);
  EXPECT_EQ(manager.error_type(), mojom::ErrorPanelType::kSignIn);

  // Guest client connects in background; policy error must not be cleared.
  manager.OnWebClientStateChanged(mojom::WebClientState::kResponsive);
  EXPECT_EQ(manager.error_type(), mojom::ErrorPanelType::kSignIn);
  EXPECT_EQ(manager.state(),
            GlicNoWebviewContentsManager::DisplayState::kShowingOverlay);
  EXPECT_EQ(manager.active_web_contents(), manager.overlay_contents());
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       TransientErrorClearedOnGuestNavigation) {
  GlicNoWebviewContentsManager manager(GetProfile(), &service()->enabling(),
                                       /*initially_hidden=*/false);
  manager.SetVisibility(content::Visibility::VISIBLE);

  manager.SetErrorState(mojom::ErrorPanelType::kOffline);
  EXPECT_EQ(manager.error_type(), mojom::ErrorPanelType::kOffline);

  // Starting a new guest navigation clears the transient error and displays
  // loading.
  manager.OnGuestNavigationStarted();
  EXPECT_EQ(manager.error_type(), std::nullopt);
  auto overlay_state = manager.GetOverlayStateForTesting();
  ASSERT_TRUE(overlay_state && overlay_state->is_loading());
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewContentsManagerBrowserTest,
                       TransientErrorClearedOnGuestErrorPage) {
  GlicNoWebviewContentsManager manager(GetProfile(), &service()->enabling(),
                                       /*initially_hidden=*/false);
  manager.SetVisibility(content::Visibility::VISIBLE);

  manager.SetErrorState(mojom::ErrorPanelType::kError);
  EXPECT_EQ(manager.error_type(), mojom::ErrorPanelType::kError);

  // Guest navigates to /sorry/ CAPTCHA page; transient error is cleared to show
  // CAPTCHA.
  manager.OnGuestNavigated(GURL("https://gemini.google.com/sorry/index"),
                           /*is_api_allowed=*/false,
                           mojom::GuestPageType::kGuestError,
                           /*is_initial_commit=*/false);
  EXPECT_EQ(manager.error_type(), std::nullopt);
  EXPECT_EQ(manager.state(),
            GlicNoWebviewContentsManager::DisplayState::kShowingGuest);
  EXPECT_EQ(manager.active_web_contents(), manager.guest_contents());
}

}  // namespace glic
