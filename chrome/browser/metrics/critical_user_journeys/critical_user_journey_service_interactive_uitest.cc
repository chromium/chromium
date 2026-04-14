// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browsing_data/browsing_data_important_sites_util.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_registry.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_service.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_service_factory.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_session.h"
#include "chrome/browser/metrics/critical_user_journeys/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/download_test_observer.h"
#include "ui/actions/actions.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_sequence.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/download/download_display.h"
#include "chrome/browser/ui/views/download/bubble/download_toolbar_ui_controller.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)

namespace metrics {

namespace {
BASE_FEATURE(kAppMenuJourney, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kBranchingJourney, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAnyOfStartJourney, base::FEATURE_ENABLED_BY_DEFAULT);

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kClearBrowsingDataPage);

const std::string GetMetricJourneyPrefix(const base::Feature& feature) {
  return base::StrCat({"CriticalUserJourney.", feature.name});
}

}  // namespace

class TestCriticalUserJourneyService : public CriticalUserJourneyService {
 public:
  explicit TestCriticalUserJourneyService(Profile* profile)
      : CriticalUserJourneyService(profile) {}

 protected:
  void RegisterJourneys(CriticalUserJourneyRegistry* registry) override {
    // Simple Journey: Click App Menu button (triggers start), then click New
    // Tab button (triggers end).
    HatsParams params;
    params.trigger = "TestHatsTrigger";
    registry->AddJourney(
        CriticalUserJourney::Builder(&kAppMenuJourney)
            .AddStep(kToolbarAppMenuButtonElementId,
                     ui::InteractionSequence::StepType::kActivated, 1)
            .AddStep(kNewTabButtonElementId,
                     ui::InteractionSequence::StepType::kActivated, 2)
            .LaunchHatsSurveyOnCompletion(params)
            .Build());

    // Branching Journey: Click App Menu button (triggers start), then click
    // New Tab button (branch 1) or click the toolbar forward button.
    registry->AddJourney(
        CriticalUserJourney::Builder(&kBranchingJourney)
            .AddStep(kToolbarAppMenuButtonElementId,
                     ui::InteractionSequence::StepType::kActivated, 1)
            .AddAnyOf({
                Branch(kNewTabButtonElementId,
                       ui::InteractionSequence::StepType::kActivated, 2),
                Branch(kReloadButtonElementId,
                       ui::InteractionSequence::StepType::kActivated, 3),
            })
            .Build());

    // AnyOf Start Journey: Click New Tab button or Avatar button (triggers
    // start), then click the App Menu Button.
    registry->AddJourney(
        CriticalUserJourney::Builder(&kAnyOfStartJourney)
            .AddAnyOf({
                Branch(kNewTabButtonElementId,
                       ui::InteractionSequence::StepType::kActivated, 1),
                Branch(kReloadButtonElementId,
                       ui::InteractionSequence::StepType::kActivated, 2),
            })
            .AddStep(kToolbarAppMenuButtonElementId,
                     ui::InteractionSequence::StepType::kActivated, 3)
            .Build());
  }
};

class CriticalUserJourneyServiceInteractiveTest
    : public InteractiveBrowserTest {
 public:
  CriticalUserJourneyServiceInteractiveTest() {
    feature_list_.InitAndEnableFeature(kCriticalUserJourneyService);
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&CriticalUserJourneyServiceInteractiveTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    HatsServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildMockHatsService));
    CriticalUserJourneyServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          auto service = std::make_unique<TestCriticalUserJourneyService>(
              Profile::FromBrowserContext(context));
          service->Initialize();
          return service;
        }));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(CriticalUserJourneyServiceInteractiveTest,
                       ClickAppMenuThenNewTabCompletesJourney) {
  base::HistogramTester histograms;

  const std::string step_reached =
      base::StrCat({GetMetricJourneyPrefix(kAppMenuJourney), ".StepReached"});
  const std::string result =
      base::StrCat({GetMetricJourneyPrefix(kAppMenuJourney), ".Result"});

  auto* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetForProfile(browser()->profile(), true));
  EXPECT_CALL(*mock_hats_service,
              LaunchSurvey("TestHatsTrigger", testing::_, testing::_,
                           testing::_, testing::_, testing::_, testing::_))
      .Times(1);

  RunTestSequence(
      // Step 1: Click App Menu.
      PressButton(kToolbarAppMenuButtonElementId),

      // Step 2: Click New Tab button.
      PressButton(kNewTabButtonElementId));

  // Verification: The journey should complete asynchronously.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return histograms.GetBucketCount(
               result, CriticalUserJourneySession::JourneyResult::kCompleted) >
           0;
  }));

  histograms.ExpectBucketCount(step_reached, 1, 1);
  histograms.ExpectBucketCount(step_reached, 2, 1);
  histograms.ExpectUniqueSample(
      result, CriticalUserJourneySession::JourneyResult::kCompleted, 1);
}

