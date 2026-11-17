// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/json/string_escape.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_management_interactive_test_base.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_test_user_variation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "third_party/omnibox_proto/aim_eligibility_response.pb.h"

namespace contextual_tasks {

namespace {

DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kMenuItemsDisabledReadyEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kFlyoutTabDisabledReadyEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kInputClearedEvent);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kZeroStateChangedEvent);

// Holds `GetPageContext()` in flight until `CompletePendingPageContextRequest`
// is called, reproducing the window between selecting a tab in the UI and
// `TabContextualizationController::GetPageContext()` finishing its asynchronous
// page content and screenshot capture.
class StalledPageContextTabContextualizationController
    : public TestTabContextualizationController {
 public:
  explicit StalledPageContextTabContextualizationController(
      tabs::TabInterface* tab)
      : TestTabContextualizationController(tab) {}
  ~StalledPageContextTabContextualizationController() override = default;

  using TestTabContextualizationController::GetPageContext;
  void GetPageContext(GetPageContextCallback callback) override {
    if (stall_page_context_) {
      pending_callback_ = std::move(callback);
      return;
    }
    TestTabContextualizationController::GetPageContext(std::move(callback));
  }

  bool HasPendingPageContextRequest() const {
    return !pending_callback_.is_null();
  }

  void CompletePendingPageContextRequest() {
    stall_page_context_ = false;
    if (pending_callback_) {
      TestTabContextualizationController::GetPageContext(
          std::move(pending_callback_));
    }
  }

 private:
  bool stall_page_context_ = true;
  GetPageContextCallback pending_callback_;
};

}  // namespace

