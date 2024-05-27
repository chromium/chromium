// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/supervised_user/family_live_test.h"
#include "chrome/test/supervised_user/family_member.h"
#include "chrome/test/supervised_user/test_state_seeded_observer.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"

namespace supervised_user {
namespace {

static constexpr std::string_view kPermissionRequestUrl =
    "https://families.google.com/u/0/manage/family/";

// All tests in this unit are subject to flakiness because they interact with a
// system that can be externally modified during execution.
class UrlFilterUiTest : public InteractiveFamilyLiveTest,
                        public testing::WithParamInterface<FamilyIdentifier> {
 public:
  UrlFilterUiTest()
      : InteractiveFamilyLiveTest(
            /*family_identifier=*/GetParam(),
            /*extra_enabled_hosts=*/std::vector<std::string>(
                {"example.com", "bestgore.com"})) {}

 protected:
  auto ParentOpensControlListPage(ui::ElementIdentifier kParentTab,
                                  const GURL& gurl) {
    return Steps(NavigateWebContents(kParentTab, gurl),
                 WaitForWebContentsReady(kParentTab, gurl));
  }

  auto ParentRemovesUrlsFromControlList(ui::ElementIdentifier kParentTab) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kFiltersAreRemoved);

    StateChange all_url_filters_removed;
    all_url_filters_removed.type = StateChange::Type::kDoesNotExist;
    all_url_filters_removed.where = {"#view_container li"};
    all_url_filters_removed.event = kFiltersAreRemoved;

    return Steps(ExecuteJsAt(kParentTab, {"#view_container"}, R"js(
          view_container => {
            // Clicks remove on all filters.
            for(const li of view_container.querySelectorAll("li")) {
                li.children[1].children[0].click();
            }
          }
        )js"),
                 WaitForStateChange(kParentTab, all_url_filters_removed));
  }

  StateChange RemoteApprovalButtonAppeared() {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kStateChange);
    StateChange state_change;
    state_change.type = StateChange::Type::kExists;
    state_change.where = {"#frame-blocked #remote-approvals-button"};
    state_change.event = kStateChange;
    state_change.continue_across_navigation = true;
    return state_change;
  }

  // Clicks the approval request button for a pending request on Family Link.
  auto ParentApprovesPermissionRequest(ui::ElementIdentifier kParentTab) {
    return Steps(ExecuteJsAt(
        kParentTab, {"#view_container"},
        // The "Allow All" is the last button on the permission request page.
        R"js(
                (view_container) => {
                  const buttons = view_container.querySelectorAll("div[role='button']");
                  const allow = buttons[buttons.length - 1];
                  allow.click();
                }
              )js"));
  }

  // Clicks the remote approval request button on the supervised user
  // interstitial.
  auto ChildRequestsRemoteApproval(ui::ElementIdentifier kChildTab) {
    return Steps(ExecuteJsAt(kChildTab,
                             {"#frame-blocked #remote-approvals-button"},
                             R"js( (button) => { button.click(); } )js"));
  }

  // Checks that a permission request exists on Family link.
  StateChange RemotePermissionRequestAppeared() {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kTextFound);
    StateChange state_change;
    state_change.type = StateChange::Type::kExistsAndConditionTrue;
    state_change.where = {"#view_container"};

    // The "Allow All" (host approval) is the last button on the permission
    // request page.
    std::string_view remote_permission_approval_button_text = "ALLOW ALL";
    state_change.test_function =
        base::StringPrintf(R"js(
          (view_container) => {
            const buttons = view_container.querySelectorAll("div[role='button']");
            return buttons[buttons.length - 1].innerText == '%s'; }
          )js",
                           remote_permission_approval_button_text.data());
    state_change.event = kTextFound;
    state_change.continue_across_navigation = true;
    return state_change;
  }

  // Checks if a page title matches the given regexp in ecma script dialect.
  StateChange PageWithMatchingTitle(std::string_view title_regexp) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kStateChange);
    StateChange state_change;
    state_change.type = StateChange::Type::kConditionTrue;
    state_change.event = kStateChange;
    state_change.test_function = base::StringPrintf(R"js(
      () => /%s/.test(document.title)
    )js",
                                                    title_regexp.data());
    state_change.continue_across_navigation = true;
    return state_change;
  }
};

