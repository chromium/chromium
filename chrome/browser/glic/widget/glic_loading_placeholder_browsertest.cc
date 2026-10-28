// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>

#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/host/glic_no_webview_contents_manager.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/widget/glic_loading_placeholder_view.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/test/button_test_api.h"
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

GlicView* GetGlicViewForInstance(GlicInstanceImpl* instance) {
  if (!instance || !instance->GetActiveEmbedder()) {
    return nullptr;
  }
  return static_cast<GlicView*>(instance->GetActiveEmbedder()->GetView().get());
}

class GlicNoWebviewNativeLoadingPlaceholderDesktopBrowserTest
    : public GlicBrowserTest {
 public:
  GlicNoWebviewNativeLoadingPlaceholderDesktopBrowserTest() {
    SetGlicPagePath("/glic/test_data/page.html");
    feature_list_.InitWithFeatures(
        {features::kGlicNoWebview, features::kGlicNativeLoadingPlaceholder},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicNoWebviewNativeLoadingPlaceholderDesktopBrowserTest,
                       SidePanelShowsPlaceholderAndSwitchesToOverlayOnError) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  auto* manager = GetNoWebviewContentsManager(instance);
  ASSERT_NE(manager, nullptr);

  GlicView* glic_view = GetGlicViewForInstance(instance);
  ASSERT_NE(glic_view, nullptr);
  EXPECT_TRUE(glic_view->IsShowingLoadingPlaceholder());
  EXPECT_FALSE(glic_view->holder()->GetVisible());

  manager->OnWebClientStateChanged(mojom::WebClientState::kError);
  EXPECT_FALSE(glic_view->IsShowingLoadingPlaceholder());
  EXPECT_TRUE(glic_view->holder()->GetVisible());

  manager->ClearErrorState();
  EXPECT_TRUE(glic_view->IsShowingLoadingPlaceholder());
  EXPECT_FALSE(glic_view->holder()->GetVisible());
  ASSERT_OK(RunUntilNull([&]() { return manager->overlay_contents(); }));
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewNativeLoadingPlaceholderDesktopBrowserTest,
                       NativePlaceholderCloseButtonClosesPanel) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  GlicView* glic_view = GetGlicViewForInstance(instance);
  ASSERT_NE(glic_view, nullptr);
  ASSERT_TRUE(glic_view->IsShowingLoadingPlaceholder());
  auto* placeholder = glic_view->loading_placeholder_view_for_testing();
  ASSERT_NE(placeholder, nullptr);
  EXPECT_EQ(placeholder->mode_for_testing(),
            GlicLoadingPlaceholderView::Mode::kSidePanel);
  EXPECT_EQ(placeholder->block_count_for_testing(), 4u);
  ASSERT_NE(placeholder->close_button_for_testing(), nullptr);

  views::test::ButtonTestApi(placeholder->close_button_for_testing())
      .NotifyDefaultMouseClick();
  ASSERT_OK(WaitForGlicClose(instance));
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewNativeLoadingPlaceholderDesktopBrowserTest,
                       DetachedFloatingPanelShowsFloatingNativePlaceholder) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * detached_instance,
                       OpenGlicForActiveTabAndDetach());
  EXPECT_TRUE(detached_instance->host().IsShowingNativeLoading());
  auto* detached_manager = GetNoWebviewContentsManager(detached_instance);
  ASSERT_NE(detached_manager, nullptr);
  EXPECT_EQ(detached_manager->overlay_contents(), nullptr);

  GlicView* glic_view = GetGlicViewForInstance(detached_instance);
  ASSERT_NE(glic_view, nullptr);
  EXPECT_TRUE(glic_view->IsShowingLoadingPlaceholder());
  EXPECT_FALSE(glic_view->holder()->GetVisible());
  auto* placeholder = glic_view->loading_placeholder_view_for_testing();
  ASSERT_NE(placeholder, nullptr);
  EXPECT_EQ(placeholder->mode_for_testing(),
            GlicLoadingPlaceholderView::Mode::kFloating);
  EXPECT_EQ(placeholder->block_count_for_testing(), 3u);
}

