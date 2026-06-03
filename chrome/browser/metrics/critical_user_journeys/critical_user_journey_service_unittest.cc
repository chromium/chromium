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
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_session.h"
#include "chrome/browser/metrics/critical_user_journeys/features.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
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
            .AddStep(kTestElementId2, ui::InteractionSequence::StepType::kShown,
                     2)
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
  EXPECT_CALL(*mock_hats_service,
              LaunchSurvey("TestHatsTrigger", testing::_, testing::_,
                           testing::_, testing::_, testing::_, testing::_))
      .WillOnce(
          [&run_loop](const std::string& trigger,
                      base::OnceClosure success_callback,
                      base::OnceClosure failure_callback,
                      const SurveyBitsData& product_specific_bits_data,
                      const SurveyStringData& product_specific_string_data,
                      const std::optional<std::string>& supplied_trigger_id,
                      const HatsService::SurveyOptions& survey_options) {
            std::move(success_callback).Run();
            run_loop.Quit();
          });

  // Trigger the journey steps.
  // Show element 1
  ui::test::TestElement el1(kTestElementId1, kTestContext);
  el1.Show();

  // Show element 2 (triggers completion)
  ui::test::TestElement el2(kTestElementId2, kTestContext);
  el2.Show();

  run_loop.Run();

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

}  // namespace metrics
