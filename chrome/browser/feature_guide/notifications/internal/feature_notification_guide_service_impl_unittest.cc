// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_guide/notifications/internal/feature_notification_guide_service_impl.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "chrome/browser/feature_guide/notifications/config.h"
#include "chrome/browser/feature_guide/notifications/internal/utils.h"
#include "chrome/browser/notifications/scheduler/test/mock_notification_schedule_service.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using testing::_;
using testing::Return;

namespace feature_guide {
namespace {
constexpr char16_t kTestNotificationTitle[] = u"title_";
constexpr char16_t kTestNotificationMessage[] = u"message_";

std::string ToString(FeatureType feature) {
  switch (feature) {
    case FeatureType::kIncognitoTab:
      return "incognito_tab";
    case FeatureType::kVoiceSearch:
      return "voice_search";
    case FeatureType::kNTPSuggestionCard:
      return "ntp_suggestion_card";
    case FeatureType::kDefaultBrowser:
      return "default_browser";
    case FeatureType::kSignIn:
      return "sign_in";
    default:
      return "default_feature";
  }
}

class TestDelegate : public FeatureNotificationGuideService::Delegate {
 public:
  std::u16string GetNotificationTitle(FeatureType feature) override {
    return std::u16string(kTestNotificationTitle) +
           base::ASCIIToUTF16(ToString(feature));
  }

  std::u16string GetNotificationMessage(FeatureType feature) override {
    return std::u16string(kTestNotificationMessage) +
           base::ASCIIToUTF16(ToString(feature));
  }

  void OnNotificationClick(FeatureType feature) override {}
  void CloseNotification(const std::string& notification_guid) override {}
  bool ShouldSkipFeature(FeatureType feature) override { return false; }
  std::string GetNotificationParamGuidForFeature(FeatureType feature) override {
    return "guid_" + ToString(feature);
  }

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

segmentation_platform::ClassificationResult GetClassificationResult() {
  segmentation_platform::ClassificationResult result(
      segmentation_platform::PredictionStatus::kSucceeded);
  result.ordered_labels.emplace_back(
      segmentation_platform::kChromeLowUserEngagementUmaName);
  return result;
}

class FeatureNotificationGuideServiceImplTest : public testing::Test {
 public:
  FeatureNotificationGuideServiceImplTest() = default;
  ~FeatureNotificationGuideServiceImplTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {segmentation_platform::features::
             kSegmentationPlatformLowEngagementFeature},
        {});
    config_.enabled_features.emplace_back(FeatureType::kIncognitoTab);
    config_.enabled_features.emplace_back(FeatureType::kVoiceSearch);
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
  segmentation_platform::MockSegmentationPlatformService
      segmentation_platform_service_;
  Config config_;
  base::SimpleTestClock test_clock_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<FeatureNotificationGuideServiceImpl> service_;
};

TEST_F(FeatureNotificationGuideServiceImplTest, BasicFlow) {
  EXPECT_CALL(segmentation_platform_service_,
              GetClassificationResult(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(GetClassificationResult()));

  EXPECT_CALL(tracker_, WouldTriggerHelpUI(_)).WillRepeatedly(Return(true));
  service_->OnSchedulerInitialized(std::set<std::string>());
  auto queued_params = notifcation_scheduler_.GetQueuedParamsAndClear();
  ASSERT_EQ(2u, queued_params.size());
  EXPECT_EQ(notifications::SchedulerClientType::kFeatureGuide,
            queued_params[0]->type);

  EXPECT_EQ(base::ASCIIToUTF16(std::string("title_incognito_tab")),
            queued_params[0]->notification_data.title);
  EXPECT_EQ(base::ASCIIToUTF16(std::string("message_incognito_tab")),
            queued_params[0]->notification_data.message);
  EXPECT_EQ(
      FeatureType::kIncognitoTab,
      FeatureFromCustomData(queued_params[0]->notification_data.custom_data));
  EXPECT_EQ(test_clock_.Now(),
            queued_params[0]->schedule_params.deliver_time_start.value());

  EXPECT_EQ(base::ASCIIToUTF16(std::string("title_voice_search")),
            queued_params[1]->notification_data.title);
  EXPECT_EQ(base::ASCIIToUTF16(std::string("message_voice_search")),
            queued_params[1]->notification_data.message);
  EXPECT_EQ(
      FeatureType::kVoiceSearch,
      FeatureFromCustomData(queued_params[1]->notification_data.custom_data));
  EXPECT_EQ(test_clock_.Now() + base::Days(7),
            queued_params[1]->schedule_params.deliver_time_start.value());
}

TEST_F(FeatureNotificationGuideServiceImplTest, SkipAlreadyScheduledFeatures) {
  EXPECT_CALL(segmentation_platform_service_,
              GetClassificationResult(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(GetClassificationResult()));
  EXPECT_CALL(tracker_, WouldTriggerHelpUI(_)).WillRepeatedly(Return(true));
  std::set<std::string> scheduled_guids;
  scheduled_guids.insert("guid_incognito_tab");
  service_->OnSchedulerInitialized(scheduled_guids);
  auto queued_params = notifcation_scheduler_.GetQueuedParamsAndClear();
  EXPECT_EQ(1u, queued_params.size());
  EXPECT_EQ(base::ASCIIToUTF16(std::string("title_voice_search")),
            queued_params[0]->notification_data.title);
  EXPECT_EQ(test_clock_.Now(),
            queued_params[0]->schedule_params.deliver_time_start.value());
}

}  // namespace

}  // namespace feature_guide