IN_PROC_BROWSER_TEST_F(
    GlicNoWebviewNativeLoadingPlaceholderDesktopBrowserTest,
    DelayedColdLoadPreservesFocusWithoutCloseRingAndTransfersToGuest) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  auto* manager = GetNoWebviewContentsManager(instance);
  ASSERT_NE(manager, nullptr);

  ASSERT_OK(FocusGlic(instance));
  EXPECT_TRUE(instance->host().IsShowingNativeLoading());
  EXPECT_TRUE(instance->HasFocus());

  GlicView* glic_view = GetGlicViewForInstance(instance);
  ASSERT_NE(glic_view, nullptr);
  auto* placeholder = glic_view->loading_placeholder_view_for_testing();
  ASSERT_NE(placeholder, nullptr);
  ASSERT_NE(placeholder->close_button_for_testing(), nullptr);
  EXPECT_FALSE(placeholder->close_button_for_testing()->HasFocus());
  EXPECT_FALSE(views::FocusRing::Get(placeholder->close_button_for_testing())
                   ->ShouldPaintForTesting());

  // Complete the guest load to a real client without calling Focus() again;
  // focus must automatically transfer from the placeholder to the guest.
  const GURL test_client_url =
      embedded_https_test_server().GetURL("/glic/test_client/index.html");
  manager->guest_contents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(test_client_url));

  ASSERT_OK(WaitForGlicClient(instance));
  EXPECT_FALSE(instance->host().IsShowingNativeLoading());
  ASSERT_OK(RunUntilEqual([&]() { return instance->HasFocus(); }, true));
  EXPECT_EQ(true, content::EvalJs(instance->host().web_client_contents(),
                                  "document.hasFocus()"));
}

IN_PROC_BROWSER_TEST_F(GlicNoWebviewNativeLoadingPlaceholderDesktopBrowserTest,
                       EscapeHotkeyClosesSidePanelWhileNativeLoading) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_TRUE(instance->host().IsShowingNativeLoading());
  ASSERT_OK(FocusGlic(instance));

  TriggerHotkey(LocalHotkeyManager::Command::kClose);
  ASSERT_OK(WaitForGlicClose(instance));
}

IN_PROC_BROWSER_TEST_F(
    GlicNoWebviewNativeLoadingPlaceholderDesktopBrowserTest,
    MidLoadDetachAndReattachSwitchesModeWithoutPrematureMetricOrFocusRing) {
  base::HistogramTester histogram_tester;

  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_NE(tab, nullptr);
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_TRUE(instance->host().IsShowingNativeLoading());

  GlicView* side_panel_view = GetGlicViewForInstance(instance);
  ASSERT_NE(side_panel_view, nullptr);
  ASSERT_NE(side_panel_view->loading_placeholder_view_for_testing(), nullptr);
  EXPECT_EQ(side_panel_view->loading_placeholder_view_for_testing()
                ->mode_for_testing(),
            GlicLoadingPlaceholderView::Mode::kSidePanel);

  // Detach to Floating UI mid-load.
  instance->Detach(*tab);
  ASSERT_OK(RunUntilEqual([&]() { return instance->IsDetached(); }, true,
                          "Timed out waiting for detach"));
  EXPECT_TRUE(instance->host().IsShowingNativeLoading());
  ASSERT_OK(FocusGlic(instance));

  GlicView* floating_view = GetGlicViewForInstance(instance);
  ASSERT_NE(floating_view, nullptr);
  auto* floating_placeholder =
      floating_view->loading_placeholder_view_for_testing();
  ASSERT_NE(floating_placeholder, nullptr);
  EXPECT_EQ(floating_placeholder->mode_for_testing(),
            GlicLoadingPlaceholderView::Mode::kFloating);
  EXPECT_FALSE(
      views::FocusRing::Get(floating_placeholder->close_button_for_testing())
          ->ShouldPaintForTesting());
  histogram_tester.ExpectTotalCount("Glic.Overlay.DisplayDuration.Loading", 0);

  // Re-attach to Side Panel mid-load.
  instance->Attach(tab->GetHandle());
  ASSERT_OK(RunUntilEqual([&]() { return instance->IsDetached(); }, false,
                          "Timed out waiting for re-attach"));
  EXPECT_TRUE(instance->host().IsShowingNativeLoading());
  GlicView* reattached_view = GetGlicViewForInstance(instance);
  ASSERT_NE(reattached_view, nullptr);
  ASSERT_NE(reattached_view->loading_placeholder_view_for_testing(), nullptr);
  EXPECT_EQ(reattached_view->loading_placeholder_view_for_testing()
                ->mode_for_testing(),
            GlicLoadingPlaceholderView::Mode::kSidePanel);
  histogram_tester.ExpectTotalCount("Glic.Overlay.DisplayDuration.Loading", 0);

  // Closing the loading panel emits exactly one loading duration sample once
  // the 30ms embedder deactivation debounce settles.
  ASSERT_OK(CloseGlicForTabAndWait(tab));
  ASSERT_OK(RunUntilEqual(
      [&]() {
        return histogram_tester
            .GetAllSamples("Glic.Overlay.DisplayDuration.Loading")
            .size();
      },
      size_t{1}));
  histogram_tester.ExpectTotalCount("Glic.Overlay.DisplayDuration.Loading", 1);
  histogram_tester.ExpectTotalCount(
      "Glic.Overlay.DisplayDuration.LoadingAndCompleted", 0);
}

