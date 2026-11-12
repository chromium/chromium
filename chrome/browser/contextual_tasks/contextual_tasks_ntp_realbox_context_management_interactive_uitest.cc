// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <vector>

#include "base/json/string_escape.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_management_interactive_test_base.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_test_user_variation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/common/webui_url_constants.h"
#include "components/contextual_search/pref_names.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/rect.h"

namespace contextual_tasks {

// -----------------------------------------------------------------------------
// NTP realbox-driven context management.
//
// The NTP composebox (`ntp-composebox`) renders the same WebUI
// `cr-composebox-contextual-entrypoint-and-menu` as the side panel, so the
// DeepQuery helpers in the base class apply once rooted at
// `{"ntp-app", "#composebox"}`.
//
// Unlike `NtpRealboxUiTestBase`, this fixture keeps `kContextualTasks`
// enabled: the tab-strip underline is driven by `ActiveTaskContextProvider`,
// which is only attached to the browser window when the Contextual Tasks UI is
// enabled. Without it the underline could never be asserted from the NTP.
// -----------------------------------------------------------------------------
class ContextualTasksNtpRealboxContextManagementInteractiveUiTest
    : public ContextualTasksContextManagementInteractiveTestBase,
      public testing::WithParamInterface<UserVariation> {
 public:
  ContextualTasksNtpRealboxContextManagementInteractiveUiTest()
      : ContextualTasksContextManagementInteractiveTestBase(
            {"ntp-app", "#composebox"}) {}
  ~ContextualTasksNtpRealboxContextManagementInteractiveUiTest() override =
      default;

  UserVariation GetUserVariation() const override { return GetParam(); }

  void SetUpFeatureList() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        GetDefaultEnabledFeatures();
    enabled_features.push_back({omnibox::kContextManagementInComposebox,
                                {{"enable_tab_deselection", "true"}}});
    enabled_features.push_back({omnibox::kTabFaviconChipsToCoins, {}});
    // The realbox-next + composebox surface under test.
    enabled_features.push_back({ntp_realbox::kNtpRealboxNext, {}});
    enabled_features.push_back(
        {ntp_composebox::kNtpComposebox,
         {{ntp_composebox::kContextMenuEnableMultiTabSelection.name, "true"}}});
    enabled_features.push_back({omnibox::kAimEnabled, {}});
    // Lets `IsNtpComposeboxEnabled()` pass without a content-sharing pref.
    enabled_features.push_back({omnibox::kAimUsePecApi, {}});

    std::vector<base::test::FeatureRef> disabled_features =
        GetDefaultDisabledFeatures();
    // Eligibility is supplied by `MockAimEligibilityService`; don't let the
    // real service race it.
    disabled_features.push_back(omnibox::kAimServerEligibilityEnabled);
    disabled_features.push_back(omnibox::kAimFuseboxEligibilityCheckEnabled);
    disabled_features.push_back(omnibox::kComposeboxPersistentAimButtonRealbox);

    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  void SetUpOnMainThread() override {
    ContextualTasksContextManagementInteractiveTestBase::SetUpOnMainThread();

    // The expanded composebox and its flyout need room; a default-sized window
    // can clip the flyout off-screen.
    browser()->GetWindow()->SetBounds(gfx::Rect(0, 0, 1280, 1024));

    // `ContextualSearchboxHandler::AddTabContext()` bails unless content
    // sharing is allowed.
    browser()->GetProfile()->GetPrefs()->SetInteger(
        contextual_search::kSearchContentSharingSettings,
        static_cast<int>(
            contextual_search::SearchContentSharingSettingsValue::kEnabled));

    // `ntp_composebox::IsNtpComposeboxEnabled()` also requires Fusebox
    // eligibility, which the base mock leaves unstubbed (and so false).
    ON_CALL(*GetMockAimEligibilityService(browser()->GetProfile()),
            IsFuseboxEligible())
        .WillByDefault(testing::Return(true));
  }

 protected:
  // --- NTP DeepQuery paths ---
  const DeepQuery kComposeButton = {"ntp-app", "ntp-searchbox",
                                    "#composeButton", "#composeButton"};
  // Only stamped into the DOM while the composebox is expanded.
  const DeepQuery kComposeboxDialog = {"ntp-app", "#composeboxDialog"};

  // Inline `ntp-searchbox` ("classic realbox") context menu paths.
  const DeepQuery kRealboxInlineEntrypoint = {"ntp-app", "ntp-searchbox",
                                              "#context", "#entrypointButton",
                                              "#entrypoint"};
  const DeepQuery kRealboxInlineShareTabsTrigger = {
      "ntp-app", "ntp-searchbox", "#context",
      "cr-composebox-contextual-action-menu", "#shareTabsTrigger"};
  const DeepQuery kRealboxInlineShareTabsFlyout = {
      "ntp-app", "ntp-searchbox", "#context",
      "cr-composebox-contextual-action-menu", ".share-tabs-flyout"};
  const DeepQuery kRealboxComposeboxInput = {
      "ntp-app", "#composebox", "#composebox", "#composeboxInput", "#input"};
  const DeepQuery kRealboxSubmitButtonHost = {
      "ntp-app", "#composebox", "#composebox", "cr-composebox-submit"};
  const DeepQuery kRealboxSubmitButton = {"ntp-app", "#composebox",
                                          "#composebox", "cr-composebox-submit",
                                          "#submitContainer"};

  // Opens an NTP in a new foreground tab instrumented as `contents_id` and
  // expands the realbox into the composebox.
  auto OpenNtpComposebox(const ui::ElementIdentifier& contents_id) {
    return Steps(
        AddInstrumentedTab(contents_id, chrome::ChromeUINewTabURLAsGURL()),
        WaitForElementExists(contents_id, kComposeButton),
        ExecuteJsAt(contents_id, kComposeButton, "el => el.click()"),
        WaitForElementExists(contents_id, kComposeboxDialog),
        WaitForElementExists(contents_id, kComposeboxContainer));
  }

  // Clicks the inline "+" button on the collapsed `ntp-searchbox` (without
  // clicking `#composeButton` first), opens its "Add tabs" flyout, and clicks
  // the tab matching `matcher`, which expands `#composeboxDialog`.
  auto SelectTabFromInlineRealboxPlusMenu(
      const ui::ElementIdentifier& contents_id,
      std::string_view matcher) {
    StateChange menu_open;
    menu_open.type = StateChange::Type::kExistsAndConditionTrue;
    menu_open.where = kRealboxInlineShareTabsTrigger;
    menu_open.test_function = "el => !!el.getRootNode().host?.open";
    menu_open.event = kMenuOpenEvent;

    StateChange flyout_visible;
    flyout_visible.type = StateChange::Type::kExistsAndConditionTrue;
    flyout_visible.where = kRealboxInlineShareTabsFlyout;
    flyout_visible.test_function = "el => !el.hidden";
    flyout_visible.event = kFlyoutVisibleEvent;

    StateChange tab_button_ready;
    tab_button_ready.type = StateChange::Type::kExistsAndConditionTrue;
    tab_button_ready.where = kRealboxInlineShareTabsFlyout;
    tab_button_ready.test_function = base::StringPrintf(
        R"(el => {
          %s
          return !!findTabBtn(el, %s);
        })",
        kFindFlyoutTabButtonJs, base::GetQuotedJSONString(matcher).c_str());
    tab_button_ready.event = kCheckedReadyEvent;

