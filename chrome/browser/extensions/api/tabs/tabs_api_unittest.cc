// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_api.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <utility>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
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
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/split_tab_collection.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_window_builder.h"
#include "ash/wm/window_pin_util.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_content_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace extensions {

namespace {

base::ListValue RunTabsQueryFunction(content::BrowserContext* browser_context,
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
    return tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile());
  }

#if BUILDFLAG(IS_CHROMEOS)
  aura::Window* root_window() { return test_helper_.GetContext(); }
#endif

  // Returns whether the commit succeeded or not.
  bool CommitPendingLoadForController(
      content::NavigationController& controller);

  std::vector<content::WebContents*> CreateAndGetWebContents(int count);

 private:
  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

  // The browser (and accompanying window).
  raw_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;

#if BUILDFLAG(IS_CHROMEOS)
  ash::AshTestHelper test_helper_;
#endif
};

void TabsApiUnitTest::SetUp() {
#if BUILDFLAG(IS_CHROMEOS)
  ash::AshTestHelper::InitParams ash_params;
  ash_params.start_session = true;
  test_helper_.SetUp(std::move(ash_params));
#endif
  // Force TabManager/TabLifecycleUnitSource creation.
  g_browser_process->GetTabManager();

  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  auto browser_window = std::make_unique<TestBrowserWindow>();
  browser_window_ = browser_window.get();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = browser_window.release();
  browser_ = Browser::DeprecatedCreateOwnedForTesting(params);

  tab_groups::TabGroupSyncService* saved_service = sync_service();
  ASSERT_TRUE(saved_service);
  saved_service->SetIsInitializedForTesting(true);
}

void TabsApiUnitTest::TearDown() {
  // Do this first before resetting `browser_`.
  GetTabStripModel()->CloseAllTabs();

  browser_window_ = nullptr;
  browser_.reset();
  ExtensionServiceTestBase::TearDown();
#if BUILDFLAG(IS_CHROMEOS)
  test_helper_.TearDown();
#endif
}

bool TabsApiUnitTest::CommitPendingLoadForController(
    content::NavigationController& controller) {
  if (!controller.GetPendingEntry()) {
    return false;
  }

  content::RenderFrameHostTester::CommitPendingLoad(&controller);
  return true;
}

std::vector<content::WebContents*> TabsApiUnitTest::CreateAndGetWebContents(
    int count) {
  std::vector<int> tab_ids;
  std::vector<content::WebContents*> web_contentses;
  for (int i = 0; i < count; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

    CreateSessionServiceTabHelper(contents.get());
    tab_ids.push_back(
        sessions::SessionTabHelper::IdForTab(contents.get()).id());
    web_contentses.push_back(contents.get());

    GetTabStripModel()->AppendWebContents(std::move(contents),
                                          /*foreground=*/true);
  }
  CHECK_EQ(count, GetTabStripModel()->count());
  return web_contentses;
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
  browser()->tab_strip_model()->ChangeTabGroupVisuals(group, visual_data);

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

#if BUILDFLAG(IS_CHROMEOS)
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
  ash::PinWindow(browser_window()->GetNativeWindow(), /*trusted=*/true);

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
  profile()->GetPrefs()->SetBoolean(prefs::kDisableScreenshots, true);

  // Run the function and check result.
  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), "[{}]", profile(), api_test_utils::FunctionMode::kNone);
  EXPECT_EQ(tabs_constants::kScreenshotsDisabled, error);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(TabsApiUnitTest, CannotDuplicatePictureInPictureWindows) {
  // Create picture-in-picture browser.
  auto pip_window = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_PICTURE_IN_PICTURE;
  params.window = pip_window.release();
  std::unique_ptr<Browser> pip_browser;
  pip_browser = Browser::DeprecatedCreateOwnedForTesting(params);
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
  // Check that the tab has discarded
  content::WebContents* new_contents_at_index =
      GetTabStripModel()->GetWebContentsAt(index);
  EXPECT_TRUE(new_contents_at_index->WasDiscarded());
}

