// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/handoff_button_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/button/label_button.h"

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
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicActor, {}},
         {features::kGlicActorUi,
          {{features::kGlicActorUiHandoffButtonName, "true"}}}},
        /*disabled_features=*/{});
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
                                           result_future.GetCallback());
    ExpectOkResult(result_future);
  }

  auto HoverOverlay(bool is_hovering) {
    return Do([=, this]() {
      ActorUiTabController::From(browser()->tab_strip_model()->GetActiveTab())
          ->SetHandoffButtonHoverStatus(is_hovering);
    });
  }

 protected:
  TaskId task_id_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorUiHandoffButtonControllerInteractiveUiTest,
                       WidgetIsCreatedAndDestroyed) {
  StartActingOnTab();
  RunTestSequence(
      HoverOverlay(true),
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
                       ButtonClickToPauseTaskKeepsButtonVisibleWithNoHover) {
  StartActingOnTab();
  RunTestSequence(
      HoverOverlay(true),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)),
      InAnyContext(
          PressButton(HandoffButtonController::kHandoffButtonElementId)),
      HoverOverlay(false),
      // Button stays visible since the client is in control.
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)));
}

IN_PROC_BROWSER_TEST_F(ActorUiHandoffButtonControllerInteractiveUiTest,
                       ButtonTextChangesOnClick) {
  StartActingOnTab();
  RunTestSequence(
      HoverOverlay(true),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)),
      InAnyContext(
          CheckViewProperty(HandoffButtonController::kHandoffButtonElementId,
                            &views::LabelButton::GetText, TAKE_OVER_TASK_TEXT)),
      // Start polling the button's text property.
      InAnyContext(PollViewProperty(
          kButtonTextState, HandoffButtonController::kHandoffButtonElementId,
          &views::LabelButton::GetText)),
      InAnyContext(
          PressButton(HandoffButtonController::kHandoffButtonElementId)),
      // Verify the text change on the button. This waits for the
      // notification chain and UI update to complete.
      WaitForState(kButtonTextState, GIVE_TASK_BACK_TEXT));
}

IN_PROC_BROWSER_TEST_F(ActorUiHandoffButtonControllerInteractiveUiTest,
                       ButtonHidesAndReshowsOnTabSwitch) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
  StartActingOnTab();
  RunTestSequence(
      HoverOverlay(true),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)),
      // Switch to the second tab.
      AddInstrumentedTab(kSecondTab, GURL("about:blank")),
      InAnyContext(
          WaitForHide(HandoffButtonController::kHandoffButtonElementId)),
      // Switch back to the first tab.
      SelectTab(kTabStripElementId, 0), HoverOverlay(true),
      InAnyContext(
          WaitForShow(HandoffButtonController::kHandoffButtonElementId)));
}

}  // namespace
}  // namespace actor::ui
