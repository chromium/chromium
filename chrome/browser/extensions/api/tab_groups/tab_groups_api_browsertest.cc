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
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_event_router.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_event_router_factory.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_initialized_observer.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/data_sharing/public/features.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension_builder.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

// NOTE: While the tests in this file appear like they could be unit tests, they
// use browser windows and tabs. As such they need to be browser tests. See
// docs/chrome_browser_design_principles.md. This also makes them easier to port
// to desktop Android.

namespace extensions {
namespace {

base::Value::List RunTabGroupsQueryFunction(
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
    web_contents_.push_back(browser()->tab_strip_model()->GetWebContentsAt(0));

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
    // changes to tab groups.
    WaitForTabGroupSyncServiceInitialized();
  }
  void TearDownOnMainThread() override {
    web_contents_.clear();
    browser()->tab_strip_model()->CloseAllTabs();
    ExtensionBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* web_contents(int index) { return web_contents_[index]; }

  void WaitForTabGroupSyncServiceInitialized() {
    auto observer =
        std::make_unique<tab_groups::TabGroupSyncServiceInitializedObserver>(
            tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile()));
    observer->Wait();
  }

 private:
  // The original web contentses in order.
  std::vector<raw_ptr<content::WebContents, VectorExperimental>> web_contents_;
};

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
  base::Value::List groups_list =
      RunTabGroupsQueryFunction(profile(), extension.get(), kTitleQueryInfo);

  ASSERT_EQ(0u, groups_list.size());

  tab_strip_model2->CloseAllTabs();
}

// Test that querying groups by title returns the correct groups.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsQueryTitle) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model->SupportsTabGroups());

  // Create 3 groups with different titles.
  const tab_groups::TabGroupColorId color = tab_groups::TabGroupColorId::kGrey;

  tab_groups::TabGroupId group1 = tab_strip_model->AddToNewGroup({0});
  tab_groups::TabGroupVisualData visual_data1(u"Sample title", color);
  tab_strip_model->ChangeTabGroupVisuals(group1, visual_data1);

  tab_groups::TabGroupId group2 = tab_strip_model->AddToNewGroup({1});
  tab_groups::TabGroupVisualData visual_data2(u"Sample title suffixed", color);
  tab_strip_model->ChangeTabGroupVisuals(group2, visual_data2);

  tab_groups::TabGroupId group3 = tab_strip_model->AddToNewGroup({2});
  tab_groups::TabGroupVisualData visual_data3(u"Prefixed Sample title", color);
  tab_strip_model->ChangeTabGroupVisuals(group3, visual_data3);

  // Query by title and verify results.
  const char* kTitleQueryInfo = R"([{"title": "Sample title"}])";
  base::Value::List groups_list =
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

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model->SupportsTabGroups());

  // Create 3 groups with different colors.
  tab_groups::TabGroupId group1 = tab_strip_model->AddToNewGroup({0});
  tab_groups::TabGroupVisualData visual_data1(
      std::u16string(), tab_groups::TabGroupColorId::kGrey);
  tab_strip_model->ChangeTabGroupVisuals(group1, visual_data1);

  tab_groups::TabGroupId group2 = tab_strip_model->AddToNewGroup({1});
  tab_groups::TabGroupVisualData visual_data2(
      std::u16string(), tab_groups::TabGroupColorId::kRed);
  tab_strip_model->ChangeTabGroupVisuals(group2, visual_data2);

  tab_groups::TabGroupId group3 = tab_strip_model->AddToNewGroup({2});
  tab_groups::TabGroupVisualData visual_data3(
      std::u16string(), tab_groups::TabGroupColorId::kBlue);
  tab_strip_model->ChangeTabGroupVisuals(group3, visual_data3);

  // Query by color and verify results.
  const char* kColorQueryInfo = R"([{"color": "blue"}])";
  base::Value::List groups_list =
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

  void ShareTabGroup(const tab_groups::TabGroupId& group_id,
                     const syncer::CollaborationId& collaboration_id) {
    tab_groups::TabGroupSyncService* service =
        static_cast<tab_groups::TabGroupSyncService*>(
            tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile()));
    service->MakeTabGroupSharedForTesting(group_id, collaboration_id);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that querying groups by color returns the correct groups.
