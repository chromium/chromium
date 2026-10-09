// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_deref.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/background/glic/glic_background_mode_manager.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/glic/browser_ui/glic_selection_widget.h"
#include "chrome/browser/glic/glic_selection_observer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/glic/selection/selection_suggestion.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/selection/features.h"
#include "chrome/browser/selection/suggestion_service.h"
#include "chrome/browser/selection/suggestion_tool.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/lens/lens_preselection_bubble.h"
#include "chrome/browser/ui/tabs/split_tab_menu_model.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/test/split_view_interactive_test_mixin.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/proto/features/quick_answers.pb.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "components/page_content_annotations/content/page_context_fetcher_options.h"
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
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/interaction/element_tracker_views.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/shell.h"
#include "chromeos/constants/chromeos_features.h"
#endif

namespace glic {

namespace {

views::View* GetOverlayView(content::WebContents* tab_contents) {
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab_contents);
  if (!controller) {
    return nullptr;
  }
  return controller->GetOverlayViewForTesting();
}

views::View* GetOverlayView(BrowserWindowInterface* browser, int index) {
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

  auto GetPointWithOffset(int x, int y) {
    return base::BindLambdaForTesting([this, x, y](ui::TrackedElement* el) {
      auto* view = AsView<views::View>(el);
      return view->GetBoundsInScreen().origin() + gfx::Vector2d(x, y);
    });
  }

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

  auto CheckOverlayRoundedCorners(int index,
                                  const gfx::RoundedCornersF& expected_radii) {
    return CheckResult(
        base::BindLambdaForTesting([this, index]() {
          auto* tab_contents =
              browser()->tab_strip_model()->GetWebContentsAt(index);
          auto* overlay_view = GetOverlayView(tab_contents);
          if (!overlay_view || !overlay_view->layer()) {
            return gfx::RoundedCornersF();
          }
          return overlay_view->layer()->rounded_corner_radii();
        }),
        expected_radii);
  }

  GURL GetEmptyDocURL() const {
    return embedded_test_server()->GetURL("/empty.html");
  }

  void ShowWithSelection(selection::InteractionOptionsPtr options) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    auto* controller =
        SelectionOverlayController::FromTabWebContents(web_contents);
    ASSERT_TRUE(controller);
    gfx::Rect view_bounds = web_contents->GetViewBounds();
    controller->ShowWithSelection(
        web_contents->GetPrimaryMainFrame(),
        gfx::Rect(view_bounds.x() + 10, view_bounds.y() + 10, 100, 50),
        std::move(options));
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

namespace {

class FakePromptSuggestionTool : public ::selection::SuggestionTool {
 public:
  explicit FakePromptSuggestionTool(tabs::TabInterface* tab) : tab_(tab) {
    QueueServerResponse();
  }
  ~FakePromptSuggestionTool() override = default;

  ToolId GetToolId() const override {
    return optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME;
  }

  std::unique_ptr<::selection::Suggestion> CreateSuggestion(
      const ::selection::AreaOfInterest& processed_area,
      const optimization_guide::proto::SmartSelectionSuggestion&
          server_suggestion) override {
    QueueServerResponse();
    return std::make_unique<SelectionSuggestion>(*tab_, u"Ask Gemini");
  }

 private:
  void QueueServerResponse() {
    optimization_guide::proto::SmartSelectionSuggestionsResponse response;
    auto* s = response.add_suggestions();
    s->set_tool(
        optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
    s->set_label("Ask Gemini");
    optimization_guide::proto::Any any;
    any.set_value(response.SerializeAsString());
    any.set_type_url(
        base::StrCat({"type.googleapis.com/", response.GetTypeName()}));
    OptimizationGuideKeyedServiceFactory::GetForProfile(tab_->GetProfile())
        ->AddExecutionResultForTesting(
            optimization_guide::ModelBasedCapabilityKey::
                kSmartSelectionSuggestions,
            optimization_guide::OptimizationGuideModelExecutionResult(
                std::move(any), nullptr));
  }

  raw_ptr<tabs::TabInterface> tab_;
};

}  // namespace

class SelectionOverlayInteractiveTestWithPrompt
    : public SelectionOverlayInteractiveTest {
 public:
  SelectionOverlayInteractiveTestWithPrompt() {
    feature_list_.InitWithFeatures(
        {features::kGlicSelectionOverlayPrompt,
         features::kGlicSelectionOverlayPromptBox,
         ::selection::kSmartSelectionServerSuggestions},
        {});
  }

  void SetUpOnMainThread() override {
    SelectionOverlayInteractiveTest::SetUpOnMainThread();
    tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
    fake_tool_ = std::make_unique<FakePromptSuggestionTool>(tab);
    if (auto* service = ::selection::SuggestionService::From(tab)) {
      service->RegisterTool(fake_tool_.get());
    }
  }

  void TearDownOnMainThread() override {
    tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
    if (tab && fake_tool_) {
      if (auto* service = ::selection::SuggestionService::From(tab)) {
        service->UnregisterTool(fake_tool_.get());
      }
    }
    fake_tool_.reset();
    SelectionOverlayInteractiveTest::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<FakePromptSuggestionTool> fake_tool_;
};

class SelectionOverlayInteractiveTestWithPromptWithoutBox
    : public SelectionOverlayInteractiveTestWithPrompt {
 public:
  SelectionOverlayInteractiveTestWithPromptWithoutBox() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicSelectionOverlayPrompt,
                              ::selection::kSmartSelectionServerSuggestions},
        /*disabled_features=*/{features::kGlicSelectionOverlayPromptBox});
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

class SelectionOverlayInteractiveTestWithTextSelection
    : public SelectionOverlayInteractiveTest {
 public:
  SelectionOverlayInteractiveTestWithTextSelection() {
    // The inline cue widget is not needed to pre-select the text selection.
    feature_list_.InitWithFeatures({::features::kGlicSelectionSmallChip},
                                   {::features::kGlicSelectionPrompt});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
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

// Starting a capture session while text is selected sends the pre-selected
// region to the web client without the user adjusting it.
IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTestWithTextSelection,
                       PreSelectedRegionReachesClient) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kHasSelectionBounds);

  const DeepQuery kRenderer = {"selection-overlay-app",
                               "glic-selection-overlay",
                               "post-selection-renderer"};

  RunTestSequence(
      InstrumentTab(kActiveTab),
      NavigateWebContents(kActiveTab,
                          embedded_test_server()->GetURL("/title2.html")),
      // Focus the page so that the text selection is reported.
      WaitForWebContentsPainted(kActiveTab),
      MoveMouseTo(kActiveTab, DeepQuery{"body"}), ClickMouse(), Do([this]() {
        browser()->tab_strip_model()->GetActiveWebContents()->SelectAll();
      }),
      PollState(
          kHasSelectionBounds,
          [this]() {
            auto* selection_observer = GlicSelectionObserver::From(
                browser()->tab_strip_model()->GetActiveTab());
            return selection_observer &&
                   selection_observer->GetCurrentSelectionBounds().has_value();
          }),
      WaitForState(kHasSelectionBounds, true), OpenGlic(),
      ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.selectedRegions.length === 1"),
      // The region must also reach the web client, which only happens if the
      // deferred render runs once the overlay and the page context are ready.
      WaitForJsResultAt(kGlicContentsElementId, {"#additionalContextResult"},
                        "el => el.innerText.includes('Region: ')"));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       ShowWithSelectionSendsInteractionOptions) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);
  const DeepQuery kOverlay = {"selection-overlay-app",
                              "glic-selection-overlay"};

