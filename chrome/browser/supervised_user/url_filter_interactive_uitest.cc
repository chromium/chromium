// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/supervised_user/family_live_test.h"
#include "chrome/test/supervised_user/family_member.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"

namespace supervised_user {
namespace {

// TODO(b/303401498): Use dedicated RPCs in supervised user e2e desktop tests
// instead of clicking around the pages.
class UrlFilterUiTest : public InteractiveBrowserTestT<FamilyLiveTest> {
 public:
  UrlFilterUiTest()
      : InteractiveBrowserTestT<FamilyLiveTest>(
            /*extra_enabled_hosts=*/std::vector<std::string>(
                {"example.com", "www.pornhub.com"})) {}

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

  auto ParentAddsUrlToControlList(ui::ElementIdentifier kParentTab,
                                  std::string_view url) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kNewUrlPopupOpened);
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kNewUrlIsBlocked);

    StateChange add_new_url_popup_visible;
    add_new_url_popup_visible.type = StateChange::Type::kExists;
    add_new_url_popup_visible.where = {"div[role='dialog']"};
    add_new_url_popup_visible.event = kNewUrlPopupOpened;

    StateChange new_url_is_blocked;
    new_url_is_blocked.type = StateChange::Type::kExists;
    new_url_is_blocked.where = {
        base::StringPrintf("#view_container li[aria-label='%s']", url.data())};
    new_url_is_blocked.event = kNewUrlIsBlocked;

    return Steps(ExecuteJsAt(kParentTab, {"#view_container"}, R"js(
          view_container => {
            const buttons = view_container.querySelectorAll("div[role='button']");
            buttons[buttons.length - 1].click();
          }
        )js"),
                 WaitForStateChange(kParentTab, add_new_url_popup_visible),

                 ExecuteJsAt(kParentTab, {"div[role='dialog']"},
                             base::StringPrintf(R"js(
          dialog => {
            const submitButtonIndex = 9;
            dialog.querySelector("input").value = "%s";
            dialog.querySelectorAll("div[role='button'] span")[submitButtonIndex].click();
          }
        )js",
                                                url.data())),
                 WaitForStateChange(kParentTab, new_url_is_blocked));
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

IN_PROC_BROWSER_TEST_F(UrlFilterUiTest, ParentBlocksPage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kChildElementId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kParentControlsTab);

  TurnOnSyncFor(head_of_household());
  TurnOnSyncFor(child());

  // Child activity is happening in this tab.
  int tab_index = 0;

  RunTestSequence(
      InstrumentTab(kParentControlsTab, /*tab_index=*/0,
                    /*in_browser=*/head_of_household().browser()),

      // Clear all existing filters.
      ParentOpensControlListPage(
          kParentControlsTab, head_of_household().GetAllowListUrlFor(child())),
      ParentRemovesUrlsFromControlList(kParentControlsTab),

      ParentOpensControlListPage(
          kParentControlsTab, head_of_household().GetBlockListUrlFor(child())),
      ParentRemovesUrlsFromControlList(kParentControlsTab),

      // At this point, it's the blocklist control page that stays open.

      // Supervised user navigates to any page.
      InstrumentTab(kChildElementId, tab_index, child().browser()),
      NavigateWebContents(kChildElementId, GetRoutedUrl("https://example.com")),
      WaitForStateChange(kChildElementId,
                         PageWithMatchingTitle("Example Domain")),

      // Supervisor blocks that page and supervised user sees interstitial
      // blocked page screen.
      ParentAddsUrlToControlList(kParentControlsTab, "example.com"),
      WaitForStateChange(kChildElementId, RemoteApprovalButtonAppeared()));
}

IN_PROC_BROWSER_TEST_F(UrlFilterUiTest, ParentAllowsPageBlockedBySafeSites) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kChildElementId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kParentControlsTab);

  TurnOnSyncFor(head_of_household());
  TurnOnSyncFor(child());

  // Child activity is happening in this tab.
  int tab_index = 0;

  RunTestSequence(
      InstrumentTab(kParentControlsTab, /*tab_index=*/0,
                    /*in_browser=*/head_of_household().browser()),

      // Clear all existing filters.
      ParentOpensControlListPage(
          kParentControlsTab, head_of_household().GetBlockListUrlFor(child())),
      ParentRemovesUrlsFromControlList(kParentControlsTab),

      ParentOpensControlListPage(
          kParentControlsTab, head_of_household().GetAllowListUrlFor(child())),
      ParentRemovesUrlsFromControlList(kParentControlsTab),

      // At this point, it's the allowlist control page that stays open.

      // Supervised user navigates to inappropriate page and is blocked.
      InstrumentTab(kChildElementId, tab_index, child().browser()),
      NavigateWebContents(kChildElementId,
                          GetRoutedUrl("https://www.pornhub.com")),
      WaitForStateChange(kChildElementId, RemoteApprovalButtonAppeared()),

      // Supervisor allows that page and supervised user consumes content.
      ParentAddsUrlToControlList(kParentControlsTab, "www.pornhub.com"),
      WaitForStateChange(kChildElementId, PageWithMatchingTitle("Pornhub")));
}

}  // namespace
}  // namespace supervised_user
