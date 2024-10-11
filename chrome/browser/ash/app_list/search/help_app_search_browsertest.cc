// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/webui/help_app_ui/help_app_manager.h"
#include "ash/webui/help_app_ui/help_app_manager_factory.h"
#include "ash/webui/help_app_ui/search/search.mojom.h"
#include "ash/webui/help_app_ui/search/search_handler.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/search/test/app_list_search_test_helper.h"
#include "chrome/browser/ash/app_list/search/test/search_results_changed_waiter.h"
#include "chrome/browser/ash/app_list/search/test/test_continue_files_search_provider.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/webapps/common/web_app_id.h"

namespace app_list::test {

class HelpAppSearchBrowserTestBase : public AppListSearchBrowserTest {
 public:
  HelpAppSearchBrowserTestBase() {
    // TODO: Remove parameterization on kProductivityLauncher.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{ash::features::kProductivityLauncher, {{"enable_continue", "true"}}},
         {{ash::features::kHelpAppLauncherSearch}, {}}},
        {});
  }

  ~HelpAppSearchBrowserTestBase() override = default;

  HelpAppSearchBrowserTestBase(const HelpAppSearchBrowserTestBase&) = delete;
  HelpAppSearchBrowserTestBase& operator=(const HelpAppSearchBrowserTestBase&) =
      delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    AppListSearchBrowserTest::SetUpOnMainThread();
    AppListClientImpl::GetInstance()->UpdateProfile();
    web_app::test::WaitUntilReady(
        web_app::WebAppProvider::GetForTest(browser()->profile()));
  }

  void ShowAppListAndWaitForHelpAppZeroStateResults() {
    ShowAppListAndWaitForZeroStateResults(
        {ash::AppListSearchResultType::kZeroStateHelpApp});
  }

  void ShowAppListAndWaitForZeroStateResults(
      const std::set<ash::AppListSearchResultType>& result_types) {
    SearchResultsChangedWaiter results_waiter(GetClient()->search_controller(),
                                              result_types);
    GetClient()->ShowAppList(ash::AppListShowSource::kSearchKey);
    ash::AppListTestApi().WaitForBubbleWindow(
        /*wait_for_opening_animation=*/false);
    results_waiter.Wait();
  }

  // Returns the first published continue section result.
  const ChromeSearchResult* FindLeadingContinueSectionResult() {
    for (const ChromeSearchResult* result : PublishedResults()) {
      if (result->display_type() == ash::SearchResultDisplayType::kContinue)
        return result;
    }
    return nullptr;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class HelpAppSearchBrowserTest : public HelpAppSearchBrowserTestBase {
 public:
  HelpAppSearchBrowserTest() = default;

  // HelpAppSearchBrowserTestBase:
  void SetUpOnMainThread() override {
    HelpAppSearchBrowserTestBase::SetUpOnMainThread();

    AppListClientImpl::GetInstance()->UpdateProfile();

    app_list::SearchController* search_controller =
        AppListClientImpl::GetInstance()->search_controller();

    // For release note chips to show in productivity launcher, continue section
    // needs to be visible, which will only be the case if other continue
    // results exist - create search provider to inject continue section
    // results.
    // Adds providers for both local and drive files to test that results
    // provided by help app are ranked before both types of files.
    auto local_continue_section_provider =
        std::make_unique<TestContinueFilesSearchProvider>(
            /*for_drive_files=*/false);
    local_continue_section_provider_ = local_continue_section_provider.get();
    EXPECT_EQ(1u, search_controller->ReplaceProvidersForResultTypeForTest(
                      local_continue_section_provider_->ResultType(),
                      std::move(local_continue_section_provider)));

    auto drive_continue_section_provider =
        std::make_unique<TestContinueFilesSearchProvider>(
            /*for_drive_files=*/true);
    drive_continue_section_provider_ = drive_continue_section_provider.get();
    EXPECT_EQ(1u, search_controller->ReplaceProvidersForResultTypeForTest(
                      drive_continue_section_provider_->ResultType(),
                      std::move(drive_continue_section_provider)));
  }

  raw_ptr<TestContinueFilesSearchProvider, DanglingUntriaged>
      local_continue_section_provider_ = nullptr;
  raw_ptr<TestContinueFilesSearchProvider, DanglingUntriaged>
      drive_continue_section_provider_ = nullptr;
};

// Test that Help App shows up as Release notes if pref shows we have some times
// left to show it.
IN_PROC_BROWSER_TEST_F(HelpAppSearchBrowserTest,
                       AppListSearchHasReleaseNotesSuggestionChip) {
  ash::SystemWebAppManager::GetForTest(GetProfile())
      ->InstallSystemAppsForTesting();
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);

  ShowAppListAndWaitForHelpAppZeroStateResults();

  auto* result = FindResult("help-app://updates");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->title(), l10n_util::GetStringUTF16(
                                 IDS_HELP_APP_WHATS_NEW_CONTINUE_TASK_TITLE));
  EXPECT_EQ(result->metrics_type(), ash::HELP_APP_UPDATES);
  EXPECT_EQ(result->display_type(), DisplayType::kContinue);
}

