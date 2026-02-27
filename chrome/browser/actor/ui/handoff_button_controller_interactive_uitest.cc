// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
#include "chrome/browser/actor/ui/actor_ui_interactive_browser_test.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/handoff_button_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/test/split_view_browser_test_mixin.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/widget/glic_view.h"

namespace actor::ui {
namespace {

using actor::mojom::ActionResultPtr;
using base::test::TestFuture;
using TabHandle = tabs::TabInterface::Handle;
using DeepQuery = ::WebContentsInteractionTestUtil::DeepQuery;

class ActorUiHandoffButtonControllerInteractiveUiTest
    : public ActorUiInteractiveBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        // Use a dummy URL so we don't make a network request.
        {
            {features::kGlicURLConfig,
             {{features::kGlicGuestURL.name, "about:blank"}}},
            {features::kGlicHandoffButtonShowInImmersiveMode, {}},
            {features::kGlicHandoffButtonHideWhenOmniboxPopupOpened, {}},
            {features::kGlicActorUi,
             {{features::kGlicActorUiHandoffButtonName, "true"}}},
        },
        /*disabled_features=*/{features::kGlicDetached});
    InteractiveBrowserTest::SetUp();
  }

  auto ClearOmniboxFocus() {
    return WithView(kOmniboxElementId, [](OmniboxViewViews* omnibox_view) {
      omnibox_view->GetFocusManager()->ClearFocus();
    });
  }

#if BUILDFLAG(IS_MAC)
  auto ToggleImmersiveFullscreen() {
    return [&]() { ui_test_utils::ToggleFullscreenModeAndWait(browser()); };
  }

  auto IsInImmersiveFullscreen() {
    return [&]() {
      auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
      return browser_view->GetWidget()->IsFullscreen() &&
             ImmersiveModeController::From(browser())->IsEnabled();
    };
  }
#endif  // BUILDFLAG(IS_MAC)

 protected:
  glic::GlicTestEnvironment glic_test_env_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorUiHandoffButtonControllerInteractiveUiTest,
                       WidgetIsCreatedAndDestroyed) {
  StartActingOnTab();
  RunTestSequence(ClearOmniboxFocus(),
                  InAnyContext(WaitForShow(
                      HandoffButtonController::kHandoffButtonElementId)),
                  // Trigger the event to destroy the button.
                  Do([&]() { CompleteTask(); }),
                  InAnyContext(WaitForHide(
                      HandoffButtonController::kHandoffButtonElementId)));
}

IN_PROC_BROWSER_TEST_F(ActorUiHandoffButtonControllerInteractiveUiTest,
                       ButtonClickToPauseTaskHidesButton) {
  StartActingOnTab();
  RunTestSequence(
      ClearOmniboxFocus(),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)),
      InAnyContext(
          PressButton(HandoffButtonController::kHandoffButtonElementId)),
      // Button hides since the client is in control.
      InAnyContext(
          WaitForHide(HandoffButtonController::kHandoffButtonElementId)));
}

IN_PROC_BROWSER_TEST_F(ActorUiHandoffButtonControllerInteractiveUiTest,
                       ButtonHidesAndReshowsOnTabSwitch) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
  StartActingOnTab();
  RunTestSequence(
      ClearOmniboxFocus(),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)),
      // Switch to the second tab.
      AddInstrumentedTab(kSecondTab, GURL("about:blank")),
      InAnyContext(
          WaitForHide(HandoffButtonController::kHandoffButtonElementId)),
      // Switch back to the first tab.
      SelectTab(kTabStripElementId, 0), ClearOmniboxFocus(),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)));
}