class ContextualTasksContextManagementInteractiveUiTest
    : public ContextualTasksContextManagementInteractiveTestBase,
      public testing::WithParamInterface<UserVariation> {
 public:
  ContextualTasksContextManagementInteractiveUiTest() = default;
  ~ContextualTasksContextManagementInteractiveUiTest() override = default;

  UserVariation GetUserVariation() const override { return GetParam(); }

 protected:
  // Side-panel-only DeepQuery paths.
  const DeepQuery kComposeboxInput = {"contextual-tasks-app", "#composebox",
                                      "#composebox", "#composeboxInput",
                                      "#input"};

  const DeepQuery kSubmitButton = {"contextual-tasks-app", "#composebox",
                                   "#composebox", "cr-composebox-submit",
                                   "#submitContainer"};

  const DeepQuery kNewThreadButton = {"contextual-tasks-app", "#toolbar",
                                      "#newThreadButton"};

  const DeepQuery kThreadFrame = {"contextual-tasks-app", "#threadFrame"};

  // Verifies whether `#imageUpload`, `#fileUpload`, tool buttons, and the
  // incompatible model (`MODEL_MODE_GEMINI_REGULAR`) in the `+` context menu
  // match `expected_disabled`.
  auto VerifyIncompatibleMenuItemsDisabled(
      const ui::ElementIdentifier& contents_id,
      bool expected_disabled) {
    StateChange state;
    state.type = StateChange::Type::kExistsAndConditionTrue;
    state.where = kContextMenu;
    state.test_function = base::StringPrintf(
        R"(
        el => {
          const root = el.shadowRoot || el;
          const imageUpload = root.querySelector('#imageUpload');
          const fileUpload = root.querySelector('#fileUpload');
          const toolBtns = Array.from(
              root.querySelectorAll('button.dropdown-item[data-mode]'));
          const regularModelBtn = root.querySelector(
              'button.dropdown-item[data-model="%d"]');
          if (!imageUpload || !fileUpload || toolBtns.length === 0 ||
              !regularModelBtn) {
            return false;
          }
          const expected = %s;
          return imageUpload.disabled === expected &&
                 fileUpload.disabled === expected &&
                 toolBtns.every(btn => btn.disabled === expected) &&
                 regularModelBtn.disabled === expected;
        }
        )",
        static_cast<int>(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR),
        expected_disabled ? "true" : "false");
    state.event = kMenuItemsDisabledReadyEvent;
    return WaitForStateChange(contents_id, state);
  }

  // Verifies whether a specific tab button in the Share Tabs flyout is
  // disabled.
  auto VerifyFlyoutTabDisabled(const ui::ElementIdentifier& contents_id,
                               std::string_view matcher,
                               bool expected_disabled) {
    StateChange state;
    state.type = StateChange::Type::kExistsAndConditionTrue;
    state.where = kShareTabsFlyout;
    state.test_function = base::StringPrintf(
        R"(
        el => {
          %s
          const btn = findTabBtn(el, %s);
          if (!btn) {
            return false;
          }
          return btn.disabled === %s;
        }
        )",
        kFindFlyoutTabButtonJs, base::GetQuotedJSONString(matcher).c_str(),
        expected_disabled ? "true" : "false");
    state.event = kFlyoutTabDisabledReadyEvent;
    return WaitForStateChange(contents_id, state);
  }

  // Types and submits a text query in the side panel composebox.
  auto SubmitSidePanelQuery(const ui::ElementIdentifier& contents_id,
                            const std::string& query) {
    const DeepQuery kSubmitButtonHost = {"contextual-tasks-app", "#composebox",
                                         "#composebox", "cr-composebox-submit"};

    StateChange submit_enabled;
    submit_enabled.type = StateChange::Type::kExistsAndConditionTrue;
    submit_enabled.where = kSubmitButtonHost;
    submit_enabled.test_function = "el => !el.disabled";
    submit_enabled.event = kSubmitEnabledEvent;

    StateChange input_cleared;
    input_cleared.type = StateChange::Type::kExistsAndConditionTrue;
    input_cleared.where = kComposeboxInput;
    input_cleared.test_function = "el => !el.value || el.value === ''";
    input_cleared.event = kInputClearedEvent;

    StateChange tabs_submitted;
    tabs_submitted.type = StateChange::Type::kExistsAndConditionTrue;
    tabs_submitted.where = kComposeboxContainer;
    tabs_submitted.test_function = R"(el => {
      const count = el.addedTabsIds ? el.addedTabsIds.size : 0;
      const restored = el.aimThreadRestoredTabs || [];
      return restored.length >= count;
    })";
    tabs_submitted.event = kUploadsCompleteEvent;

    return Steps(WaitForElementExists(contents_id, kComposeboxInput),
                 ExecuteJsAt(contents_id, kComposeboxInput,
                             base::StringPrintf(
                                 R"((el) => {
                  el.value = %s;
                  el.dispatchEvent(new Event('input', { bubbles: true }));
                  el.dispatchEvent(new Event('change', { bubbles: true }));
                })",
                                 base::GetQuotedJSONString(query).c_str())),
                 WaitForStateChange(contents_id, submit_enabled),
                 ExecuteJsAt(contents_id, kSubmitButton, "el => el.click()"),
                 WaitForStateChange(contents_id, input_cleared),
                 WaitForStateChange(contents_id, tabs_submitted));
  }

  // Commits an AIM thread URL with `q` and `mtid` inside `#threadFrame` so
  // `ContextualTasksUI::FrameNavObserver` transitions the current task out of
  // zero state and associates it with `thread_id`.
  auto CommitActiveThreadUrlInSidePanel(
      const ui::ElementIdentifier& contents_id,
      const std::string& query,
      const std::string& thread_id) {
    StateChange initial_load_done;
    initial_load_done.type = StateChange::Type::kExistsAndConditionTrue;
    initial_load_done.where = {"contextual-tasks-app"};
    initial_load_done.test_function =
        "el => el.hasFinishedTopLevelNavigationForTesting_ === true && "
        "!el.pendingUrl_";
    initial_load_done.event = kElementExistsEvent;

    StateChange non_zero_state;
    non_zero_state.type = StateChange::Type::kExistsAndConditionTrue;
    non_zero_state.where = {"contextual-tasks-app"};
    non_zero_state.test_function = base::StringPrintf(
        "el => el.isZeroState_ === false && !el.isFrameLoading && "
        "window.location.href.includes(%s)",
        base::GetQuotedJSONString("mtid=" + thread_id).c_str());
    non_zero_state.event = kZeroStateChangedEvent;

    const std::string thread_url =
        base::StringPrintf("https://www.google.com/search?udm=50&q=%s&mtid=%s",
                           query.c_str(), thread_id.c_str());

    return Steps(
        WaitForStateChange(contents_id, initial_load_done),
        ExecuteJsAt(
            contents_id, kThreadFrame,
            base::StringPrintf("el => { el.src = %s; }",
                               base::GetQuotedJSONString(thread_url).c_str())),
        WaitForStateChange(contents_id, non_zero_state));
  }

  // Clicks the New Thread button in the side panel toolbar and waits for the
  // app to transition back to zero state on the new thread.
  auto StartNewThreadFromSidePanel(const ui::ElementIdentifier& contents_id) {
    StateChange zero_state;
    zero_state.type = StateChange::Type::kExistsAndConditionTrue;
    zero_state.where = {"contextual-tasks-app"};
    zero_state.test_function =
        "el => el.isZeroState_ === true && !el.isFrameLoading && "
        "!el.pendingUrl_";
    zero_state.event = kZeroStateChangedEvent;
    zero_state.continue_across_navigation = true;

    return Steps(WaitForElementExists(contents_id, kNewThreadButton),
                 ExecuteJsAt(contents_id, kNewThreadButton, "el => el.click()"),
                 WaitForStateChange(contents_id, zero_state));
  }
};