// Tests that calling chrome.tabs.discard on a saved tab does discard.
TEST_F(TabsApiUnitTest, TabsDiscardSavedTabGroupTabAllowed) {
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
  browser()->tab_strip_model()->ChangeTabGroupVisuals(group, visual_data);

  auto function = base::MakeRefCounted<TabsDiscardFunction>();
  function->set_extension(extension);
  EXPECT_TRUE(api_test_utils::RunFunction(
      function.get(), base::StringPrintf("[%d]", tab_id), profile(),
      api_test_utils::FunctionMode::kNone));

  // Check that the tab has discarded
  content::WebContents* new_contents_at_index =
      GetTabStripModel()->GetWebContentsAt(0);
  EXPECT_TRUE(new_contents_at_index->WasDiscarded());
}

TEST_F(TabsApiUnitTest, SplitTabsWithHighlightFunction) {
  // Add a couple of web contents to the browser and mark them as split.
  for (int i = 0; i < /*numTabs=*/2; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
    CreateSessionServiceTabHelper(contents.get());
    GetTabStripModel()->AppendWebContents(std::move(contents),
                                          /*foreground=*/true);
  }
  GetTabStripModel()->ActivateTabAt(0);
  GetTabStripModel()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kLinkContextMenu);

  // Run extension to highlight tabs
  auto extension = CreateTabsExtension();
  std::string args = base::StringPrintf("[{\"tabs\": [%d]}]", 0);
  scoped_refptr<TabsHighlightFunction> function =
      base::MakeRefCounted<TabsHighlightFunction>();
  function->set_extension(extension);
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  // Check that both sides of the split are selected.
  ASSERT_TRUE(
      GetTabStripModel()->selection_model().GetListSelectionModel().IsSelected(
          0));
  ASSERT_TRUE(
      GetTabStripModel()->selection_model().GetListSelectionModel().IsSelected(
          1));
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
  browser()->tab_strip_model()->ChangeTabGroupVisuals(group, visual_data);

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

// Tests that calling chrome.tabs.move() works when a tab is moved within a
// split view.
TEST_F(TabsApiUnitTest, TabsMoveWithinSplitView) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("TabsMoveWithinSplitView").Build();

  // Add several web contents to the browser and get their tab IDs.
  std::vector<content::WebContents*> web_contentses =
      CreateAndGetWebContents(5);

  // Create a split with tabs 3 and 4.
  GetTabStripModel()->AddToNewSplit(
      {3}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kTabContextMenu);
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(3).has_value());
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(4).has_value());

  // Use the TabsMoveFunction to move tab at index 0 to the middle of the split
  // view with tabs 3 and 4.
  int tab_extension_id = sessions::SessionTabHelper::IdForTab(
                             GetTabStripModel()->GetWebContentsAt(0))
                             .id();
  auto function = base::MakeRefCounted<TabsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([[%d], {"index": 3}])";
  const std::string args = base::StringPrintf(kFormatArgs, tab_extension_id);

  EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  EXPECT_EQ(ExtensionFunction::ResponseType::kSucceeded,
            *function->response_type());

  // Expect that the tab has been moved between the two tabs previously in a
  // split view and that the split view has been destroyed.
  EXPECT_EQ(GetTabStripModel()->GetWebContentsAt(2), web_contentses[3]);
  EXPECT_EQ(GetTabStripModel()->GetWebContentsAt(3), web_contentses[0]);
  EXPECT_EQ(GetTabStripModel()->GetWebContentsAt(4), web_contentses[4]);
  EXPECT_FALSE(GetTabStripModel()->GetSplitForTab(2).has_value());
  EXPECT_FALSE(GetTabStripModel()->GetSplitForTab(4).has_value());
}