IN_PROC_BROWSER_TEST_F(CriticalUserJourneyServiceInteractiveTest,
                       DuplicateJourneyPreemptsOldOne) {
  base::HistogramTester histograms;

  const std::string step_reached =
      base::StrCat({GetMetricJourneyPrefix(kAppMenuJourney), ".StepReached"});
  const std::string result =
      base::StrCat({GetMetricJourneyPrefix(kAppMenuJourney), ".Result"});

  RunTestSequence(
      // 1. Click App Menu (triggers first session).
      PressButton(kToolbarAppMenuButtonElementId),

      // 2. Click App Menu again (should preempt first session).
      PressButton(kToolbarAppMenuButtonElementId),

      // 3. Click New Tab button (completes the second session).
      PressButton(kNewTabButtonElementId));

  // Verification: The journey should complete asynchronously.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return histograms.GetBucketCount(
               result, CriticalUserJourneySession::JourneyResult::kCompleted) >
           0;
  }));

  // Both sessions reached step 1.
  histograms.ExpectBucketCount(step_reached, 1, 2);
  // Only the second session reached step 2.
  histograms.ExpectBucketCount(step_reached, 2, 1);

  // Only one session (the second one) completed.
  histograms.ExpectUniqueSample(
      result, CriticalUserJourneySession::JourneyResult::kCompleted, 1);
}

IN_PROC_BROWSER_TEST_F(CriticalUserJourneyServiceInteractiveTest,
                       BranchingJourneyCompletion) {
  base::HistogramTester histograms;

  const std::string step_reached =
      base::StrCat({GetMetricJourneyPrefix(kBranchingJourney), ".StepReached"});
  const std::string result =
      base::StrCat({GetMetricJourneyPrefix(kBranchingJourney), ".Result"});

  RunTestSequence(
      // Step 1: Click App Menu (triggers start).
      PressButton(kToolbarAppMenuButtonElementId),

      // Step 2: Click the New Tab button (the first branch).
      PressButton(kNewTabButtonElementId));

  // Verification
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return histograms.GetBucketCount(
               result, CriticalUserJourneySession::JourneyResult::kCompleted) >
           0;
  }));

  histograms.ExpectBucketCount(step_reached, 1, 1);
  histograms.ExpectBucketCount(step_reached, 2, 1);
  histograms.ExpectUniqueSample(
      result, CriticalUserJourneySession::JourneyResult::kCompleted, 1);
}

IN_PROC_BROWSER_TEST_F(CriticalUserJourneyServiceInteractiveTest,
                       AnyOfStartJourneyFirstBranch) {
  base::HistogramTester histograms;

  const std::string step_reached = base::StrCat(
      {GetMetricJourneyPrefix(kAnyOfStartJourney), ".StepReached"});
  const std::string result =
      base::StrCat({GetMetricJourneyPrefix(kAnyOfStartJourney), ".Result"});

  RunTestSequence(
      // Step 1: Click New Tab Button (triggers start).
      PressButton(kNewTabButtonElementId),

      // Step 2: Click the App Menu Button.
      PressButton(kToolbarAppMenuButtonElementId));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return histograms.GetBucketCount(
               result, CriticalUserJourneySession::JourneyResult::kCompleted) >
           0;
  }));

  histograms.ExpectBucketCount(step_reached, 1, 1);
  histograms.ExpectBucketCount(step_reached, 3, 1);
  histograms.ExpectUniqueSample(
      result, CriticalUserJourneySession::JourneyResult::kCompleted, 1);
}

IN_PROC_BROWSER_TEST_F(CriticalUserJourneyServiceInteractiveTest,
                       AnyOfStartJourneySecondBranch) {
  base::HistogramTester histograms;

  const std::string step_reached = base::StrCat(
      {GetMetricJourneyPrefix(kAnyOfStartJourney), ".StepReached"});
  const std::string result =
      base::StrCat({GetMetricJourneyPrefix(kAnyOfStartJourney), ".Result"});

  RunTestSequence(
      // Step 1: Click the Avatar Button (triggers start).
      PressButton(kReloadButtonElementId),

      // Step 2: Click the App Menu Button.
      PressButton(kToolbarAppMenuButtonElementId));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return histograms.GetBucketCount(
               result, CriticalUserJourneySession::JourneyResult::kCompleted) >
           0;
  }));

  histograms.ExpectBucketCount(step_reached, 2, 1);
  histograms.ExpectBucketCount(step_reached, 3, 1);
  histograms.ExpectUniqueSample(
      result, CriticalUserJourneySession::JourneyResult::kCompleted, 1);
}