// --- Test 1: Synchronous UI consistency for tab sign posting when attached and
// detached ---
IN_PROC_BROWSER_TEST_P(ContextualTasksContextManagementInteractiveUiTest,
                       TabSignPostingConsistency_AttachAndDetach) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab2);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");

  RunTestSequence(
      // Setup 3 tabs: Tab 0 active, Tab 1 and Tab 2 in background.
      InstrumentTab(kPrimaryTab, 0), AddInstrumentedTab(kBackgroundTab1, kUrl1),
      AddInstrumentedTab(kBackgroundTab2, kUrl2),
      SelectTab(kTabStripElementId, 0), OpenSidePanelWithWebContents(),

      // Initial 0-tabs steady state across all elements.
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 0),
      VerifyUnderlinedTabs({}), OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 0),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", false),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", false),

      // Select Tab 1.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      VerifyUnderlinedTabs({1}),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 1),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", false),

      // Select Tab 2.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title2"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 2),
      VerifyUnderlinedTabs({1, 2}),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 2),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 2),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", true),

      // Deselect Tab 1.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      VerifyUnderlinedTabs({2}),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 1),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", false),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", true));
}

IN_PROC_BROWSER_TEST_P(
    ContextualTasksContextManagementInteractiveUiTest,
    ClearInputPlate_RemovesUnsubmittedTabUnderlinesAndPreservesSubmittedTabs) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab2);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");
  const DeepQuery kCancelIcon = {"contextual-tasks-app", "#composebox",
                                 "#composebox", "#composeboxInput",
                                 "#cancelIcon"};

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), AddInstrumentedTab(kBackgroundTab1, kUrl1),
      AddInstrumentedTab(kBackgroundTab2, kUrl2),
      SelectTab(kTabStripElementId, 0), OpenSidePanelWithWebContents(),

      // Attach Tab 1 without submitting, then click the cancel button to clear
      // the uncommitted input plate. Its tab-strip underline must be removed.
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      VerifyUnderlinedTabs({1}),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),
      WaitForElementExists(kSidePanelWebContentsId, kCancelIcon),
      ExecuteJsAt(kSidePanelWebContentsId, kCancelIcon, "el => el.click()"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 0),
      VerifyUnderlinedTabs({}),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 0),

      // Re-attach Tab 1 and submit Turn 1.
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      VerifyUnderlinedTabs({1}),
      SubmitSidePanelQuery(kSidePanelWebContentsId, "First turn query"),
      VerifyUnderlinedTabs({1}),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),

      // In Turn 2, attach Tab 2 without submitting, then click the cancel
      // button. Tab 2's uncommitted underline is removed while Tab 1's
      // submitted underline and coin are preserved.
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      ToggleFlyoutTab(kSidePanelWebContentsId, "title2"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      VerifyUnderlinedTabs({1, 2}),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 2),
      WaitForElementExists(kSidePanelWebContentsId, kCancelIcon),
      ExecuteJsAt(kSidePanelWebContentsId, kCancelIcon, "el => el.click()"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 0),
      VerifyUnderlinedTabs({1}),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),
      VerifyPlusButtonCoinTabs(kSidePanelWebContentsId, {"title1"}));
}

// --- Test 2: Turn 2 with no new tabs retains existing context across all
// elements ---
IN_PROC_BROWSER_TEST_P(ContextualTasksContextManagementInteractiveUiTest,
                       MultiTurn_NoNewTabsAdded_PersistsAcrossTurns) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab1);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), AddInstrumentedTab(kBackgroundTab1, kUrl1),
      SelectTab(kTabStripElementId, 0), OpenSidePanelWithWebContents(),

      // Turn 1: Attach Tab 1 and submit
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      VerifyUnderlinedTabs({1}),
      SubmitSidePanelQuery(kSidePanelWebContentsId, "First turn query"),

      // Turn 2: Submit follow-up without touching tabs
      SubmitSidePanelQuery(kSidePanelWebContentsId, "Second turn follow-up"),

      // Assert Tab 1 context persists across turns
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),
      VerifyUnderlinedTabs({1}), OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 1),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", true));
}

