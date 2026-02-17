// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/common/extensions/api/tab_groups.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tab_groups/tab_group_color.h"
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
base::DictValue RunTabGroupsGetFunction(
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

  tab_groups::TabGroupVisualData visual_data(
      u"Title", tab_groups::TabGroupColorId::kCyan, /*is_collapsed=*/true);
  tab_list->SetTabGroupVisualData(*group, visual_data);

  // Call the chrome.tabGroups.get() function with a valid group id.
  auto extension = CreateTabGroupsExtension();
  int group_id = ExtensionTabUtil::GetGroupId(*group);
  constexpr char kFormatArgs[] = R"([%d])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  base::DictValue group_info =
      RunTabGroupsGetFunction(profile(), extension.get(), args);

  // Group info was returned.
  EXPECT_EQ(group_id, *group_info.FindInt("id"));
  EXPECT_EQ(ExtensionTabUtil::GetWindowId(browser_window_interface()),
            *group_info.FindInt("windowId"));
  EXPECT_FALSE(*group_info.FindBool("shared"));
  EXPECT_EQ("Title", *group_info.FindString("title"));
  EXPECT_EQ("cyan", *group_info.FindString("color"));
  EXPECT_TRUE(*group_info.FindBool("collapsed"));
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

IN_PROC_BROWSER_TEST_F(TabGroupsApiTest, GetApi) {
  ASSERT_TRUE(RunExtensionTest("tab_groups/get")) << message_;
}

IN_PROC_BROWSER_TEST_F(TabGroupsApiTest, TestTabGroupsWorks) {
  ASSERT_TRUE(RunExtensionTest("tab_groups/basics")) << message_;
}

// Tests that events are restricted to their respective browser contexts,
// especially between on-the-record and off-the-record browsers.
IN_PROC_BROWSER_TEST_F(TabGroupsApiTest, TestTabGroupEventsAcrossProfiles) {
  // Create an incognito window.
  auto type = BrowserWindowInterface::Type::TYPE_NORMAL;
  Profile* incognito_profile =
      GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  BrowserWindowCreateParams create_params = BrowserWindowCreateParams(
      type, *incognito_profile, /*from_user_gesture=*/false);
  base::test::TestFuture<BrowserWindowInterface*> future;
  CreateBrowserWindow(std::move(create_params), future.GetCallback());
  BrowserWindowInterface* incognito_browser = future.Get();

  // Get the tab list for the incognito window.
  TabListInterface* incognito_tab_list =
      TabListInterface::From(incognito_browser);
  ASSERT_TRUE(incognito_tab_list);

  // Ensure the incognito window has a tab (some platforms like Win/Mac/Linux do
  // not create a tab automatically).
  if (incognito_tab_list->GetTabCount() == 0) {
    incognito_tab_list->OpenTab(GURL("about:blank"), /*index=*/-1);
  }

  // The EventRouter is shared between on- and off-the-record profiles, so
  // this observer will catch events for each. To verify that the events are
  // restricted to their respective contexts, we check the event metadata.
  TestEventRouterObserver event_observer(EventRouter::Get(GetProfile()));

  // Create a tab group in the main (on-the-record) window.
  TabListInterface* tab_list =
      TabListInterface::From(browser_window_interface());
  ASSERT_TRUE(tab_list);
  tabs::TabHandle tab0 = tab_list->GetTab(0)->GetHandle();
  tab_list->CreateTabGroup({tab0});

  // A created event was fired in the main profile's context.
  ASSERT_TRUE(
      event_observer.events().contains(api::tab_groups::OnCreated::kEventName));
  Event* normal_event =
      event_observer.events().at(api::tab_groups::OnCreated::kEventName).get();
  EXPECT_EQ(normal_event->restrict_to_browser_context, GetProfile());

  event_observer.ClearEvents();

  // Create a tab group in the incognito window.
  tabs::TabHandle incognito_tab0 = incognito_tab_list->GetTab(0)->GetHandle();
  incognito_tab_list->CreateTabGroup({incognito_tab0});

  // A created event was fired in the incognito context.
  ASSERT_TRUE(
      event_observer.events().contains(api::tab_groups::OnCreated::kEventName));
  Event* incognito_event =
      event_observer.events().at(api::tab_groups::OnCreated::kEventName).get();
  EXPECT_EQ(incognito_event->restrict_to_browser_context, incognito_profile);
}

#if !BUILDFLAG(IS_ANDROID)
// Not supported on Android because DetachTabGroup is not available in the
// Android tab groups API.
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
  EXPECT_TRUE(
      event_observer.events().contains(api::tab_groups::OnRemoved::kEventName));

  event_observer.ClearEvents();

  browser()->tab_strip_model()->InsertDetachedTabGroupAt(
      std::move(detached_group), 1);

  // Group added as well as the tab's group changed event should be sent.
  event_observer.WaitForEventWithName(api::tab_groups::OnCreated::kEventName);
  event_observer.WaitForEventWithName(api::tab_groups::OnUpdated::kEventName);

  EXPECT_TRUE(
      event_observer.events().contains(api::tab_groups::OnCreated::kEventName));
  EXPECT_TRUE(
      event_observer.events().contains(api::tab_groups::OnUpdated::kEventName));
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(TabGroupsApiTest, SetGroupTitleToEmoji) {
  ASSERT_TRUE(RunExtensionTest("tab_groups/emoji",
                               {.extension_url = "emoji_title.html"}))
      << message_;

  TabListInterface* tab_list =
      TabListInterface::From(browser_window_interface());
  ASSERT_TRUE(tab_list);
  std::optional<tab_groups::TabGroupId> group = tab_list->GetTab(0)->GetGroup();
  ASSERT_TRUE(group.has_value());
  std::optional<tab_groups::TabGroupVisualData> visual_data =
      tab_list->GetTabGroupVisualData(*group);
  ASSERT_TRUE(visual_data);

  EXPECT_EQ(visual_data->title(), std::u16string(u"🤡"));
}

}  // namespace
}  // namespace extensions