  RunTestSequence(
      Do([this]() {
        ShowWithSelection(selection::InteractionOptions::New(
            /*hide_handles=*/false, /*disable_multi_select=*/true));
      }),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, kOverlay,
                        "el => !el.hideHandles && el.disableMultiSelect"));
}

// With `disable_multi_select`, the overlay shows the normal cursor and a
// background click dismisses it instead of drawing a new region.
IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       DisableMultiSelectClickDismissesOverlay) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);
  const DeepQuery kOverlay = {"selection-overlay-app",
                              "glic-selection-overlay"};
  const DeepQuery kRenderer = {"selection-overlay-app",
                               "glic-selection-overlay",
                               "post-selection-renderer"};

  RunTestSequence(
      Do([this]() {
        ShowWithSelection(selection::InteractionOptions::New(
            /*hide_handles=*/false, /*disable_multi_select=*/true));
      }),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.hasSelection()"),
      WaitForJsResultAt(
          kOverlayWebContentsId, kOverlay,
          "el => getComputedStyle(el.shadowRoot.querySelector("
          "'#selectionOverlay')).cursor === 'default' && "
          "getComputedStyle(el.shadowRoot.querySelector('#cursor'))"
          ".visibility === 'hidden'"),
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(300, 300)),
      ClickMouse(), WaitForHide(OverlayBaseController::kOverlayId));
}

