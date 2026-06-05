// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/background/glic/glic_background_mode_manager.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/lens/lens_preselection_bubble.h"
#include "chrome/browser/ui/tabs/split_tab_menu_model.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/test/split_view_interactive_test_mixin.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/interaction/element_tracker_views.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/shell.h"
#include "chromeos/constants/chromeos_features.h"
#endif

namespace glic {

namespace {
auto GetPointWithOffset(int x, int y) {
  return base::BindLambdaForTesting([x, y](ui::TrackedElement* el) {
    auto* view = views::test::InteractiveViewsTestApi::AsView<views::View>(el);
    return view->GetBoundsInScreen().origin() + gfx::Vector2d(x, y);
  });
}

views::View* GetOverlayView(content::WebContents* tab_contents) {
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab_contents);
  if (!controller) {
    return nullptr;
  }
  return controller->GetOverlayViewForTesting();
}

views::View* GetOverlayView(Browser* browser, int index) {
  auto* tab_contents = browser->tab_strip_model()->GetWebContentsAt(index);
  if (!tab_contents) {
    return nullptr;
  }
  return GetOverlayView(tab_contents);
}

class ActiveTabObserver : public TabStripModelObserver,
                          public ui::test::StateObserver<bool> {
 public:
  explicit ActiveTabObserver(TabStripModel* tab_strip_model)
      : tab_strip_model_(tab_strip_model) {
    tab_strip_model_->AddObserver(this);
  }

  ~ActiveTabObserver() override { tab_strip_model_->RemoveObserver(this); }

  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (selection.active_tab_changed() ||
        change.type() == TabStripModelChange::kMoved) {
      OnStateObserverStateChanged(true);
    }
  }

 private:
  raw_ptr<TabStripModel> tab_strip_model_;
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ActiveTabObserver, kActiveTabChanged);