class RealCriticalUserJourneyServiceInteractiveTest
    : public WebUiInteractiveTestMixin<InteractiveBrowserTest> {
 private:
  base::test::ScopedFeatureList feature_list_;

 public:
  RealCriticalUserJourneyServiceInteractiveTest() {
    feature_list_.InitWithFeatures(
        {kCriticalUserJourneyService, kViewDownloadedFileJourney,
         kViewDownloadedFileFromAppMenuJourney, kClearBrowsingHistoryJourney},
        {});
  }
  ~RealCriticalUserJourneyServiceInteractiveTest() override = default;

  void SetUp() override { InteractiveBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                                 false);
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // The Download Bubble UI is not used on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
  // A helper function to trigger a real download and wait for it to complete.
  auto DownloadTestFile() {
    return Steps(Do([this]() {
      GURL url = embedded_test_server()->GetURL("/downloads/a_zip_file.zip");
      content::DownloadManager* manager =
          browser()->profile()->GetDownloadManager();
      std::unique_ptr<content::DownloadTestObserver> observer(
          new content::DownloadTestObserverTerminal(
              manager, 1,
              content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
      ui_test_utils::NavigateToURLWithDisposition(
          browser(), url, WindowOpenDisposition::CURRENT_TAB,
          ui_test_utils::BROWSER_TEST_NO_WAIT);
      observer->WaitForFinished();
    }));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)
};

#if !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(RealCriticalUserJourneyServiceInteractiveTest,
                       DownloadJourneyCompletion) {
  base::HistogramTester histograms;

  const std::string step_reached = base::StrCat(
      {GetMetricJourneyPrefix(kViewDownloadedFileJourney), ".StepReached"});
  const std::string result = base::StrCat(
      {GetMetricJourneyPrefix(kViewDownloadedFileJourney), ".Result"});

  RunTestSequence(
      // Trigger a real download and wait for it to complete.
      DownloadTestFile(),

      // The download is now complete. Forcibly open the download bubble to
      // satisfy Step 2 of the journey (the bubble must be shown).
      Do([this]() {
        if (auto* display =
                browser()->GetFeatures().download_toolbar_ui_controller()) {
          display->ShowDetails();
        }
      }),

      // The Download Bubble UI is drawn in a separate widget and thus has a
      // different ElementContext. Use InAnyContext to find the row.
      InAnyContext(Steps(WaitForShow(kDownloadBubbleRowElementId),
                         PressButton(kDownloadBubbleOpenButtonId))),

      // Hide the bubble before teardown to allow the test to cleanup properly.
      Do([this]() {
        if (auto* display =
                browser()->GetFeatures().download_toolbar_ui_controller()) {
          display->HideDetails();
        }
      }));

  // Verification
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return histograms.GetBucketCount(
               result, CriticalUserJourneySession::JourneyResult::kCompleted) >
           0;
  }));

  histograms.ExpectBucketCount(step_reached, 1, 1);  // Download Ended
  histograms.ExpectBucketCount(step_reached, 2, 1);  // Bubble Shown
  histograms.ExpectBucketCount(step_reached, 4, 1);  // Activated
  histograms.ExpectUniqueSample(
      result, CriticalUserJourneySession::JourneyResult::kCompleted, 1);
}