IN_PROC_BROWSER_TEST_F(ActorUiHandoffButtonControllerInteractiveUiTest,
                       ButtonReparentsToNewWindowOnDrag) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kMovedTabId);
  StartActingOnTab();
  RunTestSequence(
      ClearOmniboxFocus(),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)),
      // Label the new tab with the previously defined local identifier.
      InstrumentNextTab(kMovedTabId, AnyBrowser()),
      // Move the first tab (at index 0) to a new window.
      Do([&]() { chrome::MoveTabsToNewWindow(browser(), {0}); }),
      InAnyContext(WaitForWebContentsReady(kMovedTabId)),
      InAnyContext(CheckElement(
          kMovedTabId,
          [](::ui::TrackedElement* el) {
            auto* const web_contents =
                AsInstrumentedWebContents(el)->web_contents();
            // This will be true only when the tab is fully attached.
            return tabs::TabInterface::GetFromContents(web_contents) != nullptr;
          })),
      InAnyContext(ActivateSurface(kMovedTabId)),
      InAnyContext(WithElement(
          kOmniboxElementId,
          [](::ui::TrackedElement* el) {
            // 1. Cast to the framework-specific element type
            auto* tracked_element_views = el->AsA<views::TrackedElementViews>();
            if (tracked_element_views) {
              // 2. Get the raw view pointer from it
              auto* omnibox_view = tracked_element_views->view();
              if (omnibox_view) {
                omnibox_view->GetFocusManager()->ClearFocus();
              }
            }
          })),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)));
}

// This test is only for Mac where we have immersive fullscreen.
#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ActorUiHandoffButtonControllerInteractiveUiTest,
                       ButtonReappearsAfterFullscreenToggle) {
  StartActingOnTab();
  RunTestSequence(
      ClearOmniboxFocus(),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)),
      Do(ToggleImmersiveFullscreen()), Check(IsInImmersiveFullscreen()),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)),
      // Exit fullscreen.
      Do(ToggleImmersiveFullscreen()),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)));
}
#endif  // BUILDFLAG(IS_MAC)

// TODO(crbug.com/465113623) Test flaky on Wayland.
#if BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
#define MAYBE_ButtonHidesWhenOmniboxIsFocused \
  DISABLED_ButtonHidesWhenOmniboxIsFocused
#else
#define MAYBE_ButtonHidesWhenOmniboxIsFocused ButtonHidesWhenOmniboxIsFocused
#endif
IN_PROC_BROWSER_TEST_F(ActorUiHandoffButtonControllerInteractiveUiTest,
                       MAYBE_ButtonHidesWhenOmniboxIsFocused) {
  StartActingOnTab();
  RunTestSequence(
      ClearOmniboxFocus(),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)),
      FocusElement(kOmniboxElementId), EnterText(kOmniboxElementId, u"test"),
      InAnyContext(
          WaitForHide(HandoffButtonController::kHandoffButtonElementId)),
      ClearOmniboxFocus(),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)));
}

IN_PROC_BROWSER_TEST_F(ActorUiHandoffButtonControllerInteractiveUiTest,
                       GlicSidePanelTogglesOnWhenButtonClicked) {
  StartActingOnTab();
  RunTestSequence(ClearOmniboxFocus(), EnsureNotPresent(kSidePanelElementId),
                  EnsureNotPresent(kGlicViewElementId),
                  InAnyContext(WaitForShow(
                      HandoffButtonController::kHandoffButtonElementId)),
                  InAnyContext(CheckViewProperty(
                      HandoffButtonController::kHandoffButtonElementId,
                      &views::LabelButton::GetText, TAKE_OVER_TASK_TEXT)),
                  InAnyContext(PressButton(
                      HandoffButtonController::kHandoffButtonElementId)),
                  InAnyContext(WaitForShow(kSidePanelElementId)),
                  InAnyContext(WaitForShow(kGlicViewElementId)));
}

// State identifier for polling the visible handoff button count
using VisibleCountObserver = ::ui::test::PollingStateObserver<int>;
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(VisibleCountObserver,
                                    kVisibleHandoffButtonCountState);