class SelectionOverlayInteractiveTest : public test::InteractiveGlicTest {
 public:
  SelectionOverlayInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {::features::kGlicCaptureRegion,
         // Only supports multi-instance mode for now.
         ::features::kGlicMultiInstance},
        {});
  }
  ~SelectionOverlayInteractiveTest() override = default;

  auto GetOverlayVisibilityAt(int index) {
    return base::BindLambdaForTesting([this, index]() {
      auto* overlay_view = GetOverlayView(browser(), index);
      return overlay_view && overlay_view->GetVisible();
    });
  }

  auto CheckOverlayBoundsMatchContents(int index) {
    return CheckResult(
        base::BindLambdaForTesting([this, index]() {
          auto* tab_contents =
              browser()->tab_strip_model()->GetWebContentsAt(index);
          auto* overlay_view = GetOverlayView(tab_contents);
          auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
          auto* contents_container =
              browser_view->GetContentsContainerViewFor(tab_contents);
          if (!overlay_view || !contents_container) {
            return false;
          }
          auto* contents_view = contents_container->contents_view();
          return contents_view && overlay_view->GetBoundsInScreen() ==
                                      contents_view->GetBoundsInScreen();
        }),
        true);
  }

  GURL GetEmptyDocURL() const {
    return embedded_test_server()->GetURL("/empty.html");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class SelectionOverlayInteractiveTestWithPolyline
    : public SelectionOverlayInteractiveTest {
 public:
  SelectionOverlayInteractiveTestWithPolyline() {
    feature_list_.InitAndEnableFeature(features::kGlicRegionSelectionLine);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class SelectionOverlayInteractiveTestWithSplitView
    : public SplitViewInteractiveTestMixin<SelectionOverlayInteractiveTest> {
 public:
  SelectionOverlayInteractiveTestWithSplitView() = default;
  ~SelectionOverlayInteractiveTestWithSplitView() override = default;

  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    std::vector<base::test::FeatureRefAndParams> features;
    features.push_back({::features::kInitialWebUI, {}});
    features.push_back({::features::kWebUIReloadButton, {}});
    features.push_back({::features::kWebUISplitTabsButton, {}});
    return features;
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest, SmokeTest) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      OpenGlic(),
      // captureRegionBtn of the test client calls `captureRegion()` on the glic
      // API.
      ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      // glic-selection-overlay is expected to be displayed.
      WaitForElementVisible(kOverlayWebContentsId, {"selection-overlay-app",
                                                    "glic-selection-overlay"}),
      WaitForShow(kLensPreselectionBubbleElementId));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       OverlayHiddenOnBackgroundedTab) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      Do([this]() { chrome::AddTabAt(browser(), GetEmptyDocURL(), -1, true); }),
      Do([this]() { browser()->tab_strip_model()->ActivateTabAt(0); }),
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      CheckResult(GetOverlayVisibilityAt(0), true),
      Do([this]() { browser()->tab_strip_model()->ActivateTabAt(1); }),
      CheckResult(GetOverlayVisibilityAt(0), false),
      Do([this]() { browser()->tab_strip_model()->ActivateTabAt(0); }),
      CheckResult(GetOverlayVisibilityAt(0), true));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       OverlayAttachToCorrectContainerInSplitView) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      Do([this]() {
        chrome::AddTabAt(browser(), GetEmptyDocURL(), -1, true);
        browser()->tab_strip_model()->AddToNewSplit(
            {0}, split_tabs::SplitTabVisualData(),
            split_tabs::SplitTabCreatedSource::kToolbarButton);
        int last_index = browser()->tab_strip_model()->count() - 1;
        browser()->tab_strip_model()->ActivateTabAt(last_index);
        TrackGlicInstanceWithTabIndex(last_index);
      }),
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      // Verify that the overlay is in the correct container.
      CheckResult(
          [this]() {
            auto* active_contents =
                browser()->tab_strip_model()->GetActiveWebContents();
            if (!active_contents) {
              return false;
            }
            EXPECT_EQ(active_contents->GetURL(), GetEmptyDocURL());
            return GetOverlayView(active_contents) != nullptr;
          },
          true));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest, MultiRegionSelection) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  const DeepQuery kOverlayApp = {"selection-overlay-app"};
  const DeepQuery kRenderer = {"selection-overlay-app",
                               "glic-selection-overlay",
                               "post-selection-renderer"};
  const DeepQuery kStaticRegion = {"selection-overlay-app",
                                   "glic-selection-overlay",
                                   "post-selection-renderer", ".static-region"};

  RunTestSequence(
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, kOverlayApp,
                        "el => el.screenshot_ !== null"),
      // 0. Verify multi-region selection is enabled.
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.multiRegionSelectionEnabled === true"),

      // 1. Draw first region (drag from 50,50 to 150,150).
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(50, 50)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(150, 150)),
      // Verify first region is active.
      WaitForJsResultAt(
          kOverlayWebContentsId, kRenderer,
          "el => el.hasSelection() && el.selectedRegions.length === 1"),

      // 2. Draw second region (starting far from the first one).
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(300, 300)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(400, 400)),
      // Verify the newly added region which the mouse is on is the active
      // region.
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.hasSelection()"),
      // Verify that the first region is now a static region and that there are
      // 2 total regions.
      WaitForElementVisible(kOverlayWebContentsId, kStaticRegion),
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.staticRegions.length === 1 && "
                        "el.selectedRegions.length === 2"),

      // 3. Move mouse back over the first region (center is 100, 100).
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(100, 100)),

      // Verify that hovering upon the first created region is now the active
      // region, and the second region becomes static.
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.hasSelection()"),
      WaitForElementVisible(kOverlayWebContentsId, kStaticRegion),
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.staticRegions.length === 1 && "
                        "el.selectedRegions.length === 2")

  );
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest, DeleteActiveRegion) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  const DeepQuery kOverlayApp = {"selection-overlay-app"};
  const DeepQuery kRenderer = {"selection-overlay-app",
                               "glic-selection-overlay",
                               "post-selection-renderer"};
  const DeepQuery kCloseButton = {"selection-overlay-app",
                                  "glic-selection-overlay",
                                  "post-selection-renderer", ".close-button"};
  const DeepQuery kStaticRegion = {"selection-overlay-app",
                                   "glic-selection-overlay",
                                   "post-selection-renderer", ".static-region"};

  RunTestSequence(
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, kOverlayApp,
                        "el => el.screenshot_ !== null"),
      // 0. Verify multi-region selection is enabled.
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.multiRegionSelectionEnabled === true"),

      // 1. Draw first region (drag from 50,50 to 150,150).
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(50, 50)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(150, 150)),
      // Verify first region is active.
      WaitForJsResultAt(
          kOverlayWebContentsId, kRenderer,
          "el => el.hasSelection() && el.selectedRegions.length === 1"),

      // 2. Draw second region (starting far from the first one).
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(300, 300)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(400, 400)),
      // Verify the newly added region which the mouse is on is the active
      // region.
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.hasSelection()"),
      // Verify that the first region is now a static region and that there are
      // 2 total regions.
      WaitForElementVisible(kOverlayWebContentsId, kStaticRegion),
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.staticRegions.length === 1 && "
                        "el.selectedRegions.length === 2"),

      // 3. Move mouse back over the first region (center is 100, 100).
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(100, 100)),

      // Verify that hovering upon the first created region is now the active
      // region, and the second region becomes static.
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.hasSelection()"),
      WaitForElementVisible(kOverlayWebContentsId, kStaticRegion),
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.staticRegions.length === 1 && "
                        "el.selectedRegions.length === 2"),
      // Click the close button for closing first region.
      ClickElement(kOverlayWebContentsId, kCloseButton),
      // Now move mouse to the first region.
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(350, 350)),
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.hasSelection() && "
                        "el.selectedRegions.length === 1"));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       DeleteLastRegionClosesUI) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);
  const DeepQuery kOverlayApp = {"selection-overlay-app"};
  const DeepQuery kRenderer = {"selection-overlay-app",
                               "glic-selection-overlay",
                               "post-selection-renderer"};
  const DeepQuery kCloseButton = {"selection-overlay-app",
                                  "glic-selection-overlay",
                                  "post-selection-renderer", ".close-button"};

  RunTestSequence(
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, kOverlayApp,
                        "el => el.screenshot_ !== null"),

      // 1. Draw a region (drag from 50,50 to 150,150).
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(50, 50)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(150, 150)),
      // Verify this region is active.
      WaitForJsResultAt(
          kOverlayWebContentsId, kRenderer,
          "el => el.hasSelection() && el.selectedRegions.length === 1"),

      // 2. Move mouse back over the close button.
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(100, 100)),

      // 3. Move mouse directly to the close button.
      MoveMouseTo(kOverlayWebContentsId, kCloseButton),

      // 4. Click the mouse. This avoids failing if the element disappears
      // immediately.
      ClickMouse(),

      // 5. Verify that the overlay is dismissed.
      WaitForHide(OverlayBaseController::kOverlayId));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       OverlayDismissedOnNavigation) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      InstrumentTab(kActiveTab), OpenGlic(),
      ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      // glic-selection-overlay is expected to be displayed.
      WaitForElementVisible(kOverlayWebContentsId, {"selection-overlay-app",
                                                    "glic-selection-overlay"}),
      NavigateWebContents(kActiveTab,
                          embedded_test_server()->GetURL("/empty.html")),
      WaitForHide(OverlayBaseController::kOverlayId));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest, OverlayDismissedOnEsc) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      // glic-selection-overlay is expected to be displayed.
      WaitForElementVisible(kOverlayWebContentsId, {"selection-overlay-app",
                                                    "glic-selection-overlay"}),
      SendKeyPress(OverlayBaseController::kOverlayId, ui::VKEY_ESCAPE),
      WaitForHide(OverlayBaseController::kOverlayId));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       EscDismissesOverlayFirst) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      // glic-selection-overlay is expected to be displayed.
      WaitForElementVisible(kOverlayWebContentsId, {"selection-overlay-app",
                                                    "glic-selection-overlay"}),
      FocusElement(test::kGlicContentsElementId),
      SendKeyPress(test::kGlicContentsElementId, ui::VKEY_ESCAPE),
      WaitForHide(OverlayBaseController::kOverlayId),
      EnsurePresent(test::kGlicHostElementId),
      SendKeyPress(test::kGlicContentsElementId, ui::VKEY_ESCAPE),
      WaitForHide(test::kGlicContentsElementId));
}