// Adding a new tab in Turn 2 on top of a tab submitted in Turn 1 signposts both
// tabs before and after Turn 2 submission across all four surfaces.
IN_PROC_BROWSER_TEST_P(ContextualTasksContextManagementInteractiveUiTest,
                       MultiTurn_AddNewTabInTurn2_PersistsBothTabs) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab2);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), AddInstrumentedTab(kBackgroundTab1, kUrl1),
      AddInstrumentedTab(kBackgroundTab2, kUrl2),
      SelectTab(kTabStripElementId, 0), OpenSidePanelWithWebContents(),

      // Turn 1: Attach Tab 1 and submit.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      VerifyUnderlinedTabs({1}),
      SubmitSidePanelQuery(kSidePanelWebContentsId, "First turn query"),
      CommitActiveThreadUrlInSidePanel(kSidePanelWebContentsId,
                                       "First+turn+query", "thread-1"),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),
      VerifyUnderlinedTabs({1}),

      // Turn 2: Attach Tab 2 and verify pre-submit signposting for both tabs.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title2"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 2),
      VerifyPlusButtonCoinTabs(kSidePanelWebContentsId, {"title1", "title2"}),
      VerifyUnderlinedTabs({1, 2}),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 2),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", true),

      // Submit Turn 2 and verify both tabs remain signposted post-submit.
      SubmitSidePanelQuery(kSidePanelWebContentsId, "Second turn query"),
      CommitActiveThreadUrlInSidePanel(kSidePanelWebContentsId,
                                       "Second+turn+query", "thread-1"),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 2),
      VerifyPlusButtonCoinTabs(kSidePanelWebContentsId, {"title1", "title2"}),
      VerifyUnderlinedTabs({1, 2}),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 2),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", true));
}

// Deselecting a previously submitted tab in Turn 2 immediately removes its sign
// posting pre-submit and keeps it removed after Turn 2 is submitted.
IN_PROC_BROWSER_TEST_P(
    ContextualTasksContextManagementInteractiveUiTest,
    MultiTurn_DeselectSubmittedTabInTurn2_RemovesFromContext) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab2);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), AddInstrumentedTab(kBackgroundTab1, kUrl1),
      AddInstrumentedTab(kBackgroundTab2, kUrl2),
      SelectTab(kTabStripElementId, 0), OpenSidePanelWithWebContents(),

      // Turn 1: Attach Tab 1 and Tab 2 and submit.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      ToggleFlyoutTab(kSidePanelWebContentsId, "title2"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 2),
      VerifyUnderlinedTabs({1, 2}),
      SubmitSidePanelQuery(kSidePanelWebContentsId, "First turn query"),
      CommitActiveThreadUrlInSidePanel(kSidePanelWebContentsId,
                                       "First+turn+query", "thread-1"),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 2),
      VerifyUnderlinedTabs({1, 2}),

      // Turn 2: Deselect previously submitted Tab 1 and verify pre-submit
      // removal across all four signposting surfaces.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),
      VerifyPlusButtonCoinTabs(kSidePanelWebContentsId, {"title2"}),
      VerifyUnderlinedTabs({2}), OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 1),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", false),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", true),

      // Submit Turn 2 and verify only Tab 2 persists post-submit.
      SubmitSidePanelQuery(kSidePanelWebContentsId, "Second turn query"),
      CommitActiveThreadUrlInSidePanel(kSidePanelWebContentsId,
                                       "Second+turn+query", "thread-1"),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),
      VerifyPlusButtonCoinTabs(kSidePanelWebContentsId, {"title2"}),
      VerifyUnderlinedTabs({2}), OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 1),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", false),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", true));
}

