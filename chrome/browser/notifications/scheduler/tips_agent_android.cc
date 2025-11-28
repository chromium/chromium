// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/tips_agent_android.h"

#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/notifications/scheduler/public/tips_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TipsAgent_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

TipsAgentAndroid::TipsAgentAndroid() = default;

TipsAgentAndroid::~TipsAgentAndroid() = default;

namespace {

notifications::TipsNotificationsFeatureType GetFeatureType(
    const std::string& label) {
  if (label == segmentation_platform::kEnhancedSafeBrowsing) {
    return notifications::TipsNotificationsFeatureType::kEnhancedSafeBrowsing;
  } else if (label == segmentation_platform::kQuickDelete) {
    return notifications::TipsNotificationsFeatureType::kQuickDelete;
  } else if (label == segmentation_platform::kGoogleLens) {
    return notifications::TipsNotificationsFeatureType::kGoogleLens;
  } else if (label == segmentation_platform::kBottomOmnibox) {
    return notifications::TipsNotificationsFeatureType::kBottomOmnibox;
  } else {
    NOTREACHED();
  }
}

void RunGetClassificationResultCallback(
    Profile* profile,
    const segmentation_platform::ClassificationResult& result) {
  // If there are no suggestions then no notification will be scheduled.
  if (result.ordered_labels.empty()) {
    return;
  }

  const std::string& action_label = result.ordered_labels[0];
  notifications::TipsNotificationsFeatureType feature_type =
      GetFeatureType(action_label);

  notifications::NotificationScheduleService* service =
      NotificationScheduleServiceFactory::GetForKey(profile->GetProfileKey());

  // Setup the schedule params to either be for testing with instant and delayed
  // 2 minutes notifications, or the base use case of 2-4 hours.
  // The standard priority is low to include throttling logic from the scheduler
  // service, however testing params will allow this to be unthrottled.
  notifications::ScheduleParams schedule_params;
  schedule_params.priority =
      (segmentation_platform::features::kStartTimeMinutes.Get() < 5)
          ? notifications::ScheduleParams::Priority::kNoThrottle
          : notifications::ScheduleParams::Priority::kLow;
  schedule_params.deliver_time_start =
      base::Time::Now() +
      base::Minutes(segmentation_platform::features::kStartTimeMinutes.Get());
  schedule_params.deliver_time_end =
      base::Time::Now() +
      base::Minutes(segmentation_platform::features::kStartTimeMinutes.Get()) +
      base::Minutes(segmentation_platform::features::kWindowTimeMinutes.Get());

  notifications::NotificationData data =
      notifications::GetTipsNotificationData(feature_type);
  service->Schedule(std::make_unique<notifications::NotificationParams>(
      notifications::SchedulerClientType::kTips, std::move(data),
      std::move(schedule_params)));
}

}  // namespace

void TipsAgentAndroid::ShowTipsPromo(
    notifications::TipsNotificationsFeatureType feature_type) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_TipsAgent_showTipsPromo(env, static_cast<jint>(feature_type));
}

static void JNI_TipsAgent_MaybeScheduleNotification(
    JNIEnv* env,
    Profile* profile,
    jboolean j_is_bottom_omnibox) {
  if (!profile) {
    return;
  }

  bool is_bottom_omnibox = static_cast<bool>(j_is_bottom_omnibox);

  segmentation_platform::SegmentationPlatformService*
      segmentation_platform_service = segmentation_platform::
          SegmentationPlatformServiceFactory::GetForProfile(profile);
  if (!segmentation_platform_service) {
    return;
  }

  segmentation_platform::PredictionOptions prediction_options;
  prediction_options.on_demand_execution = true;

  PrefService* pref_service = profile->GetPrefs();
  auto input_context =
      base::MakeRefCounted<segmentation_platform::InputContext>();

  bool is_enhanced_safe_browsing =
      safe_browsing::GetSafeBrowsingState(*pref_service) ==
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION;
  input_context->metadata_args.emplace(
      segmentation_platform::kEnhancedSafeBrowsingStatus,
      segmentation_platform::processing::ProcessedValue(
          is_enhanced_safe_browsing));

  bool quick_delete_ever_used =
      pref_service->GetBoolean(browsing_data::prefs::kQuickDeleteEverUsed);
  input_context->metadata_args.emplace(
      segmentation_platform::kQuickDeleteUsage,
      segmentation_platform::processing::ProcessedValue(
          quick_delete_ever_used));

  input_context->metadata_args.emplace(
      segmentation_platform::kBottomOmniboxStatus,
      segmentation_platform::processing::ProcessedValue(is_bottom_omnibox));

  bool bottom_omnibox_ever_used =
      pref_service->GetBoolean(omnibox::kBottomOmniboxEverUsed);
  input_context->metadata_args.emplace(
      segmentation_platform::kBottomOmniboxUsage,
      segmentation_platform::processing::ProcessedValue(
          bottom_omnibox_ever_used));

  bool enhanced_safe_browsing_tip_shown =
      pref_service->GetBoolean(prefs::kAndroidTipNotificationShownESB);
  input_context->metadata_args.emplace(
      segmentation_platform::kEnhancedSafeBrowsingTipShown,
      segmentation_platform::processing::ProcessedValue(
          enhanced_safe_browsing_tip_shown));

  bool quick_delete_tip_shown =
      pref_service->GetBoolean(prefs::kAndroidTipNotificationShownQuickDelete);
  input_context->metadata_args.emplace(
      segmentation_platform::kQuickDeleteTipShown,
      segmentation_platform::processing::ProcessedValue(
          quick_delete_tip_shown));

  bool google_lens_tip_shown =
      pref_service->GetBoolean(prefs::kAndroidTipNotificationShownLens);
  input_context->metadata_args.emplace(
      segmentation_platform::kGoogleLensTipShown,
      segmentation_platform::processing::ProcessedValue(google_lens_tip_shown));

  bool bottom_omnibox_tip_shown = pref_service->GetBoolean(
      prefs::kAndroidTipNotificationShownBottomOmnibox);
  input_context->metadata_args.emplace(
      segmentation_platform::kBottomOmniboxTipShown,
      segmentation_platform::processing::ProcessedValue(
          bottom_omnibox_tip_shown));

  segmentation_platform_service->GetClassificationResult(
      segmentation_platform::kTipsNotificationsRankerKey, prediction_options,
      input_context,
      base::BindOnce(&RunGetClassificationResultCallback, profile));
}

static void JNI_TipsAgent_RemovePendingNotifications(JNIEnv* env,
                                                     Profile* profile) {
  if (!profile) {
    return;
  }

  notifications::NotificationScheduleService* service =
      NotificationScheduleServiceFactory::GetForKey(profile->GetProfileKey());
  service->DeleteNotifications(notifications::SchedulerClientType::kTips);
}

DEFINE_JNI(TipsAgent)