// When glic is in floating mode and when only the first tab has context shared,
// on a second tab, pressing esc in the floaty dismisses the floaty and
// therefore the selection overlay in the first tab.
//
// Fails on Wayland platforms and flaky on Mac.
#if BUILDFLAG(SUPPORTS_OZONE_WAYLAND) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_EscDismissesFloatyOnSecondTab \
  DISABLED_EscDismissesFloatyOnSecondTab
#else
#define MAYBE_EscDismissesFloatyOnSecondTab EscDismissesFloatyOnSecondTab
#endif
IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       MAYBE_EscDismissesFloatyOnSecondTab) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAboutBlankTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kEmptyTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  TrackFloatingGlicInstance();
  RunTestSequence(
      InstrumentTab(kAboutBlankTab),
      OpenGlicFloatingWindow(GlicInstrumentMode::kHostAndContents,
                             /*conversation_id=*/std::nullopt),
      ClickMockGlicElement({"#pinFocusedTab"}),
      ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      WaitForElementVisible(kOverlayWebContentsId, {"selection-overlay-app",
                                                    "glic-selection-overlay"}),
      AddInstrumentedTab(kEmptyTab,
                         embedded_test_server()->GetURL("/empty.html")),
      FocusElement(kEmptyTab),
      InAnyContext(ActivateSurface(test::kGlicHostElementId)),
      InAnyContext(SendKeyPress(test::kGlicHostElementId, ui::VKEY_ESCAPE)),
      InAnyContext(WaitForHide(test::kGlicHostElementId)),
      WaitForHide(OverlayBaseController::kOverlayId));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       FocusBackToGlicAfterSelection) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kGlicHasFocus);

  RunTestSequence(
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      WaitForElementVisible(kOverlayWebContentsId, {"selection-overlay-app",
                                                    "glic-selection-overlay"}),
      CheckResult(
          [this]() {
            auto* instance = GetGlicInstanceImpl();
            return instance && instance->HasFocus();
          },
          false),
      PollState(kGlicHasFocus,
                [this]() {
                  auto* instance = GetGlicInstanceImpl();
                  return instance && instance->HasFocus();
                }),
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(10, 10)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(100, 100)),
      WaitForState(kGlicHasFocus, true));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       NoFocusToGlicAfterKeyboardSelection) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      WaitForElementVisible(kOverlayWebContentsId, {"selection-overlay-app",
                                                    "glic-selection-overlay"}),
      CheckResult(
          [this]() {
            auto* instance = GetGlicInstanceImpl();
            return instance && instance->HasFocus();
          },
          false),
      // Trigger the selection from the WebUI mimicking keyboard slider change.
      InAnyContext(WithElement(
          kOverlayWebContentsId,
          [](ui::TrackedElement* el) {
            content::WebContents* overlay_contents =
                InteractiveBrowserTest::AsInstrumentedWebContents(el)
                    ->web_contents();

            static constexpr std::string_view kJs =
                "(async () => {"
                "  const { RegionSource } = await import("
                "      '/lens/selection_overlay_base_handler.js');"
                "  const app = document.querySelector('selection-overlay-app');"
                "  const renderer = app.shadowRoot"
                "      .querySelector('glic-selection-overlay')"
                "      .shadowRoot.querySelector('post-selection-renderer');"
                "  renderer.baseHandler.adjustRegionSelected("
                "      {x: 0.1, y: 0.1, width: 0.2, height: 0.2},"
                "      RegionSource.KEYBOARD);"
                "  return true;"
                "})();";

            ASSERT_TRUE(content::ExecJs(overlay_contents, kJs));
          })),
      // Wait deterministically for the selection to be processed and rendered.
      WaitForJsResultAt(kOverlayWebContentsId,
                        {"selection-overlay-app", "glic-selection-overlay",
                         "post-selection-renderer"},
                        "el => el.selectedRegions.length === 1"),
      // Verify that glic still does not have focus.
      CheckResult(
          [this]() {
            auto* instance = GetGlicInstanceImpl();
            return instance && instance->HasFocus();
          },
          false));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       BubbleHidesAfterSelection) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      WaitForShow(kLensPreselectionBubbleElementId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(50, 50)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(150, 150)),
      WaitForHide(kLensPreselectionBubbleElementId));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       BubbleReshowOnTabSwitches) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      Do([this]() { chrome::AddTabAt(browser(), GetEmptyDocURL(), -1, true); }),
      Do([this]() { browser()->tab_strip_model()->ActivateTabAt(0); }),
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      WaitForShow(kLensPreselectionBubbleElementId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      // 1. Switch to Tab B (index 1) and verify bubble is hidden
      Do([this]() { browser()->tab_strip_model()->ActivateTabAt(1); }),
      WaitForHide(kLensPreselectionBubbleElementId),
      // 2. Switch back to Tab A (index 0) and verify bubble is reshown
      Do([this]() { browser()->tab_strip_model()->ActivateTabAt(0); }),
      WaitForShow(kLensPreselectionBubbleElementId));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       BubbleHiddenOnTabSwitchesAfterSelection) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kHasSelection);

  RunTestSequence(
      Do([this]() { chrome::AddTabAt(browser(), GetEmptyDocURL(), -1, true); }),
      Do([this]() { browser()->tab_strip_model()->ActivateTabAt(0); }),
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      WaitForShow(kLensPreselectionBubbleElementId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      // `WaitForHide(kLensPreselectionBubbleElementId)` will satisfy as soon as
      // the mouse click is registered. Makes sure the browser receives the
      // selected region before switching tabs. Otherwise the test is flaky on
      // slow bots such as MSAN / ASAN.
      PollState(kHasSelection,
                [this]() {
                  return SelectionOverlayController::FromTabWebContents(
                             browser()->tab_strip_model()->GetWebContentsAt(0))
                             ->GetSelectedRegionCount() == 1;
                }),
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(50, 50)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(150, 150)),
      WaitForHide(kLensPreselectionBubbleElementId),
      WaitForState(kHasSelection, true),
      Do([this]() { browser()->tab_strip_model()->ActivateTabAt(1); }),
      Do([this]() { browser()->tab_strip_model()->ActivateTabAt(0); }),
      EnsureNotPresent(kLensPreselectionBubbleElementId));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest, BubbleUIColor) {
  RunTestSequence(
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      WaitForShow(kLensPreselectionBubbleElementId),
      WaitForShow(lens::LensPreselectionBubble::kCancelButtonElementId),
      CheckView(kLensPreselectionBubbleElementId,
                [](views::View* view) {
                  auto* bubble =
                      static_cast<views::BubbleDialogDelegateView*>(view);
                  return bubble->background_color() ==
                         kColorGlicSelectionOverlayToast;
                }),
      CheckView(lens::LensPreselectionBubble::kCancelButtonElementId,
                [](views::View* view) {
                  auto* button = static_cast<views::MdTextButton*>(view);
                  return button->GetBgColorIdOverride() ==
                         kColorGlicSelectionOverlayToast;
                }),
      CheckView(lens::LensPreselectionBubble::kCancelButtonElementId,
                [](views::View* view) {
                  auto* button = static_cast<views::MdTextButton*>(view);
                  return button->GetCurrentTextColor() ==
                         button->GetColorProvider()->GetColor(
                             kColorGlicSelectionOverlayToastCancelButton);
                }));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest, BubbleUICancelClicked) {
  RunTestSequence(
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      WaitForShow(kLensPreselectionBubbleElementId),
      WaitForShow(lens::LensPreselectionBubble::kCancelButtonElementId),
      PressButton(lens::LensPreselectionBubble::kCancelButtonElementId),
      WaitForHide(OverlayBaseController::kOverlayId));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest, BubbleUIIcon) {
  RunTestSequence(
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      WaitForShow(kLensPreselectionBubbleElementId),
      CheckView(kLensPreselectionBubbleElementId, [](views::View* view) {
        auto* bubble = static_cast<views::BubbleDialogDelegateView*>(view);
        for (views::View* child : bubble->children()) {
          auto* image_view = views::AsViewClass<views::ImageView>(child);
          if (image_view) {
            const ui::ImageModel& model = image_view->GetImageModel();
            if (model.IsVectorIcon()) {
              return model.GetVectorIcon().vector_icon() ==
                     &(features::IsRoundedIconsEnabled()
                           ? vector_icons::kCropFreeIcon
                           : vector_icons::kCropFreeOldIcon);
            }
          }
        }
        return false;
      }));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       SelectionDisabledWithTaskActingOnTab) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(
      ui::test::PollingStateObserver<
          std::optional<actor::mojom::ActionResultCode>>,
      kAddTabResult);
  std::optional<actor::mojom::ActionResultCode> add_tab_result;

  RunTestSequence(
      OpenGlic(),
      // Start a task on the current tab.
      Do([this, &add_tab_result]() {
        auto* actor_service =
            actor::ActorKeyedService::Get(browser()->profile());
        ASSERT_TRUE(actor_service);
        actor::TaskId task_id = actor_service->CreateTask(
            actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
        actor::ActorTask* task = actor_service->GetTask(task_id);
        ASSERT_TRUE(task);
        content::WebContents* web_contents =
            browser()->tab_strip_model()->GetActiveWebContents();
        tabs::TabInterface* tab =
            tabs::TabInterface::GetFromContents(web_contents);
        ASSERT_TRUE(tab);
        task->AddTab(
            tab->GetHandle(), /*stop_task_on_detach=*/true,
            base::BindLambdaForTesting(
                [&add_tab_result](actor::mojom::ActionResultPtr result) {
                  add_tab_result = result->code;
                }));
      }),
      PollState(kAddTabResult, [&add_tab_result]() { return add_tab_result; }),
      WaitForState(kAddTabResult,
                   std::make_optional(actor::mojom::ActionResultCode::kOk)),
      ClickMockGlicElement({"#captureRegionBtn"}), Wait(base::Seconds(1)),
      EnsureNotPresent(OverlayBaseController::kOverlayId));
}