// Tests that calling chrome.tabs.move() works when a tab within a split view is
// moved.
TEST_F(TabsApiUnitTest, TabsMoveFromSplitView) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("TabsMoveFromSplitView").Build();

  // Add several web contents to the browser and get their tab IDs.
  std::vector<content::WebContents*> web_contentses =
      CreateAndGetWebContents(5);

  // Create a split with tabs 3 and 4.
  GetTabStripModel()->AddToNewSplit({3}, split_tabs::SplitTabVisualData(),
                                    split_tabs::SplitTabCreatedSource());
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(3).has_value());
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(4).has_value());

  // Use the TabsMoveFunction to move split tab at index 3 to index 0.
  int tab_extension_id = sessions::SessionTabHelper::IdForTab(
                             GetTabStripModel()->GetWebContentsAt(3))
                             .id();
  auto function = base::MakeRefCounted<TabsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([[%d], {"index": 0}])";
  const std::string args = base::StringPrintf(kFormatArgs, tab_extension_id);

  EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));
  EXPECT_EQ(ExtensionFunction::ResponseType::kSucceeded,
            *function->response_type());

  // Expect that the tab has been moved to index 0 and the original split view
  // is removed.
  EXPECT_EQ(GetTabStripModel()->GetWebContentsAt(0), web_contentses[3]);
  EXPECT_EQ(GetTabStripModel()->GetWebContentsAt(1), web_contentses[0]);
  EXPECT_FALSE(GetTabStripModel()->GetSplitForTab(0).has_value());
  EXPECT_FALSE(GetTabStripModel()->GetSplitForTab(4).has_value());
}

// Tests that chrome.tabs.duplicate removes split view.
TEST_F(TabsApiUnitTest, TabsDuplicateSplitView) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("TabsDuplicateSplitView").Build();

  // Add a couple of web contents to the browser and mark them as split.
  CreateAndGetWebContents(2);
  GetTabStripModel()->ActivateTabAt(0);
  GetTabStripModel()->AddToNewSplit({1}, split_tabs::SplitTabVisualData(),
                                    split_tabs::SplitTabCreatedSource());

  // Check that the two tabs are split
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(0).has_value());
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(1).has_value());

  // Use the TabsDuplicateFunction to duplicate the tab at index 0.
  int tab_extension_id = sessions::SessionTabHelper::IdForTab(
                             GetTabStripModel()->GetWebContentsAt(0))
                             .id();
  auto function = base::MakeRefCounted<TabsDuplicateFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([%d])";
  const std::string args = base::StringPrintf(kFormatArgs, tab_extension_id);

  EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));
  EXPECT_EQ(ExtensionFunction::ResponseType::kSucceeded,
            *function->response_type());

  // Expect that there is one new tab in the tab strip the split view has been
  // removed.
  EXPECT_EQ(3, GetTabStripModel()->count());
  EXPECT_FALSE(GetTabStripModel()->GetSplitForTab(0).has_value());
  EXPECT_FALSE(GetTabStripModel()->GetSplitForTab(1).has_value());
  EXPECT_FALSE(GetTabStripModel()->GetSplitForTab(2).has_value());
}

// Tests that calling chrome.tabs.discard on an inactive tab in an active split
// will discard that tab.
TEST_F(TabsApiUnitTest, TabsDiscardInactiveTabInActiveSplitView) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("TabsDeleteFromSplitView").Build();

  // Add a couple of web contents to the browser and mark them as split.
  CreateAndGetWebContents(2);
  GetTabStripModel()->ActivateTabAt(0);
  GetTabStripModel()->AddToNewSplit({1}, split_tabs::SplitTabVisualData(),
                                    split_tabs::SplitTabCreatedSource());

  // Check that the two tabs are split and the tab at index 0 is active.
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(0).has_value());
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(1).has_value());
  EXPECT_EQ(0, GetTabStripModel()->active_index());

  // The tab discard function should succeed.
  int tab_extension_id = sessions::SessionTabHelper::IdForTab(
                             GetTabStripModel()->GetWebContentsAt(1))
                             .id();
  auto function = base::MakeRefCounted<TabsDiscardFunction>();
  function->set_extension(extension);
  EXPECT_TRUE(api_test_utils::RunFunction(
      function.get(), base::StringPrintf("[%d]", tab_extension_id), profile(),
      api_test_utils::FunctionMode::kNone));
  EXPECT_EQ(ExtensionFunction::ResponseType::kSucceeded,
            *function->response_type());

  // The tab should be discarded.
  content::WebContents* new_contents_at_index =
      GetTabStripModel()->GetWebContentsAt(1);
  EXPECT_TRUE(new_contents_at_index->WasDiscarded());
}