class ActorUiHandoffButtonSplitViewTest
    : public SplitViewBrowserTestMixin<ActorUiInteractiveBrowserTest> {
 public:
  ActorUiHandoffButtonSplitViewTest() = default;
  ~ActorUiHandoffButtonSplitViewTest() override = default;

  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    return {{features::kGlicURLConfig,
             {{features::kGlicGuestURL.name, "about:blank"}}},
            {features::kGlic, {}},
            {features::kGlicActorUi,
             {{features::kGlicActorUiHandoffButtonName, "true"}}}};
  }

  void SetUpOnMainThread() override {
    ActorUiInteractiveBrowserTest::SetUpOnMainThread();
    // Add a second tab
    ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank?second"),
                              ::ui::PAGE_TRANSITION_TYPED));
    ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
    browser()->tab_strip_model()->ActivateTabAt(0);

    // Enter split view by adding tab at index 1 to the split with active tab 0
    tab_strip_model()->ExecuteContextMenuCommand(
        1, TabStripModel::CommandAddToSplit);

    // Verify both tabs are now in the same split
    std::optional<split_tabs::SplitTabId> split_id0 =
        browser()->tab_strip_model()->GetSplitForTab(0);
    std::optional<split_tabs::SplitTabId> split_id1 =
        browser()->tab_strip_model()->GetSplitForTab(1);

    ASSERT_TRUE(split_id0.has_value());
    ASSERT_TRUE(split_id1.has_value());
    ASSERT_EQ(split_id0.value(), split_id1.value());

    MultiContentsView* multi_view = this->multi_contents_view();
    ASSERT_NE(multi_view, nullptr);

    content::WebContents* wc0 =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    content::WebContents* wc1 =
        browser()->tab_strip_model()->GetWebContentsAt(1);
    ASSERT_NE(wc0, nullptr);
    ASSERT_NE(wc1, nullptr);

    ContentsWebView* webview0 = nullptr;
    ContentsWebView* webview1 = nullptr;

    // Wait until the WebContents are properly parented within
    // MultiContentsView's children.
    ASSERT_TRUE(base::test::RunUntil([&]() {
      webview0 = FindWebViewForWebContents(multi_view, wc0);
      webview1 = FindWebViewForWebContents(multi_view, wc1);
      return webview0 != nullptr && webview1 != nullptr;
    })) << "WebContents not parented in MultiContentsView hierarchy in time";

    ASSERT_NE(webview0, nullptr);
    ASSERT_NE(webview1, nullptr);

    // Determine left and right panes based on their x-coordinates.
    if (webview1->GetBoundsInScreen().x() < webview0->GetBoundsInScreen().x()) {
      wc_left_ = wc1;
      wc_right_ = wc0;
      left_pane_context_ =
          views::ElementTrackerViews::GetContextForView(webview1);
      right_pane_context_ =
          views::ElementTrackerViews::GetContextForView(webview0);
    } else {
      wc_left_ = wc0;
      wc_right_ = wc1;
      left_pane_context_ =
          views::ElementTrackerViews::GetContextForView(webview0);
      right_pane_context_ =
          views::ElementTrackerViews::GetContextForView(webview1);
    }
    ASSERT_NE(wc_left_, nullptr);
    ASSERT_NE(wc_right_, nullptr);
  }

 protected:
  // Helper to traverse the view hierarchy and find the ContentsWebView
  // associated with a specific WebContents.
  ContentsWebView* FindWebViewForWebContents(views::View* root,
                                             content::WebContents* target_wc) {
    if (!root || !target_wc) {
      return nullptr;
    }

    std::deque<views::View*> queue;
    queue.push_back(root);

    while (!queue.empty()) {
      views::View* current = queue.front();
      queue.pop_front();

      // Check if current view is the WebView we need
      if (auto* cwv = views::AsViewClass<ContentsWebView>(current)) {
        if (cwv->web_contents() == target_wc) {
          return cwv;
        }
      }

      // Continue search
      for (views::View* child : current->children()) {
        queue.push_back(child);
      }
    }
    return nullptr;
  }

  void StartActingOnTab(content::WebContents* wc, TaskId& task_id) {
    ASSERT_NE(wc, nullptr)
        << "WebContents is null. SetUpOnMainThread likely failed.";

    tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(wc);
    ASSERT_NE(tab, nullptr);

    task_id = actor_keyed_service()->CreateTask(NoEnterprisePolicyChecker());
    TestFuture<actor::mojom::ActionResultPtr> future;
    actor_keyed_service()->GetTask(task_id)->AddTab(tab->GetHandle(),
                                                    future.GetCallback());
    ExpectOkResult(future);

    actor::PerformActionsFuture result_future;
    std::vector<std::unique_ptr<actor::ToolRequest>> actions;
    actions.push_back(actor::MakeWaitRequest());
    actor_keyed_service()->PerformActions(task_id, std::move(actions),
                                          actor::ActorTaskMetadata(),
                                          result_future.GetCallback());
    ExpectOkResult(result_future);
  }

  auto PollVisibleHandoffButtons() {
    return PollState(kVisibleHandoffButtonCountState, []() -> int {
      auto* tracker = ::ui::ElementTracker::GetElementTracker();
      auto elements = tracker->GetAllMatchingElementsInAnyContext(
          HandoffButtonController::kHandoffButtonElementId);
      int visible_count = 0;
      for (auto* element : elements) {
        auto* view_element = element->AsA<views::TrackedElementViews>();
        if (view_element && view_element->view() &&
            view_element->view()->GetVisible()) {
          visible_count++;
        }
      }
      return visible_count;
    });
  }

  void TearDownOnMainThread() override {
    // Clear raw_ptrs before the browser shuts down and destroys the
    // WebContents.
    wc_left_ = nullptr;
    wc_right_ = nullptr;
    ActorUiInteractiveBrowserTest::TearDownOnMainThread();
  }

  ::ui::ElementContext left_pane_context_;
  ::ui::ElementContext right_pane_context_;
  raw_ptr<content::WebContents, DisableDanglingPtrDetection> wc_left_;
  raw_ptr<content::WebContents, DisableDanglingPtrDetection> wc_right_;
  TaskId task_id_left_;
  TaskId task_id_right_;
};

