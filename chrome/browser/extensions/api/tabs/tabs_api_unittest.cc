// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/api/tabs/tabs_api.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_window_builder.h"
#include "ash/wm/window_pin_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_content_manager.h"
#endif

namespace extensions {

namespace {

base::Value::List RunTabsQueryFunction(content::BrowserContext* browser_context,
                                       const Extension* extension,
                                       const std::string& query_info) {
  auto function = base::MakeRefCounted<TabsQueryFunction>();
  function->set_extension(extension);
  std::optional<base::Value> value =
      api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), query_info, browser_context,
          api_test_utils::FunctionMode::kNone);
  return std::move(*value).TakeList();
}

// Creates an extension with "tabs" permission.
scoped_refptr<const Extension> CreateTabsExtension() {
  return ExtensionBuilder("Extension with tabs permission")
      .AddAPIPermission("tabs")
      .Build();
}

// Creates a WebContents, attaches it to the tab strip, and navigates so we
// have |urls| as history.
content::WebContents* CreateAndAppendWebContentsWithHistory(
    Profile* profile,
    TabStripModel* tab_strip_model,
    const std::vector<GURL>& urls) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile, nullptr);
  content::WebContents* raw_web_contents = web_contents.get();

  tab_strip_model->AppendWebContents(std::move(web_contents), true);

  for (const auto& url : urls) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(raw_web_contents,
                                                               url);
    EXPECT_EQ(url, raw_web_contents->GetLastCommittedURL());
    EXPECT_EQ(url, raw_web_contents->GetVisibleURL());
  }

  return raw_web_contents;
}

}  // namespace

class TabsApiUnitTest : public ExtensionServiceTestBase {
 public:
  TabsApiUnitTest(const TabsApiUnitTest&) = delete;
  TabsApiUnitTest& operator=(const TabsApiUnitTest&) = delete;

 protected:
  TabsApiUnitTest()
      : ExtensionServiceTestBase(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::MainThreadType::UI)) {}
  ~TabsApiUnitTest() override = default;

  Browser* browser() { return browser_.get(); }
  TestBrowserWindow* browser_window() { return browser_window_.get(); }

  TabStripModel* GetTabStripModel() { return browser_->tab_strip_model(); }
  content::WebContents* GetActiveWebContents() {
    return GetTabStripModel()->GetActiveWebContents();
  }

  tab_groups::TabGroupSyncService* sync_service() {
    return tab_groups::SavedTabGroupUtils::GetServiceForProfile(
        browser()->profile());
  }

  void MaybeSaveLocalTabGroup(const tab_groups::LocalTabGroupID& local_id);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  aura::Window* root_window() { return test_helper_.GetContext(); }
#endif

  // Returns whether the commit succeeded or not.
  bool CommitPendingLoadForController(
      content::NavigationController& controller);

 private:
  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

  // The browser (and accompanying window).
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::AshTestHelper test_helper_;
#endif
};

void TabsApiUnitTest::SetUp() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::AshTestHelper::InitParams ash_params;
  ash_params.start_session = true;
  test_helper_.SetUp(std::move(ash_params));
#endif
  // Force TabManager/TabLifecycleUnitSource creation.
  g_browser_process->GetTabManager();

  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  browser_window_ = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = browser_window_.get();
  browser_.reset(Browser::Create(params));

  tab_groups::TabGroupSyncService* saved_service = sync_service();
  ASSERT_TRUE(saved_service);
  saved_service->SetIsInitializedForTesting(true);
}

void TabsApiUnitTest::TearDown() {
  // Do this first before resetting `browser_`.
  GetTabStripModel()->CloseAllTabs();

  browser_.reset();
  browser_window_.reset();
  ExtensionServiceTestBase::TearDown();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  test_helper_.TearDown();
#endif
}

void TabsApiUnitTest::MaybeSaveLocalTabGroup(
    const tab_groups::LocalTabGroupID& local_id) {
  if (tab_groups::IsTabGroupsSaveV2Enabled()) {
    // In V2, all tab groups are automatically saved by default. For this
    // reason, there is no need to manually save the group again.
    return;
  }

  tab_groups::SavedTabGroup saved_group =
      tab_groups::SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(local_id);
  sync_service()->SaveGroup(std::move(saved_group));

  ASSERT_TRUE(sync_service()->GetGroup(local_id));
}

bool TabsApiUnitTest::CommitPendingLoadForController(
    content::NavigationController& controller) {
  if (!controller.GetPendingEntry()) {
    return false;
  }

  content::RenderFrameHostTester::CommitPendingLoad(&controller);
  return true;
}

// Bug fix for crbug.com/1196309. Ensure that an extension can't update the tab
// strip while a tab drag is in progress.
TEST_F(TabsApiUnitTest, IsTabStripEditable) {
  // Add a couple of web contents to the browser and get their tab IDs.
  constexpr int kNumTabs = 2;
  std::vector<int> tab_ids;
  std::vector<content::WebContents*> web_contentses;
  for (int i = 0; i < kNumTabs; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
    CreateSessionServiceTabHelper(contents.get());
    tab_ids.push_back(
        sessions::SessionTabHelper::IdForTab(contents.get()).id());
    web_contentses.push_back(contents.get());
    GetTabStripModel()->AppendWebContents(std::move(contents),
                                          /*foreground=*/true);
  }
  ASSERT_EQ(kNumTabs, GetTabStripModel()->count());

  ASSERT_TRUE(browser_window()->IsTabStripEditable());
  auto extension = CreateTabsExtension();

  // Succeed while tab drag not in progress.
  {
    std::string args = base::StringPrintf("[{\"tabs\": [%d]}]", 0);
    scoped_refptr<TabsHighlightFunction> function =
        base::MakeRefCounted<TabsHighlightFunction>();
    function->set_extension(extension);
    ASSERT_TRUE(api_test_utils::RunFunction(
        function.get(), args, profile(), api_test_utils::FunctionMode::kNone));
  }

  // Start logical drag.
  browser_window()->SetIsTabStripEditable(false);
  ASSERT_FALSE(browser_window()->IsTabStripEditable());

  // Succeed with updates that don't interact with the tab strip model.
  {
    const char* url = "https://example.com/";
    std::string args =
        base::StringPrintf("[%d, {\"url\": \"%s\"}]", tab_ids[0], url);
    scoped_refptr<TabsUpdateFunction> function =
        base::MakeRefCounted<TabsUpdateFunction>();
    function->set_extension(extension);
    std::optional<base::Value> value =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), args, profile(),
            api_test_utils::FunctionMode::kNone);
    ASSERT_TRUE(value && value->is_dict());
    EXPECT_EQ(*value->GetDict().FindString(tabs_constants::kPendingUrlKey),
              url);
  }

  // Succeed while edit in progress and calling chrome.tabs.query.
  {
    const char* args = "[{}]";
    scoped_refptr<TabsQueryFunction> function =
        base::MakeRefCounted<TabsQueryFunction>();
    function->set_extension(extension);
    ASSERT_TRUE(api_test_utils::RunFunction(
        function.get(), args, profile(), api_test_utils::FunctionMode::kNone));
  }

  // Succeed while edit in progress and calling chrome.tabs.get.
  {
    std::string args = base::StringPrintf("[%d]", tab_ids[0]);
    scoped_refptr<TabsGetFunction> function =
        base::MakeRefCounted<TabsGetFunction>();
    function->set_extension(extension);
    ASSERT_TRUE(api_test_utils::RunFunction(
        function.get(), args, profile(), api_test_utils::FunctionMode::kNone));
  }

  // Bug fix for crbug.com/1198717. Error updating tabs while drag in progress.
  {
    std::string args =
        base::StringPrintf("[%d, {\"highlighted\": true}]", tab_ids[0]);
    auto function = base::MakeRefCounted<TabsUpdateFunction>();
    function->set_extension(extension);
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function.get(), args, profile());
    EXPECT_EQ(ExtensionTabUtil::kTabStripNotEditableError, error);
  }

  // Error highlighting tab while drag in progress.
  {
    std::string args = base::StringPrintf("[{\"tabs\": [%d]}]", tab_ids[0]);
    auto function = base::MakeRefCounted<TabsHighlightFunction>();
    function->set_extension(extension);
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function.get(), args, profile(), api_test_utils::FunctionMode::kNone);
    EXPECT_EQ(ExtensionTabUtil::kTabStripNotEditableError, error);
  }

  // Bug fix for crbug.com/1197146. Tab group modification during drag.
  {
    std::string args = base::StringPrintf("[{\"tabIds\": [%d]}]", tab_ids[0]);
    scoped_refptr<TabsGroupFunction> function =
        base::MakeRefCounted<TabsGroupFunction>();
    function->set_extension(extension);
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function.get(), args, profile());
    EXPECT_EQ(ExtensionTabUtil::kTabStripNotEditableError, error);
  }

  // TODO(solomonkinard): Consider adding tests for drag cancellation.
}