// Navigating an attached tab to a new URL or closing an attached tab removes
// its tab-strip underline and removes it from the composebox context.
IN_PROC_BROWSER_TEST_P(
    ContextualTasksContextManagementInteractiveUiTest,
    AttachedTabNavigatedOrClosed_RemovesUnderlineAndContext) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab2);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");
  const GURL kUrl3 = embedded_test_server()->GetURL("/title3.html");

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), AddInstrumentedTab(kBackgroundTab1, kUrl1),
      AddInstrumentedTab(kBackgroundTab2, kUrl2),
      SelectTab(kTabStripElementId, 0), OpenSidePanelWithWebContents(),

      // Attach both background tabs.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      ToggleFlyoutTab(kSidePanelWebContentsId, "title2"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 2),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 2),
      VerifyUnderlinedTabs({1, 2}),

      // Navigate attached Tab 1 to a new URL (`kUrl3`). Its underline and
      // composebox context attachment must be removed while Tab 2 remains.
      NavigateWebContents(kBackgroundTab1, kUrl3),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),
      VerifyPlusButtonCoinTabs(kSidePanelWebContentsId, {"title2"}),
      VerifyUnderlinedTabs({2}), OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 1),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title3", false),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", true),

      // Close attached Tab 2. All signposting must return to the empty state.
      Do([this]() { browser()->GetAllTabInterfaces()[2]->Close(); }),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 0),
      VerifyUnderlinedTabs({}), OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 0));
}

