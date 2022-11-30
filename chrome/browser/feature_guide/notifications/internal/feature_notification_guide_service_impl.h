// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_INTERNAL_FEATURE_NOTIFICATION_GUIDE_SERVICE_IMPL_H_
#define CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_INTERNAL_FEATURE_NOTIFICATION_GUIDE_SERVICE_IMPL_H_

#include <deque>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/feature_guide/notifications/config.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/feature_guide/notifications/feature_type.h"

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

namespace notifications {
class NotificationScheduleService;
struct NotificationData;
}  // namespace notifications

namespace segmentation_platform {
class SegmentationPlatformService;
struct SegmentSelectionResult;
}  // namespace segmentation_platform

namespace feature_guide {

class FeatureNotificationGuideServiceImpl
    : public FeatureNotificationGuideService {
 public:
  FeatureNotificationGuideServiceImpl(
      std::unique_ptr<FeatureNotificationGuideService::Delegate> delegate,
      const Config& config,
      notifications::NotificationScheduleService* notification_scheduler,
      feature_engagement::Tracker* tracker,
      segmentation_platform::SegmentationPlatformService*
          segmentation_platform_service,
      base::Clock* clock);
  ~FeatureNotificationGuideServiceImpl() override;

  void OnSchedulerInitialized(const std::set<std::string>& guids) override;
  void BeforeShowNotification(
      std::unique_ptr<notifications::NotificationData> notification_data,
      NotificationDataCallback callback) override;
  void OnClick(FeatureType feature) override;

  Delegate* GetDelegate() { return delegate_.get(); }

 private:
  void OnTrackerInitialized(bool init_success);
  void OnQuerySegmentationPlatform(
      const segmentation_platform::SegmentSelectionResult& result);
  void StartCheckingForEligibleFeatures();
  void ScheduleNotification(FeatureType feature, bool schedule_immediately);
  void CloseRedundantNotifications();
  void CheckForLowEnagedUser();

  std::unique_ptr<FeatureNotificationGuideService::Delegate> delegate_;
  raw_ptr<notifications::NotificationScheduleService> notification_scheduler_;
  raw_ptr<feature_engagement::Tracker> tracker_;
  raw_ptr<segmentation_platform::SegmentationPlatformService>
      segmentation_platform_service_;
  raw_ptr<base::Clock> clock_;
  Config config_;

  std::set<std::string> scheduled_feature_guids_;
  absl::optional<base::Time> last_notification_schedule_time_;
  bool is_low_engaged_user_{false};

  base::WeakPtrFactory<FeatureNotificationGuideServiceImpl> weak_ptr_factory_{
      this};
};

}  // namespace feature_guide

#endif  // CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_INTERNAL_FEATURE_NOTIFICATION_GUIDE_SERVICE_IMPL_H_