TEST_F(TabsApiUnitTest, QueryWithoutTabsPermission) {
  GURL tab_urls[] = {GURL("http://www.google.com"),
                     GURL("http://www.example.com"),
                     GURL("https://www.google.com")};
  std::string tab_titles[] = {"", "Sample title", "Sample title"};

  // Add 3 web contentses to the browser.
  content::WebContents* web_contentses[std::size(tab_urls)];
  for (size_t i = 0; i < std::size(tab_urls); ++i) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    content::WebContents* raw_web_contents = web_contents.get();
    web_contentses[i] = raw_web_contents;
    GetTabStripModel()->AppendWebContents(std::move(web_contents), true);
    EXPECT_EQ(GetActiveWebContents(), raw_web_contents);
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(raw_web_contents);
    web_contents_tester->NavigateAndCommit(tab_urls[i]);
    raw_web_contents->GetController().GetVisibleEntry()->SetTitle(
        base::ASCIIToUTF16(tab_titles[i]));
  }

  const char* kTitleAndURLQueryInfo =
      "[{\"title\": \"Sample title\", \"url\": \"*://www.google.com/*\"}]";

  // An extension without "tabs" permission will see none of the 3 tabs.
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  base::Value::List tabs_list_without_permission =
      RunTabsQueryFunction(profile(), extension.get(), kTitleAndURLQueryInfo);
  EXPECT_EQ(0u, tabs_list_without_permission.size());

  // An extension with "tabs" permission however will see the third tab.
  scoped_refptr<const Extension> extension_with_permission =
      ExtensionBuilder()
          .SetManifest(
              base::Value::Dict()
                  .Set("name", "Extension with tabs permission")
                  .Set("version", "1.0")
                  .Set("manifest_version", 2)
                  .Set("permissions", base::Value::List().Append("tabs")))
          .Build();
  base::Value::List tabs_list_with_permission = RunTabsQueryFunction(
      profile(), extension_with_permission.get(), kTitleAndURLQueryInfo);
  ASSERT_EQ(1u, tabs_list_with_permission.size());

  const base::Value& third_tab_info = tabs_list_with_permission[0];
  ASSERT_TRUE(third_tab_info.is_dict());
  std::optional<int> third_tab_id = third_tab_info.GetDict().FindInt("id");
  EXPECT_EQ(ExtensionTabUtil::GetTabId(web_contentses[2]), third_tab_id);
}

TEST_F(TabsApiUnitTest, QueryWithHostPermission) {
  GURL tab_urls[] = {GURL("http://www.google.com"),
                     GURL("http://www.example.com"),
                     GURL("https://www.google.com/test")};
  std::string tab_titles[] = {"", "Sample title", "Sample title"};

  // Add 3 web contentses to the browser.
  content::WebContents* web_contentses[std::size(tab_urls)];
  for (size_t i = 0; i < std::size(tab_urls); ++i) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    content::WebContents* raw_web_contents = web_contents.get();
    web_contentses[i] = raw_web_contents;
    GetTabStripModel()->AppendWebContents(std::move(web_contents), true);
    EXPECT_EQ(GetActiveWebContents(), raw_web_contents);
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(raw_web_contents);
    web_contents_tester->NavigateAndCommit(tab_urls[i]);
    raw_web_contents->GetController().GetVisibleEntry()->SetTitle(
        base::ASCIIToUTF16(tab_titles[i]));
  }

  const char* kTitleAndURLQueryInfo =
      "[{\"title\": \"Sample title\", \"url\": \"*://www.google.com/*\"}]";

  // An extension with "host" permission will only see the third tab.
  scoped_refptr<const Extension> extension_with_permission =
      ExtensionBuilder()
          .SetManifest(base::Value::Dict()
                           .Set("name", "Extension with tabs permission")
                           .Set("version", "1.0")
                           .Set("manifest_version", 2)
                           .Set("permissions", base::Value::List().Append(
                                                   "*://www.google.com/*")))
          .Build();

  {
    base::Value::List tabs_list_with_permission = RunTabsQueryFunction(
        profile(), extension_with_permission.get(), kTitleAndURLQueryInfo);
    ASSERT_EQ(1u, tabs_list_with_permission.size());

    const base::Value& third_tab_info = tabs_list_with_permission[0];
    ASSERT_TRUE(third_tab_info.is_dict());
    std::optional<int> third_tab_id = third_tab_info.GetDict().FindInt("id");
    EXPECT_EQ(ExtensionTabUtil::GetTabId(web_contentses[2]), third_tab_id);
  }

  // Try the same without title, first and third tabs will match.
  const char* kURLQueryInfo = "[{\"url\": \"*://www.google.com/*\"}]";
  {
    base::Value::List tabs_list_with_permission = RunTabsQueryFunction(
        profile(), extension_with_permission.get(), kURLQueryInfo);
    ASSERT_EQ(2u, tabs_list_with_permission.size());

    const base::Value& first_tab_info = tabs_list_with_permission[0];
    ASSERT_TRUE(first_tab_info.is_dict());
    const base::Value& third_tab_info = tabs_list_with_permission[1];
    ASSERT_TRUE(third_tab_info.is_dict());

    std::vector<int> expected_tabs_ids;
    expected_tabs_ids.push_back(ExtensionTabUtil::GetTabId(web_contentses[0]));
    expected_tabs_ids.push_back(ExtensionTabUtil::GetTabId(web_contentses[2]));

    std::optional<int> first_tab_id = first_tab_info.GetDict().FindInt("id");
    ASSERT_TRUE(first_tab_id);
    EXPECT_TRUE(base::Contains(expected_tabs_ids, *first_tab_id));

    std::optional<int> third_tab_id = third_tab_info.GetDict().FindInt("id");
    ASSERT_TRUE(third_tab_id);
    EXPECT_TRUE(base::Contains(expected_tabs_ids, *third_tab_id));
  }
}

// Test that using the PDF extension for tab updates is treated as a
// renderer-initiated navigation. crbug.com/660498
TEST_F(TabsApiUnitTest, PDFExtensionNavigation) {
  auto manifest = base::Value::Dict()
                      .Set("name", "pdfext")
                      .Set("description", "desc")
                      .Set("version", "0.1")
                      .Set("manifest_version", 2)
                      .Set("permissions", base::Value::List().Append("tabs"));
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetID(extension_misc::kPdfExtensionId)
          .Build();
  ASSERT_TRUE(extension);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContents* raw_web_contents = web_contents.get();
  ASSERT_TRUE(raw_web_contents);
  GetTabStripModel()->AppendWebContents(std::move(web_contents), true);
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(raw_web_contents);
  const GURL kGoogle("http://www.google.com");
  web_contents_tester->NavigateAndCommit(kGoogle);
  EXPECT_EQ(kGoogle, raw_web_contents->GetLastCommittedURL());
  EXPECT_EQ(kGoogle, raw_web_contents->GetVisibleURL());

  CreateSessionServiceTabHelper(raw_web_contents);
  int tab_id = sessions::SessionTabHelper::IdForTab(raw_web_contents).id();

  auto function = base::MakeRefCounted<TabsUpdateFunction>();
  function->SetBrowserContextForTesting(profile());
  function->set_extension(extension.get());
  function->SetArgs(base::test::ParseJsonList(
      base::StringPrintf(R"([%d, {"url":"http://example.com"}])", tab_id)));
  api_test_utils::SendResponseHelper response_helper(function.get());
  function->RunWithValidation().Execute();

  EXPECT_EQ(kGoogle, raw_web_contents->GetLastCommittedURL());
  EXPECT_EQ(kGoogle, raw_web_contents->GetVisibleURL());

  // Clean up.
  response_helper.WaitForResponse();
  base::RunLoop().RunUntilIdle();
}

// Tests that non-validation failure in tabs.executeScript results in error, and
// not bad_message.
// Regression test for https://crbug.com/642794.
TEST_F(TabsApiUnitTest, ExecuteScriptNoTabIsNonFatalError) {
  scoped_refptr<const Extension> extension_with_tabs_permission =
      CreateTabsExtension();
  auto function = base::MakeRefCounted<TabsExecuteScriptFunction>();
  function->set_extension(extension_with_tabs_permission);
  const char* kArgs = R"(["", {"code": ""}])";
  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), kArgs,
      profile(),  // profile() doesn't have any tabs.
      api_test_utils::FunctionMode::kNone);
  EXPECT_EQ(tabs_constants::kNoTabInBrowserWindowError, error);
}

// Tests that calling chrome.tabs.update updates the URL as expected.
TEST_F(TabsApiUnitTest, TabsUpdate) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("UpdateTest").Build();
  const GURL kExampleCom("http://example.com");
  const GURL kChromiumOrg("https://chromium.org");

  // Add a web contents to the browser.
  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  content::WebContents* raw_contents = contents.get();
  GetTabStripModel()->AppendWebContents(std::move(contents), true);
  EXPECT_EQ(GetActiveWebContents(), raw_contents);
  CreateSessionServiceTabHelper(raw_contents);
  int tab_id = sessions::SessionTabHelper::IdForTab(raw_contents).id();

  // Navigate the browser to example.com
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(raw_contents);
  web_contents_tester->NavigateAndCommit(kExampleCom);
  EXPECT_EQ(kExampleCom, raw_contents->GetLastCommittedURL());

  // Use the TabsUpdateFunction to navigate to chromium.org
  auto function = base::MakeRefCounted<TabsUpdateFunction>();
  function->set_extension(extension);
  static constexpr char kFormatArgs[] = R"([%d, {"url": "%s"}])";
  const std::string args =
      base::StringPrintf(kFormatArgs, tab_id, kChromiumOrg.spec().c_str());
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));
  ASSERT_TRUE(
      CommitPendingLoadForController(GetActiveWebContents()->GetController()));
  EXPECT_EQ(kChromiumOrg, raw_contents->GetLastCommittedURL());
}

