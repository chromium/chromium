// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engine_choice_utils.h"
#include "components/search_engines/search_engine_utils.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

// TODO(b/280753754): Convert these tests to interactive ui tests.

using testing::_;
using EntryPoint = SearchEngineChoiceService::EntryPoint;

namespace {

const std::string kCustomSearchEngineDomain = "bar.com";
// Class that mocks `SearchEngineChoiceService`.
class MockSearchEngineChoiceService : public SearchEngineChoiceService {
 public:
  explicit MockSearchEngineChoiceService(Profile* profile)
      : SearchEngineChoiceService(
            *profile,
            *TemplateURLServiceFactory::GetForProfile(profile)) {
    ON_CALL(*this, NotifyDialogOpened)
        .WillByDefault([this](Browser* browser, base::OnceClosure callback) {
          number_of_browsers_with_dialogs_open_++;
          SearchEngineChoiceService::NotifyDialogOpened(browser,
                                                        std::move(callback));
        });

    ON_CALL(*this, NotifyChoiceMade)
        .WillByDefault([this](int prepopulate_id, EntryPoint entry_point) {
          number_of_browsers_with_dialogs_open_ = 0;
          SearchEngineChoiceService::NotifyChoiceMade(prepopulate_id,
                                                      entry_point);
        });
  }
  ~MockSearchEngineChoiceService() override = default;

  static std::unique_ptr<KeyedService> Create(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);

    if (!SearchEngineChoiceServiceFactory::
            IsProfileEligibleForChoiceScreenForTesting(CHECK_DEREF(profile))) {
      return nullptr;
    }

    return std::make_unique<testing::NiceMock<MockSearchEngineChoiceService>>(
        profile);
  }

  unsigned int GetNumberOfBrowsersWithDialogsOpen() const {
    return number_of_browsers_with_dialogs_open_;
  }

  MOCK_METHOD(void,
              NotifyDialogOpened,
              (Browser*, base::OnceClosure),
              (override));
  MOCK_METHOD(void, NotifyChoiceMade, (int, EntryPoint), (override));

 private:
  unsigned int number_of_browsers_with_dialogs_open_ = 0;
};

void SetUserSelectedDefaultSearchProvider(
    TemplateURLService* template_url_service) {
  TemplateURLData data;
  data.SetShortName(base::UTF8ToUTF16(kCustomSearchEngineDomain));
  data.SetKeyword(base::UTF8ToUTF16(kCustomSearchEngineDomain));
  data.SetURL("https://" + kCustomSearchEngineDomain + "url?bar={searchTerms}");
  data.new_tab_url = "https://" + kCustomSearchEngineDomain + "newtab";
  data.alternate_urls.push_back("https://" + kCustomSearchEngineDomain +
                                "alt#quux={searchTerms}");

  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
}