class SelectionOverlayHotkeyInteractiveTest
    : public SelectionOverlayInteractiveTest {
 public:
  SelectionOverlayHotkeyInteractiveTest() {
    scoped_feature_list_.InitWithFeatures(
        {::features::kGlicDefaultTabContextSetting}, {});
  }
  ~SelectionOverlayHotkeyInteractiveTest() override = default;

  void SetUpOnMainThread() override {
    SelectionOverlayInteractiveTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                                 true);
  }

  void TearDownOnMainThread() override {
    g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                                 false);
    SelectionOverlayInteractiveTest::TearDownOnMainThread();
  }

  // Only call this on the browser UI thread.
  static bool IsHotkeySupported() {
    // ChromeOS uses ash's accelerator controller rather than global accelerator
    // listener.
#if BUILDFLAG(IS_CHROMEOS)
    if (ash::Shell::HasInstance()) {
      return ash::Shell::Get()->accelerator_controller() != nullptr;
    }
    return false;
#else
    auto* const global_shortcut_listener =
        ui::GlobalAcceleratorListener::GetInstance();
    return global_shortcut_listener != nullptr &&
           !global_shortcut_listener->IsRegistrationHandledExternally();
#endif
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Flaky on linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_HotkeyTogglesSelectionOverlayOnOff \
  DISABLED_HotkeyTogglesSelectionOverlayOnOff
#else
#define MAYBE_HotkeyTogglesSelectionOverlayOnOff \
  HotkeyTogglesSelectionOverlayOnOff
#endif
IN_PROC_BROWSER_TEST_F(SelectionOverlayHotkeyInteractiveTest,
                       MAYBE_HotkeyTogglesSelectionOverlayOnOff) {
  if (!IsHotkeySupported()) {
    GTEST_SKIP() << "Hotkey not supported on the platform";
  }

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      OpenGlic(),
      // SimulateAcceleratorPress() did not work.
      Do([this]() {
        content::WebContents* web_contents =
            browser()->tab_strip_model()->GetActiveWebContents();
        tabs::TabInterface* tab =
            tabs::TabInterface::GetFromContents(web_contents);
        GlicKeyedService* glic_keyed_service =
            GlicKeyedService::Get(browser()->profile());
        GlicInvokeOptions options(
            glic::mojom::InvocationSource::kCaptureRegionHotkey);
        options.wait_for_panel_open = true;
        options.target = Target(*tab);
        glic_keyed_service->Invoke(std::move(options));
      }),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      WaitForElementVisible(kOverlayWebContentsId, {"selection-overlay-app",
                                                    "glic-selection-overlay"}),
      Do([this]() {
        content::WebContents* web_contents =
            browser()->tab_strip_model()->GetActiveWebContents();
        SelectionOverlayController::FromTabWebContents(web_contents)->Close();
      }),
      WaitForHide(OverlayBaseController::kOverlayId));
}

