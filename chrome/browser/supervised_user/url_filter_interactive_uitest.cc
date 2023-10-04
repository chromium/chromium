// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/supervised_user/custom_state_observers.h"
#include "chrome/test/supervised_user/family_live_test.h"
#include "chrome/test/supervised_user/family_member.h"
#include "content/public/test/browser_test.h"

namespace supervised_user {
namespace {

// TODO(b/303401498): Use dedicated RPCs in supervised user e2e desktop tests
// instead of clicking around the pages.
class UrlFilterUiTest : public InteractiveBrowserTestT<FamilyLiveTest> {
 public:
  UrlFilterUiTest()
      : InteractiveBrowserTestT<FamilyLiveTest>(
            /*extra_enabled_hosts=*/std::vector<std::string>({"example.com"})) {
  }

 protected:
  auto ParentOpensBlockListPage() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabPageElementId);
    return Steps(
        InstrumentTab(kTabPageElementId, /*tab_index=*/0,
                      /*in_browser=*/head_of_household().browser()),
        NavigateWebContents(kTabPageElementId,
                            head_of_household().GetBlockListUrlFor(child())),
        WaitForWebContentsReady(
            kTabPageElementId,
            head_of_household().GetBlockListUrlFor(child())));
  }

  auto ParentRemovesUrlsFromBlockList() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabPageElementId);
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kFiltersAreRemoved);

    StateChange all_url_filters_removed;
    all_url_filters_removed.type = StateChange::Type::kDoesNotExist;
    all_url_filters_removed.where = {"#view_container li"};
    all_url_filters_removed.event = kFiltersAreRemoved;

    return Steps(
        InstrumentTab(kTabPageElementId, /*tab_index=*/0,
                      /*in_browser=*/head_of_household().browser()),
        ExecuteJsAt(kTabPageElementId, {"#view_container"}, R"js(
          view_container => {
            // Clicks remove on all filters.
            for(const li of view_container.querySelectorAll("li")) {
                li.children[1].children[0].click();
            }
          }
        )js"),
        WaitForStateChange(kTabPageElementId, all_url_filters_removed));
  }

  auto ParentAddsUrlToBlockList(std::string_view url) {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabPageElementId);
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

    return Steps(
        InstrumentTab(kTabPageElementId, /*tab_index=*/0,
                      /*in_browser=*/head_of_household().browser(),
                      /*wait_for_ready=*/true),

        ExecuteJsAt(kTabPageElementId, {"#view_container"}, R"js(
          view_container => {
            const buttons = view_container.querySelectorAll("div[role='button']");
            buttons[buttons.length - 1].click();
          }
        )js"),
        WaitForStateChange(kTabPageElementId, add_new_url_popup_visible),

        ExecuteJsAt(kTabPageElementId, {"div[role='dialog']"},
                    base::StringPrintf(R"js(
          dialog => {
            const submitButtonIndex = 9;
            dialog.querySelector("input").value = "%s";
            dialog.querySelectorAll("div[role='button'] span")[submitButtonIndex].click();
          }
        )js",
                                       url.data())),
        WaitForStateChange(kTabPageElementId, new_url_is_blocked));
  }
};

IN_PROC_BROWSER_TEST_F(UrlFilterUiTest, ParentBlocksPage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kChildElementId);
  // TODO(b/303402087): Use kombucha's native predicates to assert display of
  // interstitial page
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(TabTitleObserver, kTabTitleState);

  TurnOnSyncFor(head_of_household());
  TurnOnSyncFor(child());

  // Child activity is happening in this tab.
  int tab_index = 0;

  RunTestSequence(
      // Clear all existing filters.
      ParentOpensBlockListPage(), ParentRemovesUrlsFromBlockList(),

      // Supervised user navigates to any page.
      InstrumentTab(kChildElementId, tab_index, child().browser()),
      ObserveState(kTabTitleState, child().browser(), tab_index),
      NavigateWebContents(kChildElementId, GURL("https://example.com")),
      WaitForState(kTabTitleState, ::testing::Eq(u"Example Domain")),

      // Supervisor blocks that page and supervised user sees interstitial
      // blocked page screen.
      ParentAddsUrlToBlockList("example.com"),
      WaitForState(kTabTitleState, ::testing::Eq(u"Site blocked")));
}

}  // namespace
}  // namespace supervised_user
