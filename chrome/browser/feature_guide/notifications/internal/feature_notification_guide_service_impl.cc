// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_guide/notifications/internal/feature_notification_guide_service_impl.h"

#include <string>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
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
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
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
  DCHECK(delegate_);
  delegate_->SetService(this);
}

FeatureNotificationGuideServiceImpl::~FeatureNotificationGuideServiceImpl() =
    default;

void FeatureNotificationGuideServiceImpl::OnSchedulerInitialized(
    const std::set<std::string>& guids) {
  scheduled_feature_guids_ = guids;

  VLOG(1) << __func__ << ": number of notifications scheduled: " << guids.size()
          << ", config.featues_size: " << config_.enabled_features.size();
  tracker_->AddOnInitializedCallback(
      base::BindOnce(&FeatureNotificationGuideServiceImpl::OnTrackerInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FeatureNotificationGuideServiceImpl::OnTrackerInitialized(
    bool init_success) {
  if (!init_success)
    return;

  CheckForLowEnagedUser();
}

void FeatureNotificationGuideServiceImpl::CheckForLowEnagedUser() {
  auto closure = base::BindOnce(
      &FeatureNotificationGuideServiceImpl::StartCheckingForEligibleFeatures,
      weak_ptr_factory_.GetWeakPtr());

  // Skip low engagement check if enabled. For testing only.
  if (base::FeatureList::IsEnabled(
          feature_guide::features::kSkipCheckForLowEngagedUsers)) {
    is_low_engaged_user_ = true;
    std::move(closure).Run();
    return;
  }

  // Use tracker instead of segmentation if enabled.
  if (base::FeatureList::IsEnabled(
          feature_guide::features::kUseFeatureEngagementForUserTargeting)) {
#if BUILDFLAG(IS_ANDROID)
    if (tracker_->ShouldTriggerHelpUI(
            feature_engagement::kIPHLowUserEngagementDetectorFeature)) {
      is_low_engaged_user_ = true;
    }
#endif
    std::move(closure).Run();
    return;
  }

  if (!base::FeatureList::IsEnabled(
          feature_guide::features::kSegmentationModelLowEngagedUsers)) {
    is_low_engaged_user_ = false;
    std::move(closure).Run();
    return;
  }

  // Check segmentation model result.
  segmentation_platform_service_->GetSelectedSegment(
      segmentation_platform::kChromeLowUserEngagementSegmentationKey,
      base::BindOnce(
          [](bool* is_low_engaged_user, base::OnceClosure closure,
             const segmentation_platform::SegmentSelectionResult&
                 segment_selection_result) {
            *is_low_engaged_user =
                segment_selection_result.is_ready &&
                segment_selection_result.segment.has_value() &&
                segment_selection_result.segment.value() ==
                    segmentation_platform::proto::SegmentId::
                        OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT;
            std::move(closure).Run();
          },
          &is_low_engaged_user_, std::move(closure)));
}

void FeatureNotificationGuideServiceImpl::CloseRedundantNotifications() {
  for (auto feature : config_.enabled_features) {
#if BUILDFLAG(IS_ANDROID)
    const auto* used_iph_feature = GetUsedIphFeatureForFeature(feature);
    bool feature_was_used =
        used_iph_feature && tracker_->WouldTriggerHelpUI(*used_iph_feature);
    if (!feature_was_used)
      continue;
#endif

    std::string notification_guid =
        delegate_->GetNotificationParamGuidForFeature(feature);
    delegate_->CloseNotification(notification_guid);
  }
}

void FeatureNotificationGuideServiceImpl::StartCheckingForEligibleFeatures() {
  VLOG(1) << __func__ << ": is_low_engaged_user=" << is_low_engaged_user_;
  bool schedule_immediately = true;
  for (auto feature : config_.enabled_features) {
    std::string guid = delegate_->GetNotificationParamGuidForFeature(feature);
    if (base::Contains(scheduled_feature_guids_, guid))
      continue;

    if (!is_low_engaged_user_ && ShouldTargetLowEngagedUsers(feature))
      continue;

    if (delegate_->ShouldSkipFeature(feature))
      continue;

#if BUILDFLAG(IS_ANDROID)
    if (!tracker_->WouldTriggerHelpUI(
            GetNotificationIphFeatureForFeature(feature))) {
      VLOG(0) << __func__ << ": didn't meet trigger criteria for feature="
              << static_cast<int>(feature);
      continue;
    }
#endif

    ScheduleNotification(feature, schedule_immediately);

    // For the second feature onwards, we need to schedule notification with a
    // few days delay. Only the first feature is fired immediately.
    schedule_immediately = false;
  }

  // TODO(shaktisahu): Maybe post a task with few seconds delay.
  CloseRedundantNotifications();
}

void FeatureNotificationGuideServiceImpl::ScheduleNotification(
    FeatureType feature,
    bool schedule_immediately) {
  VLOG(1) << __func__ << ": feature=" << static_cast<int>(feature);
  notifications::NotificationData data;
  data.title = delegate_->GetNotificationTitle(feature);
  data.message = delegate_->GetNotificationMessage(feature);

  FeatureToCustomData(feature, &data.custom_data);

  notifications::ScheduleParams schedule_params;
  schedule_params.priority =
      notifications::ScheduleParams::Priority::kNoThrottle;

  // Show notification immediately or a few days.
  schedule_params.deliver_time_start =
      last_notification_schedule_time_.value_or(clock_->Now()) +
      (schedule_immediately ? base::Days(0)
                            : config_.notification_deliver_time_delta);
  schedule_params.deliver_time_end =
      schedule_params.deliver_time_start.value() + kDeliverEndTimeDelta;
  last_notification_schedule_time_ = schedule_params.deliver_time_start.value();
  auto params = std::make_unique<notifications::NotificationParams>(
      notifications::SchedulerClientType::kFeatureGuide, std::move(data),
      std::move(schedule_params));
  params->guid = delegate_->GetNotificationParamGuidForFeature(feature);
  notification_scheduler_->Schedule(std::move(params));
}

void FeatureNotificationGuideServiceImpl::BeforeShowNotification(
    std::unique_ptr<notifications::NotificationData> notification_data,
    NotificationDataCallback callback) {
  FeatureType feature = FeatureFromCustomData(notification_data->custom_data);
  VLOG(1) << __func__ << ": checking feature=" << static_cast<int>(feature);
  DCHECK(feature != FeatureType::kInvalid);

#if BUILDFLAG(IS_ANDROID)
  if (!tracker_->ShouldTriggerHelpUI(
          GetNotificationIphFeatureForFeature(feature))) {
    std::move(callback).Run(nullptr);
    return;
  }
#endif

  VLOG(1) << __func__ << ": triggering feature " << static_cast<int>(feature);
  std::move(callback).Run(config_.feature_notification_tracking_only
                              ? nullptr
                              : std::move(notification_data));
}

void FeatureNotificationGuideServiceImpl::OnClick(FeatureType feature) {
  delegate_->OnNotificationClick(feature);
}

}  // namespace feature_guide