IN_PROC_BROWSER_TEST_F(
    SelectionOverlayHotkeyInteractiveTest,
    CannotRequestCaptureRegionViaHotkeyWithoutTabContextPermission) {
  if (!IsHotkeySupported()) {
    GTEST_SKIP() << "Hotkey not supported on the platform";
  }
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  RunTestSequence(
      InstrumentTab(kActiveTab),
      // chrome://settings/'s context cannot be shared.
      NavigateWebContents(kActiveTab, GURL(chrome::kChromeUISettingsURL)),
      OpenGlic(),
      // SimulateAcceleratorPress() did not work.
      Do([this]() {
        content::WebContents* web_contents =
            browser()->tab_strip_model()->GetActiveWebContents();
        tabs::TabInterface* tab =
            tabs::TabInterface::GetFromContents(web_contents);
        GlicKeyedService* glic_keyed_service =
            GlicKeyedService::Get(browser()->profile());
        GlicInvokeOptions options(
            glic::mojom::InvocationSource::kCaptureRegionHotkey);
        options.wait_for_panel_open = true;
        options.target = Target(*tab);
        glic_keyed_service->Invoke(std::move(options));
      }),
      Wait(base::Seconds(1)),
      EnsureNotPresent(OverlayBaseController::kOverlayId));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTestWithPolyline,
                       SelectionPolylineWebUI) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  const DeepQuery kRenderer = {"selection-overlay-app",
                               "glic-selection-overlay",
                               "post-selection-renderer"};

  RunTestSequence(
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),

      // Trigger the polyline selection from the WebUI.
      InAnyContext(WithElement(
          kOverlayWebContentsId,
          [](ui::TrackedElement* el) {
            content::WebContents* overlay_contents =
                InteractiveBrowserTest::AsInstrumentedWebContents(el)
                    ->web_contents();

            static constexpr std::string_view kJs =
                "(async () => {"
                "  const { RegionSource } = await import("
                "      '/lens/selection_overlay_base_handler.js');"
                "  const app = document.querySelector('selection-overlay-app');"
                "  const renderer = app.shadowRoot"
                "      .querySelector('glic-selection-overlay')"
                "      .shadowRoot.querySelector('post-selection-renderer');"
                "  renderer.baseHandler.adjustPolylineSelected("
                "      [{x: 0.1, y: 0.1}, {x: 0.2, y: 0.2}, {x: 0.3, y: 0.1}],"
                "      RegionSource.SELECTION);"
                "  return true;"
                "})();";

            ASSERT_TRUE(content::ExecJs(overlay_contents, kJs));
          })),

      // Verify that the region contains the polyline points.
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        R"(el => {
                          const p = el.selectedRegions[0].polyline;
                          return el.selectedRegions.length === 1 &&
                                 p.length === 3 &&
                                 Math.abs(p[0].x - 0.1) < 0.001 &&
                                 Math.abs(p[0].y - 0.1) < 0.001 &&
                                 Math.abs(p[1].x - 0.2) < 0.001 &&
                                 Math.abs(p[1].y - 0.2) < 0.001 &&
                                 Math.abs(p[2].x - 0.3) < 0.001 &&
                                 Math.abs(p[2].y - 0.1) < 0.001;
                        })"));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTestWithSplitView,
                       OverlayRemainsOnFocusChangeInSplitView) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      Do([this]() { chrome::AddTabAt(browser(), GetEmptyDocURL(), -1, true); }),
      EnterSplitView(/*active_tab=*/0, /*other_tab=*/1), Do([this]() {
        browser()->tab_strip_model()->ActivateTabAt(0);
        TrackGlicInstanceWithTabIndex(0);
      }),
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      CheckResult(GetOverlayVisibilityAt(0), true),
      ObserveState(kActiveTabChanged, browser()->tab_strip_model()),
      FocusInactiveTabInSplit(), WaitForState(kActiveTabChanged, true),
      CheckResult(GetOverlayVisibilityAt(0), true));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTestWithSplitView,
                       OverlaySticksToTabOnReverse) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      Do([this]() { chrome::AddTabAt(browser(), GetEmptyDocURL(), -1, true); }),
      EnterSplitView(/*active_tab=*/0, /*other_tab=*/1), Do([this]() {
        browser()->tab_strip_model()->ActivateTabAt(0);
        TrackGlicInstanceWithTabIndex(0);
      }),
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      CheckResult(GetOverlayVisibilityAt(0), true),
      ObserveState(kActiveTabChanged, browser()->tab_strip_model()),
      PressButton(kToolbarSplitTabsToolbarButtonElementId),
      WaitForShow(SplitTabMenuModel::kReversePositionMenuItem),
      SelectMenuItem(SplitTabMenuModel::kReversePositionMenuItem),
      WaitForState(kActiveTabChanged, true),
      CheckResult(GetOverlayVisibilityAt(1), true),
      CheckResult(GetOverlayVisibilityAt(0), false));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTestWithSplitView,
                       OverlaySticksToTabOnSplitAndConfined) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      CheckResult(GetOverlayVisibilityAt(0), true),
      Do([this]() { chrome::AddTabAt(browser(), GetEmptyDocURL(), -1, true); }),
      EnterSplitView(/*active_tab=*/0, /*other_tab=*/1),
      CheckResult(GetOverlayVisibilityAt(0), true),
      CheckOverlayBoundsMatchContents(0));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTestWithSplitView,
                       OverlayHiddenOnSwapOutAndReshowsOnFocus) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      Do([this]() {
        chrome::AddTabAt(browser(), GetEmptyDocURL(), -1, true);
        chrome::AddTabAt(browser(), GetEmptyDocURL(), -1, true);
      }),
      // SplitView(tab0|tab1), tab2. Tab0 is the focus.
      EnterSplitView(/*active_tab=*/0, /*other_tab=*/1), Do([this]() {
        browser()->tab_strip_model()->ActivateTabAt(0);
        TrackGlicInstanceWithTabIndex(0);
      }),
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      CheckResult(GetOverlayVisibilityAt(0), true),
      ObserveState(kActiveTabChanged, browser()->tab_strip_model()),
      // SplitView(tab2|tab1), tab0. Tab2 is the focus.
      Do([this]() {
        auto* tab_0 = browser()->tab_strip_model()->GetTabAtIndex(0);
        browser()->tab_strip_model()->UpdateTabInSplit(
            tab_0, 2, TabStripModel::SplitUpdateType::kSwap);
      }),
      WaitForState(kActiveTabChanged, true),
      CheckResult(GetOverlayVisibilityAt(2), false),
      Do([this]() { browser()->tab_strip_model()->ActivateTabAt(2); }),
      CheckResult(GetOverlayVisibilityAt(2), true),
      CheckOverlayBoundsMatchContents(2));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTestWithSplitView,
                       OverlaySticksToTabAOnSwapSibling) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      Do([this]() {
        chrome::AddTabAt(browser(), GetEmptyDocURL(), -1, true);
        chrome::AddTabAt(browser(), GetEmptyDocURL(), -1, true);
      }),
      // SplitView(tab0|tab1), tab2. Tab0 is the focus.
      EnterSplitView(/*active_tab=*/0, /*other_tab=*/1), Do([this]() {
        browser()->tab_strip_model()->ActivateTabAt(0);
        TrackGlicInstanceWithTabIndex(0);
      }),
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      CheckResult(GetOverlayVisibilityAt(0), true),
      // SplitView(tab0|tab2), tab1. Tab0 is the focus.
      Do([this]() {
        auto* tab_1 = browser()->tab_strip_model()->GetTabAtIndex(1);
        browser()->tab_strip_model()->UpdateTabInSplit(
            tab_1, 2, TabStripModel::SplitUpdateType::kSwap);
      }),
      CheckResult(GetOverlayVisibilityAt(0), true),
      CheckOverlayBoundsMatchContents(0));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTestWithSplitView,
                       OverlayGoneWhenTabClosed) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      Do([this]() { chrome::AddTabAt(browser(), GetEmptyDocURL(), -1, true); }),
      EnterSplitView(/*active_tab=*/0, /*other_tab=*/1), Do([this]() {
        browser()->tab_strip_model()->ActivateTabAt(0);
        TrackGlicInstanceWithTabIndex(0);
      }),
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      CheckResult(GetOverlayVisibilityAt(0), true),
      // Close tab 0 (in focus) and wait for focus change.
      ObserveState(kActiveTabChanged, browser()->tab_strip_model()),
      PressButton(kToolbarSplitTabsToolbarButtonElementId),
      WaitForShow(SplitTabMenuModel::kCloseStartTabMenuItem),
      SelectMenuItem(SplitTabMenuModel::kCloseStartTabMenuItem),
      WaitForState(kActiveTabChanged, true),
      CheckResult(GetOverlayVisibilityAt(0), false));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTestWithSplitView,
                       OverlayRemainsAndExpandsWhenSiblingClosed) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  RunTestSequence(
      Do([this]() { chrome::AddTabAt(browser(), GetEmptyDocURL(), -1, true); }),
      EnterSplitView(/*active_tab=*/0, /*other_tab=*/1), Do([this]() {
        browser()->tab_strip_model()->ActivateTabAt(0);
        TrackGlicInstanceWithTabIndex(0);
      }),
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      CheckResult(GetOverlayVisibilityAt(0), true),
      // Close tab 1 (not in focus).
      PressButton(kToolbarSplitTabsToolbarButtonElementId),
      WaitForShow(SplitTabMenuModel::kCloseEndTabMenuItem),
      SelectMenuItem(SplitTabMenuModel::kCloseEndTabMenuItem),
      WaitForHide(SplitTabMenuModel::kCloseEndTabMenuItem),
      CheckOverlayBoundsMatchContents(0));
}

}  // namespace glic
