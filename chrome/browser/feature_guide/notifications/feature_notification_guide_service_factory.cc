// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service_factory.h"

#include "base/feature_list.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/feature_guide/notifications/config.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/feature_guide/notifications/feature_type.h"
#include "chrome/browser/feature_guide/notifications/internal/feature_notification_guide_service_impl.h"
#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/feature_guide/notifications/android/feature_notification_guide_bridge.h"
#endif

namespace feature_guide {
namespace {

// Default notification time interval in days.
constexpr int kDefaultNotificationTimeIntervalDays = 7;

void AddFeatureIfEnabled(std::vector<FeatureType>* enabled_features,
                         const std::string& feature_name,
                         FeatureType feature_type,
                         int repeat_count) {
  if (!base::GetFieldTrialParamByFeatureAsBool(
          features::kFeatureNotificationGuide, feature_name, false)) {
    return;
  }

  for (int i = 0; i < repeat_count; i++) {
    enabled_features->emplace_back(feature_type);
  }
}

std::vector<FeatureType> GetEnabledFeaturesFromVariations() {
  std::vector<FeatureType> enabled_features;
  int repeat_count = base::GetFieldTrialParamByFeatureAsInt(
      features::kFeatureNotificationGuide, "feature_notification_repeat_count",
      1);

  AddFeatureIfEnabled(&enabled_features, "enable_feature_incognito_tab",
                      FeatureType::kIncognitoTab, repeat_count);
  AddFeatureIfEnabled(&enabled_features, "enable_feature_ntp_suggestion_card",
                      FeatureType::kNTPSuggestionCard, repeat_count);
  AddFeatureIfEnabled(&enabled_features, "enable_feature_voice_search",
                      FeatureType::kVoiceSearch, repeat_count);
  AddFeatureIfEnabled(&enabled_features, "enable_feature_default_browser",
                      FeatureType::kDefaultBrowser, repeat_count);
  AddFeatureIfEnabled(&enabled_features, "enable_feature_sign_in",
                      FeatureType::kSignIn, repeat_count);
  return enabled_features;
}

base::TimeDelta GetNotificationStartTimeDeltaFromVariations() {
  int notification_interval_days = base::GetFieldTrialParamByFeatureAsInt(
      features::kFeatureNotificationGuide, "notification_interval_days",
      kDefaultNotificationTimeIntervalDays);
  return base::Days(notification_interval_days);
}

}  // namespace

// static
FeatureNotificationGuideServiceFactory*
FeatureNotificationGuideServiceFactory::GetInstance() {
  return base::Singleton<FeatureNotificationGuideServiceFactory>::get();
}

// static
FeatureNotificationGuideService*
FeatureNotificationGuideServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<FeatureNotificationGuideService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

FeatureNotificationGuideServiceFactory::FeatureNotificationGuideServiceFactory()
    : ProfileKeyedServiceFactory("FeatureNotificationGuideService") {
  DependsOn(NotificationScheduleServiceFactory::GetInstance());
  DependsOn(feature_engagement::TrackerFactory::GetInstance());
  DependsOn(
      segmentation_platform::SegmentationPlatformServiceFactory::GetInstance());
}

KeyedService* FeatureNotificationGuideServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  auto* notification_scheduler =
      NotificationScheduleServiceFactory::GetForKey(profile->GetProfileKey());
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile);
  segmentation_platform::SegmentationPlatformService*
      segmentation_platform_service = segmentation_platform::
          SegmentationPlatformServiceFactory::GetForProfile(profile);
  Config config;
  config.enabled_features = GetEnabledFeaturesFromVariations();
  config.notification_deliver_time_delta =
      GetNotificationStartTimeDeltaFromVariations();
  config.feature_notification_tracking_only =
      base::GetFieldTrialParamByFeatureAsBool(
          features::kFeatureNotificationGuide,
          "feature_notification_tracking_only", false);
  std::unique_ptr<FeatureNotificationGuideService::Delegate> delegate;
#if BUILDFLAG(IS_ANDROID)
  delegate.reset(new FeatureNotificationGuideBridge());
#endif
  return new FeatureNotificationGuideServiceImpl(
      std::move(delegate), config, notification_scheduler, tracker,
      segmentation_platform_service, base::DefaultClock::GetInstance());
}

bool FeatureNotificationGuideServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace feature_guide
