// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_guide/notifications/internal/feature_notification_guide_service_impl.h"

#include <string>
#include <vector>

#include "base/test/gmock_callback_support.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/feature_guide/notifications/config.h"
#include "chrome/browser/notifications/scheduler/test/mock_notification_schedule_service.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/public/segment_selection_result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using testing::_;
using testing::Return;

namespace feature_guide {
namespace {
constexpr char16_t kTestNotificationTitle[] = u"test_title";
constexpr char16_t kTestNotificationMessage[] = u"test_message";

class TestDelegate : public FeatureNotificationGuideService::Delegate {
 public:
  std::u16string GetNotificationTitle(FeatureType feature) override {
    return std::u16string(kTestNotificationTitle);
  }

  std::u16string GetNotificationMessage(FeatureType feature) override {
    return std::u16string(kTestNotificationMessage);
  }

  void OnNotificationClick(FeatureType feature) override {}

  ~TestDelegate() override = default;
};

class TestScheduler
    : public notifications::test::MockNotificationScheduleService {
 public:
  void Schedule(std::unique_ptr<notifications::NotificationParams>
                    notification_params) override {
    queued_params_.emplace_back(std::move(notification_params));
  }

  std::vector<std::unique_ptr<notifications::NotificationParams>>
  GetQueuedParamsAndClear() {
    return std::move(queued_params_);
  }

 private:
  std::vector<std::unique_ptr<notifications::NotificationParams>>
      queued_params_;
};

class TestSegmentationPlatformService
    : public segmentation_platform::SegmentationPlatformService {
 public:
  void GetSelectedSegment(const std::string& segmentation_key,
                          segmentation_platform::SegmentationPlatformService::
                              SegmentSelectionCallback callback) override {
    segmentation_platform::SegmentSelectionResult result;
    result.is_ready = true;
    result.segment = optimization_guide::proto::OptimizationTarget::
        OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT;
    std::move(callback).Run(result);
  }
  segmentation_platform::SegmentSelectionResult GetCachedSegmentResult(
      const std::string& segmentation_key) override {
    segmentation_platform::SegmentSelectionResult result;
    result.is_ready = true;
    result.segment = optimization_guide::proto::OptimizationTarget::
        OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT;
    return result;
  }
  void EnableMetrics(bool signal_collection_allowed) override {}
  segmentation_platform::ServiceProxy* GetServiceProxy() override {
    return nullptr;
  }
};

class FeatureNotificationGuideServiceImplTest : public testing::Test {
 public:
  FeatureNotificationGuideServiceImplTest() = default;
  ~FeatureNotificationGuideServiceImplTest() override = default;

  void SetUp() override {
    config_.enabled_features.emplace_back(FeatureType::kIncognitoTab);
    config_.notification_deliver_time_delta = base::Days(7);
    service_ = std::make_unique<FeatureNotificationGuideServiceImpl>(
        std::make_unique<TestDelegate>(), config_, &notifcation_scheduler_,
        &tracker_, &segmentation_platform_service_, &test_clock_);
    EXPECT_CALL(tracker_, AddOnInitializedCallback(_))
        .WillOnce(RunOnceCallback<0>(true));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestScheduler notifcation_scheduler_;
  feature_engagement::test::MockTracker tracker_;
  TestSegmentationPlatformService segmentation_platform_service_;
  Config config_;
  base::SimpleTestClock test_clock_;
  std::unique_ptr<FeatureNotificationGuideServiceImpl> service_;
};

TEST_F(FeatureNotificationGuideServiceImplTest, BasicFlow) {
  EXPECT_CALL(tracker_, WouldTriggerHelpUI(_)).WillRepeatedly(Return(true));
  service_->OnSchedulerInitialized(std::set<std::string>());
  auto queued_params = notifcation_scheduler_.GetQueuedParamsAndClear();
  EXPECT_EQ(1u, queued_params.size());
  EXPECT_EQ(notifications::SchedulerClientType::kFeatureGuide,
            queued_params[0]->type);
  EXPECT_EQ(kTestNotificationTitle, queued_params[0]->notification_data.title);
  EXPECT_EQ(kTestNotificationMessage,
            queued_params[0]->notification_data.message);
  EXPECT_EQ(test_clock_.Now() + base::Days(7),
            queued_params[0]->schedule_params.deliver_time_start.value());
}

}  // namespace

}  // namespace feature_guide
