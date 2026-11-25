// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_service_factory.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class HistoryServiceFactoryTest : public testing::Test {
 public:
  HistoryServiceFactoryTest() {
    scoped_feature_list_.InitAndEnableFeature(
        history::kDeferHistoryBackendInit);
  }

  void SetUp() override { AfterStartupTaskUtils::UnsafeResetForTesting(); }

  void TearDown() override {
    // Drain any queued `AfterStartupTaskUtils` fallback tasks before resetting.
    AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();
    task_environment_.RunUntilIdle();
    AfterStartupTaskUtils::UnsafeResetForTesting();
  }

  std::unique_ptr<TestingProfile> BuildProfile() {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(HistoryServiceFactory::GetInstance(),
                              HistoryServiceFactory::GetDefaultFactory());
    return builder.Build();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(HistoryServiceFactoryTest, SchedulesDeferredInitOnWindowPresented) {
  auto profile1 = BuildProfile();
  auto profile2 = BuildProfile();

  history::HistoryService* service1 = HistoryServiceFactory::GetForProfile(
      profile1.get(), ServiceAccessType::EXPLICIT_ACCESS);
  history::HistoryService* service2 = HistoryServiceFactory::GetForProfile(
      profile2.get(), ServiceAccessType::EXPLICIT_ACCESS);
  ASSERT_TRUE(service1);
  ASSERT_TRUE(service2);
  EXPECT_FALSE(service1->is_backend_init_scheduled_for_testing());
  EXPECT_FALSE(service2->is_backend_init_scheduled_for_testing());

  // An incognito window of `profile1` presenting schedules the original
  // profile's deferred backend init without affecting other profiles.
  HistoryServiceFactory::OnBrowserWindowPresented(
      profile1->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  EXPECT_TRUE(service1->is_backend_init_scheduled_for_testing());
  EXPECT_FALSE(service2->is_backend_init_scheduled_for_testing());

  // `profile2` never presents, so `AfterStartupTaskUtils` is its fallback.
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(service2->is_backend_init_scheduled_for_testing());
}

}  // namespace