// With `disable_multi_select`, a background drag dismisses the overlay instead
// of drawing a new region.
IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       DisableMultiSelectDragDismissesOverlay) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);
  const DeepQuery kRenderer = {"selection-overlay-app",
                               "glic-selection-overlay",
                               "post-selection-renderer"};

  RunTestSequence(
      Do([this]() {
        ShowWithSelection(selection::InteractionOptions::New(
            /*hide_handles=*/false, /*disable_multi_select=*/true));
      }),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                        "el => el.hasSelection()"),
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(300, 300)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(400, 400)),
      WaitForHide(OverlayBaseController::kOverlayId));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       ShowWithSelectionHidesHandles) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);
  const DeepQuery kRenderer = {"selection-overlay-app",
                               "glic-selection-overlay",
                               "post-selection-renderer"};

  RunTestSequence(Do([this]() {
                    ShowWithSelection(selection::InteractionOptions::New(
                        /*hide_handles=*/true, /*disable_multi_select=*/false));
                  }),
                  WaitForShow(OverlayBaseController::kOverlayId),
                  InstrumentNonTabWebView(kOverlayWebContentsId,
                                          OverlayBaseController::kOverlayId),
                  WaitForJsResultAt(kOverlayWebContentsId, kRenderer,
                                    "el => el.hasAttribute('hide-handles')"));
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
      FocusElement(kGlicContentsElementId),
      SendKeyPress(kGlicContentsElementId, ui::VKEY_ESCAPE),
      WaitForHide(OverlayBaseController::kOverlayId),
      EnsurePresent(kGlicHostElementId),
      SendKeyPress(kGlicContentsElementId, ui::VKEY_ESCAPE),
      WaitForHide(kGlicContentsElementId));
}