    return Steps(
        WaitForElementExists(contents_id, kRealboxInlineEntrypoint),
        ExecuteJsAt(contents_id, kRealboxInlineEntrypoint, "el => el.click()"),
        WaitForStateChange(contents_id, menu_open),
        ExecuteJsAt(contents_id, kRealboxInlineShareTabsTrigger,
                    R"(el => {
              el.dispatchEvent(
                  new PointerEvent('pointerenter', {bubbles: true}));
              const menu = el.getRootNode().host;
              if (menu) {
                menu.pointerOverFlyout_ = true;
                menu.pointerOverTrigger_ = true;
                if (typeof menu.cancelCloseTimer_ === 'function') {
                  menu.cancelCloseTimer_();
                }
                if (typeof menu.onShareTabsRowPointerenter_ === 'function') {
                  menu.onShareTabsRowPointerenter_();
                }
                if (typeof menu.setShareTabsFlyoutOpen_ === 'function') {
                  menu.setShareTabsFlyoutOpen_(true);
                } else {
                  menu.shareTabsFlyoutOpen = true;
                }
              }
            })"),
        WaitForStateChange(contents_id, flyout_visible),
        WaitForStateChange(contents_id, tab_button_ready),
        ExecuteJsAt(contents_id, kRealboxInlineShareTabsFlyout,
                    base::StringPrintf(
                        R"(el => {
                  %s
                  const btn = findTabBtn(el, %s);
                  btn.click();
                })",
                        kFindFlyoutTabButtonJs,
                        base::GetQuotedJSONString(matcher).c_str())),
        WaitForElementExists(contents_id, kComposeboxDialog),
        WaitForElementExists(contents_id, kComposeboxContainer));
  }

  // Types and submits a query in the expanded NTP composebox.
  auto SubmitRealboxComposeboxQuery(const ui::ElementIdentifier& contents_id,
                                    const std::string& query) {
    StateChange submit_enabled;
    submit_enabled.type = StateChange::Type::kExistsAndConditionTrue;
    submit_enabled.where = kRealboxSubmitButtonHost;
    submit_enabled.test_function = "el => !el.disabled";
    submit_enabled.event = kSubmitEnabledEvent;

    return Steps(
        WaitForElementExists(contents_id, kRealboxComposeboxInput),
        ExecuteJsAt(contents_id, kRealboxComposeboxInput,
                    base::StringPrintf(
                        R"((el) => {
                  el.value = %s;
                  el.dispatchEvent(new Event('input', { bubbles: true }));
                  el.dispatchEvent(new Event('change', { bubbles: true }));
                })",
                        base::GetQuotedJSONString(query).c_str())),
        WaitForStateChange(contents_id, submit_enabled),
        ExecuteJsAt(contents_id, kRealboxSubmitButton, "el => el.click()")
            .SetMustRemainVisible(false));
  }
};

