// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_service.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_registry.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_service_factory.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_session.h"
#include "chrome/browser/metrics/critical_user_journeys/features.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/webui/settings/settings_element_ids.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"

namespace metrics {

namespace {
BASE_FEATURE(kTestJourney, base::FEATURE_ENABLED_BY_DEFAULT);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementId1);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementId2);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestElementId3);
constexpr ui::ElementContext kTestContext =
    ui::ElementContext::CreateFakeContextForTesting(1);
}  // namespace

class TestCriticalUserJourneyService : public CriticalUserJourneyService {
 public:
  explicit TestCriticalUserJourneyService(Profile* profile)
      : CriticalUserJourneyService(profile) {}

 protected:
  void RegisterJourneys(CriticalUserJourneyRegistry* registry) override {
    HatsParams params;
    params.trigger = "TestHatsTrigger";
    registry->AddJourney(
        CriticalUserJourney::Builder(&kTestJourney)
            .AddStep(kTestElementId1, ui::InteractionSequence::StepType::kShown,
                     1)
            .AddAnyOf({
                Branch(kTestElementId2,
                       ui::InteractionSequence::StepType::kShown, 2),
                Branch(kTestElementId3,
                       ui::InteractionSequence::StepType::kHidden, 3)
                    .SetExitBranch(),
            })
            .LaunchHatsSurveyOnCompletion(params)
            .Build());
  }
};

class CriticalUserJourneyServiceTest : public testing::Test {
 public:
  CriticalUserJourneyServiceTest() {
    feature_list_.InitAndEnableFeature(kCriticalUserJourneyService);
  }
  ~CriticalUserJourneyServiceTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    HatsServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildMockHatsService));

    service_ = std::make_unique<TestCriticalUserJourneyService>(&profile_);
    service_->Initialize();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<TestCriticalUserJourneyService> service_;
};

TEST_F(CriticalUserJourneyServiceTest, HaTSSurveyLogging) {
  base::HistogramTester histograms;

  auto* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetForProfile(&profile_, true));

  base::RunLoop run_loop;
  SurveyStringData captured_psd;
  EXPECT_CALL(*mock_hats_service,
              LaunchSurvey("TestHatsTrigger", testing::_, testing::_,
                           testing::_, testing::_, testing::_, testing::_))
      .WillOnce([&run_loop, &captured_psd](
                    const std::string& trigger,
                    base::OnceClosure success_callback,
                    base::OnceClosure failure_callback,
                    const SurveyBitsData& product_specific_bits_data,
                    const SurveyStringData& product_specific_string_data,
                    const std::optional<std::string>& supplied_trigger_id,
                    const HatsService::SurveyOptions& survey_options) {
        captured_psd = product_specific_string_data;
        std::move(success_callback).Run();
        run_loop.Quit();
        return HatsService::LaunchError::kNone;
      });

  // Trigger the journey steps.
  // Show element 1
  ui::test::TestElement el1(kTestElementId1, kTestContext);
  el1.Show();

  // Show element 2 (triggers completion via primary branch)
  ui::test::TestElement el2(kTestElementId2, kTestContext);
  el2.Show();

  run_loop.Run();

  EXPECT_EQ(captured_psd[kCujJourneyOutcomeKey], kCujJourneyOutcomeCompleted);
  EXPECT_EQ(captured_psd[kCujJourneyNameKey], "TestJourney");
  EXPECT_EQ(captured_psd[kCujEntryStepKey], kCujDefaultEntryStep);
  EXPECT_FALSE(captured_psd[kCujJourneyDurationKey].empty());
  EXPECT_EQ(captured_psd.size(), 4u);

  // Verify histograms.
  const std::string hats_event_histogram =
      "CriticalUserJourney.TestJourney.HaTSSurveyEvent";

  histograms.ExpectBucketCount(
      hats_event_histogram,
      static_cast<int>(
          CriticalUserJourneyService::CriticalUserJourneyHaTSEvent::kTriggered),
      1);
  histograms.ExpectBucketCount(
      hats_event_histogram,
      static_cast<int>(
          CriticalUserJourneyService::CriticalUserJourneyHaTSEvent::kShown),
      1);
  histograms.ExpectBucketCount(
      hats_event_histogram,
      static_cast<int>(
          CriticalUserJourneyService::CriticalUserJourneyHaTSEvent::kFailed),
      0);
}