// When glic is in floating mode and when only the first tab has context shared,
// on a second tab, pressing esc in the floaty dismisses the floaty and
// therefore the selection overlay in the first tab.
//
// Fails on Wayland platforms and flaky on Mac.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS)
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
      InAnyContext(ActivateSurface(kGlicHostElementId)),
      InAnyContext(SendKeyPress(kGlicHostElementId, ui::VKEY_ESCAPE)),
      InAnyContext(WaitForHide(kGlicHostElementId)),
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
            actor::ActorKeyedService::Get(browser()->GetProfile());
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
            GlicKeyedService::Get(browser()->GetProfile());
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
            GlicKeyedService::Get(browser()->GetProfile());
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
                       OverlayFocusActivatesItsTabInSplitView) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<int>,
                                      kActiveTabIndex);

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
      PollState(
          kActiveTabIndex,
          [this]() { return browser()->tab_strip_model()->active_index(); }),
      FocusInactiveTabInSplit(), WaitForState(kActiveTabIndex, 1),
      // The overlay covers tab 0, so focusing it must bring tab 0 back.
      MoveMouseTo(base::BindLambdaForTesting([this]() {
        return GetOverlayView(browser(), 0)->GetBoundsInScreen().CenterPoint();
      })),
      ClickMouse(), WaitForState(kActiveTabIndex, 0),
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
      CheckOverlayBoundsMatchContents(0),
      CheckOverlayRoundedCorners(
          0, MultiContentsView::kSplitViewContentRoundedCorners));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTestWithSplitView,
                       OverlayRespectsSplitViewRoundedCorners) {
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
      CheckOverlayRoundedCorners(
          0, MultiContentsView::kSplitViewContentRoundedCorners));
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

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       CloseFloatingWindowClearsOverlay) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  TrackFloatingGlicInstance();
  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              "Wayland unsupported"),
      OpenGlicFloatingWindow(GlicInstrumentMode::kHostAndContents,
                             /*conversation_id=*/std::nullopt),
      ClickMockGlicElement({"#pinFocusedTab"}),
      ActivateSurface(kBrowserViewElementId),
      WaitForJsResultAt(kGlicContentsElementId, {"body"},
                        "el => window.client.getFocusedTabId() !== ''"),
      ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, {"selection-overlay-app"},
                        "el => el.screenshot_ !== null"),
      WaitForElementVisible(kOverlayWebContentsId, {"selection-overlay-app",
                                                    "glic-selection-overlay"}),
      ActivateSurface(kBrowserViewElementId), CloseGlicWindow(),
      WaitForHide(OverlayBaseController::kOverlayId));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       CloseSidePanelClearsOverlay) {
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
      ToggleGlicWindow(GlicWindowMode::kAttached),
      InAnyContext(WaitForHide(kGlicHostElementId)),
      WaitForHide(OverlayBaseController::kOverlayId));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTest,
                       PromptHiddenByDefaultWhenDisabled) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);
  const DeepQuery kOverlayApp = {"selection-overlay-app"};
  const DeepQuery kSelectionOverlay = {"selection-overlay-app",
                                       "glic-selection-overlay"};

  RunTestSequence(
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, kOverlayApp,
                        "el => el.screenshot_ !== null"),
      WaitForElementVisible(kOverlayWebContentsId, kSelectionOverlay),
      WaitForJsResultAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => el.isScreenshotRendered && el.selectionOverlayRect.width > 0 "
          "&& el.selectionOverlayRect.height > 0"),
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(50, 50)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(150, 150)),
      WaitForJsResultAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => el.shadowRoot.querySelector('#floatingPromptContainer') === "
          "null"));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTestWithPrompt,
                       FloatingPromptAndChipsShownOnSelection) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);
  const DeepQuery kOverlayApp = {"selection-overlay-app"};
  const DeepQuery kSelectionOverlay = {"selection-overlay-app",
                                       "glic-selection-overlay"};

  RunTestSequence(
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, kOverlayApp,
                        "el => el.screenshot_ !== null"),
      WaitForElementVisible(kOverlayWebContentsId, kSelectionOverlay),
      WaitForJsResultAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => el.isScreenshotRendered && el.selectionOverlayRect.width > 0 "
          "&& el.selectionOverlayRect.height > 0 && !el.showFloatingPrompt"),
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(50, 50)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(150, 150)),
      WaitForJsResultAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => {"
          "  const container = "
          "el.shadowRoot.querySelector('#floatingPromptContainer');"
          "  return container !== null && !container.hidden;"
          "}"),
      WaitForJsResultAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => {"
          "  const input = el.shadowRoot.querySelector('#promptInput');"
          "  return input !== null && input.placeholder === 'Ask Gemini';"
          "}"),
      WaitForJsResultAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => {"
          "  const chips = "
          "Array.from(el.shadowRoot.querySelectorAll('.action-chip'));"
          "  if (chips.length !== 1) return false;"
          "  const titles = chips.map(c => "
          "c.querySelector('.chip-label')?.textContent?.trim());"
          "  return titles[0] === 'Ask Gemini';"
          "}"),
      WaitForJsResultAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => {"
          "  const container = "
          "el.shadowRoot.querySelector('#floatingPromptContainer');"
          "  const rect = container.getBoundingClientRect();"
          "  return rect.height > 38 && rect.height <= 96;"
          "}"));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTestWithPromptWithoutBox,
                       ChipsShownWithoutInputBoxWhenPromptBoxDisabled) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);
  const DeepQuery kOverlayApp = {"selection-overlay-app"};
  const DeepQuery kSelectionOverlay = {"selection-overlay-app",
                                       "glic-selection-overlay"};
  const DeepQuery kCloseRegionButton = {
      "selection-overlay-app", "glic-selection-overlay", "#closeRegionButton"};

  RunTestSequence(
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, kOverlayApp,
                        "el => el.screenshot_ !== null"),
      WaitForElementVisible(kOverlayWebContentsId, kSelectionOverlay),
      WaitForJsResultAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => el.isScreenshotRendered && el.selectionOverlayRect.width > 0 "
          "&& el.selectionOverlayRect.height > 0 && !el.showFloatingPrompt"),
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(50, 50)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(250, 150)),
      WaitForJsResultAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => {"
          "  const container = "
          "el.shadowRoot.querySelector('#floatingPromptContainer');"
          "  return container !== null && !container.hidden;"
          "}"),
      WaitForJsResultAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => el.shadowRoot.querySelector('#promptInput') === null"),
      WaitForJsResultAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => {"
          "  const chips = "
          "Array.from(el.shadowRoot.querySelectorAll('.action-chip'));"
          "  if (chips.length !== 1) return false;"
          "  const titles = chips.map(c => "
          "c.querySelector('.chip-label')?.textContent?.trim());"
          "  return titles[0] === 'Ask Gemini';"
          "}"),
      WaitForJsResultAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => {"
          "  const renderer = "
          "el.shadowRoot.querySelector('#postSelectionRenderer');"
          "  const rendererCloseBtn = "
          "renderer?.shadowRoot?.querySelector('.close-button');"
          "  return renderer?.hideCloseButton === true && "
          "      rendererCloseBtn !== null && "
          "      getComputedStyle(rendererCloseBtn).display === 'none';"
          "}"),
      WaitForJsResultAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => {"
          "  const container = "
          "el.shadowRoot.querySelector('#floatingPromptContainer');"
          "  const closeBtn = "
          "el.shadowRoot.querySelector('#closeRegionButton');"
          "  const chip = el.shadowRoot.querySelector('.action-chip');"
          "  if (!container || !closeBtn || !chip) return false;"
          "  const containerRect = container.getBoundingClientRect();"
          "  const btnRect = closeBtn.getBoundingClientRect();"
          "  const chipRect = chip.getBoundingClientRect();"
          "  const overlayRect = el.selectionOverlayRect;"
          "  const sel = el.activeSelection;"
          "  const selRight = overlayRect.left + (sel.left + sel.width) * "
          "overlayRect.width;"
          "  const selTop = overlayRect.top + sel.top * overlayRect.height;"
          "  return Math.abs(containerRect.left - selRight) <= 2 && "
          "      Math.abs(containerRect.bottom - selTop) <= 2 && "
          "      btnRect.left >= chipRect.right;"
          "}"),
      MoveMouseTo(kOverlayWebContentsId, kCloseRegionButton), ClickMouse(),
      WaitForHide(OverlayBaseController::kOverlayId));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTestWithPrompt,
                       FloatingPromptPositionClampedWithinViewport) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);
  const DeepQuery kOverlayApp = {"selection-overlay-app"};
  const DeepQuery kSelectionOverlay = {"selection-overlay-app",
                                       "glic-selection-overlay"};

  RunTestSequence(
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, kOverlayApp,
                        "el => el.screenshot_ !== null"),
      WaitForElementVisible(kOverlayWebContentsId, kSelectionOverlay),
      WaitForJsResultAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => el.isScreenshotRendered && el.selectionOverlayRect.width > 0 "
          "&& el.selectionOverlayRect.height > 0"),
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(500, 200)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(700, 350)),
      WaitForJsResultAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => {"
          "  const container = "
          "el.shadowRoot.querySelector('#floatingPromptContainer');"
          "  if (!container || container.hidden) return false;"
          "  const rect = container.getBoundingClientRect();"
          "  return rect.left >= 0 && rect.right <= window.innerWidth && "
          "         rect.top >= 0 && rect.bottom <= window.innerHeight;"
          "}"));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTestWithPrompt,
                       ClickActionChipTriggersExecution) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);
  const DeepQuery kOverlayApp = {"selection-overlay-app"};
  const DeepQuery kSelectionOverlay = {"selection-overlay-app",
                                       "glic-selection-overlay"};

  RunTestSequence(
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, kOverlayApp,
                        "el => el.screenshot_ !== null"),
      WaitForElementVisible(kOverlayWebContentsId, kSelectionOverlay),
      WaitForJsResultAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => el.isScreenshotRendered && el.selectionOverlayRect.width > 0 "
          "&& el.selectionOverlayRect.height > 0"),
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(50, 50)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(150, 150)),
      WaitForJsResultAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => {"
          "  const chips = "
          "Array.from(el.shadowRoot.querySelectorAll('.action-chip'));"
          "  const chip = chips.find(c => "
          "c.querySelector('.chip-label')?.textContent === 'Ask Gemini');"
          "  if (!chip) return false;"
          "  chip.click();"
          "  return true;"
          "}"));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTestWithPrompt,
                       SubmitPromptInputOnEnter) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);
  const DeepQuery kOverlayApp = {"selection-overlay-app"};
  const DeepQuery kSelectionOverlay = {"selection-overlay-app",
                                       "glic-selection-overlay"};

  RunTestSequence(
      OpenGlic(), ClickMockGlicElement({"#captureRegionBtn"}),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, kOverlayApp,
                        "el => el.screenshot_ !== null"),
      WaitForElementVisible(kOverlayWebContentsId, kSelectionOverlay),
      WaitForJsResultAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => el.isScreenshotRendered && el.selectionOverlayRect.width > 0 "
          "&& el.selectionOverlayRect.height > 0"),
      MoveMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(50, 50)),
      DragMouseTo(OverlayBaseController::kOverlayId,
                  GetPointWithOffset(150, 150)),
      WaitForJsResultAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => el.shadowRoot.querySelector('#promptInput') !== null"),
      ExecuteJsAt(
          kOverlayWebContentsId, kSelectionOverlay,
          "el => {"
          "  const input = el.shadowRoot.querySelector('#promptInput');"
          "  input.value = 'Explain this image';"
          "  input.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter', "
          "bubbles: true}));"
          "}"));
}