// Adding tabs from the NTP realbox composebox's "+" > "Add tabs" flyout
// signposts each tab consistently, before anything is submitted, in all four
// places: favicon coins on the "+" button, coins next to "Sharing n tabs" on
// the menu trigger row, a checkmark on the tab's flyout row, and an underline
// on the tab strip.
IN_PROC_BROWSER_TEST_P(
    ContextualTasksNtpRealboxContextManagementInteractiveUiTest,
    RealboxAddTabs_SignpostsConsistently) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNtpTab);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");

  RunTestSequence(
      // Tabs 0 and 1 are the candidates. The NTP opens as tab 2 and is active;
      // it is not itself shareable, so there is no auto-suggested active tab
      // and the composebox starts empty.
      InstrumentTab(kFirstTab, 0), NavigateWebContents(kFirstTab, kUrl1),
      AddInstrumentedTab(kSecondTab, kUrl2), OpenNtpComposebox(kNtpTab),

      // Nothing is shared before the user acts.
      VerifyPlusButtonCoins(kNtpTab, 0), VerifyUnderlinedTabs({}),
      OpenShareTabsFlyout(kNtpTab), VerifyMenuTriggerState(kNtpTab, 0),
      VerifyFlyoutTabChecked(kNtpTab, "title1", false),
      VerifyFlyoutTabChecked(kNtpTab, "title2", false),

      // Add tab 0.
      ToggleFlyoutTab(kNtpTab, "title1"),
      WaitForFileUploadsComplete(kNtpTab, 1), VerifyPlusButtonCoins(kNtpTab, 1),
      VerifyPlusButtonCoinTabs(kNtpTab, {"title1"}), VerifyUnderlinedTabs({0}),
      OpenShareTabsFlyout(kNtpTab), VerifyMenuTriggerState(kNtpTab, 1),
      VerifyFlyoutTabChecked(kNtpTab, "title1", true),
      VerifyFlyoutTabChecked(kNtpTab, "title2", false),

      // Add tab 1 on top.
      ToggleFlyoutTab(kNtpTab, "title2"),
      WaitForFileUploadsComplete(kNtpTab, 2), VerifyPlusButtonCoins(kNtpTab, 2),
      VerifyPlusButtonCoinTabs(kNtpTab, {"title1", "title2"}),
      VerifyUnderlinedTabs({0, 1}), OpenShareTabsFlyout(kNtpTab),
      VerifyMenuTriggerState(kNtpTab, 2),
      VerifyFlyoutTabChecked(kNtpTab, "title1", true),
      VerifyFlyoutTabChecked(kNtpTab, "title2", true));
}