// Tests that Help App release notes chip is shown before other continue section
// items.
IN_PROC_BROWSER_TEST_F(HelpAppSearchBrowserTest, ReleaseNoteChipRankedFirst) {
  ash::SystemWebAppManager::GetForTest(GetProfile())
      ->InstallSystemAppsForTesting();
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);

  local_continue_section_provider_->set_count(5);
  drive_continue_section_provider_->set_count(5);

  ShowAppListAndWaitForHelpAppZeroStateResults();

  auto* result = FindResult("help-app://updates");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->display_type(), DisplayType::kContinue);
  EXPECT_EQ(FindLeadingContinueSectionResult(), result);
}

// Test that the number of times the suggestion chip should show decreases when
// the chip is shown.
IN_PROC_BROWSER_TEST_F(HelpAppSearchBrowserTest,
                       ReleaseNotesDecreasesTimesShownOnAppListOpen) {
  ash::SystemWebAppManager::GetForTest(GetProfile())
      ->InstallSystemAppsForTesting();
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);
  ash::AppListTestApi app_list_test_api;
  app_list_test_api.SetContinueSectionPrivacyNoticeAccepted();

  local_continue_section_provider_->set_count(5);
  drive_continue_section_provider_->set_count(5);

  ShowAppListAndWaitForHelpAppZeroStateResults();

  GetClient()->GetNotifier()->FireImpressionTimerForTesting(
      ash::AppListNotifier::Location::kContinue);

  const int times_left_to_show = GetProfile()->GetPrefs()->GetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow);
  EXPECT_EQ(times_left_to_show, 2);
}

// Test that the number of times the suggestion chip should show decreases when
// the chip is shown in tablet mode.
// https://crbug.com/1489431: test is flaky on bots.
IN_PROC_BROWSER_TEST_F(
    HelpAppSearchBrowserTest,
    DISABLED_ReleaseNotesDecreasesTimesShownOnAppListOpenInTabletMode) {
  ash::SystemWebAppManager::GetForTest(GetProfile())
      ->InstallSystemAppsForTesting();
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);
  ash::AppListTestApi app_list_test_api;
  app_list_test_api.SetContinueSectionPrivacyNoticeAccepted();

  local_continue_section_provider_->set_count(5);
  drive_continue_section_provider_->set_count(5);

  SearchResultsChangedWaiter results_waiter(
      GetClient()->search_controller(),
      {ash::AppListSearchResultType::kZeroStateHelpApp});
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  // Minimize the browser window to show home screen.
  browser()->window()->Minimize();
  ash::AppListTestApi().WaitForAppListShowAnimation(/*is_bubble_window=*/false);
  results_waiter.Wait();

  GetClient()->GetNotifier()->FireImpressionTimerForTesting(
      ash::AppListNotifier::Location::kContinue);

  const int times_left_to_show = GetProfile()->GetPrefs()->GetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow);
  EXPECT_EQ(times_left_to_show, 2);
}

// Test that clicking the Release Notes suggestion chip launches the Help app on
// the What's New page.
IN_PROC_BROWSER_TEST_F(HelpAppSearchBrowserTest,
                       ClickingReleaseNotesSuggestionChipLaunchesHelpApp) {
  ash::SystemWebAppManager::GetForTest(GetProfile())
      ->InstallSystemAppsForTesting();
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);

  ShowAppListAndWaitForHelpAppZeroStateResults();

  ChromeSearchResult* result = FindResult("help-app://updates");

  // Open the search result. This should open the help app at the expected url.
  size_t num_browsers = chrome::GetTotalBrowserCount();
  const GURL expected_url("chrome://help-app/updates");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();

  GetClient()->OpenSearchResult(
      GetClient()->GetModelUpdaterForTest()->model_id(), result->id(),
      /*event_flags=*/0, ash::AppListLaunchedFrom::kLaunchedFromContinueTask,
      ash::AppListLaunchType::kAppSearchResult, /*suggestion_index=*/0,
      /*launch_as_default=*/false);

  navigation_observer.Wait();

  EXPECT_EQ(num_browsers + 1, chrome::GetTotalBrowserCount());
  EXPECT_EQ(expected_url, chrome::FindLastActive()
                              ->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetVisibleURL());

  // Clicking on the chip should stop showing it in the future.
  const int times_left_to_show = GetProfile()->GetPrefs()->GetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow);
  EXPECT_EQ(times_left_to_show, 0);
}