namespace {

class SelectionOverlayInteractiveTestWithInlineFulfillment
    : public SelectionOverlayInteractiveTest {
 public:
  SelectionOverlayInteractiveTestWithInlineFulfillment() {
    feature_list_.InitFromCommandLine(
        /*enable_features=*/
        "GlicSelectionOverlayPrompt,GlicSelectionSmallChip,GlicSelectionPrompt,"
        "QuickAnswersSelectionSuggestions",
        /*disable_features=*/"");
  }

 protected:
  const DeepQuery kSelectionOverlay = {"selection-overlay-app",
                                       "glic-selection-overlay"};

  auto OpenExplainCard(ui::ElementIdentifier tab,
                       ui::ElementIdentifier overlay) {
    const DeepQuery kOverlayApp = {"selection-overlay-app"};

    // Text for the user to select.
    static constexpr char kAddTextJs[] = R"js(
      () => {
        document.body.innerHTML = '<p>Explain this sentence please</p>';
      }
    )js";
    static constexpr char kClickExplainChipJs[] = R"js(
      el => {
        const chip = [...el.shadowRoot.querySelectorAll('.action-chip')].find(
            c => c.querySelector('.chip-label')?.textContent === 'Explain');
        chip?.click();
        return !!chip;
      }
    )js";

