// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/handoff_button_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/widget/glic_view.h"
#endif

namespace actor::ui {
namespace {

using actor::mojom::ActionResultPtr;
using base::test::TestFuture;
using TabHandle = tabs::TabInterface::Handle;
using DeepQuery = ::WebContentsInteractionTestUtil::DeepQuery;

using ButtonTextObserver =
    views::test::PollingViewPropertyObserver<std::u16string,
                                             views::LabelButton>;
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ButtonTextObserver, kButtonTextState);

class ActorUiHandoffButtonControllerInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTest::SetUpCommandLine(command_line);
#if BUILDFLAG(ENABLE_GLIC)
    command_line->AppendSwitch(switches::kGlicDev);
    // Skips FRE experience.
    command_line->AppendSwitch(switches::kGlicAutomation);
#endif
  }

  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        // Use a dummy URL so we don't make a network request.
        {
#if BUILDFLAG(ENABLE_GLIC)
            {features::kGlicURLConfig,
             {{features::kGlicGuestURL.name, "about:blank"}}},
            // Enable kGlic and kTabstripComboButton to allow glic service to be
            // created for testing.
            {features::kGlic, {}},
            {features::kTabstripComboButton, {}},
#endif
            {features::kGlicActor, {}},
            {features::kGlicActorUi,
             {{features::kGlicActorUiHandoffButtonName, "true"}}},
#if BUILDFLAG(IS_MAC)
            {features::kImmersiveFullscreen, {}},
#endif  // BUILDFLAG(IS_MAC)
        },
        /*disabled_features=*/{
#if BUILDFLAG(ENABLE_GLIC)
            features::kGlicDetached
#endif
        });
    InteractiveBrowserTest::SetUp();
  }

  ActorKeyedService* GetActorKeyedService() {
    return ActorKeyedService::Get(browser()->profile());
  }

  void StartActingOnTab() {
    task_id_ = GetActorKeyedService()->CreateTask();
    TestFuture<actor::mojom::ActionResultPtr> future;
    GetActorKeyedService()->GetTask(task_id_)->AddTab(
        browser()->GetActiveTabInterface()->GetHandle(), future.GetCallback());
    ExpectOkResult(future);
    actor::PerformActionsFuture result_future;
    std::vector<std::unique_ptr<actor::ToolRequest>> actions;
    actions.push_back(actor::MakeWaitRequest());
    GetActorKeyedService()->PerformActions(task_id_, std::move(actions),
                                           actor::ActorTaskMetadata(),
                                           result_future.GetCallback());
    ExpectOkResult(result_future);
  }

  auto ClearOmniboxFocus() {
    return WithView(kOmniboxElementId, [](OmniboxViewViews* omnibox_view) {
      omnibox_view->GetFocusManager()->ClearFocus();
    });
  }

#if BUILDFLAG(IS_MAC)
  auto EnterImmersiveFullscreen() {
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
  TaskId task_id_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorUiHandoffButtonControllerInteractiveUiTest,
                       WidgetIsCreatedAndDestroyed) {
  StartActingOnTab();
  RunTestSequence(
      ClearOmniboxFocus(),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)),
      // Trigger the event to destroy the button.
      Do([&]() {
        GetActorKeyedService()->StopTask(task_id_, /*success*/ true);
      }),
      InAnyContext(
          WaitForHide(HandoffButtonController::kHandoffButtonElementId)));
}

IN_PROC_BROWSER_TEST_F(ActorUiHandoffButtonControllerInteractiveUiTest,
                       ButtonClickToPauseTaskKeepsButtonVisible) {
  StartActingOnTab();
  RunTestSequence(
      ClearOmniboxFocus(),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)),
      InAnyContext(
          PressButton(HandoffButtonController::kHandoffButtonElementId)),
      // Button stays visible since the client is in control.
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)));
}

IN_PROC_BROWSER_TEST_F(ActorUiHandoffButtonControllerInteractiveUiTest,
                       ButtonTextChangesOnClick) {
  StartActingOnTab();
  RunTestSequence(
      ClearOmniboxFocus(),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)),
      InAnyContext(CheckViewProperty(
          HandoffButtonController::kHandoffButtonElementId,
          &views::LabelButton::GetText,
          l10n_util::GetStringUTF16(IDS_TAKE_OVER_TASK_LABEL))),
      // Start polling the button's text property.
      InAnyContext(PollViewProperty(
          kButtonTextState, HandoffButtonController::kHandoffButtonElementId,
          &views::LabelButton::GetText)),
      InAnyContext(
          PressButton(HandoffButtonController::kHandoffButtonElementId)),
      // Verify the text change on the button. This waits for the
      // notification chain and UI update to complete.
      WaitForState(kButtonTextState,
                   l10n_util::GetStringUTF16(IDS_GIVE_TASK_BACK_LABEL)));
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
                       ButtonHidesInImmersiveFullscreen) {
  StartActingOnTab();
  RunTestSequence(ClearOmniboxFocus(), Do(EnterImmersiveFullscreen()),
                  Check(IsInImmersiveFullscreen()),
                  // Verify the button does not show.
                  InAnyContext(EnsureNotPresent(
                      HandoffButtonController::kHandoffButtonElementId)));
}
#endif  // BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(ActorUiHandoffButtonControllerInteractiveUiTest,
                       ButtonHidesWhenOmniboxIsFocused) {
  StartActingOnTab();
  RunTestSequence(
      ClearOmniboxFocus(),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)),
      FocusElement(kOmniboxElementId),
      InAnyContext(
          WaitForHide(HandoffButtonController::kHandoffButtonElementId)),
      ClearOmniboxFocus(),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)));
}

#if BUILDFLAG(ENABLE_GLIC)
IN_PROC_BROWSER_TEST_F(ActorUiHandoffButtonControllerInteractiveUiTest,
                       GlicSidePanelTogglesOnWhenButtonClicked) {
  browser()->GetFeatures().side_panel_ui()->SetNoDelaysForTesting(true);
  StartActingOnTab();
  RunTestSequence(
      ClearOmniboxFocus(), EnsureNotPresent(kSidePanelElementId),
      EnsureNotPresent(kGlicViewElementId),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)),
      InAnyContext(
          CheckViewProperty(HandoffButtonController::kHandoffButtonElementId,
                            &views::LabelButton::GetText, TAKE_OVER_TASK_TEXT)),
      InAnyContext(PollViewProperty(
          kButtonTextState, HandoffButtonController::kHandoffButtonElementId,
          &views::LabelButton::GetText)),
      InAnyContext(
          PressButton(HandoffButtonController::kHandoffButtonElementId)),
      WaitForState(kButtonTextState, GIVE_TASK_BACK_TEXT),
      InAnyContext(WaitForShow(kSidePanelElementId)),
      InAnyContext(WaitForShow(kGlicViewElementId)));
}
#endif
}  // namespace
}  // namespace actor::ui