// Un-checking tabs from the NTP realbox composebox's "Add tabs" flyout clears
// each tab's sign posting, and removing the last one returns every surface to
// its empty state ("Add tabs" with no coins, no underlines).
//
// Note the NTP does not set `composeboxContextMenuEnableTabDeselection`; that
// flag only gates un-checking tabs *restored* from a previous thread. Tabs
// added in the current turn are deselectable whenever multi-tab selection is
// on (`ContextualActionMenu.onTabClick_`), which this fixture enables.
IN_PROC_BROWSER_TEST_P(
    ContextualTasksNtpRealboxContextManagementInteractiveUiTest,
    RealboxRemoveTabs_ClearsSignposting) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNtpTab);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");

  RunTestSequence(
      InstrumentTab(kFirstTab, 0), NavigateWebContents(kFirstTab, kUrl1),
      AddInstrumentedTab(kSecondTab, kUrl2), OpenNtpComposebox(kNtpTab),

      // Start from two shared tabs.
      ToggleFlyoutTab(kNtpTab, "title1"),
      WaitForFileUploadsComplete(kNtpTab, 1),
      ToggleFlyoutTab(kNtpTab, "title2"),
      WaitForFileUploadsComplete(kNtpTab, 2),
      VerifyPlusButtonCoinTabs(kNtpTab, {"title1", "title2"}),
      VerifyUnderlinedTabs({0, 1}),

      // Un-check tab 0. Only tab 1 should remain signposted anywhere.
      ToggleFlyoutTab(kNtpTab, "title1"),
      WaitForFileUploadsComplete(kNtpTab, 1), VerifyPlusButtonCoins(kNtpTab, 1),
      VerifyPlusButtonCoinTabs(kNtpTab, {"title2"}), VerifyUnderlinedTabs({1}),
      OpenShareTabsFlyout(kNtpTab), VerifyMenuTriggerState(kNtpTab, 1),
      VerifyFlyoutTabChecked(kNtpTab, "title1", false),
      VerifyFlyoutTabChecked(kNtpTab, "title2", true),

      // Un-check tab 1. Everything should be back to the empty state.
      ToggleFlyoutTab(kNtpTab, "title2"),
      WaitForFileUploadsComplete(kNtpTab, 0), VerifyPlusButtonCoins(kNtpTab, 0),
      VerifyUnderlinedTabs({}), OpenShareTabsFlyout(kNtpTab),
      VerifyMenuTriggerState(kNtpTab, 0),
      VerifyFlyoutTabChecked(kNtpTab, "title1", false),
      VerifyFlyoutTabChecked(kNtpTab, "title2", false));
}