class GlicNoWebviewReadyNativeLoadingPlaceholderDesktopBrowserTest
    : public GlicBrowserTest {
 public:
  GlicNoWebviewReadyNativeLoadingPlaceholderDesktopBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kGlicNoWebview, features::kGlicNativeLoadingPlaceholder},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    GlicNoWebviewReadyNativeLoadingPlaceholderDesktopBrowserTest,
    TransitionsFromNativeLoadingToGuestHidesPlaceholder) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  GlicView* glic_view = GetGlicViewForInstance(instance);
  ASSERT_NE(glic_view, nullptr);
  EXPECT_FALSE(glic_view->IsShowingLoadingPlaceholder());
  EXPECT_TRUE(glic_view->holder()->GetVisible());
}

class GlicNoWebviewNoWebUiLoaderDesktopBrowserTest : public GlicBrowserTest {
 public:
  GlicNoWebviewNoWebUiLoaderDesktopBrowserTest() {
    SetGlicPagePath("/glic/test_data/page.html");
    feature_list_.InitWithFeatures(
        {features::kGlicNoWebview, features::kGlicNativeLoadingPlaceholder,
         features::kGlicNoWebUiLoader},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicNoWebviewNoWebUiLoaderDesktopBrowserTest,
                       NoWebUiLoaderSuppressesNativePlaceholderOnView) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());

  GlicView* glic_view = GetGlicViewForInstance(instance);
  ASSERT_NE(glic_view, nullptr);
  EXPECT_FALSE(glic_view->IsShowingLoadingPlaceholder());
  EXPECT_TRUE(glic_view->holder()->GetVisible());
}

class GlicLegacyWebviewWarmedNativeLoadingDesktopBrowserTest
    : public GlicBrowserTest {
 public:
  GlicLegacyWebviewWarmedNativeLoadingDesktopBrowserTest() {
    SetGlicPagePath("/hung");
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicNativeLoadingPlaceholder, {}},
         {features::kGlicWebClientLoadTimes,
          {{features::kGlicPreLoadingTimeMs.name, "0"},
           {features::kGlicMinLoadingTimeMs.name, "10"},
           {features::kGlicMaxLoadingTimeMs.name, "30000"}}}},
        {features::kGlicNoWebview});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicLegacyWebviewWarmedNativeLoadingDesktopBrowserTest,
                       HiddenReopenShowsPlaceholderOnViewAndHidesOnReady) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  PreventBlankDeletionOnClose(instance);
  ASSERT_OK(CloseGlicForTabAndWait(GetTabListInterface()->GetActiveTab()));
  // While hidden, `/hung` never creates a web client, so `chrome://glic` stays
  // in `kFinishLoading` rather than advancing to `kWarmed`.
  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kFinishLoading));

  ASSERT_OK_AND_ASSIGN(instance, OpenGlicForActiveTab());
  GlicView* glic_view = GetGlicViewForInstance(instance);
  ASSERT_NE(glic_view, nullptr);
  EXPECT_TRUE(glic_view->IsShowingLoadingPlaceholder());
  EXPECT_FALSE(glic_view->holder()->GetVisible());

  const GURL test_client_url =
      embedded_https_test_server().GetURL("/glic/test_client/index.html");
  ASSERT_TRUE(content::ExecJs(
      instance->host().webui_contents(),
      content::JsReplace(
          "import('//resources/js/load_time_data.js').then(({loadTimeData}) => "
          "{ loadTimeData.overrideValues({glicGuestURL: $1}); "
          "window.appRouter.reload(); });",
          test_client_url.spec())));

  ASSERT_OK(WaitForGlicClient(instance));
  EXPECT_FALSE(glic_view->IsShowingLoadingPlaceholder());
  EXPECT_TRUE(glic_view->holder()->GetVisible());
}

class GlicLegacyWebviewTimeoutDesktopBrowserTest : public GlicBrowserTest {
 public:
  GlicLegacyWebviewTimeoutDesktopBrowserTest() {
    SetGlicPagePath("/glic/test_data/page.html");
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicNativeLoadingPlaceholder, {}},
         {features::kGlicWebClientLoadTimes,
          {{features::kGlicPreLoadingTimeMs.name, "0"},
           {features::kGlicMinLoadingTimeMs.name, "10"},
           {features::kGlicMaxLoadingTimeMs.name, "100"}}}},
        {features::kGlicNoWebview});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicLegacyWebviewTimeoutDesktopBrowserTest,
                       TimeoutTransitionsViewFromPlaceholderToErrorPanel) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForWebUiState(mojom::WebUiState::kError));

  GlicView* glic_view = GetGlicViewForInstance(instance);
  ASSERT_NE(glic_view, nullptr);
  EXPECT_FALSE(glic_view->IsShowingLoadingPlaceholder());
  EXPECT_TRUE(glic_view->holder()->GetVisible());
}

}  // namespace
}  // namespace glic