webapps::AppId InstallPWA(Profile* profile, const GURL& start_url) {
  auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info->start_url = start_url;
  web_app_info->scope = start_url.GetWithoutFilename();
  web_app_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  web_app_info->title = u"A Web App";
  return web_app::test::InstallWebApp(profile, std::move(web_app_info));
}

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
    // The search engine choice feature is enabled for countries in the EEA
    // region.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, "BE");

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

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // We want the dialog to be enabled after browser creation so that it
    // doesn't get displayed before running the test.
    SearchEngineChoiceService::SetDialogDisabledForTests(
        /*dialog_disabled=*/false);
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
    tab_waiter.Wait();

    for (Browser* new_browser : *BrowserList::GetInstance()) {
      WaitForTabsToLoad(new_browser);
    }

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

  void CheckChoiceScreenWasDisplayedRecordedOnce() {
    histogram_tester_.ExpectBucketCount(
        search_engines::kSearchEngineChoiceScreenEventsHistogram,
        search_engines::SearchEngineChoiceScreenEvents::
            kChoiceScreenWasDisplayed,
        1);
  }

  void CheckDefaultWasSetRecorded() {
    histogram_tester_.ExpectBucketCount(
        search_engines::kSearchEngineChoiceScreenEventsHistogram,
        search_engines::SearchEngineChoiceScreenEvents::kDefaultWasSet, 1);
  }

  void CheckNavigationConditionRecorded(
      search_engines::SearchEngineChoiceScreenConditions condition,
      int count) {
    histogram_tester_.ExpectBucketCount(
        search_engines::kSearchEngineChoiceScreenNavigationConditionsHistogram,
        condition, count);
  }

  void CheckProfileInitConditionRecorded(
      search_engines::SearchEngineChoiceScreenConditions condition,
      int count) {
    histogram_tester_.ExpectBucketCount(
        search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
        condition, count);
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

 private:
  base::AutoReset<bool> scoped_chrome_build_override_ =
      SearchEngineChoiceServiceFactory::ScopedChromeBuildOverrideForTesting(
          /*force_chrome_build=*/true);
  base::test::ScopedFeatureList feature_list_{switches::kSearchEngineChoice};
  base::CallbackListSubscription create_services_subscription_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(SearchEngineChoiceBrowserTest,
                       RestoreBrowserWithMultipleTabs) {
  // Open 2 more tabs in addition to the existing tab.
  for (int i = 0; i < 2; i++) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(chrome::kChromeUINewTabPageURL),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  EXPECT_EQ(browser()->tab_strip_model()->count(), 3);
  auto* service = static_cast<MockSearchEngineChoiceService*>(
      SearchEngineChoiceServiceFactory::GetForProfile(browser()->profile()));
  ASSERT_TRUE(service);

  // Make sure that the dialog gets opened only once and the display is
  // recorded.
  EXPECT_CALL(*service, NotifyDialogOpened(_, _)).Times(1);
  CheckChoiceScreenWasDisplayedRecordedOnce();

  QuitAndRestoreBrowser(browser());
  ASSERT_TRUE(browser());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 3);
}

IN_PROC_BROWSER_TEST_F(SearchEngineChoiceBrowserTest, BackgroundTab) {
  // Navigate the current tab to the settings page.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);

  auto* service = static_cast<MockSearchEngineChoiceService*>(
      SearchEngineChoiceServiceFactory::GetForProfile(browser()->profile()));
  ASSERT_TRUE(service);
  EXPECT_FALSE(service->IsShowingDialog(browser()));

  // Load an eligible tab in the background, the dialog does not open.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_FALSE(service->IsShowingDialog(browser()));

  // Switch to the eligible tab after it's loaded, the dialog opens.
  browser()->tab_strip_model()->ActivateTabAt(1);
  ASSERT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      GURL(chrome::kChromeUINewTabPageURL));
  EXPECT_TRUE(service->IsShowingDialog(browser()));
}

IN_PROC_BROWSER_TEST_F(SearchEngineChoiceBrowserTest,
                       RestoreSessionWithMultipleBrowsers) {
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  Profile* profile = browser()->profile();

  // Open another browser with the same profile.
  Browser* new_browser = CreateBrowser(profile);
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
  auto* service = static_cast<MockSearchEngineChoiceService*>(
      SearchEngineChoiceServiceFactory::GetForProfile(profile));

  // Make sure that we have 2 dialogs open, one for each browser.
  EXPECT_CALL(*service, NotifyDialogOpened(_, _)).Times(2);
  // Make sure that the display was recorded only once.
  CheckChoiceScreenWasDisplayedRecordedOnce();

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
  // Navigate the current tab to the settings page.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUISettingsURL),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);

  auto* service = static_cast<MockSearchEngineChoiceService*>(
      SearchEngineChoiceServiceFactory::GetForProfile(browser()->profile()));
  ASSERT_TRUE(service);
  EXPECT_FALSE(service->IsShowingDialog(browser()));

  // Make sure that the dialog doesn't open if the restored tab is the settings
  // page.
  QuitAndRestoreBrowser(browser());
  ASSERT_TRUE(browser());
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_EQ(GURL(chrome::kChromeUISettingsURL),
            browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL());
  EXPECT_FALSE(service->IsShowingDialog(browser()));

  // Make sure that the dialog doesn't get displayed after navigating to
  // `chrome://welcome`.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIWelcomeURL),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_FALSE(service->IsShowingDialog(browser()));

  // Make sure that the dialog doesn't open on the devtools url
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIDevToolsURL),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_FALSE(service->IsShowingDialog(browser()));

  // Dialog gets displayed when we navigate to chrome://new-tab-page.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_TRUE(service->IsShowingDialog(browser()));
}