// Tests that calling chrome.tabs.delete works when a tab within a split view
// is deleted.
TEST_F(TabsApiUnitTest, TabsDeleteFromSplitView) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("TabsDeleteFromSplitView").Build();

  // Add a couple of web contents to the browser and mark them as split.
  CreateAndGetWebContents(2);
  GetTabStripModel()->ActivateTabAt(0);
  GetTabStripModel()->AddToNewSplit({1}, split_tabs::SplitTabVisualData(),
                                    split_tabs::SplitTabCreatedSource());

  // Check that the two tabs are split
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(0).has_value());
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(1).has_value());

  // Use the TabsRemoveFunction to remove the tab at index 0.
  int tab_extension_id = sessions::SessionTabHelper::IdForTab(
                             GetTabStripModel()->GetWebContentsAt(0))
                             .id();
  auto function = base::MakeRefCounted<TabsRemoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([[%d]])";
  const std::string args = base::StringPrintf(kFormatArgs, tab_extension_id);

  EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));
  EXPECT_EQ(ExtensionFunction::ResponseType::kSucceeded,
            *function->response_type());

  // Expect that the tab has been removed and the remaining tab is not in a
  // split view.
  EXPECT_EQ(1, GetTabStripModel()->count());
  EXPECT_FALSE(GetTabStripModel()->GetSplitForTab(0).has_value());
}

TEST_F(TabsApiUnitTest, TabsQueryWithSplitView) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("TabsDeleteFromSplitView").Build();

  // Add a couple of web contents to the browser and mark the first two as
  // split.
  CreateAndGetWebContents(5);
  GetTabStripModel()->ActivateTabAt(0);
  GetTabStripModel()->AddToNewSplit({1}, split_tabs::SplitTabVisualData(),
                                    split_tabs::SplitTabCreatedSource());

  // Check that the two tabs are split
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(0).has_value());
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(1).has_value());

  // Use the TabsQueryFunction to get the list of tabs without a split.
  const char* kNoSplitQueryInfo = "[{\"splitViewId\": -1}]";
  base::ListValue tabs_list_without_split =
      RunTabsQueryFunction(profile(), extension.get(), kNoSplitQueryInfo);
  EXPECT_EQ(3u, tabs_list_without_split.size());

  int split_id = ExtensionTabUtil::GetSplitId(
      GetTabStripModel()->GetSplitForTab(0).value());

  constexpr char kFormatArgs[] = R"([{"splitViewId": %d}])";
  const std::string args = base::StringPrintf(kFormatArgs, split_id);
  base::ListValue tabs_list_with_split =
      RunTabsQueryFunction(profile(), extension.get(), args);
  EXPECT_EQ(2u, tabs_list_with_split.size());
  EXPECT_EQ(split_id, tabs_list_with_split[0].GetDict().FindInt("splitViewId"));
}