IN_PROC_BROWSER_TEST_F(RealCriticalUserJourneyServiceInteractiveTest,
                       ViewDownloadedFileFromAppMenuCompletion) {
  base::HistogramTester histograms;

  const std::string step_reached = base::StrCat(
      {GetMetricJourneyPrefix(kViewDownloadedFileFromAppMenuJourney),
       ".StepReached"});
  const std::string result = base::StrCat(
      {GetMetricJourneyPrefix(kViewDownloadedFileFromAppMenuJourney),
       ".Result"});

  RunTestSequence(
      // Step 1: Trigger a real download and wait for it to complete.
      DownloadTestFile(),

      // Step 2: Open App Menu
      PressButton(kToolbarAppMenuButtonElementId),

      // Step 3: Click Downloads Menu Item
      SelectMenuItem(AppMenuModel::kDownloadsMenuItem),

      // Step 4: Trigger the custom event for opening the downloaded file.
      Do([this]() {
        auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
        auto* element =
            ui::ElementTracker::GetElementTracker()->GetUniqueElement(
                kToolbarAppMenuButtonElementId,
                browser_view->GetElementContext());
        if (element) {
          ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
              element, kDownloadedFileOpenedCustomEventId);
        }
      }),
      // Ensure the menu closes before tearing down the test fixture. This is an
      // asynchronous process on MacOS.
      WaitForHide(AppMenuModel::kDownloadsMenuItem));

  // Verification
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return histograms.GetBucketCount(
               result, CriticalUserJourneySession::JourneyResult::kCompleted) >
           0;
  }));

  histograms.ExpectBucketCount(step_reached, 1, 1);  // Download Ended
  histograms.ExpectBucketCount(step_reached, 2, 1);  // App Menu Clicked
  histograms.ExpectBucketCount(step_reached, 3,
                               1);  // Downloads Menu Item Clicked
  histograms.ExpectBucketCount(step_reached, 4, 1);  // Downloaded File Opened
  histograms.ExpectUniqueSample(
      result, CriticalUserJourneySession::JourneyResult::kCompleted, 1);
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(RealCriticalUserJourneyServiceInteractiveTest,
                       ClearBrowsingHistoryViaAppMenu) {
  base::HistogramTester histograms;

  const std::string step_reached = base::StrCat(
      {GetMetricJourneyPrefix(kClearBrowsingHistoryJourney), ".StepReached"});
  const std::string result = base::StrCat(
      {GetMetricJourneyPrefix(kClearBrowsingHistoryJourney), ".Result"});

  const WebContentsInteractionTestUtil::DeepQuery kDeleteBrowsingHistoryButton =
      {"settings-ui",
       "settings-main",
       "settings-privacy-page-index",
       "settings-privacy-page",
       "settings-clear-browsing-data-dialog-v2",
       "#deleteButton"};

  RunTestSequence(
      InstrumentTab(kFirstTabElementId),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kClearBrowsingDataMenuItem),
      WaitForHide(AppMenuModel::kClearBrowsingDataMenuItem),
      WaitForWebContentsReady(
          kFirstTabElementId,
          chrome::GetSettingsUrl(chrome::kClearBrowserDataSubPage)),
      InstrumentTab(kClearBrowsingDataPage),
      ClickElement(kClearBrowsingDataPage, kDeleteBrowsingHistoryButton),
      WaitForEvent(kBrowserViewElementId,
                   browsing_data_important_sites_util::
                       kClearBrowsingDataHistoryEventId));

  histograms.ExpectBucketCount(step_reached, 2, 1);  // App Menu Clicked
  histograms.ExpectBucketCount(step_reached, 3,
                               1);  // Clear browsing dialog shown
  histograms.ExpectBucketCount(step_reached, 4,
                               1);  // Clear browsing data history event
  histograms.ExpectUniqueSample(
      result, CriticalUserJourneySession::JourneyResult::kCompleted, 1);
}

IN_PROC_BROWSER_TEST_F(RealCriticalUserJourneyServiceInteractiveTest,
                       ClearBrowsingHistoryViaKeyboardShortcut) {
  base::HistogramTester histograms;

  const std::string step_reached = base::StrCat(
      {GetMetricJourneyPrefix(kClearBrowsingHistoryJourney), ".StepReached"});
  const std::string result = base::StrCat(
      {GetMetricJourneyPrefix(kClearBrowsingHistoryJourney), ".Result"});

  const WebContentsInteractionTestUtil::DeepQuery kDeleteBrowsingHistoryButton =
      {"settings-ui",
       "settings-main",
       "settings-privacy-page-index",
       "settings-privacy-page",
       "settings-clear-browsing-data-dialog-v2",
       "#deleteButton"};

  ui::Accelerator clear_browsing_data_accelerator;
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(browser_view->GetAccelerator(IDC_CLEAR_BROWSING_DATA,
                                           &clear_browsing_data_accelerator));

  RunTestSequence(
      InstrumentTab(kFirstTabElementId),
      SendAccelerator(kBrowserViewElementId, clear_browsing_data_accelerator),
      WaitForWebContentsReady(
          kFirstTabElementId,
          chrome::GetSettingsUrl(chrome::kClearBrowserDataSubPage)),
      InstrumentTab(kClearBrowsingDataPage),
      ClickElement(kClearBrowsingDataPage, kDeleteBrowsingHistoryButton),
      WaitForEvent(kBrowserViewElementId,
                   browsing_data_important_sites_util::
                       kClearBrowsingDataHistoryEventId));

  histograms.ExpectBucketCount(step_reached, 1, 1);  // App Menu Clicked
  histograms.ExpectBucketCount(step_reached, 3,
                               1);  // Clear browsing dialog shown
  histograms.ExpectBucketCount(step_reached, 4,
                               1);  // Clear browsing data history event
  histograms.ExpectUniqueSample(
      result, CriticalUserJourneySession::JourneyResult::kCompleted, 1);
}

}  // namespace metrics
