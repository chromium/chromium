// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/tab_groups.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/test/extension_test_message_listener.h"

namespace extensions {

namespace {

using TabGroupsApiTest = ExtensionApiTest;

// TODO(crbug.com/371432155): Port to desktop Android when tabs.group() is
// supported.
IN_PROC_BROWSER_TEST_F(TabGroupsApiTest, TestGet) {
  ASSERT_TRUE(RunExtensionTest("tab_groups/get")) << message_;
}

// TODO(crbug.com/40910190): Test is flaky.
IN_PROC_BROWSER_TEST_F(TabGroupsApiTest, TestTabGroupsWorks) {
  ASSERT_TRUE(RunExtensionTest("tab_groups/basics")) << message_;
}

// Tests that events are restricted to their respective browser contexts,
// especially between on-the-record and off-the-record browsers.
// Note: unit tests don't support multiple profiles, so this has to be a browser
// test.
IN_PROC_BROWSER_TEST_F(TabGroupsApiTest, TestTabGroupEventsAcrossProfiles) {
  Browser* incognito_browser =
      OpenURLOffTheRecord(profile(), GURL("about:blank"));

  // The EventRouter is shared between on- and off-the-record profiles, so
  // this observer will catch events for each. To verify that the events are
  // restricted to their respective contexts, we check the event metadata.
  TestEventRouterObserver event_observer(EventRouter::Get(profile()));

  browser()->tab_strip_model()->AddToNewGroup({0});
  ASSERT_TRUE(base::Contains(event_observer.events(),
                             api::tab_groups::OnCreated::kEventName));
  Event* normal_event =
      event_observer.events().at(api::tab_groups::OnCreated::kEventName).get();
  EXPECT_EQ(normal_event->restrict_to_browser_context, profile());

  event_observer.ClearEvents();

  incognito_browser->tab_strip_model()->AddToNewGroup({0});
  ASSERT_TRUE(base::Contains(event_observer.events(),
                             api::tab_groups::OnCreated::kEventName));
  Event* incognito_event =
      event_observer.events().at(api::tab_groups::OnCreated::kEventName).get();
  EXPECT_EQ(incognito_event->restrict_to_browser_context,
            incognito_browser->profile());
}

// TODO(crbug.com/405219902): Port to desktop Android when the tab group event
// router is supported.
IN_PROC_BROWSER_TEST_F(TabGroupsApiTest, TestGroupDetachedAndReInserted) {
  // Open 3 tabs.
  NavigateToURLInNewTab(GURL("about:blank"));
  NavigateToURLInNewTab(GURL("about:blank"));
  NavigateToURLInNewTab(GURL("about:blank"));

  // Create a group with 2 tabs.
  auto* tab_list = TabListInterface::From(browser_window_interface());
  std::vector<tabs::TabHandle> tabs;
  tabs.push_back(tab_list->GetTab(0)->GetHandle());
  tabs.push_back(tab_list->GetTab(1)->GetHandle());
  tab_groups::TabGroupId group = tab_list->CreateTabGroup(tabs);

  TestEventRouterObserver event_observer(EventRouter::Get(profile()));

  std::unique_ptr<DetachedTabCollection> detached_group =
      browser()->tab_strip_model()->DetachTabGroupForInsertion(group);

  event_observer.WaitForEventWithName(api::tab_groups::OnRemoved::kEventName);
  EXPECT_TRUE(base::Contains(event_observer.events(),
                             api::tab_groups::OnRemoved::kEventName));

  event_observer.ClearEvents();

  browser()->tab_strip_model()->InsertDetachedTabGroupAt(
      std::move(detached_group), 1);

  // Group added as well as the tab's group changed event should be sent.
  event_observer.WaitForEventWithName(api::tab_groups::OnCreated::kEventName);
  event_observer.WaitForEventWithName(api::tab_groups::OnUpdated::kEventName);

  EXPECT_TRUE(base::Contains(event_observer.events(),
                             api::tab_groups::OnCreated::kEventName));
  EXPECT_TRUE(base::Contains(event_observer.events(),
                             api::tab_groups::OnUpdated::kEventName));
}

IN_PROC_BROWSER_TEST_F(TabGroupsApiTest, SetGroupTitleToEmoji) {
  ASSERT_TRUE(RunExtensionTest("tab_groups/emoji",
                               {.extension_url = "emoji_title.html"}))
      << message_;

  std::optional<tab_groups::TabGroupId> group =
      browser()->tab_strip_model()->GetTabGroupForTab(0);
  ASSERT_TRUE(group.has_value());
  const tab_groups::TabGroupVisualData* visual_data = browser()
                                                          ->tab_strip_model()
                                                          ->group_model()
                                                          ->GetTabGroup(*group)
                                                          ->visual_data();
  EXPECT_EQ(visual_data->title(), std::u16string(u"🤡"));
}

}  // namespace
}  // namespace extensions
