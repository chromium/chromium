// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/hats/auto_picture_in_picture_hats_service.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/picture_in_picture/hats/auto_picture_in_picture_hats_service_factory.h"
#include "chrome/browser/picture_in_picture/hats/auto_picture_in_picture_hats_test_base.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "media/base/media_switches.h"
#include "media/base/picture_in_picture_events_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using media::PictureInPictureEventsInfo;

class AutoPictureInPictureHatsServiceTest
    : public AutoPictureInPictureHatsTestBase {
 public:
  AutoPictureInPictureHatsServiceTest() = default;

  ~AutoPictureInPictureHatsServiceTest() override = default;

  std::vector<base::test::FeatureRef> GetEnabledFeatures() override {
    return {media::kAutoPictureInPictureSurveys};
  }

  std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    return {};
  }

  AutoPictureInPictureHatsService* service() {
    ON_CALL(*mock_hats_service(), CanShowAnySurvey(false))
        .WillByDefault(testing::Return(true));
    AutoPictureInPictureHatsService* service =
        AutoPictureInPictureHatsServiceFactory::GetForProfile(profile());
    service->set_clock_for_testing(task_environment_.GetMockTickClock());
    return service;
  }
};

TEST_F(AutoPictureInPictureHatsServiceTest,
       AutoPictureInPictureWindowOpenedStartsActiveWindowContext) {
  GURL test_url("https://example.com");
  auto reason = PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing;

  EXPECT_FALSE(service()->get_active_window_context_for_testing().has_value());

  service()->AutoPictureInPictureWindowOpened(reason, test_url);

  auto& context = service()->get_active_window_context_for_testing();
  ASSERT_TRUE(context.has_value());
  EXPECT_EQ(context->reason, reason);
  EXPECT_EQ(context->origin, test_url);
  EXPECT_FALSE(context->prompt_result.has_value());
  EXPECT_EQ(context->start_time, task_environment_.NowTicks());
}

TEST_F(AutoPictureInPictureHatsServiceTest, SetPromptResultUpdatesContext) {
  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing,
      GURL("https://example.com"));

  service()->SetPromptResult(AutoPipSettingHelper::PromptResult::kBlock);

  auto& context = service()->get_active_window_context_for_testing();
  ASSERT_TRUE(context.has_value());
  EXPECT_EQ(context->prompt_result, AutoPipSettingHelper::PromptResult::kBlock);
}

TEST_F(AutoPictureInPictureHatsServiceTest,
       WindowClosedResetsContextWithPromptResult) {
  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing,
      GURL("https://example.com"));
  service()->SetPromptResult(AutoPipSettingHelper::PromptResult::kBlock);

  ASSERT_TRUE(service()->get_active_window_context_for_testing().has_value());

  service()->AutoPictureInPictureWindowClosed();

  EXPECT_FALSE(service()->get_active_window_context_for_testing().has_value());
}

TEST_F(AutoPictureInPictureHatsServiceTest,
       WindowClosedResetsContextWithoutPromptResult) {
  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing,
      GURL("https://example.com"));

  ASSERT_TRUE(service()->get_active_window_context_for_testing().has_value());

  service()->AutoPictureInPictureWindowClosed();

  EXPECT_FALSE(service()->get_active_window_context_for_testing().has_value());
}

TEST_F(AutoPictureInPictureHatsServiceTest,
       SetPromptResultWithoutContextIsNoOp) {
  service()->SetPromptResult(AutoPipSettingHelper::PromptResult::kBlock);
  EXPECT_FALSE(service()->get_active_window_context_for_testing().has_value());
}

TEST_F(AutoPictureInPictureHatsServiceTest,
       MultipleWindowOpenedOverwritesContext) {
  GURL url1("https://site1.com");
  GURL url2("https://site2.com");

  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing, url1);
  base::TimeTicks time1 = task_environment_.NowTicks();

  task_environment_.FastForwardBy(base::Seconds(10));

  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kMediaPlayback, url2);
  base::TimeTicks time2 = task_environment_.NowTicks();

  auto& context = service()->get_active_window_context_for_testing();
  ASSERT_TRUE(context.has_value());
  EXPECT_EQ(context->reason,
            PictureInPictureEventsInfo::AutoPipReason::kMediaPlayback);
  EXPECT_EQ(context->origin, url2);
  EXPECT_EQ(context->start_time, time2);
  EXPECT_NE(context->start_time, time1);
}

TEST_F(AutoPictureInPictureHatsServiceTest,
       SetPromptResultAfterWindowClosedIsNoOp) {
  service()->AutoPictureInPictureWindowOpened(
      PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing,
      GURL("https://example.com"));
  service()->AutoPictureInPictureWindowClosed();

  // This should be a no-op since the context was reset.
  service()->SetPromptResult(AutoPipSettingHelper::PromptResult::kBlock);

  EXPECT_FALSE(service()->get_active_window_context_for_testing().has_value());
}
