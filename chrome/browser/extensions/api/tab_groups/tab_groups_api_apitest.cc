// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/common/extensions/api/tab_groups.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/extension_test_message_listener.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

// Runs the chrome.tabGroups.get(groupId) function.
base::Value::Dict RunTabGroupsGetFunction(
    content::BrowserContext* browser_context,
    const Extension* extension,
    const std::string& args) {
  auto function = base::MakeRefCounted<TabGroupsGetFunction>();
  function->set_extension(extension);
  std::optional<base::Value> value =
      api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), args, browser_context,
          api_test_utils::FunctionMode::kNone);
  return std::move(*value).TakeDict();
}

// Creates an extension with "tabGroups" permission.
scoped_refptr<const Extension> CreateTabGroupsExtension() {
  return ExtensionBuilder("Extension with tabGroups permission")
      .AddAPIPermission("tabGroups")
      .Build();
}

using TabGroupsApiTest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(TabGroupsApiTest, GetFunction) {
  // Open a tab, bringing the count to 2.
  NavigateToURLInNewTab(GURL("about:blank"));
  auto* tab_list = TabListInterface::From(browser_window_interface());
  ASSERT_EQ(2, tab_list->GetTabCount());

  // Create a group with the 2 tabs.
  std::vector<tabs::TabHandle> tabs;
  tabs.push_back(tab_list->GetTab(0)->GetHandle());
  tabs.push_back(tab_list->GetTab(1)->GetHandle());
  std::optional<tab_groups::TabGroupId> group = tab_list->CreateTabGroup(tabs);
  ASSERT_TRUE(group.has_value());

  // Call the chrome.tabGroups.get() function with a valid group id.
  auto extension = CreateTabGroupsExtension();
  int group_id = ExtensionTabUtil::GetGroupId(*group);
  constexpr char kFormatArgs[] = R"([%d])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  base::Value::Dict group_info =
      RunTabGroupsGetFunction(profile(), extension.get(), args);

  // Group info was returned.
  EXPECT_EQ(group_id, *group_info.FindInt("id"));
  EXPECT_EQ(ExtensionTabUtil::GetWindowId(browser_window_interface()),
            *group_info.FindInt("windowId"));
  EXPECT_FALSE(*group_info.FindBool("shared"));
}

IN_PROC_BROWSER_TEST_F(TabGroupsApiTest, GetFunctionInvalidGroup) {
  auto extension = CreateTabGroupsExtension();
  auto function = base::MakeRefCounted<TabGroupsGetFunction>();
  function->set_extension(extension);

  // Call the chrome.tabGroups.get() function with an invalid group id (0).
  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), "[0]", profile(), api_test_utils::FunctionMode::kNone);
  EXPECT_EQ("No group with id: 0.", error);
}

// TODO(crbug.com/371432155): Port tests to desktop Android as the underlying
// API methods are enabled.
#if BUILDFLAG(ENABLE_EXTENSIONS)

// TODO(crbug.com/371432155): Port to desktop Android when tabs.group() is
// supported.
IN_PROC_BROWSER_TEST_F(TabGroupsApiTest, GetApi) {
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
  std::optional<tab_groups::TabGroupId> group = tab_list->CreateTabGroup(tabs);
  ASSERT_TRUE(group.has_value());

  TestEventRouterObserver event_observer(EventRouter::Get(profile()));

  std::unique_ptr<DetachedTabCollection> detached_group =
      browser()->tab_strip_model()->DetachTabGroupForInsertion(*group);

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

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace
}  // namespace extensions