IN_PROC_BROWSER_TEST_F(SearchEngineChoiceBrowserTest,
                       BrowserIsRemovedFromListAfterClose) {
  Profile* profile = browser()->profile();
  Browser* new_browser = CreateBrowser(profile);
  auto* service = static_cast<MockSearchEngineChoiceService*>(
      SearchEngineChoiceServiceFactory::GetForProfile(profile));

  // Navigate to a URL to display the dialog.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Check that both browsers are in the set.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2u);
  EXPECT_EQ(service->GetNumberOfBrowsersWithDialogsOpen(), 2u);
  EXPECT_TRUE(service->IsShowingDialog(browser()));
  EXPECT_TRUE(service->IsShowingDialog(new_browser));

  // Check that the open browser remains alone in the set.
  CloseBrowserSynchronously(new_browser);
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_TRUE(service->IsShowingDialog(browser()));
}

IN_PROC_BROWSER_TEST_F(SearchEngineChoiceBrowserTest,
                       DialogsOnBrowsersWithSameProfileCloseAfterMakingChoice) {
  // Create 2 browsers with the same profile.
  Profile* first_profile = browser()->profile();
  Browser* first_browser_with_first_profile = browser();
  Browser* second_browser_with_first_profile = CreateBrowser(first_profile);
  auto* first_profile_service = static_cast<MockSearchEngineChoiceService*>(
      SearchEngineChoiceServiceFactory::GetForProfile(first_profile));

  // Navigate to a URL to display the dialog.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Make sure that there are 2 dialogs open for that profile
  EXPECT_EQ(first_profile_service->GetNumberOfBrowsersWithDialogsOpen(), 2u);
  CheckChoiceScreenWasDisplayedRecordedOnce();

  // Simulate a dialog closing event for the first profile and test that the
  // dialogs for that profile are closed.
  first_profile_service->NotifyChoiceMade(
      /*prepopulate_id=*/1, EntryPoint::kDialog);
  CheckDefaultWasSetRecorded();
  EXPECT_FALSE(
      first_profile_service->IsShowingDialog(first_browser_with_first_profile));
  EXPECT_FALSE(first_profile_service->IsShowingDialog(
      second_browser_with_first_profile));
  EXPECT_EQ(first_profile_service->GetNumberOfBrowsersWithDialogsOpen(), 0u);
}

// We don't run this test on ChromeOS Ash because we can't create multiple
// profiles on Ash.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SearchEngineChoiceBrowserTest,
                       DialogGetsDisplayedForAllProfiles) {
  // Start a first profile that will later show the dialog.
  Profile* first_profile = browser()->profile();
  Browser* browser_with_first_profile = browser();
  auto* first_profile_service = static_cast<MockSearchEngineChoiceService*>(
      SearchEngineChoiceServiceFactory::GetForProfile(first_profile));
  ASSERT_TRUE(first_profile_service);

  // Create the second profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* second_profile = &profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
  auto* second_profile_service = static_cast<MockSearchEngineChoiceService*>(
      SearchEngineChoiceServiceFactory::GetForProfile(second_profile));
  ASSERT_TRUE(second_profile_service);

  // Navigate to a URL to display the dialog in the first profile.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser_with_first_profile, GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_TRUE(
      first_profile_service->IsShowingDialog(browser_with_first_profile));
  CheckChoiceScreenWasDisplayedRecordedOnce();

  // So far, no dialog check should have been failed based on a profile having
  // claimed the dialog.
  EXPECT_EQ(0, histogram_tester().GetBucketCount(
                   search_engines::
                       kSearchEngineChoiceScreenNavigationConditionsHistogram,
                   search_engines::SearchEngineChoiceScreenConditions::
                       kProfileOutOfScope));

  // Open a browser with the second profile, it should open a dialog too.
  Browser* browser_with_second_profile = CreateBrowser(second_profile);
  EXPECT_TRUE(
      second_profile_service->IsShowingDialog(browser_with_second_profile));

  // An additional success record should have been made.
  histogram_tester().ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenEventsHistogram,
      search_engines::SearchEngineChoiceScreenEvents::kChoiceScreenWasDisplayed,
      2);

  // Still expect no profile-based rejection.
  EXPECT_EQ(0, histogram_tester().GetBucketCount(
                   search_engines::
                       kSearchEngineChoiceScreenNavigationConditionsHistogram,
                   search_engines::SearchEngineChoiceScreenConditions::
                       kProfileOutOfScope));
}
#endif