TEST_F(CriticalUserJourneyServiceTest, HaTSSurveyAbandonedExitBranchPSD) {
  base::HistogramTester histograms;
  auto* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetForProfile(&profile_, true));

  base::RunLoop run_loop;
  SurveyStringData captured_psd;
  EXPECT_CALL(*mock_hats_service,
              LaunchSurvey("TestHatsTrigger", testing::_, testing::_,
                           testing::_, testing::_, testing::_, testing::_))
      .WillOnce([&run_loop, &captured_psd](
                    const std::string& trigger,
                    base::OnceClosure success_callback,
                    base::OnceClosure failure_callback,
                    const SurveyBitsData& product_specific_bits_data,
                    const SurveyStringData& product_specific_string_data,
                    const std::optional<std::string>& supplied_trigger_id,
                    const HatsService::SurveyOptions& survey_options) {
        captured_psd = product_specific_string_data;
        std::move(success_callback).Run();
        run_loop.Quit();
        return HatsService::LaunchError::kNone;
      });

  ui::test::TestElement el1(kTestElementId1, kTestContext);
  el1.Show();
  // Let the final step's branches start before driving the exit branch.
  base::RunLoop().RunUntilIdle();

  // Show and hide element 3 (ends the journey via the exit branch). The
  // trigger element stays visible.
  ui::test::TestElement el3(kTestElementId3, kTestContext);
  el3.Show();
  el3.Hide();

  run_loop.Run();

  EXPECT_EQ(captured_psd[kCujJourneyOutcomeKey], kCujJourneyOutcomeAbandoned);
  EXPECT_EQ(captured_psd[kCujJourneyNameKey], "TestJourney");
  EXPECT_EQ(captured_psd[kCujEntryStepKey], kCujDefaultEntryStep);
  EXPECT_FALSE(captured_psd[kCujJourneyDurationKey].empty());
  EXPECT_EQ(captured_psd.size(), 4u);
  histograms.ExpectUniqueSample(
      "CriticalUserJourney.TestJourney.Result",
      CriticalUserJourneySession::JourneyResult::kAbandoned, 1);
}

