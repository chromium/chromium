// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_groups/tab_groups_api.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/sync/base/collaboration_id.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/common/extension_builder.h"
#include "ui/base/base_window.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/tab_groups/tab_groups_event_router.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_event_router_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_initialized_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

// NOTE: While the tests in this file appear like they could be unit tests, they
// use browser windows and tabs. As such they need to be browser tests. See
// docs/chrome_browser_design_principles.md. This also makes them easier to port
// to desktop Android.

namespace extensions {
namespace {

using tab_groups::TabGroupId;

base::ListValue RunTabGroupsQueryFunction(
    content::BrowserContext* browser_context,
    const Extension* extension,
    const std::string& query_info) {
  auto function = base::MakeRefCounted<TabGroupsQueryFunction>();
  function->set_extension(extension);
  std::optional<base::Value> value =
      api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), query_info, browser_context,
          api_test_utils::FunctionMode::kNone);
  return std::move(*value).TakeList();
}

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

tab_groups::SavedTabGroup CreateSavedTabGroupFromLocalId(
    TabGroupId group_id,
    TabListInterface* tab_list) {
  std::optional<tab_groups::TabGroupVisualData> visual_data =
      tab_list->GetTabGroupVisualData(group_id);
  CHECK(visual_data);

#if BUILDFLAG(IS_ANDROID)
  tab_groups::LocalTabGroupID local_id = group_id.token();
#else
  tab_groups::LocalTabGroupID local_id = group_id;
#endif

  tab_groups::SavedTabGroup saved_group(visual_data->title(),
                                        visual_data->color(), {}, std::nullopt,
                                        std::nullopt, local_id);

  gfx::Range tabs_range = tab_list->GetTabGroupTabIndices(group_id);
  for (size_t i = tabs_range.start(); i < tabs_range.end(); ++i) {
    tabs::TabInterface* tab = tab_list->GetTab(i);
    content::WebContents* contents = tab->GetContents();
    tab_groups::SavedTabGroupTab saved_tab(
        contents->GetVisibleURL(), contents->GetTitle(),
        saved_group.saved_guid(), std::nullopt);
    saved_tab.SetLocalTabID(ExtensionTabUtil::GetTabId(contents));
    saved_group.AddTabLocally(std::move(saved_tab));
  }

  return saved_group;
}

// Closes all tabs in `tab_list`, closing the leftmost one each time.
void CloseAllTabs(TabListInterface* tab_list) {
  for (int i = 0; i < tab_list->GetTabCount(); ++i) {
    tab_list->GetTab(0)->Close();
  }
}

class TabGroupsApiBrowserTest : public ExtensionBrowserTest {
 public:
  TabGroupsApiBrowserTest() = default;
  TabGroupsApiBrowserTest(const TabGroupsApiBrowserTest&) = delete;
  TabGroupsApiBrowserTest& operator=(const TabGroupsApiBrowserTest&) = delete;
  ~TabGroupsApiBrowserTest() override = default;

  // ExtensionBrowserTest:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    // Browser tests start with one tab open.
    auto* tab_list = TabListInterface::From(browser_window_interface());
    tabs::TabInterface* initial_tab = tab_list->GetTab(0);
    ASSERT_TRUE(initial_tab);
    web_contents_.push_back(initial_tab->GetContents());

    // Add several tabs to the browser and get their web contents.
    constexpr int kNumTabs = 5;
    for (int i = 0; i < kNumTabs; ++i) {
      content::RenderFrameHost* render_frame_host =
          NavigateToURLInNewTab(GURL("about:blank"));
      content::WebContents* contents =
          content::WebContents::FromRenderFrameHost(render_frame_host);
      web_contents_.push_back(contents);
    }

    // Wait for the TabGroupSyncService to properly initialize before making any
    // changes to tab groups. This is not used on Android.
#if !BUILDFLAG(IS_ANDROID)
    auto observer =
        std::make_unique<tab_groups::TabGroupSyncServiceInitializedObserver>(
            tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile()));
    observer->Wait();