// Tests that calling chrome.tabs.update does not update a saved tab.
TEST_F(TabsApiUnitTest, TabsUpdateSavedTabGroupTab) {
  const GURL kExampleCom("http://example.com");
  const GURL kChromiumOrg("https://chromium.org");

  // Add a web contents to the browser.
  content::WebContents* raw_contents;
  {
    std::unique_ptr<content::WebContents> contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    raw_contents = contents.get();
    GetTabStripModel()->AppendWebContents(std::move(contents), true);
  }

  // contents used to test active state by taking active state first.
  content::WebContents* raw_non_updated_contents;
  {
    std::unique_ptr<content::WebContents> non_updated_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    raw_non_updated_contents = non_updated_contents.get();
    GetTabStripModel()->AppendWebContents(std::move(non_updated_contents),
                                          false);
  }
  ASSERT_NE(raw_contents, nullptr);
  ASSERT_NE(raw_non_updated_contents, nullptr);

  EXPECT_EQ(GetActiveWebContents(), raw_contents);
  CreateSessionServiceTabHelper(raw_contents);
  int tab_id = sessions::SessionTabHelper::IdForTab(raw_contents).id();
  int non_updated_tab_id =
      sessions::SessionTabHelper::IdForTab(raw_non_updated_contents).id();

  // Navigate the browser to example.com
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(raw_contents);
  web_contents_tester->NavigateAndCommit(kExampleCom);
  EXPECT_EQ(kExampleCom, raw_contents->GetLastCommittedURL());

  tab_groups::TabGroupSyncService* saved_service = sync_service();
  ASSERT_TRUE(saved_service);

  // Group the tab and save it.
  tab_groups::TabGroupId group = GetTabStripModel()->AddToNewGroup(
      {GetTabStripModel()->GetIndexOfWebContents(raw_contents)});
  tab_groups::TabGroupVisualData visual_data(
      u"Initial title", tab_groups::TabGroupColorId::kBlue);
  browser()
      ->tab_strip_model()
      ->group_model()
      ->GetTabGroup(group)
      ->SetVisualData(visual_data);

  MaybeSaveLocalTabGroup(group);

  EXPECT_TRUE(
      ExtensionTabUtil::TabIsInSavedTabGroup(raw_contents, GetTabStripModel()));

  {  // Test the active state change for a saved tab.
    GetTabStripModel()->ActivateTabAt(
        GetTabStripModel()->GetIndexOfWebContents(raw_non_updated_contents));
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("UpdateTest").Build();
    auto function = base::MakeRefCounted<TabsUpdateFunction>();
    function->set_extension(extension);
    static constexpr char kFormatArgs[] = R"([%d, {"active": true}])";
    const std::string args = base::StringPrintf(kFormatArgs, tab_id);
    EXPECT_TRUE(api_test_utils::RunFunction(
        function.get(), args, profile(), api_test_utils::FunctionMode::kNone));
    EXPECT_EQ(GetActiveWebContents(), raw_contents);
  }

  ASSERT_TRUE(saved_service->GetGroup(group));

  {  // Reset the active states, and then test highlighted for a saved tab.
    GetTabStripModel()->ActivateTabAt(
        GetTabStripModel()->GetIndexOfWebContents(raw_non_updated_contents));
    if (GetTabStripModel()->IsTabSelected(
            GetTabStripModel()->GetIndexOfWebContents(raw_contents))) {
      GetTabStripModel()->ToggleSelectionAt(
          GetTabStripModel()->GetIndexOfWebContents(raw_contents));
    }
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("UpdateTest").Build();
    auto function = base::MakeRefCounted<TabsUpdateFunction>();
    function->set_extension(extension);
    static constexpr char kFormatArgs[] = R"([%d, {"highlighted": true}])";
    const std::string args = base::StringPrintf(kFormatArgs, tab_id);
    EXPECT_TRUE(api_test_utils::RunFunction(
        function.get(), args, profile(), api_test_utils::FunctionMode::kNone));
    EXPECT_EQ(GetActiveWebContents(), raw_contents);
  }

  ASSERT_TRUE(saved_service->GetGroup(group));

  {  // Reset the active states, and then test selected state for a saved tab.
    GetTabStripModel()->ActivateTabAt(
        GetTabStripModel()->GetIndexOfWebContents(raw_non_updated_contents));
    if (GetTabStripModel()->IsTabSelected(
            GetTabStripModel()->GetIndexOfWebContents(raw_contents))) {
      GetTabStripModel()->ToggleSelectionAt(
          GetTabStripModel()->GetIndexOfWebContents(raw_contents));
    }
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("UpdateTest").Build();
    auto function = base::MakeRefCounted<TabsUpdateFunction>();
    function->set_extension(extension);
    static constexpr char kFormatArgs[] = R"([%d, {"selected": true}])";
    const std::string args = base::StringPrintf(kFormatArgs, tab_id);
    EXPECT_TRUE(api_test_utils::RunFunction(
        function.get(), args, profile(), api_test_utils::FunctionMode::kNone));
    EXPECT_TRUE(GetTabStripModel()->IsTabSelected(
        GetTabStripModel()->GetIndexOfWebContents(raw_contents)));
  }

  ASSERT_TRUE(saved_service->GetGroup(group));

  {  // Test Muted state.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("UpdateTest").Build();
    auto function = base::MakeRefCounted<TabsUpdateFunction>();
    function->set_extension(extension);
    static constexpr char kFormatArgs[] = R"([%d, {"muted": true}])";
    const std::string args = base::StringPrintf(kFormatArgs, tab_id);
    EXPECT_TRUE(api_test_utils::RunFunction(
        function.get(), args, profile(), api_test_utils::FunctionMode::kNone));
  }

  ASSERT_TRUE(saved_service->GetGroup(group));

  {  // Test setting the opener.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("UpdateTest").Build();
    auto function = base::MakeRefCounted<TabsUpdateFunction>();
    function->set_extension(extension);
    static constexpr char kFormatArgs[] = R"([%d, {"openerTabId": %d}])";
    const std::string args =
        base::StringPrintf(kFormatArgs, tab_id, non_updated_tab_id);
    EXPECT_TRUE(api_test_utils::RunFunction(
        function.get(), args, profile(), api_test_utils::FunctionMode::kNone));
  }

  ASSERT_TRUE(saved_service->GetGroup(group));

  {  // Test setting the disard state.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("UpdateTest").Build();
    auto function = base::MakeRefCounted<TabsUpdateFunction>();
    function->set_extension(extension);
    static constexpr char kFormatArgs[] = R"([%d, {"autoDiscardable": true}])";
    const std::string args = base::StringPrintf(kFormatArgs, tab_id);
    EXPECT_TRUE(api_test_utils::RunFunction(
        function.get(), args, profile(), api_test_utils::FunctionMode::kNone));
  }

  ASSERT_TRUE(saved_service->GetGroup(group));

  {  // Test setting URL should pass.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("UpdateTest").Build();
    auto function = base::MakeRefCounted<TabsUpdateFunction>();
    function->set_extension(extension);
    static constexpr char kFormatArgs[] = R"([%d, {"url": "%s"}])";
    const std::string args =
        base::StringPrintf(kFormatArgs, tab_id, kChromiumOrg.spec().c_str());
    EXPECT_TRUE(api_test_utils::RunFunction(
        function.get(), args, profile(), api_test_utils::FunctionMode::kNone));
  }

  ASSERT_TRUE(saved_service->GetGroup(group));

  // Test setting pinned state should pass. This must be done last since pinning
  // destroys the group.
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("UpdateTest").Build();
    auto function = base::MakeRefCounted<TabsUpdateFunction>();
    function->set_extension(extension);
    static constexpr char kFormatArgs[] = R"([%d, {"pinned": true}])";
    const std::string args = base::StringPrintf(kFormatArgs, tab_id);
    EXPECT_TRUE(api_test_utils::RunFunction(
        function.get(), args, profile(), api_test_utils::FunctionMode::kNone));
  }

  ASSERT_FALSE(saved_service->GetGroup(group));
}