    return Steps(
        InstrumentTab(tab), NavigateWebContents(tab, GetEmptyDocURL()),
        ExecuteJs(tab, kAddTextJs),
        // Select the text. The small chip shows up on it. This avoids an OS
        // mouse drag, which can hang on Windows bots.
        WaitForWebContentsPainted(tab), Do([this] {
          content::WebContents* contents =
              browser()->tab_strip_model()->GetActiveWebContents();
          contents->Focus();
          contents->SelectAll();
        }),
        // The chip is its own widget, outside the browser's context.
        InAnyContext(
            WaitForShow(GlicSelectionWidgetDelegate::kAskGeminiButtonElementId),
            PressButton(
                GlicSelectionWidgetDelegate::kAskGeminiButtonElementId)),
        // The overlay opens on the selected text.
        WaitForShow(OverlayBaseController::kOverlayId),
        InstrumentNonTabWebView(overlay, OverlayBaseController::kOverlayId),
        WaitForJsResultAt(overlay, kOverlayApp,
                          "el => el.screenshot_ !== null"),
        WaitForElementVisible(overlay, kSelectionOverlay),
        WaitForJsResultAt(overlay, kSelectionOverlay, kClickExplainChipJs));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTestWithInlineFulfillment,
                       ClickExplainChipShowsCard) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  static constexpr char kExpectedExplanation[] =
      "An explanation of your selection.";
  optimization_guide::proto::QuickAnswersResponse response_msg;
  response_msg.set_answer(kExpectedExplanation);
  optimization_guide::proto::Any any;
  any.set_value(response_msg.SerializeAsString());
  any.set_type_url(
      base::StrCat({"type.googleapis.com/", response_msg.GetTypeName()}));
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->GetProfile())
      ->AddExecutionResultForTesting(
          optimization_guide::ModelBasedCapabilityKey::kQuickAnswers,
          optimization_guide::OptimizationGuideModelExecutionResult(
              std::move(any), nullptr));

  static constexpr char kCardTextJs[] = R"js(
    el => el.shadowRoot.querySelector(
        '#inlineFulfillmentHost > glic-explain-fulfillment > .explanation')
        ?.textContent
  )js";
  static constexpr char kChipsHiddenJs[] = R"js(
    el => getComputedStyle(
        el.shadowRoot.querySelector('.action-chips-row')).display === 'none'
  )js";

  RunTestSequence(
      OpenExplainCard(kActiveTab, kOverlayWebContentsId),
      // The card shows the explanation in place of the chips.
      WaitForJsResultAt(kOverlayWebContentsId, kSelectionOverlay, kCardTextJs,
                        kExpectedExplanation),
      CheckJsResultAt(kOverlayWebContentsId, kSelectionOverlay, kChipsHiddenJs),
      // The overlay stays up for inline fulfillment.
      CheckResult(GetOverlayVisibilityAt(0), true));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTestWithInlineFulfillment,
                       GrowingCardStaysInViewport) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  static constexpr char kCardShownJs[] = R"js(
    el => !!el.shadowRoot.querySelector(
        '#inlineFulfillmentHost > glic-explain-fulfillment')
  )js";
  // In WebUI, updateFloatingPromptPosition() keeps a 16px margin from the
  // viewport edges and from the selection. Grows the card until the prompt is
  // exactly as tall as the space inside the top and bottom margins (innerHeight
  // - 2 * 16). It can no longer go below the selection, so it must move up. If
  // it didn't, it would run off the bottom by (selection bottom - 16px), which
  // is > 0 here since the selected text ends ~26px down.
  static constexpr char kGrowCardJs[] = R"js(
    el => {
      const container =
          el.shadowRoot.querySelector('#floatingPromptContainer');
      const host = el.shadowRoot.querySelector('#inlineFulfillmentHost');
      const grow = innerHeight - 32 - container.getBoundingClientRect().height;
      host.style.height = `${host.getBoundingClientRect().height + grow}px`;
    }
  )js";
  static constexpr char kPromptInViewportJs[] = R"js(
    el => {
      const rect = el.shadowRoot.querySelector('#floatingPromptContainer')
                       .getBoundingClientRect();
      return rect.top >= 0 && rect.bottom <= innerHeight;
    }
  )js";

  RunTestSequence(
      OpenExplainCard(kActiveTab, kOverlayWebContentsId),
      WaitForJsResultAt(kOverlayWebContentsId, kSelectionOverlay, kCardShownJs),
      ExecuteJsAt(kOverlayWebContentsId, kSelectionOverlay, kGrowCardJs),
      // The prompt moves so the card stays on screen.
      WaitForJsResultAt(kOverlayWebContentsId, kSelectionOverlay,
                        kPromptInViewportJs));
}

IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTestWithInlineFulfillment,
                       ClickCardAskGeminiOpensSidePanel) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);
  const DeepQuery kAskGemini = {
      "selection-overlay-app", "glic-selection-overlay",
      "#inlineFulfillmentHost > glic-explain-fulfillment > .ask-gemini"};

  RunTestSequence(OpenExplainCard(kActiveTab, kOverlayWebContentsId),
                  WaitForElementVisible(kOverlayWebContentsId, kAskGemini),
                  ClickElement(kOverlayWebContentsId, kAskGemini),
                  // Glic opens in the side panel.
                  WaitForShow(kSidePanelElementId),
                  InAnyContext(WaitForShow(kGlicViewElementId)));
}

// The small chip opens the overlay with the handles hidden, and they stay
// hidden while the Explain card shows.
IN_PROC_BROWSER_TEST_F(SelectionOverlayInteractiveTestWithInlineFulfillment,
                       HandlesHiddenWithExplainCard) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);
  const DeepQuery kRenderer = {"selection-overlay-app",
                               "glic-selection-overlay",
                               "post-selection-renderer"};

  static constexpr char kCardShownJs[] = R"js(
    el => !!el.shadowRoot.querySelector(
        '#inlineFulfillmentHost > glic-explain-fulfillment')
  )js";
  // The corners aren't drawn, and their hit boxes are gone, so the region
  // can't be resized.
  static constexpr char kHandlesHiddenJs[] = R"js(
    el => {
      const corners = el.shadowRoot.querySelector('#selectionCorners');
      const boxes = [...corners.querySelectorAll('.corner-hit-box')];
      return getComputedStyle(corners).backgroundImage === 'none' &&
          boxes.length === 4 &&
          boxes.every(box => getComputedStyle(box).display === 'none');
    }
  )js";

  RunTestSequence(
      OpenExplainCard(kActiveTab, kOverlayWebContentsId),
      WaitForJsResultAt(kOverlayWebContentsId, kSelectionOverlay, kCardShownJs),
      CheckJsResultAt(kOverlayWebContentsId, kRenderer, kHandlesHiddenJs));
}