#endif
  }
  void TearDownOnMainThread() override {
    web_contents_.clear();

    // Close all tabs.
    auto* tab_list = TabListInterface::From(browser_window_interface());
    for (tabs::TabInterface* tab : tab_list->GetAllTabs()) {
      tab->Close();
    }
    ExtensionBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* web_contents(int index) { return web_contents_[index]; }

  // Returns whether tab groups are supported by the test's main window.
  // Used as a utility function to reduce line wrapping.
  bool SupportsTabGroups() {
    return ExtensionTabUtil::SupportsTabGroups(browser_window_interface());
  }

  // Creates a tab group out of existing tabs at `tab_indices`. CHECKs that the
  // group ID exists so that callers don't have to do it.
  TabGroupId CreateTabGroup(const std::vector<int>& tab_indices) {
    auto* tab_list = TabListInterface::From(browser_window_interface());
    std::vector<tabs::TabHandle> tabs;
    for (int index : tab_indices) {
      tabs.push_back(tab_list->GetTab(index)->GetHandle());
    }
    std::optional<TabGroupId> group = tab_list->CreateTabGroup(tabs);
    CHECK(group);
    return *group;
  }

 private:
  // The original web contentses in order.
  std::vector<raw_ptr<content::WebContents, VectorExperimental>> web_contents_;
};

// TODO(crbug.com/405219902): Port to desktop Android.
#if BUILDFLAG(ENABLE_EXTENSIONS)
// Tests querying on a TabStripModel that doesn't support tab groups.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest,
                       TabStripModelWithNoTabGroupFails) {
  // Create a new window that doesn't support groups. App windows don't allow
  // tab groups.
  Browser* browser2 = CreateBrowserForApp("some app", profile());
  BrowserList::SetLastActive(browser2);

  ASSERT_FALSE(browser2->tab_strip_model()->SupportsTabGroups());

  // Add a few tabs.
  TabStripModel* tab_strip_model2 = browser2->tab_strip_model();
  constexpr int kNumTabs2 = 3;
  for (int i = 0; i < kNumTabs2; ++i) {
    ASSERT_TRUE(AddTabAtIndexToBrowser(browser2, 0, GURL("about:blank"),
                                       ui::PAGE_TRANSITION_LINK));
  }

  // Create an extension and test that the tab group query method skips the
  // unsupported tab strip without throwing an error.
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  const char* kTitleQueryInfo = R"([{"title": "Sample title"}])";
  base::ListValue groups_list =
      RunTabGroupsQueryFunction(profile(), extension.get(), kTitleQueryInfo);

  ASSERT_EQ(0u, groups_list.size());

  tab_strip_model2->CloseAllTabs();
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// Test that querying groups by title returns the correct groups.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsQueryTitle) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  ASSERT_TRUE(SupportsTabGroups());

  TabListInterface* tab_list =
      TabListInterface::From(browser_window_interface());
  ASSERT_TRUE(tab_list);

  // Create 3 groups with different titles.
  const tab_groups::TabGroupColorId color = tab_groups::TabGroupColorId::kGrey;

  TabGroupId group1 = CreateTabGroup({0});
  tab_groups::TabGroupVisualData visual_data1(u"Sample title", color);
  tab_list->SetTabGroupVisualData(group1, visual_data1);

  TabGroupId group2 = CreateTabGroup({1});
  tab_groups::TabGroupVisualData visual_data2(u"Sample title suffixed", color);
  tab_list->SetTabGroupVisualData(group2, visual_data2);

  TabGroupId group3 = CreateTabGroup({2});
  tab_groups::TabGroupVisualData visual_data3(u"Prefixed Sample title", color);
  tab_list->SetTabGroupVisualData(group3, visual_data3);

  // Query by title and verify results.
  const char* kTitleQueryInfo = R"([{"title": "Sample title"}])";
  base::ListValue groups_list =
      RunTabGroupsQueryFunction(profile(), extension.get(), kTitleQueryInfo);
  ASSERT_EQ(1u, groups_list.size());

  const base::Value& group_info = groups_list[0];
  ASSERT_TRUE(group_info.is_dict());
  EXPECT_EQ(ExtensionTabUtil::GetGroupId(group1),
            *group_info.GetDict().FindInt("id"));
}