// Verifies that selecting a tab immediately disables incompatible "+" context
// menu items via eager rule evaluation while
// `TabContextualizationController::GetPageContext()` is still in flight, and
// that the selected tab itself stays enabled so it can be deselected.
IN_PROC_BROWSER_TEST_P(
    ContextualTasksContextManagementInteractiveUiTest,
    EagerTabRuleEvaluation_DisablesMenuItemsWhileGetPageContextPending) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab2);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");

  raw_ptr<StalledPageContextTabContextualizationController> stalled_controller =
      nullptr;

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), AddInstrumentedTab(kBackgroundTab1, kUrl1),
      AddInstrumentedTab(kBackgroundTab2, kUrl2),
      SelectTab(kTabStripElementId, 0), Do([this, &stalled_controller]() {
        // Configure rules so that:
        // - `MODEL_MODE_GEMINI_PRO` is the default active model (allows all
        //   tools and inputs).
        // - `MODEL_MODE_GEMINI_REGULAR`, `TOOL_MODE_DEEP_SEARCH`, and
        //   `TOOL_MODE_CANVAS` do not allow `INPUT_TYPE_BROWSER_TAB`.
        // - `max_total_inputs` is 1, so attaching 1 tab also disables
        //   `#imageUpload`, `#fileUpload`, and unselected tabs in the flyout.
        auto* config =
            &GetMockAimEligibilityService(browser()->GetProfile())->config();

        auto* pro_model = config->add_model_configs();
        pro_model->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
        pro_model->set_menu_label("Thinking");
        auto* pro_rule = pro_model->mutable_rule();
        pro_rule->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
        pro_rule->set_allow_all_tools(true);
        pro_rule->set_allow_all_input_types(true);

        auto* regular_model = config->add_model_configs();
        regular_model->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
        regular_model->set_menu_label("Fast");
        auto* regular_rule = regular_model->mutable_rule();
        regular_rule->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
        regular_rule->set_allow_all_tools(true);
        regular_rule->add_allowed_input_types(
            omnibox::InputType::INPUT_TYPE_LENS_IMAGE);

        auto* ds_tool = config->add_tool_configs();
        ds_tool->set_tool(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
        ds_tool->mutable_rule()->set_tool(
            omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);

        auto* canvas_tool = config->add_tool_configs();
        canvas_tool->set_tool(omnibox::ToolMode::TOOL_MODE_CANVAS);
        auto* canvas_rule = canvas_tool->mutable_rule();
        canvas_rule->set_tool(omnibox::ToolMode::TOOL_MODE_CANVAS);
        canvas_rule->add_allowed_input_types(
            omnibox::InputType::INPUT_TYPE_LENS_IMAGE);

        auto* rule_set = config->mutable_rule_set();
        rule_set->add_allowed_tools(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);
        rule_set->add_allowed_tools(omnibox::ToolMode::TOOL_MODE_CANVAS);
        rule_set->add_allowed_models(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
        rule_set->add_allowed_models(
            omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
        rule_set->add_allowed_input_types(
            omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
        rule_set->add_allowed_input_types(
            omnibox::InputType::INPUT_TYPE_LENS_IMAGE);
        rule_set->add_allowed_input_types(
            omnibox::InputType::INPUT_TYPE_LENS_FILE);
        rule_set->set_max_total_inputs(1);

        // Install the stalled controller on Tab 1 so `GetPageContext()` stays
        // in flight when Tab 1 is selected.
        tabs::TabInterface* tab1 = browser()->GetAllTabInterfaces()[1];
        tab1->GetTabFeatures()->SetTabContextualizationControllerForTesting(
            nullptr);
        auto controller =
            std::make_unique<StalledPageContextTabContextualizationController>(
                tab1);
        stalled_controller = controller.get();
        tab1->GetTabFeatures()->SetTabContextualizationControllerForTesting(
            std::move(controller));
      }),
      OpenSidePanelWithWebContents(),

      // 1. Before selecting a tab: all "+" menu items and tabs are enabled.
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyIncompatibleMenuItemsDisabled(kSidePanelWebContentsId, false),
      VerifyFlyoutTabDisabled(kSidePanelWebContentsId, "title1", false),
      VerifyFlyoutTabDisabled(kSidePanelWebContentsId, "title2", false),

      // 2. Select Tab 1 while `GetPageContext()` is stalled in flight: all
      // incompatible "+" menu items and the unselected tab ("title2") must
      // immediately become disabled via eager rule evaluation, before
      // `GetPageContext()` completes, while the selected tab ("title1") stays
      // enabled so it can be deselected.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      CheckResult(
          [this, &stalled_controller]() {
            return RunUntilNestable([&stalled_controller]() {
              return stalled_controller->HasPendingPageContextRequest();
            });
          },
          true),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyIncompatibleMenuItemsDisabled(kSidePanelWebContentsId, true),
      VerifyFlyoutTabDisabled(kSidePanelWebContentsId, "title2", true),
      VerifyFlyoutTabDisabled(kSidePanelWebContentsId, "title1", false),

      // 3. Complete `GetPageContext()` and wait for the tab upload to finish:
      // incompatible "+" menu items remain disabled while Tab 1 is attached.
      Do([&stalled_controller]() {
        stalled_controller->CompletePendingPageContextRequest();
        stalled_controller = nullptr;
      }),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyIncompatibleMenuItemsDisabled(kSidePanelWebContentsId, true),

      // 4. Deselect Tab 1: incompatible "+" menu items and "title2" re-enable.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 0),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyIncompatibleMenuItemsDisabled(kSidePanelWebContentsId, false),
      VerifyFlyoutTabDisabled(kSidePanelWebContentsId, "title2", false));
}

// Starting a new thread clears all tabs submitted in the previous thread while
// showing the auto-suggested active tab on the new thread.
IN_PROC_BROWSER_TEST_P(ContextualTasksContextManagementInteractiveUiTest,
                       NewThread_ClearsPreviousThreadTabs) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab2);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");
  const GURL kUrl3 = embedded_test_server()->GetURL("/title3.html");

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), NavigateWebContents(kPrimaryTab, kUrl3),
      AddInstrumentedTab(kBackgroundTab1, kUrl1),
      AddInstrumentedTab(kBackgroundTab2, kUrl2),
      SelectTab(kTabStripElementId, 0), OpenSidePanelWithWebContents(),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),

      // Previous thread: Attach both background tabs in addition to the
      // auto-suggested active tab (title3) and submit a query.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 2),
      ToggleFlyoutTab(kSidePanelWebContentsId, "title2"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 3),
      SubmitSidePanelQuery(kSidePanelWebContentsId, "First turn query"),
      CommitActiveThreadUrlInSidePanel(kSidePanelWebContentsId,
                                       "First+turn+query", "thread-1"),

      // Verify all 3 tabs remain active in the previous thread after
      // submission.
      VerifyPlusButtonCoinTabs(kSidePanelWebContentsId,
                               {"title1", "title2", "title3"}),
      VerifyUnderlinedTabs({0, 1, 2}),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 3),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title3", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", true),

      // Transition to a new thread via the toolbar New Thread button.
      StartNewThreadFromSidePanel(kSidePanelWebContentsId),

      // Verify previous-thread tabs (title1 and title2) are cleared and only
      // the auto-suggested active tab (title3) is shown on the new thread.
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      VerifyPlusButtonCoinTabs(kSidePanelWebContentsId, {"title3"}),
      VerifyUnderlinedTabs({0}), OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 1),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title3", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", false),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", false));
}