// Tests that calling chrome.tabs.update with a JavaScript URL results
// in an error.
TEST_F(TabsApiUnitTest, TabsUpdateJavaScriptUrlNotAllowed) {
  // An extension with access to www.example.com.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(base::Value::Dict()
                           .Set("name", "Extension with a host permission")
                           .Set("version", "1.0")
                           .Set("manifest_version", 2)
                           .Set("permissions", base::Value::List().Append(
                                                   "http://www.example.com/*")))
          .Build();
  auto function = base::MakeRefCounted<TabsUpdateFunction>();
  function->set_extension(extension);

  // Add a web contents to the browser.
  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  content::WebContents* raw_contents = contents.get();
  GetTabStripModel()->AppendWebContents(std::move(contents), true);
  EXPECT_EQ(GetActiveWebContents(), raw_contents);
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(raw_contents);
  web_contents_tester->NavigateAndCommit(GURL("http://www.example.com"));
  CreateSessionServiceTabHelper(raw_contents);
  int tab_id = sessions::SessionTabHelper::IdForTab(raw_contents).id();

  static constexpr char kFormatArgs[] = R"([%d, {"url": "%s"}])";
  const std::string args = base::StringPrintf(
      kFormatArgs, tab_id, "javascript:void(document.title = 'Won't work')");
  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), args, profile(), api_test_utils::FunctionMode::kNone);
  EXPECT_EQ(ExtensionTabUtil::kJavaScriptUrlsNotAllowedInExtensionNavigations,
            error);
}

// Test that the tabs.move() function correctly rearranges sets of tabs within a
// single window.
TEST_F(TabsApiUnitTest, TabsMoveWithinWindow) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("MoveWithinWindowTest").Build();

  // Add several web contents to the browser and get their tab IDs.
  constexpr int kNumTabs = 5;
  std::vector<int> tab_ids;
  std::vector<content::WebContents*> web_contentses;
  for (int i = 0; i < kNumTabs; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

    CreateSessionServiceTabHelper(contents.get());
    tab_ids.push_back(
        sessions::SessionTabHelper::IdForTab(contents.get()).id());
    web_contentses.push_back(contents.get());

    GetTabStripModel()->AppendWebContents(std::move(contents),
                                          /*foreground=*/true);
  }
  ASSERT_EQ(kNumTabs, GetTabStripModel()->count());

  // Use the TabsMoveFunction to move tabs 0, 2, and 4 to index 1.
  auto function = base::MakeRefCounted<TabsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([[%d, %d, %d], {"index": 1}])";
  const std::string args =
      base::StringPrintf(kFormatArgs, tab_ids[0], tab_ids[2], tab_ids[4]);
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  TabStripModel* tab_strip_model = GetTabStripModel();
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(0), web_contentses[1]);
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(1), web_contentses[0]);
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(2), web_contentses[2]);
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(3), web_contentses[4]);
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(4), web_contentses[3]);
}

// Test that the tabs.move() function correctly rearranges sets of tabs across
// windows.
TEST_F(TabsApiUnitTest, TabsMoveAcrossWindows) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("MoveAcrossWindowTest").Build();

  // Add several web contents to the original browser and get their tab IDs.
  constexpr int kNumTabs = 5;
  std::vector<int> tab_ids;
  std::vector<content::WebContents*> web_contentses;
  for (int i = 0; i < kNumTabs; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

    CreateSessionServiceTabHelper(contents.get());
    tab_ids.push_back(
        sessions::SessionTabHelper::IdForTab(contents.get()).id());
    web_contentses.push_back(contents.get());

    GetTabStripModel()->AppendWebContents(std::move(contents),
                                          /*foreground=*/true);
  }
  ASSERT_EQ(kNumTabs, GetTabStripModel()->count());

  // Create a new window and add a few tabs, getting the ID of the last tab.
  auto window2 = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), /* user_gesture */ true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window2.get();
  // TestBrowserWindowOwner handles its own lifetime, and also cleans up
  // |window2|.
  new TestBrowserWindowOwner(std::move(window2));
  std::unique_ptr<Browser> browser2(Browser::Create(params));
  BrowserList::SetLastActive(browser2.get());
  int window_id2 = ExtensionTabUtil::GetWindowId(browser2.get());

  constexpr int kNumTabs2 = 3;
  for (int i = 0; i < kNumTabs2; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
    CreateSessionServiceTabHelper(contents.get());
    browser2->tab_strip_model()->AppendWebContents(std::move(contents),
                                                   /*foreground=*/true);
  }
  ASSERT_EQ(kNumTabs2, browser2->tab_strip_model()->count());

  content::WebContents* web_contents2 =
      browser2->tab_strip_model()->GetWebContentsAt(2);
  int tab_id2 = sessions::SessionTabHelper::IdForTab(web_contents2).id();

  // Use the TabsMoveFunction to move tab 2 from browser2 and tabs 0, 2, and 4
  // from the original browser to index 1 of browser2.
  constexpr int kNumTabsMovedAcrossWindows = 3;
  auto function = base::MakeRefCounted<TabsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] =
      R"([[%d, %d, %d, %d], {"windowId": %d, "index": 1}])";
  const std::string args = base::StringPrintf(
      kFormatArgs, tab_id2, tab_ids[0], tab_ids[2], tab_ids[4], window_id2);
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  TabStripModel* tab_strip_model2 = browser2->tab_strip_model();
  ASSERT_EQ(kNumTabs2 + kNumTabsMovedAcrossWindows, tab_strip_model2->count());
  EXPECT_EQ(tab_strip_model2->GetWebContentsAt(1), web_contents2);
  EXPECT_EQ(tab_strip_model2->GetWebContentsAt(2), web_contentses[0]);
  EXPECT_EQ(tab_strip_model2->GetWebContentsAt(3), web_contentses[2]);
  EXPECT_EQ(tab_strip_model2->GetWebContentsAt(4), web_contentses[4]);

  // Clean up.
  browser2->tab_strip_model()->CloseAllTabs();
}

TEST_F(TabsApiUnitTest, TabsMoveAcrossWindowsShouldRespectGroupContiguity) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("MoveAcrossWindowWithInvalidIndexTest").Build();

  // Add several web contents to the original browser and get their tab IDs.
  constexpr int kNumTabs = 5;
  std::vector<int> tab_ids;
  std::vector<content::WebContents*> web_contentses;
  for (int i = 0; i < kNumTabs; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

    CreateSessionServiceTabHelper(contents.get());
    tab_ids.push_back(
        sessions::SessionTabHelper::IdForTab(contents.get()).id());
    web_contentses.push_back(contents.get());

    GetTabStripModel()->AppendWebContents(std::move(contents),
                                          /*foreground=*/true);
  }
  ASSERT_EQ(kNumTabs, GetTabStripModel()->count());

  // Create a new window and add a few tabs, getting the ID of the last tab.
  auto window2 = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), /* user_gesture */ true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window2.get();
  // TestBrowserWindowOwner handles its own lifetime, and also cleans up
  // |window2|.
  new TestBrowserWindowOwner(std::move(window2));
  std::unique_ptr<Browser> browser2(Browser::Create(params));
  BrowserList::SetLastActive(browser2.get());
  int window_id2 = ExtensionTabUtil::GetWindowId(browser2.get());

  constexpr int kNumTabs2 = 3;
  for (int i = 0; i < kNumTabs2; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
    CreateSessionServiceTabHelper(contents.get());
    browser2->tab_strip_model()->AppendWebContents(std::move(contents),
                                                   /*foreground=*/true);
  }
  browser2->tab_strip_model()->AddToNewGroup({0, 1});
  ASSERT_EQ(kNumTabs2, browser2->tab_strip_model()->count());

  content::WebContents* web_contents2 = GetTabStripModel()->GetWebContentsAt(2);
  int tab_extension_id =
      sessions::SessionTabHelper::IdForTab(web_contents2).id();

  // Use the TabsMoveFunction to move tab at index 2 from browser2 to the middle
  // of a group in browser1.
  auto function = base::MakeRefCounted<TabsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([[%d], {"windowId": %d, "index": 1}])";
  const std::string args =
      base::StringPrintf(kFormatArgs, tab_extension_id, window_id2);

  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), args, profile(), api_test_utils::FunctionMode::kNone);
  EXPECT_EQ(tabs_constants::kInvalidTabIndexBreaksGroupContiguity, error);

  // Clean up.
  browser2->tab_strip_model()->CloseAllTabs();
}

// Tests that calling chrome.tabs.move doesn't move a saved tab.
TEST_F(TabsApiUnitTest, TabsMoveSavedTabGroupTabAllowed) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("MoveWithinWindowTest").Build();

  // Add several web contents to the browser and get their tab IDs.
  constexpr int kNumTabs = 5;
  std::vector<int> tab_ids;
  std::vector<content::WebContents*> web_contentses;
  for (int i = 0; i < kNumTabs; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

    CreateSessionServiceTabHelper(contents.get());
    tab_ids.push_back(
        sessions::SessionTabHelper::IdForTab(contents.get()).id());
    web_contentses.push_back(contents.get());

    GetTabStripModel()->AppendWebContents(std::move(contents),
                                          /*foreground=*/true);
  }
  ASSERT_EQ(kNumTabs, GetTabStripModel()->count());

  tab_groups::TabGroupSyncService* saved_service = sync_service();
  ASSERT_TRUE(saved_service);

  // Group the tab and save it.
  tab_groups::TabGroupId group = GetTabStripModel()->AddToNewGroup({0, 1, 2});
  tab_groups::TabGroupVisualData visual_data(
      u"Initial title", tab_groups::TabGroupColorId::kBlue);
  browser()
      ->tab_strip_model()
      ->group_model()
      ->GetTabGroup(group)
      ->SetVisualData(visual_data);

  MaybeSaveLocalTabGroup(group);

  // Use the TabsUpdateFunction to navigate to chromium.org
  int tab_extension_id = sessions::SessionTabHelper::IdForTab(
                             GetTabStripModel()->GetWebContentsAt(0))
                             .id();
  auto function = base::MakeRefCounted<TabsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([[%d], {"index": 1}])";
  const std::string args = base::StringPrintf(kFormatArgs, tab_extension_id);

  EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  EXPECT_EQ(GetTabStripModel()->GetWebContentsAt(0), web_contentses[1]);
  EXPECT_EQ(GetTabStripModel()->GetWebContentsAt(1), web_contentses[0]);
}