namespace {

class SelectionOverlayInteractiveScreenshotSizeCapTest
    : public SelectionOverlayInteractiveTest,
      public testing::WithParamInterface<bool> {
 public:
  SelectionOverlayInteractiveScreenshotSizeCapTest() {
    if (!GetParam()) {
      feature_list_.InitFromCommandLine(
          /*enable_features=*/"",
          /*disable_features=*/"GlicSelectionOverlayFullSizeScreenshot");
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_P(SelectionOverlayInteractiveScreenshotSizeCapTest,
                       ScreenshotSizeCap) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOverlayWebContentsId);

  const DeepQuery kOverlayApp = {"selection-overlay-app"};
  bool feature_enabled = GetParam();

  RunTestSequence(
      OpenGlic(),
      // Request a 10x10 capture, which is smaller than a test window on the
      // bot.
      Do([this]() {
        auto* controller = SelectionOverlayController::FromTabWebContents(
            browser()->tab_strip_model()->GetActiveWebContents());
        ASSERT_TRUE(controller);
        auto options = mojom::TabContextOptions::New();
        options->viewport_screenshot = true;
        options->annotated_page_content = true;
        options->screenshot_collection_options.max_width = 10;
        options->screenshot_collection_options.max_height = 10;
        controller->Show(std::move(options));
      }),
      WaitForShow(OverlayBaseController::kOverlayId),
      InstrumentNonTabWebView(kOverlayWebContentsId,
                              OverlayBaseController::kOverlayId),
      WaitForJsResultAt(kOverlayWebContentsId, kOverlayApp,
                        "el => el.screenshot_ !== null"),
      CheckJsResultAt(kOverlayWebContentsId, kOverlayApp,
                      feature_enabled
                          ? "el => el.screenshot_.imageInfo.width > 10 && "
                            "el.screenshot_.imageInfo.height > 10"
                          : "el => el.screenshot_.imageInfo.width <= 10 && "
                            "el.screenshot_.imageInfo.height <= 10",
                      true));
}

INSTANTIATE_TEST_SUITE_P(/*no prefix*/,
                         SelectionOverlayInteractiveScreenshotSizeCapTest,
                         testing::Bool());

}  // namespace glic