IN_PROC_BROWSER_TEST_F(SharedTabGroupExtensionsBrowserTest,
                       TabGroupsQueryShared) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model->SupportsTabGroups());

  // Create a group that is unshared.
  tab_groups::TabGroupId group1 = tab_strip_model->AddToNewGroup({0});
  tab_groups::TabGroupVisualData visual_data1(
      std::u16string(), tab_groups::TabGroupColorId::kGrey);
  tab_strip_model->ChangeTabGroupVisuals(group1, visual_data1);

  const char* not_shared_query = R"([{"shared": false}])";
  const char* shared_query = R"([{"shared": true}])";

  {  // Query unshared groups.
    scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

    base::Value::List groups_list =
        RunTabGroupsQueryFunction(profile(), extension.get(), not_shared_query);
    ASSERT_EQ(1u, groups_list.size());

    const base::Value& group_info = groups_list[0];
    ASSERT_EQ(base::Value::Type::DICT, group_info.type());
    EXPECT_EQ(ExtensionTabUtil::GetGroupId(group1),
              *group_info.GetDict().FindInt("id"));
  }

  {  // Query shared groups.
    scoped_refptr<const Extension> extension = CreateTabGroupsExtension();
    base::Value::List groups_list =
        RunTabGroupsQueryFunction(profile(), extension.get(), shared_query);
    ASSERT_EQ(0u, groups_list.size());
  }

  ShareTabGroup(group1, syncer::CollaborationId("collaboration_id_1"));

  {  // Query unshared groups.
    scoped_refptr<const Extension> extension = CreateTabGroupsExtension();
    base::Value::List groups_list =
        RunTabGroupsQueryFunction(profile(), extension.get(), not_shared_query);
    ASSERT_EQ(0u, groups_list.size());
  }

  {  // Query shared groups.
    scoped_refptr<const Extension> extension = CreateTabGroupsExtension();
    base::Value::List groups_list =
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

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model->SupportsTabGroups());

  // Create a group.
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({0, 1, 2});
  tab_groups::TabGroupVisualData visual_data(
      u"Title", tab_groups::TabGroupColorId::kBlue);
  tab_strip_model->ChangeTabGroupVisuals(group, visual_data);
  int group_id = ExtensionTabUtil::GetGroupId(group);

  // Use the TabGroupsGetFunction to get the group object.
  constexpr char kFormatArgs[] = R"([%d])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  base::Value::Dict group_info =
      RunTabGroupsGetFunction(profile(), extension.get(), args);

  EXPECT_EQ(group_id, *group_info.FindInt("id"));
  EXPECT_EQ("Title", *group_info.FindString("title"));
}

// Test that tabGroups.get() fails on a nonexistent group.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsGetError) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());
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

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model->SupportsTabGroups());

  TabGroupModel* tab_group_model = tab_strip_model->group_model();

  // Create a group.
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({0, 1, 2});
  tab_groups::TabGroupVisualData visual_data(
      u"Initial title", tab_groups::TabGroupColorId::kBlue);
  tab_strip_model->ChangeTabGroupVisuals(group, visual_data);
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
  const tab_groups::TabGroupVisualData* new_visual_data =
      tab_group_model->GetTabGroup(group)->visual_data();
  EXPECT_EQ(new_visual_data->title(), u"New title");
  EXPECT_EQ(new_visual_data->color(), tab_groups::TabGroupColorId::kRed);
}