// Test that the tabs.group() function correctly rearranges sets of tabs within
// a single window before grouping.
TEST_F(TabsApiUnitTest, TabsGroupWithinWindow) {
  ASSERT_TRUE(GetTabStripModel()->SupportsTabGroups());

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("GroupWithinWindowTest").Build();

  // Add several web contents to the browser and get their tab IDs.
  constexpr int kNumTabs = 5;
  std::vector<int> tab_ids;
  std::vector<content::WebContents*> web_contentses;
  for (int i = 0; i < kNumTabs; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

    CreateSessionServiceTabHelper(contents.get());
    tab_ids.push_back(
        sessions::SessionTabHelper::IdForTab(contents.get()).id());
    web_contentses.push_back(contents.get());

    GetTabStripModel()->AppendWebContents(std::move(contents),
                                          /* foreground */ true);
  }
  ASSERT_EQ(kNumTabs, GetTabStripModel()->count());

  // Use the TabsGroupFunction to group tabs 0, 2, and 4.
  auto function = base::MakeRefCounted<TabsGroupFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([{"tabIds": [%d, %d, %d]}])";
  const std::string args =
      base::StringPrintf(kFormatArgs, tab_ids[0], tab_ids[2], tab_ids[4]);
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  TabStripModel* tab_strip_model = GetTabStripModel();
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(0), web_contentses[0]);
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(1), web_contentses[2]);
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(2), web_contentses[4]);
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(3), web_contentses[1]);
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(4), web_contentses[3]);

  std::optional<tab_groups::TabGroupId> group =
      tab_strip_model->GetTabGroupForTab(0);
  EXPECT_TRUE(group.has_value());
  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(1));
  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(2));
  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(3));
  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(4));
}

// Test that the tabs.group() function correctly groups tabs even when given
// out-of-order or duplicate tab IDs.
TEST_F(TabsApiUnitTest, TabsGroupMixedTabIds) {
  ASSERT_TRUE(GetTabStripModel()->SupportsTabGroups());

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("GroupMixedTabIdsTest").Build();

  // Add several web contents to the browser and get their tab IDs.
  constexpr int kNumTabs = 5;
  std::vector<int> tab_ids;
  std::vector<content::WebContents*> web_contentses;
  for (int i = 0; i < kNumTabs; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

    CreateSessionServiceTabHelper(contents.get());
    tab_ids.push_back(
        sessions::SessionTabHelper::IdForTab(contents.get()).id());
    web_contentses.push_back(contents.get());

    GetTabStripModel()->AppendWebContents(std::move(contents),
                                          /*foreground=*/true);
  }
  ASSERT_EQ(kNumTabs, GetTabStripModel()->count());

  // Use the TabsGroupFunction to group tab 1 twice, along with tabs 3 and 2.
  auto function = base::MakeRefCounted<TabsGroupFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([{"tabIds": [%d, %d, %d, %d]}])";
  const std::string args = base::StringPrintf(
      kFormatArgs, tab_ids[1], tab_ids[1], tab_ids[3], tab_ids[2]);
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  TabStripModel* tab_strip_model = GetTabStripModel();
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(0), web_contentses[0]);
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(1), web_contentses[1]);
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(2), web_contentses[2]);
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(3), web_contentses[3]);
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(4), web_contentses[4]);

  std::optional<tab_groups::TabGroupId> group =
      tab_strip_model->GetTabGroupForTab(1);
  EXPECT_TRUE(group.has_value());
  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(0));
  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(1));
  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(2));
  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(3));
  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(4));
}

// Test that the tabs.group() function throws an error if both createProperties
// and groupId are specified.
TEST_F(TabsApiUnitTest, TabsGroupParamsError) {
  ASSERT_TRUE(GetTabStripModel()->SupportsTabGroups());

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("GroupParamsErrorTest").Build();

  // Add several web contents to the browser and get their tab IDs.
  constexpr int kNumTabs = 5;
  std::vector<int> tab_ids;
  std::vector<content::WebContents*> web_contentses;
  for (int i = 0; i < kNumTabs; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

    CreateSessionServiceTabHelper(contents.get());
    tab_ids.push_back(
        sessions::SessionTabHelper::IdForTab(contents.get()).id());
    web_contentses.push_back(contents.get());

    GetTabStripModel()->AppendWebContents(std::move(contents),
                                          /*foreground=*/true);
  }
  ASSERT_EQ(kNumTabs, GetTabStripModel()->count());

  // Add a tab to a group to have an existing group ID.
  tab_groups::TabGroupId group = GetTabStripModel()->AddToNewGroup({1});
  int group_id = ExtensionTabUtil::GetGroupId(group);

  // Attempt to specify both createProperties and groupId.
  auto function = base::MakeRefCounted<TabsGroupFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] =
      R"([{"tabIds": [%d, %d, %d],
           "groupId": %d, "createProperties": {"windowId": -1}}])";
  const std::string args = base::StringPrintf(kFormatArgs, tab_ids[0],
                                              tab_ids[2], tab_ids[4], group_id);
  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), args, profile(), api_test_utils::FunctionMode::kNone);
  EXPECT_EQ(tabs_constants::kGroupParamsError, error);
}

// Test that the tabs.group() function correctly rearranges sets of tabs across
// windows before grouping.
TEST_F(TabsApiUnitTest, TabsGroupAcrossWindows) {
  ASSERT_TRUE(GetTabStripModel()->SupportsTabGroups());

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("GroupAcrossWindowsTest").Build();

  // Add several web contents to the original browser and get their tab IDs.
  constexpr int kNumTabs = 5;
  std::vector<int> tab_ids;
  std::vector<content::WebContents*> web_contentses;
  for (int i = 0; i < kNumTabs; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

    CreateSessionServiceTabHelper(contents.get());
    tab_ids.push_back(
        sessions::SessionTabHelper::IdForTab(contents.get()).id());
    web_contentses.push_back(contents.get());

    GetTabStripModel()->AppendWebContents(std::move(contents),
                                          /*foreground=*/true);
  }
  ASSERT_EQ(kNumTabs, GetTabStripModel()->count());

  // Create a new window and add a few tabs, adding one to a group.
  auto window2 = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), /* user_gesture */ true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window2.get();
  // TestBrowserWindowOwner handles its own lifetime, and also cleans up
  // |window2|.
  new TestBrowserWindowOwner(std::move(window2));
  std::unique_ptr<Browser> browser2(Browser::Create(params));

  constexpr int kNumTabs2 = 3;
  for (int i = 0; i < kNumTabs2; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
    CreateSessionServiceTabHelper(contents.get());
    browser2->tab_strip_model()->AppendWebContents(std::move(contents),
                                                   /*foreground=*/true);
  }
  ASSERT_EQ(kNumTabs2, browser2->tab_strip_model()->count());

  tab_groups::TabGroupId group2 =
      browser2->tab_strip_model()->AddToNewGroup({1});
  int group_id2 = ExtensionTabUtil::GetGroupId(group2);

  // Use the TabsGroupFunction to group tabs 0, 2, and 4 from the original
  // browser into the same group as the one in browser2.
  constexpr int kNumTabsMovedAcrossWindows = 3;
  auto function = base::MakeRefCounted<TabsGroupFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([{"tabIds": [%d, %d, %d], "groupId": %d}])";
  const std::string args = base::StringPrintf(
      kFormatArgs, tab_ids[0], tab_ids[2], tab_ids[4], group_id2);
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  TabStripModel* tab_strip_model2 = browser2->tab_strip_model();
  ASSERT_EQ(kNumTabs2 + kNumTabsMovedAcrossWindows, tab_strip_model2->count());
  EXPECT_EQ(tab_strip_model2->GetWebContentsAt(2), web_contentses[0]);
  EXPECT_EQ(tab_strip_model2->GetWebContentsAt(3), web_contentses[2]);
  EXPECT_EQ(tab_strip_model2->GetWebContentsAt(4), web_contentses[4]);

  EXPECT_EQ(group2, tab_strip_model2->GetTabGroupForTab(1).value());
  EXPECT_EQ(group2, tab_strip_model2->GetTabGroupForTab(2).value());
  EXPECT_EQ(group2, tab_strip_model2->GetTabGroupForTab(3).value());
  EXPECT_EQ(group2, tab_strip_model2->GetTabGroupForTab(4).value());

  // Clean up.
  browser2->tab_strip_model()->CloseAllTabs();
}