// Test that querying groups by color returns the correct groups.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsQueryColor) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  ASSERT_TRUE(SupportsTabGroups());

  TabListInterface* tab_list =
      TabListInterface::From(browser_window_interface());
  ASSERT_TRUE(tab_list);

  // Create 3 groups with different colors.
  TabGroupId group1 = CreateTabGroup({0});
  tab_groups::TabGroupVisualData visual_data1(
      std::u16string(), tab_groups::TabGroupColorId::kGrey);
  tab_list->SetTabGroupVisualData(group1, visual_data1);

  TabGroupId group2 = CreateTabGroup({1});
  tab_groups::TabGroupVisualData visual_data2(
      std::u16string(), tab_groups::TabGroupColorId::kRed);
  tab_list->SetTabGroupVisualData(group2, visual_data2);

  TabGroupId group3 = CreateTabGroup({2});
  tab_groups::TabGroupVisualData visual_data3(
      std::u16string(), tab_groups::TabGroupColorId::kBlue);
  tab_list->SetTabGroupVisualData(group3, visual_data3);

  // Query by color and verify results.
  const char* kColorQueryInfo = R"([{"color": "blue"}])";
  base::ListValue groups_list =
      RunTabGroupsQueryFunction(profile(), extension.get(), kColorQueryInfo);
  ASSERT_EQ(1u, groups_list.size());

  const base::Value& group_info = groups_list[0];
  ASSERT_EQ(base::Value::Type::DICT, group_info.type());
  EXPECT_EQ(ExtensionTabUtil::GetGroupId(group3),
            *group_info.GetDict().FindInt("id"));
}

class SharedTabGroupExtensionsBrowserTest : public TabGroupsApiBrowserTest {
 public:
  SharedTabGroupExtensionsBrowserTest() {
    feature_list_.InitAndEnableFeature(
        data_sharing::features::kDataSharingFeature);
  }

  void ShareTabGroup(const TabGroupId& group_id,
                     const syncer::CollaborationId& collaboration_id) {
    tab_groups::TabGroupSyncService* service =
        static_cast<tab_groups::TabGroupSyncService*>(
            tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile()));
#if BUILDFLAG(IS_ANDROID)
    // TabGroupSyncService uses a different type on Android.
    const base::Token local_id = group_id.token();
#else
    const TabGroupId local_id = group_id;
#endif
    service->MakeTabGroupSharedForTesting(local_id, collaboration_id);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that querying groups by color returns the correct groups.
IN_PROC_BROWSER_TEST_F(SharedTabGroupExtensionsBrowserTest,
                       TabGroupsQueryShared) {
  ASSERT_TRUE(SupportsTabGroups());

  // Create a group that is unshared.
  TabGroupId group1 = CreateTabGroup({0});
  tab_groups::TabGroupVisualData visual_data1(
      std::u16string(), tab_groups::TabGroupColorId::kGrey);
  GetTabListInterface()->SetTabGroupVisualData(group1, visual_data1);

  const char* not_shared_query = R"([{"shared": false}])";
  const char* shared_query = R"([{"shared": true}])";

  {  // Query unshared groups.
    scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

    base::ListValue groups_list =
        RunTabGroupsQueryFunction(profile(), extension.get(), not_shared_query);
    ASSERT_EQ(1u, groups_list.size());

    const base::Value& group_info = groups_list[0];
    ASSERT_EQ(base::Value::Type::DICT, group_info.type());
    EXPECT_EQ(ExtensionTabUtil::GetGroupId(group1),
              *group_info.GetDict().FindInt("id"));
  }

  {  // Query shared groups.
    scoped_refptr<const Extension> extension = CreateTabGroupsExtension();
    base::ListValue groups_list =
        RunTabGroupsQueryFunction(profile(), extension.get(), shared_query);
    ASSERT_EQ(0u, groups_list.size());
  }

  ShareTabGroup(group1, syncer::CollaborationId("collaboration_id_1"));

  {  // Query unshared groups.
    scoped_refptr<const Extension> extension = CreateTabGroupsExtension();
    base::ListValue groups_list =
        RunTabGroupsQueryFunction(profile(), extension.get(), not_shared_query);
    ASSERT_EQ(0u, groups_list.size());
  }

  {  // Query shared groups.
    scoped_refptr<const Extension> extension = CreateTabGroupsExtension();
    base::ListValue groups_list =
        RunTabGroupsQueryFunction(profile(), extension.get(), shared_query);
    ASSERT_EQ(1u, groups_list.size());

    const base::Value& group_info = groups_list[0];
    ASSERT_EQ(base::Value::Type::DICT, group_info.type());
    EXPECT_EQ(ExtensionTabUtil::GetGroupId(group1),
              *group_info.GetDict().FindInt("id"));
  }
}

// Test that getting a group returns the correct metadata.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsGetSuccess) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  ASSERT_TRUE(SupportsTabGroups());

