// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_groups/tab_groups_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_constants.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_util.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_builder.h"

namespace extensions {

namespace {

std::unique_ptr<base::ListValue> RunTabGroupsQueryFunction(
    Browser* browser,
    const Extension* extension,
    const std::string& query_info) {
  auto function = base::MakeRefCounted<TabGroupsQueryFunction>();
  function->set_extension(extension);
  std::unique_ptr<base::Value> value(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), query_info, browser, api_test_utils::NONE));
  return base::ListValue::From(std::move(value));
}

std::unique_ptr<base::DictionaryValue> RunTabGroupsGetFunction(
    Browser* browser,
    const Extension* extension,
    const std::string& args) {
  auto function = base::MakeRefCounted<TabGroupsGetFunction>();
  function->set_extension(extension);
  std::unique_ptr<base::Value> value(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), args, browser, api_test_utils::NONE));
  return base::DictionaryValue::From(std::move(value));
}

// Creates an extension with "tabGroups" permission.
scoped_refptr<const Extension> CreateTabGroupsExtension() {
  return ExtensionBuilder("Extension with tabGroups permission")
      .AddPermission("tabGroups")
      .Build();
}

}  // namespace

class TabGroupsApiUnitTest : public ExtensionServiceTestBase {
 public:
  TabGroupsApiUnitTest() = default;
  TabGroupsApiUnitTest(const TabGroupsApiUnitTest&) = delete;
  TabGroupsApiUnitTest& operator=(const TabGroupsApiUnitTest&) = delete;
  ~TabGroupsApiUnitTest() override = default;

 protected:
  Browser* browser() { return browser_.get(); }
  TestBrowserWindow* browser_window() { return browser_window_.get(); }

  content::WebContents* web_contents(int index) {
    return web_contentses_[index];
  }

 private:
  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

  // The browser (and accompanying window).
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;

  // The original web contentses in order.
  std::vector<content::WebContents*> web_contentses_;
};

void TabGroupsApiUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  // Create a browser window.
  TestBrowserWindow* window = new TestBrowserWindow;
  // TestBrowserWindowOwner handles its own lifetime, and also cleans up
  // |window2|.
  new TestBrowserWindowOwner(window);
  Browser::CreateParams params(profile(), /* user_gesture */ true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window;
  browser_ = std::unique_ptr<Browser>(Browser::Create(params));
  BrowserList::SetLastActive(browser_.get());

  // Add several tabs to the browser and get their tab IDs and web contents.
  constexpr int kNumTabs = 6;
  for (int i = 0; i < kNumTabs; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
    CreateSessionServiceTabHelper(contents.get());
    web_contentses_.push_back(contents.get());
    browser_->tab_strip_model()->AppendWebContents(std::move(contents),
                                                   /* foreground */ true);
  }
}

void TabGroupsApiUnitTest::TearDown() {
  browser_->tab_strip_model()->CloseAllTabs();
  browser_.reset();
  browser_window_.reset();
  ExtensionServiceTestBase::TearDown();
}

// Test that querying groups by title returns the correct groups.
TEST_F(TabGroupsApiUnitTest, TabGroupsQueryTitle) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  TabGroupModel* tab_group_model = tab_strip_model->group_model();

  // Create 3 groups with different titles.
  const tab_groups::TabGroupColorId color = tab_groups::TabGroupColorId::kGrey;

  tab_groups::TabGroupId group1 = tab_strip_model->AddToNewGroup({0});
  tab_groups::TabGroupVisualData visual_data1(
      base::ASCIIToUTF16("Sample title"), color);
  tab_group_model->GetTabGroup(group1)->SetVisualData(visual_data1);

  tab_groups::TabGroupId group2 = tab_strip_model->AddToNewGroup({1});
  tab_groups::TabGroupVisualData visual_data2(
      base::ASCIIToUTF16("Sample title suffixed"), color);
  tab_group_model->GetTabGroup(group2)->SetVisualData(visual_data2);

  tab_groups::TabGroupId group3 = tab_strip_model->AddToNewGroup({2});
  tab_groups::TabGroupVisualData visual_data3(
      base::ASCIIToUTF16("Prefixed Sample title"), color);
  tab_group_model->GetTabGroup(group3)->SetVisualData(visual_data3);

  // Query by title and verify results.
  const char* kTitleQueryInfo = R"([{"title": "Sample title"}])";
  std::unique_ptr<base::ListValue> groups_list(
      RunTabGroupsQueryFunction(browser(), extension.get(), kTitleQueryInfo));
  ASSERT_TRUE(groups_list);
  ASSERT_EQ(1u, groups_list->GetSize());

  const base::Value& group_info = groups_list->GetList()[0];
  ASSERT_EQ(base::Value::Type::DICTIONARY, group_info.type());
  EXPECT_EQ(
      tab_groups_util::GetGroupId(group1),
      group_info.FindKeyOfType("id", base::Value::Type::INTEGER)->GetInt());
}