// Adding and removing tabs from the side panel's "Add tabs" flyout signposts
// each tab consistently in all four places: favicon coins on the "+" button,
// coins next to "Sharing n tabs" on the menu trigger row, a checkmark on the
// tab's flyout row, and an underline on the tab strip.
IN_PROC_BROWSER_TEST_P(ContextualTasksContextManagementInteractiveUiTest,
                       NewThread_AddAndRemoveTabs_SignpostsConsistently) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab1);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBackgroundTab2);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");
  const GURL kUrl3 = embedded_test_server()->GetURL("/title3.html");

  RunTestSequence(
      // Tab 0 (title3) is the active, auto-suggested tab. Tabs 1 (title1) and
      // 2 (title2) are background candidates. Navigate tab 0 before opening
      // the panel so the auto-suggestion is picked up on panel open rather
      // than racing a navigation against the composebox handler binding.
      InstrumentTab(kPrimaryTab, 0), NavigateWebContents(kPrimaryTab, kUrl3),
      AddInstrumentedTab(kBackgroundTab1, kUrl1),
      AddInstrumentedTab(kBackgroundTab2, kUrl2),
      SelectTab(kTabStripElementId, 0), OpenSidePanelWithWebContents(),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),

      // Previous thread: attach tab 1 on top of the auto-suggested tab 0 and
      // submit, so the thread switch has something to clear.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 2),
      VerifyUnderlinedTabs({0, 1}),
      SubmitSidePanelQuery(kSidePanelWebContentsId, "First turn query"),
      CommitActiveThreadUrlInSidePanel(kSidePanelWebContentsId,
                                       "First+turn+query", "thread-1"),

      // Start a new thread. Only the auto-suggested active tab should carry
      // over; tab 1's underline and all other sign posting must be gone.
      StartNewThreadFromSidePanel(kSidePanelWebContentsId),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 1),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),
      VerifyPlusButtonCoinTabs(kSidePanelWebContentsId, {"title3"}),
      VerifyUnderlinedTabs({0}), OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 1),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title3", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", false),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", false),

      // Add tab 2 on the new thread. It must be signposted everywhere,
      // including a new underline on the tab strip.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title2"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 2),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 2),
      VerifyPlusButtonCoinTabs(kSidePanelWebContentsId, {"title3", "title2"}),
      VerifyUnderlinedTabs({0, 2}),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 2),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title3", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", false),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", true),

      // Re-add tab 1, which the thread switch cleared. Nothing stale from the
      // previous thread should prevent it from being signposted again.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 3),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 3),
      VerifyPlusButtonCoinTabs(kSidePanelWebContentsId,
                               {"title3", "title1", "title2"}),
      VerifyUnderlinedTabs({0, 1, 2}),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 3),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title3", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", true),

      // Remove tab 2 again. Only its sign posting should disappear; tab 0 and
      // tab 1 must stay underlined and checked.
      ToggleFlyoutTab(kSidePanelWebContentsId, "title2"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 2),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 2),
      VerifyPlusButtonCoinTabs(kSidePanelWebContentsId, {"title3", "title1"}),
      VerifyUnderlinedTabs({0, 1}),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 2),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title3", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", false),

      // Submit on the new thread. The context assembled on this thread must
      // survive the turn with the same sign posting.
      SubmitSidePanelQuery(kSidePanelWebContentsId, "New thread query"),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 2),
      VerifyPlusButtonCoinTabs(kSidePanelWebContentsId, {"title3", "title1"}),
      VerifyUnderlinedTabs({0, 1}),
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 2),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title3", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", false));
}

IN_PROC_BROWSER_TEST_P(ContextualTasksContextManagementInteractiveUiTest,
                       ContextPersistsAfterSwitchingToAttachedTab) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAttachedTab);

  const GURL kUrl0 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl1 = embedded_test_server()->GetURL("/title2.html");

  content::WebContents* panel_contents_before_switch = nullptr;

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), NavigateWebContents(kPrimaryTab, kUrl0),
      AddInstrumentedTab(kAttachedTab, kUrl1), SelectTab(kTabStripElementId, 0),
      OpenSidePanelWithWebContents(),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 1),
      VerifyUnderlinedTabs({0}),

      // Attach Tab 1 as context from Tab 0's panel. Both Tab 0 and Tab 1
      // should now be in context in the side panel.
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      ToggleFlyoutTab(kSidePanelWebContentsId, "title2"),
      WaitForFileUploadsComplete(kSidePanelWebContentsId, 2),
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 2),
      VerifyUnderlinedTabs({0, 1}),

      Do([this, &panel_contents_before_switch]() {
        panel_contents_before_switch =
            ContextualTasksPanelController::From(browser())
                ->GetActiveWebContents();
      }),

      // Switch to the attached tab. It shares Tab 0's task, so the panel must
      // keep showing the very same WebContents and keep both Tab 0 and Tab 1
      // in context without requiring a query submission first.
      SelectTab(kTabStripElementId, 1),
      CheckResult(
          [this]() {
            return ContextualTasksPanelController::From(browser())
                ->IsPanelOpenForContextualTask();
          },
          true, "Side panel should remain open on the attached tab"),
      CheckResult(
          [this, &panel_contents_before_switch]() {
            return ContextualTasksPanelController::From(browser())
                       ->GetActiveWebContents() == panel_contents_before_switch;
          },
          true, "Tabs in the same task must share one panel WebContents"),

      // Both Tab 0 and Tab 1 must still be reflected in the side panel context.
      VerifyPlusButtonCoins(kSidePanelWebContentsId, 2),
      VerifyUnderlinedTabs({0, 1}), OpenShareTabsFlyout(kSidePanelWebContentsId),
      VerifyMenuTriggerState(kSidePanelWebContentsId, 2),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title1", true),
      VerifyFlyoutTabChecked(kSidePanelWebContentsId, "title2", true));
}