TEST_F(CriticalUserJourneyServiceTest,
       SplitRateProbabilityGateFiltersAbandonedSurveys) {
  base::test::ScopedFeatureList scoped_params;
  scoped_params.InitAndEnableFeatureWithParameters(
      kTestJourney, {{"survey_probability_on_exit", "0.25"}});

  auto* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetForProfile(&profile_, true));

  // Case A: Random roll 0.50 is above the 0.25 probability on exit.
  // The survey must NOT be launched.
  service_->SetRandDoubleCallbackForTesting(
      base::BindRepeating([]() { return 0.50; }));
  EXPECT_CALL(*mock_hats_service, LaunchSurvey(testing::_, testing::_, testing::_,
                                               testing::_, testing::_,
                                               testing::_, testing::_))
      .Times(0);

  ui::test::TestElement el1(kTestElementId1, kTestContext);
  el1.Show();
  base::RunLoop().RunUntilIdle();

  ui::test::TestElement el3(kTestElementId3, kTestContext);
  el3.Show();
  el3.Hide();
  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(mock_hats_service);

  // Case B: Random roll 0.10 is below the 0.25 probability on exit.
  // The survey MUST be launched.
  base::RunLoop run_loop;
  SurveyStringData captured_psd;
  service_->SetRandDoubleCallbackForTesting(
      base::BindRepeating([]() { return 0.10; }));
  EXPECT_CALL(*mock_hats_service,
              LaunchSurvey("TestHatsTrigger", testing::_, testing::_,
                           testing::_, testing::_, testing::_, testing::_))
      .WillOnce([&run_loop, &captured_psd](
                    const std::string& trigger,
                    base::OnceClosure success_callback,
                    base::OnceClosure failure_callback,
                    const SurveyBitsData& product_specific_bits_data,
                    const SurveyStringData& product_specific_string_data,
                    const std::optional<std::string>& supplied_trigger_id,
                    const HatsService::SurveyOptions& survey_options) {
        captured_psd = product_specific_string_data;
        std::move(success_callback).Run();
        run_loop.Quit();
        return HatsService::LaunchError::kNone;
      });

  // Re-trigger the journey via exit branch.
  ui::test::TestElement el1_b(kTestElementId1, kTestContext);
  el1_b.Show();
  base::RunLoop().RunUntilIdle();

  ui::test::TestElement el3_b(kTestElementId3, kTestContext);
  el3_b.Show();
  el3_b.Hide();

  run_loop.Run();

  EXPECT_EQ(captured_psd[kCujJourneyOutcomeKey], kCujJourneyOutcomeAbandoned);
  EXPECT_EQ(captured_psd[kCujJourneyNameKey], "TestJourney");
}

TEST_F(CriticalUserJourneyServiceTest,
       ActivatedTriggerHidingBeforeNextStepCompletesJourney) {
  class ActivatedTriggerService : public CriticalUserJourneyService {
   public:
    explicit ActivatedTriggerService(Profile* profile)
        : CriticalUserJourneyService(profile) {}

   protected:
    void RegisterJourneys(CriticalUserJourneyRegistry* registry) override {
      registry->AddJourney(
          CriticalUserJourney::Builder(&kTestJourney)
              .AddAnyOf({Branch(kTestElementId1,
                                ui::InteractionSequence::StepType::kActivated,
                                1)})
              .AddStep(kTestElementId2,
                       ui::InteractionSequence::StepType::kShown, 2)
              .Build());
    }
  };

  // Tear down the default service so only `ActivatedTriggerService` listens for
  // `kTestElementId1`.
  service_.reset();
  ActivatedTriggerService activated_service(&profile_);
  activated_service.Initialize();

  base::HistogramTester histograms;

  // Showing `el1` (and even showing and hiding `el2`) without activating `el1`
  // must not start or complete the journey.
  ui::test::TestElement el1(kTestElementId1, kTestContext);
  el1.Show();
  ui::test::TestElement el2(kTestElementId2, kTestContext);
  el2.Show();
  el2.Hide();
  base::RunLoop().RunUntilIdle();
  histograms.ExpectTotalCount("CriticalUserJourney.TestJourney.Result", 0);

  // Activating `el1` starts the session. Hiding `el1` before `el2` is shown
  // (e.g. a page action chip disappearing as the side panel opens) must not
  // abort the journey.
  el1.Activate();
  el1.Hide();
  el2.Show();
  base::RunLoop().RunUntilIdle();

  histograms.ExpectUniqueSample(
      "CriticalUserJourney.TestJourney.Result",
      CriticalUserJourneySession::JourneyResult::kCompleted, 1);
  histograms.ExpectBucketCount("CriticalUserJourney.TestJourney.StepReached", 2,
                               1);
}

