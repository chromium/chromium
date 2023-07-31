// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Class that mocks `SearchEngineChoiceService`. This class calls the parent
// class' functions but is needed to be able to use `EXPECT_CALL`.
class MockSearchEngineChoiceService : public SearchEngineChoiceService {
 public:
  MockSearchEngineChoiceService() {
    ON_CALL(*this, NotifyDialogOpened).WillByDefault([this](Browser* browser) {
      number_of_browsers_with_dialogs_open_++;
      return SearchEngineChoiceService::NotifyDialogOpened(browser);
    });
  }
  ~MockSearchEngineChoiceService() override = default;

  static std::unique_ptr<KeyedService> Create(
      content::BrowserContext* context) {
    return std::make_unique<testing::NiceMock<MockSearchEngineChoiceService>>();
  }

  unsigned int GetNumberOfDialogsWithBrowsersOpen() const {
    return number_of_browsers_with_dialogs_open_;
  }

  MOCK_METHOD(void, NotifyDialogOpened, (Browser*), (override));

 private:
  unsigned int number_of_browsers_with_dialogs_open_ = 0;
};

}  // namespace

class SearchEngineChoiceBrowserTest : public InProcessBrowserTest {
 public:
  SearchEngineChoiceBrowserTest() = default;

  SearchEngineChoiceBrowserTest(const SearchEngineChoiceBrowserTest&) = delete;
  SearchEngineChoiceBrowserTest& operator=(
      const SearchEngineChoiceBrowserTest&) = delete;

  ~SearchEngineChoiceBrowserTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  SearchEngineChoiceServiceFactory::GetInstance()
                      ->SetTestingFactoryAndUse(
                          context, base::BindRepeating(
                                       &MockSearchEngineChoiceService::Create));
                }));
  }

  // TODO(crbug.com/1468496): Make this function handle multiple browsers.
  void QuitAndRestoreBrowser(Browser* browser) {
    Profile* profile = browser->profile();
    // Enable SessionRestore to last used pages.
    SessionStartupPref startup_pref(SessionStartupPref::LAST);
    SessionStartupPref::SetStartupPref(profile, startup_pref);

    // Close the browser.
    auto keep_alive = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
    auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
        profile, ProfileKeepAliveOrigin::kBrowserWindow);
    CloseBrowserSynchronously(browser);

    ui_test_utils::AllBrowserTabAddedWaiter tab_waiter;
    SessionRestoreTestHelper restore_observer;

    // Create a new window, which should trigger session restore.
    chrome::NewEmptyWindow(profile);
    Browser* new_browser =
        chrome::FindBrowserWithWebContents(tab_waiter.Wait());

    WaitForTabsToLoad(new_browser);
    restore_observer.Wait();
    keep_alive.reset();
    profile_keep_alive.reset();
    SelectFirstBrowser();
  }

  void WaitForTabsToLoad(Browser* browser) {
    for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
      content::WebContents* contents =
          browser->tab_strip_model()->GetWebContentsAt(i);
      contents->GetController().LoadIfNecessary();
      EXPECT_TRUE(content::WaitForLoadStop(contents));
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_{switches::kSearchEngineChoice};
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(SearchEngineChoiceBrowserTest,
                       RestoreBrowserWithMultipleTabs) {
  // Open 2 more tabs in addition to the existing tab.
  for (int i = 0; i < 2; i++) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("chrome://version"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  EXPECT_EQ(browser()->tab_strip_model()->count(), 3);
  auto* service = static_cast<MockSearchEngineChoiceService*>(
      SearchEngineChoiceServiceFactory::GetForProfile(browser()->profile()));
  ASSERT_TRUE(service);

  // Make sure that the dialog gets opened only once.
  EXPECT_CALL(*service, NotifyDialogOpened(testing::_)).Times(1);
  QuitAndRestoreBrowser(browser());
  ASSERT_TRUE(browser());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 3);
}

IN_PROC_BROWSER_TEST_F(SearchEngineChoiceBrowserTest,
                       RestoreSessionWithMultipleBrowsers) {
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  Profile* profile = browser()->profile();

  // Open another browser.
  Browser* new_browser = CreateBrowser(profile);
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
  auto* service = static_cast<MockSearchEngineChoiceService*>(
      SearchEngineChoiceServiceFactory::GetForProfile(profile));

  // Make sure that we have 2 dialogs open, one for each browser.
  EXPECT_CALL(*service, NotifyDialogOpened(testing::_)).Times(2);

  // Simulate an exit by shutting down the session service. If we don't do this
  // the first window close is treated as though the user closed the window
  // and won't be restored.
  SessionServiceFactory::ShutdownForProfile(profile);

  CloseBrowserSynchronously(new_browser);
  QuitAndRestoreBrowser(browser());
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
}

IN_PROC_BROWSER_TEST_F(SearchEngineChoiceBrowserTest,
                       RestoreSettingsAndChangeUrl) {
  // navigate the current tab to the settings page.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://settings"));
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);

  auto* service = static_cast<MockSearchEngineChoiceService*>(
      SearchEngineChoiceServiceFactory::GetForProfile(browser()->profile()));
  ASSERT_TRUE(service);

  // Make sure that the dialog doesn't open if the tab is the settings page.
  EXPECT_CALL(*service, NotifyDialogOpened(testing::_)).Times(0);
  QuitAndRestoreBrowser(browser());
  ASSERT_TRUE(browser());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_EQ(GURL("chrome://settings"),
            browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL());

  // Dialog opens when we navigate away from settings.
  EXPECT_CALL(*service, NotifyDialogOpened(browser())).Times(1);
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://version"));
  EXPECT_EQ(GURL("chrome://version"),
            browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL());
}

IN_PROC_BROWSER_TEST_F(SearchEngineChoiceBrowserTest,
                       BrowserIsRemovedFromListAfterClose) {
  Profile* profile = browser()->profile();
  Browser* new_browser = CreateBrowser(profile);
  auto* service = static_cast<MockSearchEngineChoiceService*>(
      SearchEngineChoiceServiceFactory::GetForProfile(profile));

  // Check that both browsers are in the set.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
  EXPECT_EQ(service->GetNumberOfDialogsWithBrowsersOpen(), 2u);
  EXPECT_TRUE(service->IsShowingDialog(browser()));
  EXPECT_TRUE(service->IsShowingDialog(new_browser));

  // Check that the open browser remains alone in the set.
  CloseBrowserSynchronously(new_browser);
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_TRUE(service->IsShowingDialog(browser()));
}