// Context uploads never complete, reproducing real network timing where the
// user switches to a tab in the window between attaching it and its upload
// finishing.
class ContextualTasksStalledUploadInteractiveUiTest
    : public ContextualTasksContextManagementInteractiveTestBase,
      public testing::WithParamInterface<UserVariation> {
 public:
  ContextualTasksStalledUploadInteractiveUiTest() = default;
  ~ContextualTasksStalledUploadInteractiveUiTest() override = default;

  UserVariation GetUserVariation() const override { return GetParam(); }
  bool ShouldStallContextUploads() const override { return true; }
};

IN_PROC_BROWSER_TEST_P(ContextualTasksStalledUploadInteractiveUiTest,
                       PanelFollowsTabAttachedWhileUploadInFlight) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAttachedTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kControlTab);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), AddInstrumentedTab(kAttachedTab, kUrl1),
      AddInstrumentedTab(kControlTab, kUrl2), SelectTab(kTabStripElementId, 0),
      OpenSidePanelWithWebContents(),

      // Attach Tab 1 via the tab picker. Its upload will never complete, so
      // the underline is the only signal that the attach has been processed.
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      VerifyUnderlinedTabs({1}),

      CheckResult([this]() { return TaskIdForTab(0).is_valid(); }, true,
                  "Tab 0 should have a task once the panel is open"),
      CheckResult([this]() { return TaskIdForTab(1) == TaskIdForTab(0); }, true,
                  "Attached tab should share the task while upload is pending"),
      CheckResult([this]() { return TaskIdForTab(2) == TaskIdForTab(0); },
                  false, "CONTROL: an unattached tab must not share the task"),

      // Switching to the attached tab should carry the panel over.
      SelectTab(kTabStripElementId, 1),
      CheckResult(
          [this]() {
            return ContextualTasksPanelController::From(browser())
                ->IsPanelOpenForContextualTask();
          },
          true, "Side panel should follow to a tab attached as context"));
}

IN_PROC_BROWSER_TEST_P(ContextualTasksStalledUploadInteractiveUiTest,
                       TabDisassociatedWhenUploadFails) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAttachedTab);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");

  RunTestSequence(
      InstrumentTab(kPrimaryTab, 0), AddInstrumentedTab(kAttachedTab, kUrl1),
      SelectTab(kTabStripElementId, 0), OpenSidePanelWithWebContents(),

      // Attach Tab 1 via the tab picker while uploads are stalled.
      OpenShareTabsFlyout(kSidePanelWebContentsId),
      ToggleFlyoutTab(kSidePanelWebContentsId, "title1"),
      VerifyUnderlinedTabs({1}),

      CheckResult([this]() { return TaskIdForTab(0).is_valid(); }, true,
                  "Tab 0 should have a task once the panel is open"),
      CheckResult([this]() { return TaskIdForTab(1) == TaskIdForTab(0); }, true,
                  "Attached tab should share the task while upload is pending"),

      // Fail the in-flight upload and verify that Tab 1 is disassociated from
      // the task.
      Do([this]() { FailStalledUploads(); }),
      CheckResult(
          [this]() {
            return RunUntilNestable(
                [this]() { return !TaskIdForTab(1).is_valid(); });
          },
          true,
          "Tab 1 should be disassociated from the task after upload fails"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ContextualTasksContextManagementInteractiveUiTest,
                         testing::Values(UserVariation::kSignedIn,
                                         UserVariation::kSignedOut,
                                         UserVariation::kIncognito),
                         &UserVariationToString);

INSTANTIATE_TEST_SUITE_P(All,
                         ContextualTasksStalledUploadInteractiveUiTest,
                         testing::Values(UserVariation::kSignedIn,
                                         UserVariation::kSignedOut,
                                         UserVariation::kIncognito),
                         &UserVariationToString);

}  // namespace contextual_tasks