  // Create a group.
  TabGroupId group = CreateTabGroup({0, 1});

  // Set the visual data.
  auto* tab_list = TabListInterface::From(browser_window_interface());
  tab_groups::TabGroupVisualData visual_data(
      u"Title", tab_groups::TabGroupColorId::kBlue);
  tab_list->SetTabGroupVisualData(group, visual_data);
  int group_id = ExtensionTabUtil::GetGroupId(group);

  // Use the TabGroupsGetFunction to get the group object.
  constexpr char kFormatArgs[] = R"([%d])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  base::DictValue group_info =
      RunTabGroupsGetFunction(profile(), extension.get(), args);

  EXPECT_EQ(group_id, *group_info.FindInt("id"));
  EXPECT_EQ("Title", *group_info.FindString("title"));
}

// Test that tabGroups.get() fails on a nonexistent group.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsGetError) {
  ASSERT_TRUE(SupportsTabGroups());
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  // Try to get a non-existent group and expect an error.
  auto function = base::MakeRefCounted<TabGroupsGetFunction>();
  function->set_extension(extension);
  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), "[0]", profile(), api_test_utils::FunctionMode::kNone);
  EXPECT_EQ("No group with id: 0.", error);
}

// Test that updating group metadata works as expected.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsUpdateSuccess) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  ASSERT_TRUE(SupportsTabGroups());

  // Create a group.
  TabGroupId group = CreateTabGroup({0, 1});

  // Set the visual data.
  auto* tab_list = TabListInterface::From(browser_window_interface());
  tab_groups::TabGroupVisualData visual_data(
      u"Initial title", tab_groups::TabGroupColorId::kBlue);
  tab_list->SetTabGroupVisualData(group, visual_data);
  int group_id = ExtensionTabUtil::GetGroupId(group);

  // Use the TabGroupsUpdateFunction to update the title and color.
  auto function = base::MakeRefCounted<TabGroupsUpdateFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] =
      R"([%d, {"title": "New title", "color": "red"}])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  // Verify the new group metadata.
  std::optional<tab_groups::TabGroupVisualData> new_visual_data =
      tab_list->GetTabGroupVisualData(group);
  ASSERT_TRUE(new_visual_data);
  EXPECT_EQ(new_visual_data->title(), u"New title");
  EXPECT_EQ(new_visual_data->color(), tab_groups::TabGroupColorId::kRed);
}

// Test that tabGroups.update() fails on a nonexistent group.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsUpdateError) {
  ASSERT_TRUE(SupportsTabGroups());

  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  // Try to update a non-existent group and expect an error.
  auto function = base::MakeRefCounted<TabGroupsUpdateFunction>();
  function->set_extension(extension);
  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), "[0, {}]", profile(),
      api_test_utils::FunctionMode::kNone);
  EXPECT_EQ("No group with id: 0.", error);
}

// Test that tabGroups.update() passes on a saved group.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsUpdateSavedTab) {
  ASSERT_TRUE(SupportsTabGroups());

  // Create a group.
  TabGroupId group = CreateTabGroup({0, 1, 2});
  tab_groups::TabGroupVisualData visual_data(
      u"Initial title", tab_groups::TabGroupColorId::kBlue);
  TabListInterface* tab_list = GetTabListInterface();
  tab_list->SetTabGroupVisualData(group, visual_data);

  tab_groups::TabGroupSyncService* saved_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(saved_service);
  saved_service->AddGroup(CreateSavedTabGroupFromLocalId(group, tab_list));

  int group_id = ExtensionTabUtil::GetGroupId(group);
#if BUILDFLAG(IS_ANDROID)
  ASSERT_TRUE(saved_service->GetGroup(group.token()));
#else
  ASSERT_TRUE(saved_service->GetGroup(group));