// Test that grouping tabs that are in a saved group should fail.
TEST_F(TabsApiUnitTest, TabsGroupForSavedTabGroupTab) {
  ASSERT_TRUE(GetTabStripModel()->SupportsTabGroups());

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("GroupWithinWindowTest").Build();

  // create 2 tabs
  std::vector<int> tab_ids;
  for (int i = 0; i < 2; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

    CreateSessionServiceTabHelper(contents.get());
    tab_ids.push_back(
        sessions::SessionTabHelper::IdForTab(contents.get()).id());

    GetTabStripModel()->AppendWebContents(std::move(contents),
                                          /*foreground=*/true);
  }

  // group the first tab. make sure its saved.
  tab_groups::TabGroupId old_group = GetTabStripModel()->AddToNewGroup({0});
  MaybeSaveLocalTabGroup(old_group);

  // with extensions group the 2 tabs into a new group.
  auto function = base::MakeRefCounted<TabsGroupFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([{"tabIds": [%d, %d]}])";
  const std::string args =
      base::StringPrintf(kFormatArgs, tab_ids[0], tab_ids[1]);
  EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  // make sure the new group exists and is different than the old group.
  EXPECT_TRUE(GetTabStripModel()->GetTabGroupForTab(0).has_value());
  EXPECT_NE(old_group, GetTabStripModel()->GetTabGroupForTab(0).value());
  EXPECT_EQ(GetTabStripModel()->GetTabGroupForTab(0),
            GetTabStripModel()->GetTabGroupForTab(1));
}

// Test that the tabs.ungroup() function correctly ungroups tabs from a single
// group and deletes it.
TEST_F(TabsApiUnitTest, TabsUngroupSingleGroup) {
  ASSERT_TRUE(GetTabStripModel()->SupportsTabGroups());

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("UngroupSingleGroupTest").Build();

  // Add several web contents to the browser and get their tab IDs.
  constexpr int kNumTabs = 5;
  std::vector<int> tab_ids;
  std::vector<content::WebContents*> web_contentses;
  for (int i = 0; i < kNumTabs; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

    CreateSessionServiceTabHelper(contents.get());
    tab_ids.push_back(
        sessions::SessionTabHelper::IdForTab(contents.get()).id());
    web_contentses.push_back(contents.get());

    GetTabStripModel()->AppendWebContents(std::move(contents),
                                          /*foreground=*/true);
  }
  ASSERT_EQ(kNumTabs, GetTabStripModel()->count());

  // Add tabs 1, 2, and 3 to a group.
  tab_groups::TabGroupId group = GetTabStripModel()->AddToNewGroup({1, 2, 3});

  // Use the TabsUngroupFunction to ungroup tabs 1, 2, and 3.
  auto function = base::MakeRefCounted<TabsUngroupFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([[%d, %d, %d]])";
  const std::string args =
      base::StringPrintf(kFormatArgs, tab_ids[1], tab_ids[2], tab_ids[3]);
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  // Expect the group to be deleted because all tabs were ungrouped from it.
  TabStripModel* tab_strip_model = GetTabStripModel();
  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(1));
  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(2));
  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(3));
  EXPECT_FALSE(tab_strip_model->group_model()->ContainsTabGroup(group));
}

// Saved groups should be ungroupable from extensions.
TEST_F(TabsApiUnitTest, TabsUngroupSingleGroupForSavedTabGroup) {
  ASSERT_TRUE(GetTabStripModel()->SupportsTabGroups());

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("UngroupSingleGroupTest").Build();

  int tab_id;

  {
    std::unique_ptr<content::WebContents> web_contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

    CreateSessionServiceTabHelper(web_contents.get());
    tab_id = sessions::SessionTabHelper::IdForTab(web_contents.get()).id();

    GetTabStripModel()->AppendWebContents(std::move(web_contents),
                                          /*foreground=*/true);
  }
  ASSERT_EQ(1, GetTabStripModel()->count());

  tab_groups::TabGroupSyncService* saved_service = sync_service();
  ASSERT_TRUE(saved_service);

  // Group the tab and save it.
  tab_groups::TabGroupId group = GetTabStripModel()->AddToNewGroup({0});
  tab_groups::TabGroupVisualData visual_data(
      u"Initial title", tab_groups::TabGroupColorId::kBlue);
  browser()
      ->tab_strip_model()
      ->group_model()
      ->GetTabGroup(group)
      ->SetVisualData(visual_data);

  MaybeSaveLocalTabGroup(group);

  auto function = base::MakeRefCounted<TabsUngroupFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([[%d]])";
  const std::string args = base::StringPrintf(kFormatArgs, tab_id);
  EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  // The tab should no longer be in the group.
  TabStripModel* tab_strip_model = GetTabStripModel();
  EXPECT_EQ(std::nullopt, tab_strip_model->GetTabGroupForTab(0));
}

// Test that the tabs.ungroup() function correctly ungroups tabs from several
// different groups and deletes any empty ones.
TEST_F(TabsApiUnitTest, TabsUngroupFromMultipleGroups) {
  ASSERT_TRUE(GetTabStripModel()->SupportsTabGroups());

  TabStripModel* tab_strip_model = GetTabStripModel();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("UngroupFromMultipleGroupsTest").Build();

  // Add several web contents to the browser and get their tab IDs.
  constexpr int kNumTabs = 5;
  std::vector<int> tab_ids;
  std::vector<content::WebContents*> web_contentses;
  for (int i = 0; i < kNumTabs; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

    CreateSessionServiceTabHelper(contents.get());
    tab_ids.push_back(
        sessions::SessionTabHelper::IdForTab(contents.get()).id());
    web_contentses.push_back(contents.get());

    tab_strip_model->AppendWebContents(std::move(contents),
                                       /*foreground=*/true);
  }
  ASSERT_EQ(kNumTabs, tab_strip_model->count());

  // Add tabs 1, 2, and 3 to a group1, and tab 4 to group2.
  tab_groups::TabGroupId group1 = tab_strip_model->AddToNewGroup({1, 2, 3});
  tab_groups::TabGroupId group2 = tab_strip_model->AddToNewGroup({4});

  // Use the TabsUngroupFunction to ungroup tabs 2, 3, and 4.
  auto function = base::MakeRefCounted<TabsUngroupFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([[%d, %d, %d]])";
  const std::string args =
      base::StringPrintf(kFormatArgs, tab_ids[2], tab_ids[3], tab_ids[4]);
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  // Expect group2 to be deleted because all tabs were ungrouped from it.
  EXPECT_EQ(group1, tab_strip_model->GetTabGroupForTab(1));
  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(2));
  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(3));
  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(4));
  EXPECT_TRUE(tab_strip_model->group_model()->ContainsTabGroup(group1));
  EXPECT_FALSE(tab_strip_model->group_model()->ContainsTabGroup(group2));
}

TEST_F(TabsApiUnitTest, TabsGoForwardNoSelectedTabError) {
  scoped_refptr<const Extension> extension = CreateTabsExtension();
  auto function = base::MakeRefCounted<TabsGoForwardFunction>();
  function->set_extension(extension);
  // No active tab results in an error.
  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), "[]",
      profile(),  // profile() doesn't have any tabs.
      api_test_utils::FunctionMode::kNone);
  EXPECT_EQ(tabs_constants::kNoSelectedTabError, error);
}

TEST_F(TabsApiUnitTest, TabsGoForwardAndBack) {
  scoped_refptr<const Extension> extension_with_tabs_permission =
      CreateTabsExtension();

  const std::vector<GURL> urls = {GURL("http://www.foo.com"),
                                  GURL("http://www.bar.com")};
  content::WebContents* web_contents = CreateAndAppendWebContentsWithHistory(
      profile(), GetTabStripModel(), urls);
  ASSERT_TRUE(web_contents);

  CreateSessionServiceTabHelper(web_contents);
  const int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  // Go back with chrome.tabs.goBack.
  auto goback_function = base::MakeRefCounted<TabsGoBackFunction>();
  goback_function->set_extension(extension_with_tabs_permission.get());
  api_test_utils::RunFunction(goback_function.get(),
                              base::StringPrintf("[%d]", tab_id), profile(),
                              api_test_utils::FunctionMode::kIncognito);

  content::WebContents* active_webcontent = GetActiveWebContents();
  content::NavigationController& controller =
      active_webcontent->GetController();
  ASSERT_TRUE(CommitPendingLoadForController(controller));
  EXPECT_EQ(urls[0], web_contents->GetLastCommittedURL());
  EXPECT_EQ(urls[0], web_contents->GetVisibleURL());
  EXPECT_TRUE(ui::PAGE_TRANSITION_FORWARD_BACK &
              controller.GetLastCommittedEntry()->GetTransitionType());

  // Go forward with chrome.tabs.goForward.
  auto goforward_function = base::MakeRefCounted<TabsGoForwardFunction>();
  goforward_function->set_extension(extension_with_tabs_permission.get());
  api_test_utils::RunFunction(goforward_function.get(),
                              base::StringPrintf("[%d]", tab_id), profile(),
                              api_test_utils::FunctionMode::kIncognito);

  ASSERT_TRUE(CommitPendingLoadForController(controller));
  EXPECT_EQ(urls[1], web_contents->GetLastCommittedURL());
  EXPECT_EQ(urls[1], web_contents->GetVisibleURL());
  EXPECT_TRUE(ui::PAGE_TRANSITION_FORWARD_BACK &
              controller.GetLastCommittedEntry()->GetTransitionType());

  // If there's no next page, chrome.tabs.goForward should return an error.
  auto goforward_function2 = base::MakeRefCounted<TabsGoForwardFunction>();
  goforward_function2->set_extension(extension_with_tabs_permission.get());
  std::string error = api_test_utils::RunFunctionAndReturnError(
      goforward_function2.get(), base::StringPrintf("[%d]", tab_id), profile(),
      api_test_utils::FunctionMode::kNone);
  EXPECT_EQ(tabs_constants::kNotFoundNextPageError, error);
  EXPECT_EQ(urls[1], web_contents->GetLastCommittedURL());
  EXPECT_EQ(urls[1], web_contents->GetVisibleURL());
}

