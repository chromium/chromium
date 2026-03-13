// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_registry.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_service.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_service_factory.h"
#include "chrome/browser/metrics/critical_user_journeys/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace metrics {

namespace {
const char kAppMenuJourneyName[] = "AppMenuJourney";
const char kBranchingJourneyName[] = "BranchingJourney";

const std::string GetMetricJourneyPrefix(const std::string& journey) {
  return base::StrCat({"CriticalUserJourney.", journey});
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
    registry->AddJourney(
        CriticalUserJourney::Builder(kAppMenuJourneyName)
            .AddStep(kToolbarAppMenuButtonElementId,
                     ui::InteractionSequence::StepType::kActivated, 1)
            .AddStep(kNewTabButtonElementId,
                     ui::InteractionSequence::StepType::kActivated, 2)
            .Build());

    // Branching Journey: Click App Menu button (triggers start), then click
    // New Tab button (branch 1) or click the toolbar forward button.
    registry->AddJourney(
        CriticalUserJourney::Builder(kBranchingJourneyName)
            .AddStep(kToolbarAppMenuButtonElementId,
                     ui::InteractionSequence::StepType::kActivated, 1)
            .AddAnyOf({
                Branch(kNewTabButtonElementId,
                       ui::InteractionSequence::StepType::kActivated, 2),
                Branch(kToolbarForwardButtonElementId,
                       ui::InteractionSequence::StepType::kActivated, 3),
            })
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

  const std::string step_reached = base::StrCat(
      {GetMetricJourneyPrefix(kAppMenuJourneyName), ".StepReached"});
  const std::string completed =
      base::StrCat({GetMetricJourneyPrefix(kAppMenuJourneyName), ".Completed"});

  RunTestSequence(
      // Step 1: Click App Menu.
      PressButton(kToolbarAppMenuButtonElementId),

      // Step 2: Click New Tab button.
      PressButton(kNewTabButtonElementId));

  // Verification: The journey should complete asynchronously.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return histograms.GetBucketCount(completed, true) > 0; }));

  histograms.ExpectBucketCount(step_reached, 1, 1);
  histograms.ExpectBucketCount(step_reached, 2, 1);
  histograms.ExpectUniqueSample(completed, true, 1);
}

IN_PROC_BROWSER_TEST_F(CriticalUserJourneyServiceInteractiveTest,
                       BranchingJourneyCompletion) {
  base::HistogramTester histograms;

  const std::string step_reached = base::StrCat(
      {GetMetricJourneyPrefix(kBranchingJourneyName), ".StepReached"});
  const std::string completed = base::StrCat(
      {GetMetricJourneyPrefix(kBranchingJourneyName), ".Completed"});

  RunTestSequence(
      // Step 1: Click App Menu (triggers start).
      PressButton(kToolbarAppMenuButtonElementId),

      // Step 2: Click the New Tab button (the first branch).
      PressButton(kNewTabButtonElementId));

  // Verification
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return histograms.GetBucketCount(completed, true) > 0; }));

  histograms.ExpectBucketCount(step_reached, 1, 1);
  histograms.ExpectBucketCount(step_reached, 2, 1);
  histograms.ExpectUniqueSample(completed, true, 1);
}

}  // namespace metrics