IN_PROC_BROWSER_TEST_F(SearchEngineChoiceBrowserTest,
                       DialogDoesNotShowAgainAfterSettingPref) {
  Profile* profile = browser()->profile();
  auto* service = static_cast<MockSearchEngineChoiceService*>(
      SearchEngineChoiceServiceFactory::GetForProfile(profile));

  // Navigate to a URL to display the dialog.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  EXPECT_TRUE(service->IsShowingDialog(browser()));

  CheckNavigationConditionRecorded(
      search_engines::SearchEngineChoiceScreenConditions::kEligible, 1);

  // Set the pref and simulate a dialog closing event.
  service->NotifyChoiceMade(/*prepopulate_id=*/1, EntryPoint::kDialog);
  EXPECT_FALSE(service->IsShowingDialog(browser()));
  histogram_tester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 1);

  // Test that the dialog doesn't get shown again after opening the browser.
  QuitAndRestoreBrowser(browser());
  EXPECT_FALSE(service->IsShowingDialog(browser()));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SearchEngineChoiceBrowserTest,
                       DialogDoesNotOverlapWithProfileCustomizationDialog) {
  Profile* profile = browser()->profile();
  auto* service = static_cast<MockSearchEngineChoiceService*>(
      SearchEngineChoiceServiceFactory::GetForProfile(profile));

  // Show the profile customization dialog.
  browser()->signin_view_controller()->ShowModalProfileCustomizationDialog(
      /*is_local_profile_creation=*/true);

  // Navigate to a URL
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_FALSE(service->IsShowingDialog(browser()));

  CheckNavigationConditionRecorded(
      search_engines::SearchEngineChoiceScreenConditions::
          kSuppressedByOtherDialog,
      1);
}
#endif

// This test is disabled because we currently don't want to show the dialog for
// users who have custom search engines.
// TODO(b/302687046): Modify the test based on the decision towards custom
// search engines.
IN_PROC_BROWSER_TEST_F(SearchEngineChoiceBrowserTest,
                       DISABLED_ChooseCustomDefaultSearchProvider) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  SetUserSelectedDefaultSearchProvider(template_url_service);
  auto* search_engine_choice_service =
      static_cast<MockSearchEngineChoiceService*>(
          SearchEngineChoiceServiceFactory::GetForProfile(
              browser()->profile()));

  // Navigate to a URL to display the dialog.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  search_engine_choice_service->NotifyChoiceMade(
      /*prepopulate_id=*/0, EntryPoint::kDialog);
  const TemplateURL* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  EXPECT_EQ(default_search_provider->short_name(),
            base::UTF8ToUTF16(kCustomSearchEngineDomain));
}

