// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_guide/notifications/internal/feature_notification_guide_service_impl.h"

#include <string>

#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "build/build_config.h"
#include "chrome/browser/feature_guide/notifications/config.h"
#include "chrome/browser/feature_guide/notifications/feature_type.h"
#include "chrome/browser/feature_guide/notifications/internal/feature_notification_guide_notification_client.h"
#include "chrome/browser/feature_guide/notifications/internal/utils.h"
#include "chrome/browser/notifications/scheduler/public/client_overview.h"
#include "chrome/browser/notifications/scheduler/public/notification_data.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/notifications/scheduler/public/schedule_params.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/segment_selection_result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

namespace feature_guide {
namespace {

const base::TimeDelta kDeliverEndTimeDelta = base::Minutes(5);

}  // namespace

std::unique_ptr<notifications::NotificationSchedulerClient>
CreateFeatureNotificationGuideNotificationClient(ServiceGetter service_getter) {
  return std::make_unique<FeatureNotificationGuideNotificationClient>(
      service_getter);
}

FeatureNotificationGuideServiceImpl::FeatureNotificationGuideServiceImpl(
    std::unique_ptr<FeatureNotificationGuideService::Delegate> delegate,
    const Config& config,
    notifications::NotificationScheduleService* notification_scheduler,
    feature_engagement::Tracker* tracker,
    segmentation_platform::SegmentationPlatformService*
        segmentation_platform_service,
    base::Clock* clock)
    : delegate_(std::move(delegate)),
      notification_scheduler_(notification_scheduler),
      tracker_(tracker),
      segmentation_platform_service_(segmentation_platform_service),
      clock_(clock),
      config_(config) {
  DCHECK(notification_scheduler_);
  delegate_->SetService(this);
}

FeatureNotificationGuideServiceImpl::~FeatureNotificationGuideServiceImpl() =
    default;

void FeatureNotificationGuideServiceImpl::OnSchedulerInitialized(
    const std::set<std::string>& guids) {
  for (const std::string& guid : guids) {
    scheduled_features_.emplace(NotificationIdToFeature(guid));
  }

  tracker_->AddOnInitializedCallback(
      base::BindOnce(&FeatureNotificationGuideServiceImpl::OnTrackerInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FeatureNotificationGuideServiceImpl::OnTrackerInitialized(
    bool init_success) {
  if (!init_success)
    return;

  segmentation_platform_service_->GetSelectedSegment(
      segmentation_platform::kChromeLowUserEngagementSegmentationKey,
      base::BindOnce(
          &FeatureNotificationGuideServiceImpl::OnQuerySegmentationPlatform,
          weak_ptr_factory_.GetWeakPtr()));
}

void FeatureNotificationGuideServiceImpl::OnQuerySegmentationPlatform(
    const segmentation_platform::SegmentSelectionResult& result) {
  if (!result.is_ready || !result.segment.has_value())
    return;
  if (result.segment.value() !=
      optimization_guide::proto::OptimizationTarget::
          OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT) {
    return;
  }

  StartCheckingForEligibleFeatures();
}

void FeatureNotificationGuideServiceImpl::StartCheckingForEligibleFeatures() {
  for (auto feature : config_.enabled_features) {
    if (base::Contains(scheduled_features_, feature))
      continue;

#if defined(OS_ANDROID)
    if (!tracker_->WouldTriggerHelpUI(
            GetNotificationIphFeatureForFeature(feature))) {
      continue;
    }
#endif

    ScheduleNotification(feature);
  }
}

void FeatureNotificationGuideServiceImpl::ScheduleNotification(
    FeatureType feature) {
  notifications::NotificationData data;
  data.title = delegate_->GetNotificationTitle(feature);
  data.message = delegate_->GetNotificationMessage(feature);

  FeatureToCustomData(feature, &data.custom_data);

  notifications::ScheduleParams schedule_params;
  schedule_params.priority =
      notifications::ScheduleParams::Priority::kNoThrottle;

  // Show after a week.
  schedule_params.deliver_time_start =
      last_notification_schedule_time_.value_or(clock_->Now()) +
      config_.notification_deliver_time_delta;
  schedule_params.deliver_time_end =
      schedule_params.deliver_time_start.value() + kDeliverEndTimeDelta;
  last_notification_schedule_time_ = schedule_params.deliver_time_start.value();
  auto params = std::make_unique<notifications::NotificationParams>(
      notifications::SchedulerClientType::kFeatureGuide, std::move(data),
      std::move(schedule_params));
  notification_scheduler_->Schedule(std::move(params));
}

void FeatureNotificationGuideServiceImpl::BeforeShowNotification(
    std::unique_ptr<notifications::NotificationData> notification_data,
    NotificationDataCallback callback) {
  FeatureType feature = FeatureFromCustomData(notification_data->custom_data);
  DCHECK(feature != FeatureType::kInvalid);

#if defined(OS_ANDROID)
  if (!tracker_->ShouldTriggerHelpUI(
          GetNotificationIphFeatureForFeature(feature))) {
    std::move(callback).Run(nullptr);
    return;
  }
#endif

  std::move(callback).Run(std::move(notification_data));
}

void FeatureNotificationGuideServiceImpl::OnClick(FeatureType feature) {
  delegate_->OnNotificationClick(feature);
}

}  // namespace feature_guide