TEST_F(TabsApiUnitTest, TabsGoForwardAndBackSavedTabGroupTab) {
  scoped_refptr<const Extension> extension_with_tabs_permission =
      CreateTabsExtension();

  const std::vector<GURL> urls = {GURL("http://www.foo.com"),
                                  GURL("http://www.bar.com"),
                                  GURL("http://www.baz.com")};
  content::WebContents* web_contents = CreateAndAppendWebContentsWithHistory(
      profile(), GetTabStripModel(), urls);
  ASSERT_TRUE(web_contents);

  CreateSessionServiceTabHelper(web_contents);
  const int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();

  {
    // Go back with chrome.tabs.goBack.
    auto goback_function = base::MakeRefCounted<TabsGoBackFunction>();
    goback_function->set_extension(extension_with_tabs_permission.get());
    api_test_utils::RunFunction(goback_function.get(),
                                base::StringPrintf("[%d]", tab_id), profile(),
                                api_test_utils::FunctionMode::kIncognito);
    ASSERT_TRUE(CommitPendingLoadForController(web_contents->GetController()));
  }

  EXPECT_EQ(urls[1], web_contents->GetLastCommittedURL());
  EXPECT_EQ(urls[1], web_contents->GetVisibleURL());

  tab_groups::TabGroupSyncService* saved_service = sync_service();
  ASSERT_TRUE(saved_service);

  // Save the tab and expect that it can not be navigated forwards or backwards.
  tab_groups::TabGroupId group = GetTabStripModel()->AddToNewGroup(
      {GetTabStripModel()->GetIndexOfWebContents(web_contents)});
  tab_groups::TabGroupVisualData visual_data(
      u"Initial title", tab_groups::TabGroupColorId::kBlue);
  browser()
      ->tab_strip_model()
      ->group_model()
      ->GetTabGroup(group)
      ->SetVisualData(visual_data);

  MaybeSaveLocalTabGroup(group);

  {
    auto goback_function = base::MakeRefCounted<TabsGoBackFunction>();
    goback_function->set_extension(extension_with_tabs_permission.get());
    EXPECT_TRUE(api_test_utils::RunFunction(
        goback_function.get(), base::StringPrintf("[%d]", tab_id), profile(),
        api_test_utils::FunctionMode::kNone));
  }

  {
    auto goforward_function = base::MakeRefCounted<TabsGoForwardFunction>();
    goforward_function->set_extension(extension_with_tabs_permission.get());
    EXPECT_TRUE(api_test_utils::RunFunction(
        goforward_function.get(), base::StringPrintf("[%d]", tab_id), profile(),
        api_test_utils::FunctionMode::kNone));
  }

  EXPECT_EQ(urls[1], web_contents->GetLastCommittedURL());
  EXPECT_EQ(urls[1], web_contents->GetVisibleURL());
}

TEST_F(TabsApiUnitTest, TabsGoForwardAndBackWithoutTabId) {
  scoped_refptr<const Extension> extension_with_tabs_permission =
      CreateTabsExtension();
  TabStripModel* tab_strip_model = GetTabStripModel();

  // Create first tab with history.
  const std::vector<GURL> tab1_urls = {GURL("http://www.foo.com"),
                                       GURL("http://www.bar.com")};
  content::WebContents* tab1_webcontents =
      CreateAndAppendWebContentsWithHistory(profile(), tab_strip_model,
                                            tab1_urls);
  ASSERT_TRUE(tab1_webcontents);
  EXPECT_EQ(tab1_urls[1], tab1_webcontents->GetLastCommittedURL());
  EXPECT_EQ(tab1_urls[1], tab1_webcontents->GetVisibleURL());
  const int tab1_index =
      tab_strip_model->GetIndexOfWebContents(tab1_webcontents);

  // Create second tab with history.
  const std::vector<GURL> tab2_urls = {GURL("http://www.chrome.com"),
                                       GURL("http://www.google.com")};
  content::WebContents* tab2_webcontents =
      CreateAndAppendWebContentsWithHistory(profile(), tab_strip_model,
                                            tab2_urls);
  ASSERT_TRUE(tab2_webcontents);
  EXPECT_EQ(tab2_urls[1], tab2_webcontents->GetLastCommittedURL());
  EXPECT_EQ(tab2_urls[1], tab2_webcontents->GetVisibleURL());
  const int tab2_index =
      tab_strip_model->GetIndexOfWebContents(tab2_webcontents);
  ASSERT_EQ(2, tab_strip_model->count());

  // Activate first tab.
  tab_strip_model->ActivateTabAt(
      tab1_index, TabStripUserGestureDetails(
                      TabStripUserGestureDetails::GestureType::kOther));

  // Go back without tab_id. But first tab should be navigated since it's
  // activated.
  auto goback_function = base::MakeRefCounted<TabsGoBackFunction>();
  goback_function->set_extension(extension_with_tabs_permission.get());
  api_test_utils::RunFunction(goback_function.get(), "[]", profile(),
                              api_test_utils::FunctionMode::kIncognito);

  content::NavigationController& controller = tab1_webcontents->GetController();
  ASSERT_TRUE(CommitPendingLoadForController(controller));
  EXPECT_EQ(tab1_urls[0], tab1_webcontents->GetLastCommittedURL());
  EXPECT_EQ(tab1_urls[0], tab1_webcontents->GetVisibleURL());
  EXPECT_TRUE(ui::PAGE_TRANSITION_FORWARD_BACK &
              controller.GetLastCommittedEntry()->GetTransitionType());

  // Go forward without tab_id.
  auto goforward_function = base::MakeRefCounted<TabsGoForwardFunction>();
  goforward_function->set_extension(extension_with_tabs_permission.get());
  api_test_utils::RunFunction(goforward_function.get(), "[]", profile(),
                              api_test_utils::FunctionMode::kIncognito);

  ASSERT_TRUE(CommitPendingLoadForController(controller));
  EXPECT_EQ(tab1_urls[1], tab1_webcontents->GetLastCommittedURL());
  EXPECT_EQ(tab1_urls[1], tab1_webcontents->GetVisibleURL());
  EXPECT_TRUE(ui::PAGE_TRANSITION_FORWARD_BACK &
              controller.GetLastCommittedEntry()->GetTransitionType());

  // Activate second tab.
  tab_strip_model->ActivateTabAt(
      tab2_index, TabStripUserGestureDetails(
                      TabStripUserGestureDetails::GestureType::kOther));

  auto goback_function2 = base::MakeRefCounted<TabsGoBackFunction>();
  goback_function2->set_extension(extension_with_tabs_permission.get());
  api_test_utils::RunFunction(goback_function2.get(), "[]", profile(),
                              api_test_utils::FunctionMode::kIncognito);

  content::NavigationController& controller2 =
      tab2_webcontents->GetController();
  ASSERT_TRUE(CommitPendingLoadForController(controller2));
  EXPECT_EQ(tab2_urls[0], tab2_webcontents->GetLastCommittedURL());
  EXPECT_EQ(tab2_urls[0], tab2_webcontents->GetVisibleURL());
  EXPECT_TRUE(ui::PAGE_TRANSITION_FORWARD_BACK &
              controller2.GetLastCommittedEntry()->GetTransitionType());
}