IN_PROC_BROWSER_TEST_F(SearchEngineChoiceBrowserTest,
                       DialogDoesNotShowWithExtensionEnabledThatOverridesDSE) {
  Profile* profile = browser()->profile();
  auto* search_engine_choice_service =
      static_cast<MockSearchEngineChoiceService*>(
          SearchEngineChoiceServiceFactory::GetForProfile(profile));
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);

  std::unique_ptr<TemplateURLData> extension =
      GenerateDummyTemplateURLData("extension");
  template_url_service->ApplyDefaultSearchChangeForTesting(
      extension.get(), DefaultSearchManager::FROM_EXTENSION);

  // Navigate to a URL to display the dialog.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  EXPECT_FALSE(search_engine_choice_service->IsShowingDialog(browser()));

  CheckNavigationConditionRecorded(
      search_engines::SearchEngineChoiceScreenConditions::kExtensionControlled,
      1);
}

IN_PROC_BROWSER_TEST_F(SearchEngineChoiceBrowserTest,
                       DialogDoesNotShownForWebApp) {
  Profile* profile = browser()->profile();
  auto* service = static_cast<MockSearchEngineChoiceService*>(
      SearchEngineChoiceServiceFactory::GetForProfile(profile));

  const GURL start_url("https://app.site.test/example/index");
  const webapps::AppId app_id = InstallPWA(profile, start_url);

  // PWA browsers should not show the dialog.
  Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(profile, app_id);
  EXPECT_FALSE(service->IsShowingDialog(app_browser));

  // The same URL in the regular browser shows the dialog.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), start_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_TRUE(service->IsShowingDialog(browser()));
}

// TODO(crbug.com/1505043): Enable and fix test flakiness.
IN_PROC_BROWSER_TEST_F(SearchEngineChoiceBrowserTest,
                       DISABLED_DialogNotShownOverSpecificBrowserTypes) {
  Profile* profile = browser()->profile();
  auto* search_engine_choice_service =
      static_cast<MockSearchEngineChoiceService*>(
          SearchEngineChoiceServiceFactory::GetForProfile(profile));

  Browser* app_browser = Browser::Create(Browser::CreateParams::CreateForApp(
      "Test", false /* trusted_source */, gfx::Rect(), profile, true));
  chrome::AddTabAt(app_browser, GURL(), -1, true);
  EXPECT_TRUE(app_browser->is_type_app());

  NavigateParams params(app_browser, GURL("http://www.google.com/"),
                        ui::PAGE_TRANSITION_LINK);
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  Navigate(&params);
  // Navigate() should have opened a new `TYPE_APP_POPUP` window.
  Browser* app_popup_browser = params.browser;
  EXPECT_TRUE(app_popup_browser->is_type_app_popup());

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // Dialog shown over normal browser.
  EXPECT_TRUE(search_engine_choice_service->IsShowingDialog(browser()));
  // Dialog not shown over browser of type `TYPE_APP_POPUP`.
  EXPECT_FALSE(
      search_engine_choice_service->IsShowingDialog(app_popup_browser));
  // Dialog not shown over browser of type `TYPE_APP`
  EXPECT_FALSE(search_engine_choice_service->IsShowingDialog(app_browser));

  CheckNavigationConditionRecorded(
      search_engines::SearchEngineChoiceScreenConditions::
          kUnsupportedBrowserType,
      2);
}

IN_PROC_BROWSER_TEST_F(SearchEngineChoiceBrowserTest,
                       RecordingSearchEngineIsDoneAfterSettingDefault) {
  Profile* profile = browser()->profile();
  auto* service = static_cast<MockSearchEngineChoiceService*>(
      SearchEngineChoiceServiceFactory::GetForProfile(profile));
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);

  // Navigate to a URL to display the dialog.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabPageURL),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  EXPECT_TRUE(service->IsShowingDialog(browser()));

  const TemplateURL* default_search_engine =
      template_url_service->GetDefaultSearchProvider();
  const int default_search_engine_id = default_search_engine->prepopulate_id();
  const int kBingId = 3;

  EXPECT_NE(default_search_engine_id, kBingId);
  // Set the pref and simulate a dialog closing event.
  service->NotifyChoiceMade(kBingId, EntryPoint::kDialog);
  EXPECT_FALSE(service->IsShowingDialog(browser()));
  histogram_tester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_BING, 1);
}