#endif

  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  // Update the group, visual metadata, it should pass.
  auto function = base::MakeRefCounted<TabGroupsUpdateFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] =
      R"([%d, {"title": "another title", "color": "red"}])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  // Check that values were updated.
  std::optional<tab_groups::TabGroupVisualData> new_visual_data =
      tab_list->GetTabGroupVisualData(group);
  ASSERT_TRUE(new_visual_data);
  EXPECT_EQ(u"another title", new_visual_data->title());
  EXPECT_EQ(tab_groups::TabGroupColorId::kRed, new_visual_data->color());
}

// Test that moving a group to the right results in the correct tab order.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsMoveRight) {
  ASSERT_TRUE(SupportsTabGroups());

  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  // Create a group with multiple tabs.
  TabGroupId group = CreateTabGroup({1, 2, 3});
  int group_id = ExtensionTabUtil::GetGroupId(group);

  // Use the TabGroupsMoveFunction to move the group to index 2.
  auto function = base::MakeRefCounted<TabGroupsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([%d, {"index": 2}])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  TabListInterface* tab_list = GetTabListInterface();
  EXPECT_EQ(tab_list->GetTab(0)->GetContents(), web_contents(0));
  EXPECT_EQ(tab_list->GetTab(1)->GetContents(), web_contents(4));
  EXPECT_EQ(tab_list->GetTab(2)->GetContents(), web_contents(1));
  EXPECT_EQ(tab_list->GetTab(3)->GetContents(), web_contents(2));
  EXPECT_EQ(tab_list->GetTab(4)->GetContents(), web_contents(3));
  EXPECT_EQ(tab_list->GetTab(5)->GetContents(), web_contents(5));

  EXPECT_EQ(group, tab_list->GetTab(2)->GetGroup().value());
  EXPECT_EQ(group, tab_list->GetTab(3)->GetGroup().value());
  EXPECT_EQ(group, tab_list->GetTab(4)->GetGroup().value());
}

// Test that moving a group to the right of another group results in the
// correct tab order.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest,
                       TabGroupsMoveAdjacentGroupRight) {
  ASSERT_TRUE(SupportsTabGroups());

  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  // Create a group with multiple tabs.
  TabGroupId group = CreateTabGroup({1, 2, 3});
  TabGroupId group_2 = CreateTabGroup({4, 5});

  int group_id = ExtensionTabUtil::GetGroupId(group);

  // Use the TabGroupsMoveFunction to move the group to index 3.
  auto function = base::MakeRefCounted<TabGroupsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([%d, {"index": 3}])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  TabListInterface* tab_list = GetTabListInterface();
  EXPECT_EQ(tab_list->GetTab(0)->GetContents(), web_contents(0));
  EXPECT_EQ(tab_list->GetTab(1)->GetContents(), web_contents(4));
  EXPECT_EQ(tab_list->GetTab(2)->GetContents(), web_contents(5));
  EXPECT_EQ(tab_list->GetTab(3)->GetContents(), web_contents(1));
  EXPECT_EQ(tab_list->GetTab(4)->GetContents(), web_contents(2));
  EXPECT_EQ(tab_list->GetTab(5)->GetContents(), web_contents(3));

  EXPECT_EQ(group_2, tab_list->GetTab(1)->GetGroup().value());
  EXPECT_EQ(group_2, tab_list->GetTab(2)->GetGroup().value());

  EXPECT_EQ(group, tab_list->GetTab(3)->GetGroup().value());
  EXPECT_EQ(group, tab_list->GetTab(4)->GetGroup().value());
  EXPECT_EQ(group, tab_list->GetTab(5)->GetGroup().value());
}