// Test that tabGroups.update() fails on a nonexistent group.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsUpdateError) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

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
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model->SupportsTabGroups());

  TabGroupModel* tab_group_model = tab_strip_model->group_model();

  // Create a group.
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({0, 1, 2});
  tab_groups::TabGroupVisualData visual_data(
      u"Initial title", tab_groups::TabGroupColorId::kBlue);
  tab_strip_model->ChangeTabGroupVisuals(group, visual_data);

  tab_groups::TabGroupSyncService* saved_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(saved_service);
  saved_service->AddGroup(
      tab_groups::SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(group));
  int group_id = ExtensionTabUtil::GetGroupId(group);
  ASSERT_TRUE(saved_service->GetGroup(group));

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
  tab_groups::TabGroupVisualData expected_visual_data(
      u"another title", tab_groups::TabGroupColorId::kRed);
  ASSERT_EQ(expected_visual_data,
            *tab_group_model->GetTabGroup(group)->visual_data());
}

// Test that moving a group to the right results in the correct tab order.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsMoveRight) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // Create a group with multiple tabs.
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({1, 2, 3});
  int group_id = ExtensionTabUtil::GetGroupId(group);

  // Use the TabGroupsMoveFunction to move the group to index 2.
  auto function = base::MakeRefCounted<TabGroupsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([%d, {"index": 2}])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  EXPECT_EQ(tab_strip_model->GetWebContentsAt(0), web_contents(0));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(1), web_contents(4));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(2), web_contents(1));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(3), web_contents(2));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(4), web_contents(3));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(5), web_contents(5));

  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(2).value());
  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(3).value());
  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(4).value());
}

// Test that moving a group to the right of another group results in the correct
// tab order.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest,
                       TabGroupsMoveAdjacentGroupRight) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // Create a group with multiple tabs.
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({1, 2, 3});
  tab_groups::TabGroupId group_2 = tab_strip_model->AddToNewGroup({4, 5});

  int group_id = ExtensionTabUtil::GetGroupId(group);

  // Use the TabGroupsMoveFunction to move the group to index 3.
  auto function = base::MakeRefCounted<TabGroupsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([%d, {"index": 3}])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  EXPECT_EQ(tab_strip_model->GetWebContentsAt(0), web_contents(0));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(1), web_contents(4));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(2), web_contents(5));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(3), web_contents(1));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(4), web_contents(2));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(5), web_contents(3));

  EXPECT_EQ(group_2, tab_strip_model->GetTabGroupForTab(1).value());
  EXPECT_EQ(group_2, tab_strip_model->GetTabGroupForTab(2).value());

  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(3).value());
  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(4).value());
  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(5).value());
}

// Test that moving a group to the right of another group results in the correct
// tab order.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest,
                       TabGroupsMoveGroupCannotMoveToTheMiddleOfAGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // Create a group with multiple tabs.
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({1, 2});
  tab_groups::TabGroupId group_2 = tab_strip_model->AddToNewGroup({4, 5});

  int group_id = ExtensionTabUtil::GetGroupId(group);

  // Use the TabGroupsMoveFunction to move the group to index 3.
  auto function = base::MakeRefCounted<TabGroupsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([%d, {"index": 3}])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  EXPECT_FALSE(api_test_utils::RunFunction(
      function.get(), args, profile(), api_test_utils::FunctionMode::kNone));

  EXPECT_EQ(tab_strip_model->GetWebContentsAt(0), web_contents(0));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(1), web_contents(1));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(2), web_contents(2));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(3), web_contents(3));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(4), web_contents(4));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(5), web_contents(5));

  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(1).value());
  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(2).value());
  EXPECT_EQ(group_2, tab_strip_model->GetTabGroupForTab(4).value());
  EXPECT_EQ(group_2, tab_strip_model->GetTabGroupForTab(5).value());
}

// Test that moving a group to the left results in the correct tab order.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest, TabGroupsMoveLeft) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // Create a group with multiple tabs.
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({2, 3, 4});
  int group_id = ExtensionTabUtil::GetGroupId(group);

  // Use the TabGroupsMoveFunction to move the group to index 0.
  auto function = base::MakeRefCounted<TabGroupsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([%d, {"index": 0}])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  EXPECT_EQ(tab_strip_model->GetWebContentsAt(0), web_contents(2));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(1), web_contents(3));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(2), web_contents(4));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(3), web_contents(0));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(4), web_contents(1));
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(5), web_contents(5));

  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(0).value());
  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(1).value());
  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(2).value());
}

}  // namespace
}  // namespace extensions