IN_PROC_BROWSER_TEST_P(UrlFilterUiTest, ParentBlocksPage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kChildElementId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BrowserState::Observer,
                                      kSetSafeSitesStateObserverId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BrowserState::Observer,
                                      kDefineStateObserverId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BrowserState::Observer,
                                      kResetStateObserverId);

  TurnOnSyncFor(head_of_household());
  TurnOnSyncFor(child());

  // Child activity is happening in this tab.
  int tab_index = 0;
  GURL all_audiences_site_url(GetRoutedUrl("https://example.com"));

  RunTestSequence(
      WaitForStateSeeding(kResetStateObserverId, head_of_household(), child(),
                          BrowserState::Reset()),
      WaitForStateSeeding(kSetSafeSitesStateObserverId, head_of_household(),
                          child(), BrowserState::EnableSafeSites()),

      // Supervised user navigates to any page.
      InstrumentTab(kChildElementId, tab_index, child().browser()),
      NavigateWebContents(kChildElementId, all_audiences_site_url),
      WaitForStateChange(kChildElementId,
                         PageWithMatchingTitle("Example Domain")),
      // Supervisor blocks that page and supervised user sees interstitial
      // blocked page screen.
      WaitForStateSeeding(kDefineStateObserverId, head_of_household(), child(),
                          BrowserState::BlockSite(all_audiences_site_url)),
      WaitForStateChange(kChildElementId, RemoteApprovalButtonAppeared()));
}

// Sanity test, if it fails it means that resetting the test state is not
// functioning properly.
IN_PROC_BROWSER_TEST_P(UrlFilterUiTest, ClearFamilyLinkSettings) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BrowserState::Observer, kObserverId);

  TurnOnSyncFor(head_of_household());
  TurnOnSyncFor(child());

  // Clear all existing filters.
  RunTestSequence(WaitForStateSeeding(kObserverId, head_of_household(), child(),
                                      BrowserState::Reset()));
}

IN_PROC_BROWSER_TEST_P(UrlFilterUiTest, ParentAllowsPageBlockedBySafeSites) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kChildElementId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BrowserState::Observer,
                                      kDefineStateObserverId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BrowserState::Observer,
                                      kResetStateObserverId);

  TurnOnSyncFor(head_of_household());
  TurnOnSyncFor(child());

  // Child activity is happening in this tab.
  int tab_index = 0;
  GURL mature_site_url(GetRoutedUrl("https://bestgore.com"));

  RunTestSequence(
      WaitForStateSeeding(kResetStateObserverId, head_of_household(), child(),
                          BrowserState::Reset()),

      // Supervised user navigates to inappropriate page and is blocked.
      InstrumentTab(kChildElementId, tab_index, child().browser()),
      NavigateWebContents(kChildElementId, mature_site_url),
      WaitForStateChange(kChildElementId, RemoteApprovalButtonAppeared()),

      // Supervisor allows that page and supervised user consumes content.
      WaitForStateSeeding(kDefineStateObserverId, head_of_household(), child(),
                          BrowserState::AllowSite(mature_site_url)),
      WaitForStateChange(kChildElementId, PageWithMatchingTitle("Best Gore")));
}

IN_PROC_BROWSER_TEST_P(UrlFilterUiTest,
                       ParentAprovesPermissionRequestForBlockedSite) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kChildElementId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kParentApprovalTab);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BrowserState::Observer,
                                      kResetStateObserverId);

  TurnOnSyncFor(head_of_household());
  TurnOnSyncFor(child());

  // Child and parent activity is happening in these tabs.
  int child_tab_index = 0;
  int parent_tab_index = 0;

  RunTestSequence(
      WaitForStateSeeding(kResetStateObserverId, head_of_household(), child(),
                          BrowserState::Reset()),
      // Supervised user navigates to inappropriate page and is blocked, and
      // makes approval request.
      InstrumentTab(kChildElementId, child_tab_index, child().browser()),
      Log("When child navigates to blocked url"),
      NavigateWebContents(kChildElementId,
                          GetRoutedUrl("https://bestgore.com")),
      WaitForStateChange(kChildElementId, RemoteApprovalButtonAppeared()),
      Log("When child requests approval"),
      ChildRequestsRemoteApproval(kChildElementId),

      // Parent receives remote approval request for the blocked page in Family
      // Link.
      Log("When parent receives approval request"),
      InstrumentTab(kParentApprovalTab, parent_tab_index,
                    /*in_browser=*/head_of_household().browser()),
      ParentOpensControlListPage(kParentApprovalTab,
                                 GURL(kPermissionRequestUrl)),
      WaitForStateChange(kParentApprovalTab, RemotePermissionRequestAppeared()),

      // Parent approves the request and supervised user consumes the page
      // content.
      Log("When parent grants approval"),
      ParentApprovesPermissionRequest(kParentApprovalTab),
      Log("Then child gets unblocked"),
      WaitForStateChange(kChildElementId, PageWithMatchingTitle("Best Gore")));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    UrlFilterUiTest,
    testing::Values(FamilyIdentifier("FAMILY_DMA_ELIGIBLE_NO_CONSENT"),
                    FamilyIdentifier("FAMILY_DMA_ELIGIBLE_WITH_CONSENT"),
                    FamilyIdentifier("FAMILY_DMA_INELIGIBLE")),
    [](const auto& info) { return info.param->data(); });

}  // namespace
}  // namespace supervised_user
