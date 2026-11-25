// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/critical_actions/critical_action_factory.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/critical_actions/core/browser/critical_action_metrics.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "components/critical_actions/core/browser/features.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace critical_actions {

class CriticalActionFactoryTest : public testing::Test {
 public:
  CriticalActionFactoryTest() {
    feature_list_.InitAndEnableFeature(features::kCriticalActionHistory);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that GetForProfile returns a valid service for a regular
// profile and returns nullptr for an off-the-record profile.
TEST_F(CriticalActionFactoryTest, GetForProfile) {
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  CriticalActionService* service =
      CriticalActionFactory::GetForProfile(profile.get());
  EXPECT_NE(service, nullptr);

  // Check that the off-the-record profile gets a null service.
  Profile* otrProfile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  CriticalActionService* otr_service =
      CriticalActionFactory::GetForProfile(otrProfile);
  EXPECT_EQ(otr_service, nullptr);
}

// Verifies that calling GetForProfile multiple times with the same
// profile returns the same instance.
TEST_F(CriticalActionFactoryTest, ReturnsSameInstanceForSameProfile) {
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  CriticalActionService* service1 =
      CriticalActionFactory::GetForProfile(profile.get());
  EXPECT_NE(service1, nullptr);

  CriticalActionService* service2 =
      CriticalActionFactory::GetForProfile(profile.get());
  EXPECT_EQ(service1, service2);
}

// Verifies that calling GetForProfile multiple times with different
// profiles returns different instances.
TEST_F(CriticalActionFactoryTest, UniqueInstancesForDifferentProfiles) {
  std::unique_ptr<TestingProfile> profile1 = TestingProfile::Builder().Build();
  CriticalActionService* service1 =
      CriticalActionFactory::GetForProfile(profile1.get());
  EXPECT_NE(service1, nullptr);

  std::unique_ptr<TestingProfile> profile2 = TestingProfile::Builder().Build();
  CriticalActionService* service2 =
      CriticalActionFactory::GetForProfile(profile2.get());
  EXPECT_NE(service2, nullptr);
  EXPECT_NE(service1, service2);
}

// Verifies that the service is created together with the profile, without
// eagerly creating CriticalActions.db on disk.
TEST_F(CriticalActionFactoryTest, CreatedWithProfile) {
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  CriticalActionService* service =
      CriticalActionFactory::GetForProfileIfExistsForTesting(profile.get());
  EXPECT_NE(service, nullptr);
  EXPECT_EQ(service, CriticalActionFactory::GetForProfile(profile.get()));

  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_FALSE(base::PathExists(
      profile->GetPath().Append(kCriticalActionsDatabaseFilename)));
}

// Verifies that no service is created when the feature is disabled.
TEST_F(CriticalActionFactoryTest, NotCreatedWhenDisabled) {
  base::test::ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(features::kCriticalActionHistory);
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
  EXPECT_EQ(
      CriticalActionFactory::GetForProfileIfExistsForTesting(profile.get()),
      nullptr);
  EXPECT_EQ(CriticalActionFactory::GetForProfile(profile.get()), nullptr);
}

// Reproduces the bug fixed by creating the service with the profile: a page is
// visited before anything uses the service, and a critical action is then
// logged on that page. The action must be linked to the visit and persisted.
// If the service were only created on first use, it would miss the visit and
// the action would stay pending forever.
TEST_F(CriticalActionFactoryTest,
       ActionOnPageVisitedBeforeFirstUseIsPersisted) {
  constexpr int64_t kNavigationId = 42;
  TestingProfile::Builder builder;
  builder.AddTestingFactory(HistoryServiceFactory::GetInstance(),
                            HistoryServiceFactory::GetDefaultFactory());
  std::unique_ptr<TestingProfile> profile = builder.Build();
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile.get(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  ASSERT_TRUE(history_service);

  // Visit a page before the service is first used.
  history_service->AddPage(history::HistoryAddPageArgs(
      GURL("https://example.com/checkout"), base::Time::Now(),
      /*context_id=*/0, /*nav_entry_id=*/0, kNavigationId,
      /*referrer=*/GURL(), history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
      /*hidden=*/false, history::SOURCE_BROWSED,
      history::VisitResponseCodeCategory::kNot404,
      /*did_replace_entry=*/false, /*consider_for_ntp_most_visited=*/true));
  // Also delivers the OnURLVisitedWithNavigationId() notification, which is
  // posted to this thread before the flush reply.
  history::BlockUntilHistoryProcessesPendingRequests(history_service);

  // First use of the service: log an action on the visited page.
  base::HistogramTester histograms;
  CriticalActionService* service =
      CriticalActionFactory::GetForProfile(profile.get());
  ASSERT_TRUE(service);
  service->AddCriticalActionWithNavigationId(
      CriticalActionEntry::Builder()
          .SetActionType(ActionType::kFormFill)
          .SetActionSource(ActionSource::kActor)
          .SetActorTaskId("task")
          .Build(),
      kNavigationId);
  histograms.ExpectUniqueSample(
      "CriticalActions.VisitIdResolutionOutcome.Actor",
      VisitIdResolutionOutcome::kSuccess, 1);

  base::test::TestFuture<std::vector<CriticalActionEntry>> actions;
  service->GetCriticalActions(CriticalActionQueryOptions(),
                              actions.GetCallback());
  ASSERT_EQ(actions.Get().size(), 1u);
  EXPECT_NE(actions.Get()[0].visit_id, 0);
}

}  // namespace critical_actions