// Test that the help app provider provides list search results.
IN_PROC_BROWSER_TEST_F(HelpAppSearchBrowserTest,
                       HelpAppProviderProvidesListResults) {
  // Show the app list and simulate search.
  AppListClientImpl::GetInstance()->ShowAppList(
      ash::AppListShowSource::kSearchKey);
  ash::AppListTestApi().WaitForBubbleWindow(
      /*wait_for_opening_animation=*/false);
  ash::AppListTestApi().SimulateSearch(u"Fix");

  // Need this because it sets up the icon.
  ash::SystemWebAppManager::GetForTest(GetProfile())
      ->InstallSystemAppsForTesting();
  // Add some searchable content to the help app search handler.
  std::vector<ash::help_app::mojom::SearchConceptPtr> search_concepts;
  auto search_concept = ash::help_app::mojom::SearchConcept::New(
      /*id=*/"6318213",
      /*title=*/u"Fix connection problems",
      /*main_category=*/u"Help",
      /*tags=*/std::vector<std::u16string>{u"verycomplicatedsearchquery"},
      /*tag_locale=*/"en",
      /*url_path_with_parameters=*/"help/id/test",
      /*locale=*/"");
  search_concepts.push_back(std::move(search_concept));

  base::RunLoop run_loop;
  ash::help_app::HelpAppManagerFactory::GetForBrowserContext(GetProfile())
      ->search_handler()
      ->Update(std::move(search_concepts), base::BindLambdaForTesting([&]() {
                 run_loop.QuitClosure().Run();
               }));
  // Wait until the update is complete.
  run_loop.Run();

  ChromeSearchResult* result = nullptr;
  while (!result) {
    // Search repeatedly until the desired result is found. Multiple searches
    // are needed because it takes time for the icon to load.
    SearchAndWaitForProviders("verycomplicatedsearchquery",
                              {ResultType::kHelpApp});

    result = FindResult("chrome://help-app/help/id/test");
  }

  EXPECT_EQ(base::UTF16ToASCII(result->title()), "Fix connection problems");
  EXPECT_EQ(base::UTF16ToASCII(result->details()), "Help");
  EXPECT_EQ(result->display_type(), DisplayType::kList);

  // Open the search result. This should open the help app at the expected url
  // and log a metric indicating what content was launched.
  const size_t num_browsers = chrome::GetTotalBrowserCount();
  const GURL expected_url("chrome://help-app/help/id/test");
  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();
  base::HistogramTester histogram_tester;

  GetClient()->OpenSearchResult(
      GetClient()->GetModelUpdaterForTest()->model_id(), result->id(),
      /*event_flags=*/0, ash::AppListLaunchedFrom::kLaunchedFromSearchBox,
      ash::AppListLaunchType::kAppSearchResult, /*suggestion_index=*/0,
      /*launch_as_default=*/false);
  navigation_observer.Wait();

  EXPECT_EQ(num_browsers + 1, chrome::GetTotalBrowserCount());
  EXPECT_EQ(expected_url, chrome::FindLastActive()
                              ->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetVisibleURL());
  // -20424143 is the hash of the content id. This hash value can be found in
  // the enum in the google-internal histogram file.
  histogram_tester.ExpectUniqueSample("Discover.LauncherSearch.ContentLaunched",
                                      -20424143, 1);
}

using HelpAppSwaSearchBrowserTest = HelpAppSearchBrowserTestBase;

// Test that Help App shows up normally even when suggestion chip should show.
IN_PROC_BROWSER_TEST_F(HelpAppSwaSearchBrowserTest, AppListSearchHasApp) {
  ash::SystemWebAppManager::GetForTest(GetProfile())
      ->InstallSystemAppsForTesting();
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);

  ShowAppListAndWaitForZeroStateResults(
      {ash::AppListSearchResultType::kZeroStateHelpApp,
       ash::AppListSearchResultType::kZeroStateApp});

  auto* result = FindResult(web_app::kHelpAppId);
  ASSERT_TRUE(result);
  // Has regular app name as title.
  EXPECT_EQ(base::UTF16ToASCII(result->title()), "Explore");
}

IN_PROC_BROWSER_TEST_F(HelpAppSwaSearchBrowserTest, Launch) {
  Profile* profile = browser()->profile();
  ash::SystemWebAppManager::GetForTest(profile)->InstallSystemAppsForTesting();
  const webapps::AppId app_id = web_app::kHelpAppId;

  ShowAppListAndWaitForZeroStateResults(
      {ash::AppListSearchResultType::kZeroStateHelpApp,
       ash::AppListSearchResultType::kZeroStateApp});

  auto* result = FindResult(web_app::kHelpAppId);
  ASSERT_TRUE(result);
  result->Open(ui::EF_NONE);
}

}  // namespace app_list::test