// Test that moving a group to the middle of another group fails.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest,
                       TabGroupsMoveGroupCannotMoveToTheMiddleOfAGroup) {
  ASSERT_TRUE(SupportsTabGroups());

  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  // Create a group with multiple tabs.
  TabGroupId group = CreateTabGroup({1, 2});
  TabGroupId group_2 = CreateTabGroup({4, 5});

  int group_id = ExtensionTabUtil::GetGroupId(group);

  // Use the TabGroupsMoveFunction to move the group to index 3.
  auto function = base::MakeRefCounted<TabGroupsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([%d, {"index": 3}])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  EXPECT_FALSE(api_test_utils::RunFunction(
      function.get(), args, profile(), api_test_utils::FunctionMode::kNone));

  TabListInterface* tab_list = GetTabListInterface();
  EXPECT_EQ(tab_list->GetTab(0)->GetContents(), web_contents(0));
  EXPECT_EQ(tab_list->GetTab(1)->GetContents(), web_contents(1));
  EXPECT_EQ(tab_list->GetTab(2)->GetContents(), web_contents(2));
  EXPECT_EQ(tab_list->GetTab(3)->GetContents(), web_contents(3));
  EXPECT_EQ(tab_list->GetTab(4)->GetContents(), web_contents(4));
  EXPECT_EQ(tab_list->GetTab(5)->GetContents(), web_contents(5));

  EXPECT_EQ(group, tab_list->GetTab(1)->GetGroup().value());
  EXPECT_EQ(group, tab_list->GetTab(2)->GetGroup().value());
  EXPECT_EQ(group_2, tab_list->GetTab(4)->GetGroup().value());
  EXPECT_EQ(group_2, tab_list->GetTab(5)->GetGroup().value());
}

// Test that moving a group to the left results in the correct tab order.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsMoveLeft) {
  ASSERT_TRUE(SupportsTabGroups());

  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  // Create a group with multiple tabs.
  TabGroupId group = CreateTabGroup({2, 3, 4});
  int group_id = ExtensionTabUtil::GetGroupId(group);

  // Use the TabGroupsMoveFunction to move the group to index 0.
  auto function = base::MakeRefCounted<TabGroupsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([%d, {"index": 0}])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  TabListInterface* tab_list = GetTabListInterface();
  EXPECT_EQ(tab_list->GetTab(0)->GetContents(), web_contents(2));
  EXPECT_EQ(tab_list->GetTab(1)->GetContents(), web_contents(3));
  EXPECT_EQ(tab_list->GetTab(2)->GetContents(), web_contents(4));
  EXPECT_EQ(tab_list->GetTab(3)->GetContents(), web_contents(0));
  EXPECT_EQ(tab_list->GetTab(4)->GetContents(), web_contents(1));
  EXPECT_EQ(tab_list->GetTab(5)->GetContents(), web_contents(5));

  EXPECT_EQ(group, tab_list->GetTab(0)->GetGroup().value());
  EXPECT_EQ(group, tab_list->GetTab(1)->GetGroup().value());
  EXPECT_EQ(group, tab_list->GetTab(2)->GetGroup().value());
}

// Test that moving a group to another window works as expected.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsMoveAcrossWindows) {
  ASSERT_TRUE(SupportsTabGroups());

  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  // Create a group with multiple tabs.
  TabGroupId group = CreateTabGroup({2, 3, 4});
  int group_id = ExtensionTabUtil::GetGroupId(group);

  // Create a new window and add a few tabs.
  BrowserWindowInterface* browser2 =
      CreateBrowserWindowWithType(BrowserWindowInterface::TYPE_NORMAL);
  ASSERT_TRUE(ExtensionTabUtil::SupportsTabGroups(browser2));
  browser2->GetWindow()->Activate();
  int window_id2 = ExtensionTabUtil::GetWindowId(browser2);

  TabListInterface* tab_list2 = TabListInterface::From(browser2);
  ASSERT_TRUE(tab_list2);

  // CreateBrowserWindowWithType() creates zero tabs on Win/Mac/Linux, but
  // creates one tab on Android.
  // TODO(crbug.com/477611601): Reconcile this difference.
#if BUILDFLAG(IS_ANDROID)
  constexpr int kInitialTabs = 1;
#else
  constexpr int kInitialTabs = 0;