#if BUILDFLAG(IS_CHROMEOS)
// Ensure tabs.captureVisibleTab respects any Data Leak Prevention restrictions.
TEST_F(TabsApiUnitTest, ScreenshotsRestricted) {
  // Setup the function and extension.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Screenshot")
          .AddAPIPermission("tabs")
          .AddHostPermission("<all_urls>")
          .Build();
  auto function = base::MakeRefCounted<TabsCaptureVisibleTabFunction>();
  function->set_extension(extension.get());

  // Add a visible tab.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents.get());
  const GURL kGoogle("http://www.google.com");
  GetTabStripModel()->AppendWebContents(std::move(web_contents),
                                        /*foreground=*/true);
  web_contents_tester->NavigateAndCommit(kGoogle);

  // Setup Data Leak Prevention restriction.
  policy::MockDlpContentManager mock_dlp_content_manager;
  policy::ScopedDlpContentObserverForTesting scoped_dlp_content_observer_(
      &mock_dlp_content_manager);
  EXPECT_CALL(mock_dlp_content_manager, IsScreenshotApiRestricted(testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));

  // Run the function and check result.
  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), "[{}]", profile(), api_test_utils::FunctionMode::kNone);
  EXPECT_EQ(tabs_constants::kScreenshotsDisabledByDlp, error);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(TabsApiUnitTest, DontCreateTabsInLockedFullscreenMode) {
  scoped_refptr<const Extension> extension_with_tabs_permission =
      CreateTabsExtension();

  ash::TestWindowBuilder builder;
  std::unique_ptr<aura::Window> window =
      builder.SetTestWindowDelegate().AllowAllWindowStates().Build();
  browser_window()->SetNativeWindow(window.get());

  auto function = base::MakeRefCounted<TabsCreateFunction>();

  function->set_extension(extension_with_tabs_permission.get());

  // In locked fullscreen mode we should not be able to create any tabs.
  PinWindow(browser_window()->GetNativeWindow(), /*trusted=*/true);

  EXPECT_EQ(ExtensionTabUtil::kLockedFullscreenModeNewTabError,
            api_test_utils::RunFunctionAndReturnError(
                function.get(), "[{}]", profile(),
                api_test_utils::FunctionMode::kNone));
}

// Screenshot should return an error when disabled in user profile preferences.
TEST_F(TabsApiUnitTest, ScreenshotDisabledInProfilePreferences) {
  // Setup the function and extension.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Screenshot")
          .AddAPIPermission("tabs")
          .AddHostPermission("<all_urls>")
          .Build();
  auto function = base::MakeRefCounted<TabsCaptureVisibleTabFunction>();
  function->set_extension(extension.get());

  // Add a visible tab.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents.get());
  const GURL kGoogle("http://www.google.com");
  GetTabStripModel()->AppendWebContents(std::move(web_contents),
                                        /*foreground=*/true);
  web_contents_tester->NavigateAndCommit(kGoogle);

  // Disable screenshot.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kDisableScreenshots,
                                               true);

  // Run the function and check result.
  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), "[{}]", profile(), api_test_utils::FunctionMode::kNone);
  EXPECT_EQ(tabs_constants::kScreenshotsDisabled, error);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(TabsApiUnitTest, CannotDuplicatePictureInPictureWindows) {
  // Create picture-in-picture browser.
  auto pip_window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_PICTURE_IN_PICTURE;
  params.window = pip_window.get();
  std::unique_ptr<Browser> pip_browser;
  pip_browser.reset(Browser::Create(params));
  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  CreateSessionServiceTabHelper(contents.get());
  int pip_tab_id = sessions::SessionTabHelper::IdForTab(contents.get()).id();
  pip_browser->tab_strip_model()->AppendWebContents(std::move(contents),
                                                    /*foreground=*/true);

  // Attempt to duplicate the picture-in-picture tab. This should fail as
  // picture-in-picture tabs are not allowed to be duplicated.
  auto function = base::MakeRefCounted<TabsDuplicateFunction>();
  auto extension = CreateTabsExtension();
  function->set_extension(extension);
  std::string args = base::StringPrintf("[%d]", pip_tab_id);
  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), args, pip_browser->profile(),
      api_test_utils::FunctionMode::kNone);
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(tabs_constants::kCannotDuplicateTab,
                                           base::NumberToString(pip_tab_id)),
            error);

  // Tear down picture-in-picture browser.
  pip_browser->tab_strip_model()->DetachAndDeleteWebContentsAt(0);
  pip_browser.reset();
  pip_window.reset();
}

// Tests that calling chrome.tabs.discard discards the tab.
TEST_F(TabsApiUnitTest, TabsDiscard) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("DiscardTest").Build();
  const GURL kExampleCom("http://example.com");

  // Add a web contents to the browser.
  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  content::WebContents* web_contents = contents.get();
  GetTabStripModel()->AppendWebContents(std::move(contents), true);
  EXPECT_EQ(GetActiveWebContents(), web_contents);
  CreateSessionServiceTabHelper(web_contents);
  int index = GetTabStripModel()->GetIndexOfWebContents(web_contents);
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();

  // Navigate the browser to example.com
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents);
  web_contents_tester->NavigateAndCommit(kExampleCom);
  EXPECT_EQ(kExampleCom, web_contents->GetLastCommittedURL());

  // Use the TabsDiscardFunction to discard the tab.
  auto function = base::MakeRefCounted<TabsDiscardFunction>();
  function->set_extension(extension);
  static constexpr char kFormatArgs[] = R"([%d])";
  const std::string args = base::StringPrintf(kFormatArgs, tab_id);
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));
  // check that the tab has discarded
  content::WebContents* new_contents_at_index =
      GetTabStripModel()->GetWebContentsAt(index);
  EXPECT_TRUE(new_contents_at_index->WasDiscarded());
}

// Tests that calling chrome.tabs.discard on a saved tab does not discard.
TEST_F(TabsApiUnitTest, TabsDiscardSavedTabGroupTabNotAllowed) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("DiscardTest").Build();
  const GURL kExampleCom("http://example.com");

  // Add a web contents to the browser.
  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  content::WebContents* web_contents = contents.get();
  GetTabStripModel()->AppendWebContents(std::move(contents), true);
  EXPECT_EQ(GetActiveWebContents(), web_contents);
  CreateSessionServiceTabHelper(web_contents);
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();

  // Navigate the browser to example.com
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents);
  web_contents_tester->NavigateAndCommit(kExampleCom);
  EXPECT_EQ(kExampleCom, web_contents->GetLastCommittedURL());

  tab_groups::TabGroupSyncService* saved_service = sync_service();
  ASSERT_TRUE(saved_service);

  // Group the tab and save it.
  tab_groups::TabGroupId group = GetTabStripModel()->AddToNewGroup(
      {GetTabStripModel()->GetIndexOfWebContents(web_contents)});
  tab_groups::TabGroupVisualData visual_data(
      u"Initial title", tab_groups::TabGroupColorId::kBlue);
  browser()
      ->tab_strip_model()
      ->group_model()
      ->GetTabGroup(group)
      ->SetVisualData(visual_data);

  MaybeSaveLocalTabGroup(group);

  // The tab discard function should fail.
  auto function = base::MakeRefCounted<TabsDiscardFunction>();
  function->set_extension(extension);
  EXPECT_TRUE(api_test_utils::RunFunction(
      function.get(), base::StringPrintf("[%d]", tab_id), profile(),
      api_test_utils::FunctionMode::kNone));
}

#if BUILDFLAG(IS_CHROMEOS)
// Tests that calling chrome.tabs.discard on a saved tab does discard for
// extensions with locked fullscreen permission. Locked fullscreen permission
// is ChromeOS only.
TEST_F(TabsApiUnitTest,
       TabsDiscardSavedTabGroupTabAllowedForLockedFullscreenPermission) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("DiscardTest")
          .SetID("pmgljoohajacndjcjlajcopidgnhphcl")
          .AddAPIPermission("lockWindowFullscreenPrivate")
          .Build();
  const GURL kExampleCom("http://example.com");

  // Add a web contents to the browser.
  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  content::WebContents* web_contents = contents.get();
  GetTabStripModel()->AppendWebContents(std::move(contents), true);
  EXPECT_EQ(GetActiveWebContents(), web_contents);
  CreateSessionServiceTabHelper(web_contents);
  int index = GetTabStripModel()->GetIndexOfWebContents(web_contents);
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();

  // Navigate the browser to example.com
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents);
  web_contents_tester->NavigateAndCommit(kExampleCom);
  EXPECT_EQ(kExampleCom, web_contents->GetLastCommittedURL());

  tab_groups::TabGroupSyncService* saved_service = sync_service();
  ASSERT_TRUE(saved_service);

  // Group the tab and save it.
  tab_groups::TabGroupId group = GetTabStripModel()->AddToNewGroup(
      {GetTabStripModel()->GetIndexOfWebContents(web_contents)});
  tab_groups::TabGroupVisualData visual_data(
      u"Initial title", tab_groups::TabGroupColorId::kBlue);
  browser()
      ->tab_strip_model()
      ->group_model()
      ->GetTabGroup(group)
      ->SetVisualData(visual_data);

  MaybeSaveLocalTabGroup(group);

  // The tab discard function should not fail.
  auto function = base::MakeRefCounted<TabsDiscardFunction>();
  function->set_extension(extension);
  ASSERT_TRUE(api_test_utils::RunFunction(
      function.get(), base::StringPrintf("[%d]", tab_id), profile(),
      api_test_utils::FunctionMode::kNone));
  // Check that the tab was discarded.
  content::WebContents* new_contents_at_index =
      GetTabStripModel()->GetWebContentsAt(index);
  EXPECT_TRUE(new_contents_at_index->WasDiscarded());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace extensions