// Tests the production journeys registered by `CriticalUserJourneyRegistry`.
// Unlike `CriticalUserJourneyServiceTest`, this fixture does not install
// `TestCriticalUserJourneyService`, so production journeys are only driven
// through the `CriticalUserJourneyService` that `TestingProfile` creates via
// `CriticalUserJourneyServiceFactory`.
class CriticalUserJourneyProductionJourneysTest : public testing::Test {
 public:
  CriticalUserJourneyProductionJourneysTest() {
    feature_list_.InitAndEnableFeature(kCriticalUserJourneyService);
  }
  ~CriticalUserJourneyProductionJourneysTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    HatsServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildMockHatsService));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(CriticalUserJourneyProductionJourneysTest,
       SettingsGlowupJourneysHaveDistinctNamesAndOneBasedSteps) {
  CriticalUserJourneyRegistry registry;
  registry.AddJourneys();

  std::vector<std::string> settings_journey_names;
  for (const auto& journey : registry.journeys()) {
    const std::string name(journey->name());
    if (name.starts_with("Settings")) {
      settings_journey_names.push_back(name);
      ASSERT_FALSE(journey->steps().empty());
      int expected_metric_id = 1;
      for (const auto& step : journey->steps()) {
        if (step->type == ui::InteractionSequence::StepType::kSubsequence) {
          EXPECT_FALSE(step->branches.empty());
          for (const auto& branch : step->branches) {
            ASSERT_FALSE(branch->steps().empty());
            for (const auto& branch_step : branch->steps()) {
              EXPECT_EQ(branch_step->type,
                        ui::InteractionSequence::StepType::kCustomEvent);
              EXPECT_EQ(branch_step->metric_id, expected_metric_id++)
                  << "Journey " << name
                  << " branch step must have sequential 1-based metric_id";
            }
          }
        } else {
          EXPECT_EQ(step->type,
                    ui::InteractionSequence::StepType::kCustomEvent);
          EXPECT_EQ(step->metric_id, expected_metric_id++)
              << "Journey " << name
              << " step must have sequential 1-based metric_id";
        }
      }
    }
  }

  EXPECT_THAT(settings_journey_names,
              testing::UnorderedElementsAre("SettingsNavIAJourney",
                                            "SettingsSuperchargedSearchJourney",
                                            "SettingsSearchEngineJourney",
                                            "SettingsSitesDashboardJourney",
                                            "SettingsClearBrowsingDataJourney",
                                            "SettingsAppearanceThemesJourney"));
}

TEST_F(CriticalUserJourneyProductionJourneysTest,
       SettingsSearchEngineJourneyCompletesOnEitherBranch) {
  // `TestingProfile` already creates and initializes a
  // `CriticalUserJourneyService` via `CriticalUserJourneyServiceFactory`.
  ASSERT_TRUE(CriticalUserJourneyServiceFactory::GetForProfile(&profile_));

  base::HistogramTester histograms;
  ui::test::TestElement test_element(kBrowserViewElementId, kTestContext);
  test_element.Show();

  // Branch 1: Navigate to search engine settings, then change default engine.
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
      &test_element, settings::kSearchEngineNavClickedEventId);
  base::RunLoop().RunUntilIdle();
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
      &test_element, settings::kDefaultSearchEngineChangedEventId);
  base::RunLoop().RunUntilIdle();

  // Branch 2: Navigate to search engine settings, then toggle search shortcuts.
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
      &test_element, settings::kSearchEngineNavClickedEventId);
  base::RunLoop().RunUntilIdle();
  ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
      &test_element, settings::kSearchShortcutsToggledEventId);
  base::RunLoop().RunUntilIdle();

  histograms.ExpectUniqueSample(
      "CriticalUserJourney.SettingsSearchEngineJourney.Result",
      CriticalUserJourneySession::JourneyResult::kCompleted, 2);
  histograms.ExpectBucketCount(
      "CriticalUserJourney.SettingsSearchEngineJourney.StepReached", 1, 2);
  histograms.ExpectBucketCount(
      "CriticalUserJourney.SettingsSearchEngineJourney.StepReached", 2, 1);
  histograms.ExpectBucketCount(
      "CriticalUserJourney.SettingsSearchEngineJourney.StepReached", 3, 1);
}

}  // namespace metrics