// Test that querying groups by color returns the correct groups.
TEST_F(TabGroupsApiUnitTest, TabGroupsQueryColor) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  TabGroupModel* tab_group_model = tab_strip_model->group_model();

  // Create 3 groups with different colors.
  tab_groups::TabGroupId group1 = tab_strip_model->AddToNewGroup({0});
  tab_groups::TabGroupVisualData visual_data1(
      base::string16(), tab_groups::TabGroupColorId::kGrey);
  tab_group_model->GetTabGroup(group1)->SetVisualData(visual_data1);

  tab_groups::TabGroupId group2 = tab_strip_model->AddToNewGroup({1});
  tab_groups::TabGroupVisualData visual_data2(
      base::string16(), tab_groups::TabGroupColorId::kRed);
  tab_group_model->GetTabGroup(group2)->SetVisualData(visual_data2);

  tab_groups::TabGroupId group3 = tab_strip_model->AddToNewGroup({2});
  tab_groups::TabGroupVisualData visual_data3(
      base::string16(), tab_groups::TabGroupColorId::kBlue);
  tab_group_model->GetTabGroup(group3)->SetVisualData(visual_data3);

  // Query by color and verify results.
  const char* kColorQueryInfo = R"([{"color": "blue"}])";
  std::unique_ptr<base::ListValue> groups_list(
      RunTabGroupsQueryFunction(browser(), extension.get(), kColorQueryInfo));
  ASSERT_TRUE(groups_list);
  ASSERT_EQ(1u, groups_list->GetSize());

  const base::Value& group_info = groups_list->GetList()[0];
  ASSERT_EQ(base::Value::Type::DICTIONARY, group_info.type());
  EXPECT_EQ(
      tab_groups_util::GetGroupId(group3),
      group_info.FindKeyOfType("id", base::Value::Type::INTEGER)->GetInt());
}

// Test that getting a group returns the correct metadata.
TEST_F(TabGroupsApiUnitTest, TabGroupsGetSuccess) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  TabGroupModel* tab_group_model = tab_strip_model->group_model();

  // Create a group.
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({0, 1, 2});
  tab_groups::TabGroupVisualData visual_data(
      base::ASCIIToUTF16("Title"), tab_groups::TabGroupColorId::kBlue);
  tab_group_model->GetTabGroup(group)->SetVisualData(visual_data);
  int group_id = tab_groups_util::GetGroupId(group);

  // Use the TabGroupsGetFunction to get the group object.
  constexpr char kFormatArgs[] = R"([%d])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  std::unique_ptr<base::DictionaryValue> group_info(
      RunTabGroupsGetFunction(browser(), extension.get(), args));

  EXPECT_EQ(
      group_id,
      group_info->FindKeyOfType("id", base::Value::Type::INTEGER)->GetInt());

  EXPECT_EQ("Title",
            group_info->FindKeyOfType("title", base::Value::Type::STRING)
                ->GetString());
}

// Test that tabGroups.get() fails on a nonexistent group.
TEST_F(TabGroupsApiUnitTest, TabGroupsGetError) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  // Try to get a non-existent group and expect an error.
  auto function = base::MakeRefCounted<TabGroupsGetFunction>();
  function->set_extension(extension);
  std::string error = extension_function_test_utils::RunFunctionAndReturnError(
      function.get(), "[0]", browser(), api_test_utils::NONE);
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(
                tab_groups_constants::kGroupNotFoundError, "0"),
            error);
}

// Test that updating group metadata works as expected.
TEST_F(TabGroupsApiUnitTest, TabGroupsUpdateSuccess) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  TabGroupModel* tab_group_model = tab_strip_model->group_model();

  // Create a group.
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({0, 1, 2});
  tab_groups::TabGroupVisualData visual_data(
      base::ASCIIToUTF16("Initial title"), tab_groups::TabGroupColorId::kBlue);
  tab_group_model->GetTabGroup(group)->SetVisualData(visual_data);
  int group_id = tab_groups_util::GetGroupId(group);

  // Use the TabGroupsUpdateFunction to update the title and color.
  auto function = base::MakeRefCounted<TabGroupsUpdateFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] =
      R"([%d, {"title": "New title", "color": "red"}])";
  const std::string args = base::StringPrintf(kFormatArgs, group_id);
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(), args, browser(), api_test_utils::NONE));

  // Verify the new group metadata.
  const tab_groups::TabGroupVisualData* new_visual_data =
      tab_group_model->GetTabGroup(group)->visual_data();
  EXPECT_EQ(new_visual_data->title(), base::ASCIIToUTF16("New title"));
  EXPECT_EQ(new_visual_data->color(), tab_groups::TabGroupColorId::kRed);
}

// Test that tabGroups.update() fails on a nonexistent group.
TEST_F(TabGroupsApiUnitTest, TabGroupsUpdateError) {
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  // Try to update a non-existent group and expect an error.
  auto function = base::MakeRefCounted<TabGroupsUpdateFunction>();
  function->set_extension(extension);
  std::string error = extension_function_test_utils::RunFunctionAndReturnError(
      function.get(), "[0, {}]", browser(), api_test_utils::NONE);
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(
                tab_groups_constants::kGroupNotFoundError, "0"),
            error);
}

}  // namespace extensions
