// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page_platform_test_helper.h"
#include "chrome/browser/ui/android/hats/hats_service_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/test/base/android/android_ui_test_utils.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

const char kEmptyPage[] = "/empty.html";

class MockHatsService : public HatsServiceAndroid {
 public:
  explicit MockHatsService(Profile* profile) : HatsServiceAndroid(profile) {}
  ~MockHatsService() override = default;

  MOCK_METHOD(HatsService::LaunchError,
              LaunchSurveyForWebContents,
              (const std::string& trigger,
               content::WebContents* web_contents,
               const SurveyBitsData& product_specific_bits_data,
               const SurveyStringData& product_specific_string_data,
               base::OnceClosure success_callback,
               base::OnceClosure failure_callback,
               const std::optional<std::string>& supplied_trigger_id,
               const SurveyOptions& survey_options),
              (override));
};

std::unique_ptr<KeyedService> BuildMockHatsService(
    content::BrowserContext* context) {
  return std::make_unique<MockHatsService>(
      Profile::FromBrowserContext(context));
}

}  // namespace

class SafeBrowsingBlockingPageHatsSurveyAndroidTest
    : public SafeBrowsingBlockingPageRealTimeUrlCheckTest,
      public testing::WithParamInterface<bool> {
 public:
  SafeBrowsingBlockingPageHatsSurveyAndroidTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {kRedWarningSurveyAndroid, kExtendedReportingRemovePrefDependency},
        /*disabled_features=*/{kDelayedWarnings});
    SafeBrowsingBlockingPagePlatformBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SafeBrowsingBlockingPageRealTimeUrlCheckTest::SetUpCommandLine(
        command_line);
    if (IsSiteIsolationEnabled()) {
      content::IsolateAllSitesForTesting(command_line);
    }
  }

  bool IsSiteIsolationEnabled() const { return GetParam(); }

 protected:
  MockHatsService* SetUpMockHatsService() {
    return static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockHatsService)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    SafeBrowsingBlockingPageHatsSurveyAndroidTestWithIsolationSetting,
    SafeBrowsingBlockingPageHatsSurveyAndroidTest,
    testing::Bool());

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageHatsSurveyAndroidTest,
                       ReportAttachedForHatsOnTabClose_StandardReporting) {
  MockHatsService* mock_hats_service = SetUpMockHatsService();

  GetUiManager()->SetExpectEmptyReportForHats(false);
  GetUiManager()->SetExpectReportUrlForHats(true);
  GetUiManager()->SetExpectInterstitialInteractions(1);

  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  SetUpUnsafeUrl(url);
  EnableExtendedReporting(false);

  content::WebContents* main_tab = web_contents();

  EXPECT_CALL(*GetUiManager(),
              OnAttachThreatDetailsAndLaunchSurvey(/*is_tab_closed=*/true))
      .Times(1);

  base::RunLoop survey_run_loop;
  EXPECT_CALL(
      *mock_hats_service,
      LaunchSurveyForWebContents(kHatsSurveyTriggerRedWarningAndroid, main_tab,
                                 /*product_specific_bits_data=*/testing::_,
                                 /*product_specific_string_data=*/
                                 testing::Contains(testing::Pair(
                                     safe_browsing::kUserAction, "CLOSE_TAB")),
                                 /*success_callback=*/testing::_,
                                 /*failure_callback=*/testing::_,
                                 /*supplied_trigger_id=*/testing::_,
                                 /*survey_options=*/testing::_))
      .WillOnce(testing::DoAll(
          base::test::RunOnceClosure(survey_run_loop.QuitClosure()),
          testing::Return(HatsService::LaunchError::kNone)));

  // Open the unsafe URL in a new tab so that when it is closed, the original
  // main_tab remains active for the survey.
  android_ui_test_utils::OpenUrlInNewTab(profile(), main_tab, url);
  content::WebContents* interstitial_tab =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_NE(main_tab, interstitial_tab);
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingInterstitial(interstitial_tab));

  // Close the interstitial tab and wait for destruction.
  content::WebContentsDestroyedWatcher destroyed_watcher(interstitial_tab);
  interstitial_tab->Close();
  destroyed_watcher.Wait();

  // In browser tests, closing WebContents directly bypasses the Android
  // TabModel UI touch pipeline that automatically updates active tab index.
  // Explicitly ensure main_tab is selected in the TabModel as the survey
  // anchor.
  TabModel* tab_model = TabModelList::GetTabModelForWebContents(main_tab);
  if (tab_model && tab_model->GetActiveWebContents() != main_tab) {
    tab_model->SetActiveIndex(0);
  }

  // Wait for the posted survey launch task to execute on the UI thread.
  survey_run_loop.Run();

  std::string report = GetUiManager()->GetReport();
  EXPECT_TRUE(report.empty());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageHatsSurveyAndroidTest,
                       ReportAttachedForHatsOnTabClose_ExtendedReporting) {
  MockHatsService* mock_hats_service = SetUpMockHatsService();

  GetUiManager()->SetExpectEmptyReportForHats(false);
  GetUiManager()->SetExpectReportUrlForHats(true);
  GetUiManager()->SetExpectInterstitialInteractions(1);

  base::RunLoop report_sent_run_loop;
  SetReportSentCallback(report_sent_run_loop.QuitClosure());

  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  SetUpUnsafeUrl(url);
  EnableExtendedReporting(true);

  content::WebContents* main_tab = web_contents();

  EXPECT_CALL(*GetUiManager(),
              OnAttachThreatDetailsAndLaunchSurvey(/*is_tab_closed=*/true))
      .Times(1);

  base::RunLoop survey_run_loop;
  EXPECT_CALL(
      *mock_hats_service,
      LaunchSurveyForWebContents(kHatsSurveyTriggerRedWarningAndroid, main_tab,
                                 /*product_specific_bits_data=*/testing::_,
                                 /*product_specific_string_data=*/
                                 testing::Contains(testing::Pair(
                                     safe_browsing::kUserAction, "CLOSE_TAB")),
                                 /*success_callback=*/testing::_,
                                 /*failure_callback=*/testing::_,
                                 /*supplied_trigger_id=*/testing::_,
                                 /*survey_options=*/testing::_))
      .WillOnce(testing::DoAll(
          base::test::RunOnceClosure(survey_run_loop.QuitClosure()),
          testing::Return(HatsService::LaunchError::kNone)));

  // Open the unsafe URL in a new tab so that when it is closed, the original
  // main_tab remains active for the survey.
  android_ui_test_utils::OpenUrlInNewTab(profile(), main_tab, url);
  content::WebContents* interstitial_tab =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_NE(main_tab, interstitial_tab);
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingInterstitial(interstitial_tab));

  // Close the interstitial tab and wait for destruction.
  content::WebContentsDestroyedWatcher destroyed_watcher(interstitial_tab);
  interstitial_tab->Close();
  destroyed_watcher.Wait();

  // In browser tests, closing WebContents directly bypasses the Android
  // TabModel UI touch pipeline that automatically updates active tab index.
  // Explicitly ensure main_tab is selected in the TabModel as the survey
  // anchor.
  TabModel* tab_model = TabModelList::GetTabModelForWebContents(main_tab);
  if (tab_model && tab_model->GetActiveWebContents() != main_tab) {
    tab_model->SetActiveIndex(0);
  }

  report_sent_run_loop.Run();
  // Wait for the posted survey launch task to execute on the UI thread.
  survey_run_loop.Run();

  std::string report = GetUiManager()->GetReport();
  EXPECT_FALSE(report.empty());
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageHatsSurveyAndroidTest,
                       NoHatsSurveyOnTabClose_WhenSafeBrowsingSurveysDisabled) {
  MockHatsService* mock_hats_service = SetUpMockHatsService();

  profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingSurveysEnabled, false);

  GetUiManager()->SetExpectEmptyReportForHats(true);

  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  SetUpUnsafeUrl(url);
  EnableExtendedReporting(false);

  content::WebContents* main_tab = web_contents();

  EXPECT_CALL(*GetUiManager(), OnAttachThreatDetailsAndLaunchSurvey).Times(0);
  EXPECT_CALL(*mock_hats_service, LaunchSurveyForWebContents).Times(0);

  // Open the unsafe URL in a new tab so that when it is closed, the original
  // main_tab remains active.
  android_ui_test_utils::OpenUrlInNewTab(profile(), main_tab, url);
  content::WebContents* interstitial_tab =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_NE(main_tab, interstitial_tab);
  ASSERT_TRUE(
      chrome_browser_interstitials::IsShowingInterstitial(interstitial_tab));

  // Close the interstitial tab and wait for destruction.
  content::WebContentsDestroyedWatcher destroyed_watcher(interstitial_tab);
  interstitial_tab->Close();
  destroyed_watcher.Wait();

  // Post a task to UI thread and wait for it to guarantee any previously posted
  // tasks have run.
  base::RunLoop run_loop;
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                               run_loop.QuitClosure());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(SafeBrowsingBlockingPageHatsSurveyAndroidTest,
                       NoHatsSurveyOnTabClose_WhenLastTabClosed) {
  MockHatsService* mock_hats_service = SetUpMockHatsService();

  GetUiManager()->SetExpectEmptyReportForHats(false);
  GetUiManager()->SetExpectReportUrlForHats(true);
  GetUiManager()->SetExpectInterstitialInteractions(1);

  GURL url = embedded_test_server()->GetURL(kEmptyPage);
  SetUpUnsafeUrl(url);
  EnableExtendedReporting(false);

  content::WebContents* main_tab = web_contents();

  // When the only open tab is closed, no active WebContents remains in the
  // TabModel to anchor the survey on. Verify the survey is cleanly dropped
  // without crashing or hanging.
  EXPECT_CALL(*mock_hats_service, LaunchSurveyForWebContents).Times(0);

  NavigateToURL(url, /*expect_success=*/false);
  ASSERT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(main_tab));

  // Close the only tab in the browser.
  content::WebContentsDestroyedWatcher destroyed_watcher(main_tab);
  main_tab->Close();
  destroyed_watcher.Wait();

  // Flush the UI thread to ensure the posted survey task executes and
  // returns early without launching a survey.
  base::RunLoop run_loop;
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                               run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace safe_browsing