IN_PROC_BROWSER_TEST_F(ActorUiHandoffButtonSplitViewTest,
                       OmniboxFocusHidesButtonInOtherPane) {
  StartActingOnTab(wc_left_, task_id_left_);
  StartActingOnTab(wc_right_, task_id_right_);

  RunTestSequence(
      PollVisibleHandoffButtons(),
      // Wait for BOTH Handoff buttons to be visible.
      WaitForState(kVisibleHandoffButtonCountState, 2),
      // Focus Omnibox in Left Pane
      InContext(left_pane_context_, FocusElement(kOmniboxElementId)),
      InContext(left_pane_context_, EnterText(kOmniboxElementId, u"test")),
      // Check that both Handoff Buttons hide.
      WaitForState(kVisibleHandoffButtonCountState, 0),
      // Clear focus in Left Pane
      InContext(left_pane_context_,
                WithView(kOmniboxElementId,
                         [](OmniboxViewViews* view) {
                           view->RevertAll();
                           view->GetFocusManager()->ClearFocus();
                         })),
      InContext(left_pane_context_,
                FocusElement(ContentsWebView::kContentsWebViewElementId)),
      // Check that both Handoff Buttons re-show.
      WaitForState(kVisibleHandoffButtonCountState, 2),
      // Focus Omnibox in Right Pane
      InContext(right_pane_context_, FocusElement(kOmniboxElementId)),
      InContext(right_pane_context_, EnterText(kOmniboxElementId, u"test")),
      // Check that both Handoff Buttons hide.
      WaitForState(kVisibleHandoffButtonCountState, 0),
      // Clear focus in Right Pane
      InContext(right_pane_context_,
                WithView(kOmniboxElementId,
                         [](OmniboxViewViews* view) {
                           view->RevertAll();
                           view->GetFocusManager()->ClearFocus();
                         })),
      InContext(right_pane_context_,
                FocusElement(ContentsWebView::kContentsWebViewElementId)),
      // Check that both Handoff Buttons re-show.
      WaitForState(kVisibleHandoffButtonCountState, 2));
}

}  // namespace
}  // namespace actor::ui