// Selecting a tab from the collapsed `ntp-searchbox` inline "+" menu expands
// `#composeboxDialog` and signposts the selected tab across all four surfaces.
IN_PROC_BROWSER_TEST_P(
    ContextualTasksNtpRealboxContextManagementInteractiveUiTest,
    RealboxClassicInlinePlusButton_AddTabExpandsComposeboxWithSignposting) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNtpTab);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");

  RunTestSequence(
      InstrumentTab(kFirstTab, 0), NavigateWebContents(kFirstTab, kUrl1),
      AddInstrumentedTab(kSecondTab, kUrl2),
      AddInstrumentedTab(kNtpTab, chrome::ChromeUINewTabURLAsGURL()),
      VerifyUnderlinedTabs({}),

      // Select Tab 0 from the collapsed `ntp-searchbox` inline "+" menu.
      SelectTabFromInlineRealboxPlusMenu(kNtpTab, "title1"),

      // Verify the composebox expands with Tab 0 signposted across all four
      // surfaces.
      WaitForFileUploadsComplete(kNtpTab, 1), VerifyPlusButtonCoins(kNtpTab, 1),
      VerifyPlusButtonCoinTabs(kNtpTab, {"title1"}), VerifyUnderlinedTabs({0}),
      OpenShareTabsFlyout(kNtpTab), VerifyMenuTriggerState(kNtpTab, 1),
      VerifyFlyoutTabChecked(kNtpTab, "title1", true),
      VerifyFlyoutTabChecked(kNtpTab, "title2", false));
}

// Submitting a query with attached tabs from the NTP composebox navigates the
// NTP tab to the AIM page while keeping the attached tabs underlined, and
// navigating away from the AIM page clears the tab-strip underlines.
IN_PROC_BROWSER_TEST_P(
    ContextualTasksNtpRealboxContextManagementInteractiveUiTest,
    RealboxSubmitWithTabs_NavigatesToAimAndCleansUpOnExit) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNtpTab);

  const GURL kUrl1 = embedded_test_server()->GetURL("/title1.html");
  const GURL kUrl2 = embedded_test_server()->GetURL("/title2.html");

  RunTestSequence(
      InstrumentTab(kFirstTab, 0), NavigateWebContents(kFirstTab, kUrl1),
      AddInstrumentedTab(kSecondTab, kUrl2), OpenNtpComposebox(kNtpTab),

      // Attach both tabs and submit a query from the NTP composebox.
      ToggleFlyoutTab(kNtpTab, "title1"),
      WaitForFileUploadsComplete(kNtpTab, 1),
      ToggleFlyoutTab(kNtpTab, "title2"),
      WaitForFileUploadsComplete(kNtpTab, 2), VerifyUnderlinedTabs({0, 1}),
      SubmitRealboxComposeboxQuery(kNtpTab, "Compare tabs from NTP"),

      // For signed-in profiles, the navigation is intercepted into the
      // full-page Contextual Tasks UI (`chrome://contextual-tasks/`), keeping
      // both attached tabs underlined until the user navigates away. For
      // signed-out profiles (not Contextual Tasks eligible), the tab navigates
      // directly to the AIM SERP (`www.google.com`), which exits the NTP
      // composebox and clears the underlines immediately.
      GetUserVariation() == UserVariation::kSignedIn
          ? Steps(WaitForFullTabContextualTasksLoaded(kNtpTab),
                  VerifyUnderlinedTabs({0, 1}),
                  NavigateWebContents(kNtpTab, kUrl1), VerifyUnderlinedTabs({}))
          : Steps(WaitForWebContentsNavigation(kNtpTab),
                  CheckElement(
                      kNtpTab,
                      [](ui::TrackedElement* el) {
                        return AsInstrumentedWebContents(el)
                            ->web_contents()
                            ->GetLastCommittedURL()
                            .host();
                      },
                      std::string(kMockAimPageHost)),
                  VerifyUnderlinedTabs({})));
}

// The incognito NTP does not host the realbox composebox, so only the regular
// profile variations apply here.
INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksNtpRealboxContextManagementInteractiveUiTest,
    testing::Values(UserVariation::kSignedIn, UserVariation::kSignedOut),
    &UserVariationToString);

}  // namespace contextual_tasks