TEST_F(TabsApiUnitTest, TabsUngroupSingleTabFromSplitView) {
  ASSERT_TRUE(GetTabStripModel()->SupportsTabGroups());

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("TabsUngroupSingleTabFromSplitView").Build();

  // Add a couple of web contents to the browser and mark the first two as
  // split.
  std::vector<content::WebContents*> wc = CreateAndGetWebContents(5);
  GetTabStripModel()->ActivateTabAt(0);
  GetTabStripModel()->AddToNewSplit({1}, split_tabs::SplitTabVisualData(),
                                    split_tabs::SplitTabCreatedSource());

  // Add tabs 0 and 1 to a group.
  GetTabStripModel()->AddToNewGroup({0, 1});

  // Use the TabsUngroupFunction to ungroup tab 1
  auto function = base::MakeRefCounted<TabsUngroupFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([[%d]])";
  const std::string args = base::StringPrintf(
      kFormatArgs, sessions::SessionTabHelper::IdForTab(wc[1]).id());
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  // Expect the group to be deleted because all tabs were ungrouped from it but
  // the split view will remain.
  TabStripModel* tab_strip_model = GetTabStripModel();
  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(0));
  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(1));
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(0).has_value());
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(1).has_value());
}

TEST_F(TabsApiUnitTest, TabsUngroupBothTabsFromSplitView) {
  ASSERT_TRUE(GetTabStripModel()->SupportsTabGroups());

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("TabsUngroupBothTabsFromSplitView").Build();

  // Add a couple of web contents to the browser and mark the first two as
  // split.
  std::vector<content::WebContents*> wc = CreateAndGetWebContents(5);
  GetTabStripModel()->ActivateTabAt(0);
  GetTabStripModel()->AddToNewSplit({1}, split_tabs::SplitTabVisualData(),
                                    split_tabs::SplitTabCreatedSource());

  // Add tabs 0 and 1 to a group.
  GetTabStripModel()->AddToNewGroup({0, 1});

  // Use the TabsUngroupFunction to ungroup tabs 0 and 1
  auto function = base::MakeRefCounted<TabsUngroupFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([[%d, %d]])";
  const std::string args = base::StringPrintf(
      kFormatArgs, sessions::SessionTabHelper::IdForTab(wc[0]).id(),
      sessions::SessionTabHelper::IdForTab(wc[1]).id());
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  // Expect the group to be deleted because all tabs were ungrouped from it but
  // the split view will remain.
  TabStripModel* tab_strip_model = GetTabStripModel();
  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(0));
  EXPECT_FALSE(tab_strip_model->GetTabGroupForTab(1));
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(0).has_value());
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(1).has_value());
}

TEST_F(TabsApiUnitTest, TabsGroupSingleTabInSplitView) {
  ASSERT_TRUE(GetTabStripModel()->SupportsTabGroups());

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("TabsGroupSingleTabInSplitView").Build();

  // Add a couple of web contents to the browser and mark them as split.
  std::vector<content::WebContents*> wc = CreateAndGetWebContents(2);
  GetTabStripModel()->ActivateTabAt(0);
  GetTabStripModel()->AddToNewSplit({1}, split_tabs::SplitTabVisualData(),
                                    split_tabs::SplitTabCreatedSource());

  // Verify that tabs are in a split view.
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(0).has_value());
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(1).has_value());

  // Use the TabsGroupFunction to group tab 0.
  auto function = base::MakeRefCounted<TabsGroupFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([{"tabIds": [%d]}])";
  const std::string args = base::StringPrintf(
      kFormatArgs, sessions::SessionTabHelper::IdForTab(wc[0]).id());
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), args, profile(),
                                          api_test_utils::FunctionMode::kNone));

  // Expect both tabs to be in the same group and still in a split view.
  TabStripModel* tab_strip_model = GetTabStripModel();
  std::optional<tab_groups::TabGroupId> group =
      tab_strip_model->GetTabGroupForTab(0);
  EXPECT_TRUE(group.has_value());
  EXPECT_EQ(group, tab_strip_model->GetTabGroupForTab(1));
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(0).has_value());
  EXPECT_TRUE(GetTabStripModel()->GetSplitForTab(1).has_value());
}

}  // namespace extensions