#endif
  // The target number of tabs in window 2.
  constexpr int kNumTabs2 = 3;
  for (int i = 0; i < kNumTabs2 - kInitialTabs; ++i) {
    tab_list2->OpenTab(GURL("about:blank"), /*index=*/-1);
  }
  ASSERT_EQ(kNumTabs2, tab_list2->GetTabCount());

  // Use the TabGroupsMoveFunction to move the group to index 1 in the other
  // window.
  constexpr int kNumTabsMovedAcrossWindows = 3;
  auto function = base::MakeRefCounted<TabGroupsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([%d, {"windowId": %d, "index": 1}])";
  const std::string args =
      base::StringPrintf(kFormatArgs, group_id, window_id2);
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  // On some platforms (e.g. Android) tab move between windows is async, so
  // allow the operation to complete.
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(kNumTabs2 + kNumTabsMovedAcrossWindows, tab_list2->GetTabCount());
  EXPECT_EQ(tab_list2->GetTab(1)->GetContents(), web_contents(2));
  EXPECT_EQ(tab_list2->GetTab(2)->GetContents(), web_contents(3));
  EXPECT_EQ(tab_list2->GetTab(3)->GetContents(), web_contents(4));

  EXPECT_EQ(group, tab_list2->GetTab(1)->GetGroup().value());
  EXPECT_EQ(group, tab_list2->GetTab(2)->GetGroup().value());
  EXPECT_EQ(group, tab_list2->GetTab(3)->GetGroup().value());

  // Close tabs in the second window.
  CloseAllTabs(tab_list2);

  // Close tabs in the original window.
  TabListInterface* tab_list = GetTabListInterface();
  CloseAllTabs(tab_list);
}

// Test that a group is cannot be moved into the pinned tabs region.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsMoveToPinnedError) {
  ASSERT_TRUE(SupportsTabGroups());

  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  TabListInterface* tab_list = GetTabListInterface();

  // Pin the first 3 tabs.
  tab_list->PinTab(tab_list->GetTab(0)->GetHandle());
  tab_list->PinTab(tab_list->GetTab(1)->GetHandle());
  tab_list->PinTab(tab_list->GetTab(2)->GetHandle());

  // Create a group with an unpinned tab.
  TabGroupId group = CreateTabGroup({4});
  int group_id = ExtensionTabUtil::GetGroupId(group);

  // Try to move the group to index 1 and expect an error.
  auto function = base::MakeRefCounted<TabGroupsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([%d, {"index": 1}])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), args, profile(), api_test_utils::FunctionMode::kNone);
  EXPECT_EQ(
      "Cannot move the group to an index that is in the middle of pinned tabs.",
      error);
}

// Test that a group cannot be moved into the middle of another group.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest,
                       TabGroupsMoveToOtherGroupError) {
  ASSERT_TRUE(SupportsTabGroups());

  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  // Create two tab groups, one with multiple tabs and the other to move.
  CreateTabGroup({0, 1, 2});
  TabGroupId group = CreateTabGroup({4});
  int group_id = ExtensionTabUtil::GetGroupId(group);

  // Try to move the second group to index 1 and expect an error.
  auto function = base::MakeRefCounted<TabGroupsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([%d, {"index": 1}])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), args, profile(), api_test_utils::FunctionMode::kNone);
  EXPECT_EQ(
      "Cannot move the group to an index that is in the middle of another "
      "group.",
      error);
}

// Test that tab groups aren't edited while dragging.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, IsTabStripEditable) {
  ASSERT_TRUE(SupportsTabGroups());

  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();
  int group_id = ExtensionTabUtil::GetGroupId(CreateTabGroup({0}));
  const std::string args =
      base::StringPrintf(R"([%d, {"index": %d}])", group_id, 1);

  EXPECT_TRUE(ExtensionTabUtil::IsTabStripEditable());

  // Succeed moving group when tab strip is editable.
  {
    auto function = base::MakeRefCounted<TabGroupsMoveFunction>();
    function->set_extension(extension);
    EXPECT_TRUE(api_test_utils::RunFunction(
        function.get(), args, profile(), api_test_utils::FunctionMode::kNone));
  }

  // Make tab strip uneditable.
  base::AutoReset<bool> disable_tab_list_editing =
      ExtensionTabUtil::DisableTabListEditingForTesting();

  // Succeed querying group when tab strip is not editable.
  {
    const char* query_args = R"([{"title": "Sample title"}])";
    auto function = base::MakeRefCounted<TabGroupsQueryFunction>();
    function->set_extension(extension);
    EXPECT_TRUE(
        api_test_utils::RunFunction(function.get(), query_args, profile(),
                                    api_test_utils::FunctionMode::kNone));
  }

  // Gracefully cancel group tab drag if tab strip isn't editable.
  {
    auto function = base::MakeRefCounted<TabGroupsMoveFunction>();
    function->set_extension(extension);
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function.get(), args, profile());
    EXPECT_EQ(ExtensionTabUtil::kTabStripNotEditableError, error);
  }
}

// Test that moving a group within the same window but specifying the window id
// does not cause unexpected behavior.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest,
                       TabGroupsMoveGroupWithinSameWindowWithWindowId) {
  ASSERT_TRUE(SupportsTabGroups());

  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  // Create a group with multiple tabs.
  TabGroupId group = CreateTabGroup({1, 2, 3});
  int group_id = ExtensionTabUtil::GetGroupId(group);
  int window_id = ExtensionTabUtil::GetWindowId(browser_window_interface());

  // Move the group to index 1, specifying the current window id.
  auto function = base::MakeRefCounted<TabGroupsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([%d, {"windowId": %d, "index": 1}])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id, window_id);
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  // Verify the tabs are in the correct order.The group should now be at
  // index 1.
  TabListInterface* tab_list = GetTabListInterface();
  EXPECT_EQ(tab_list->GetTab(0)->GetContents(), web_contents(0));
  EXPECT_EQ(tab_list->GetTab(1)->GetContents(), web_contents(1));
  EXPECT_EQ(tab_list->GetTab(2)->GetContents(), web_contents(2));
  EXPECT_EQ(tab_list->GetTab(3)->GetContents(), web_contents(3));
  EXPECT_EQ(tab_list->GetTab(4)->GetContents(), web_contents(4));
  EXPECT_EQ(tab_list->GetTab(5)->GetContents(), web_contents(5));

  // Verify that the group is still associated with the correct tabs.
  EXPECT_EQ(group, tab_list->GetTab(1)->GetGroup().value());
  EXPECT_EQ(group, tab_list->GetTab(2)->GetGroup().value());
  EXPECT_EQ(group, tab_list->GetTab(3)->GetGroup().value());

  CloseAllTabs(tab_list);
}

IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsOnCreated) {
  ASSERT_TRUE(SupportsTabGroups());

  TestEventRouterObserver event_observer(EventRouter::Get(profile()));

  CreateTabGroup({1, 2, 3});

  EXPECT_EQ(2u, event_observer.events().size());
  EXPECT_TRUE(
      event_observer.events().contains(api::tab_groups::OnCreated::kEventName));
  EXPECT_TRUE(
      event_observer.events().contains(api::tab_groups::OnUpdated::kEventName));
}

IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsOnUpdated) {
  ASSERT_TRUE(SupportsTabGroups());

  TabGroupId group = CreateTabGroup({1, 2, 3});

  TestEventRouterObserver event_observer(EventRouter::Get(profile()));

  tab_groups::TabGroupVisualData visual_data(u"Title",
                                             tab_groups::TabGroupColorId::kRed);
  GetTabListInterface()->SetTabGroupVisualData(group, visual_data);

  EXPECT_EQ(1u, event_observer.events().size());
  EXPECT_TRUE(
      event_observer.events().contains(api::tab_groups::OnUpdated::kEventName));
}

IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsOnRemoved) {
  ASSERT_TRUE(SupportsTabGroups());

  CreateTabGroup({1});

  TestEventRouterObserver event_observer(EventRouter::Get(profile()));

  tabs::TabInterface* tab = GetTabListInterface()->GetTab(1);
  ASSERT_TRUE(tab);

  // Close the tab, which will also close the group.
  tab->Close();

  EXPECT_EQ(1u, event_observer.events().size());
  EXPECT_TRUE(
      event_observer.events().contains(api::tab_groups::OnRemoved::kEventName));
}

IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsOnMoved) {
  ASSERT_TRUE(SupportsTabGroups());

  tab_groups::TabGroupId group = CreateTabGroup({1, 2, 3});

  TestEventRouterObserver event_observer(EventRouter::Get(profile()));

  GetTabListInterface()->MoveGroupTo(group, 0);

  EXPECT_EQ(1u, event_observer.events().size());
  EXPECT_TRUE(
      event_observer.events().contains(api::tab_groups::OnMoved::kEventName));
}

}  // namespace
}  // namespace extensions
