// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/tips_agent_android.h"

#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"
#include "chrome/browser/notifications/scheduler/public/notification_entry.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tips/core/tips_prefs.h"
#include "chrome/browser/tips/core/tips_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/TipsAgent_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

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
  } else if (label == segmentation_platform::kPasswordAutofill) {
    return notifications::TipsNotificationsFeatureType::kPasswordAutofill;
  } else if (label == segmentation_platform::kSignin) {
    return notifications::TipsNotificationsFeatureType::kSignin;
  } else if (label == segmentation_platform::kCreateTabGroups) {
    return notifications::TipsNotificationsFeatureType::kCreateTabGroups;
  } else if (label == segmentation_platform::kCustomizeMVT) {
    return notifications::TipsNotificationsFeatureType::kCustomizeMVT;
  } else if (label == segmentation_platform::kRecentTabs) {
    return notifications::TipsNotificationsFeatureType::kRecentTabs;
  } else {
    NOTREACHED();
  }
}

notifications::ScheduleParams GetCurrentScheduleParams() {
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
  return schedule_params;
}

}  // namespace

// static
void TipsAgentAndroid::RunGetClassificationResultCallback(
    Profile* profile,
    notifications::NotificationScheduleService* service,
    const segmentation_platform::ClassificationResult& result) {
  // If there are no suggestions then no notification will be scheduled.
  if (result.ordered_labels.empty()) {
    return;
  }

  const std::string& action_label = result.ordered_labels[0];
  notifications::TipsNotificationsFeatureType feature_type =
      GetFeatureType(action_label);

  notifications::ScheduleParams schedule_params = GetCurrentScheduleParams();

  notifications::NotificationData data =
      notifications::GetTipsNotificationData(feature_type);
  service->Schedule(std::make_unique<notifications::NotificationParams>(
      notifications::SchedulerClientType::kTips, std::move(data),
      std::move(schedule_params)));
}

// static
void TipsAgentAndroid::ScheduleNewNotification(
    Profile* profile,
    bool is_bottom_omnibox,
    notifications::NotificationScheduleService* service) {
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

  // V1 Tips: ESB, Quick Delete, Google Lens, Bottom Omnibox

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

  bool enhanced_safe_browsing_tip_shown = pref_service->GetBoolean(
      notifications::tips::prefs::kAndroidTipNotificationShownESB);
  input_context->metadata_args.emplace(
      segmentation_platform::kEnhancedSafeBrowsingTipShown,
      segmentation_platform::processing::ProcessedValue(
          enhanced_safe_browsing_tip_shown));

  bool quick_delete_tip_shown = pref_service->GetBoolean(
      notifications::tips::prefs::kAndroidTipNotificationShownQuickDelete);
  input_context->metadata_args.emplace(
      segmentation_platform::kQuickDeleteTipShown,
      segmentation_platform::processing::ProcessedValue(
          quick_delete_tip_shown));

  bool google_lens_tip_shown = pref_service->GetBoolean(
      notifications::tips::prefs::kAndroidTipNotificationShownLens);
  input_context->metadata_args.emplace(
      segmentation_platform::kGoogleLensTipShown,
      segmentation_platform::processing::ProcessedValue(google_lens_tip_shown));

  bool bottom_omnibox_tip_shown = pref_service->GetBoolean(
      notifications::tips::prefs::kAndroidTipNotificationShownBottomOmnibox);
  input_context->metadata_args.emplace(
      segmentation_platform::kBottomOmniboxTipShown,
      segmentation_platform::processing::ProcessedValue(
          bottom_omnibox_tip_shown));

  // V2 Tips: PW Autofill, Signin, Create Tab Groups, Customize MVT, Recent Tabs

  bool is_user_signed_in =
      IdentityManagerFactory::GetForProfile(profile)->HasPrimaryAccount(
          signin::ConsentLevel::kSignin);
  input_context->metadata_args.emplace(
      segmentation_platform::kTipsIsUserSignedIn,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          is_user_signed_in));

  bool password_autofill_tip_shown = pref_service->GetBoolean(
      notifications::tips::prefs::kAndroidTipNotificationShownPasswordAutofill);
  input_context->metadata_args.emplace(
      segmentation_platform::kPasswordAutofillTipShown,
      segmentation_platform::processing::ProcessedValue(
          password_autofill_tip_shown));

  bool signin_tip_shown = pref_service->GetBoolean(
      notifications::tips::prefs::kAndroidTipNotificationShownSignin);
  input_context->metadata_args.emplace(
      segmentation_platform::kSigninTipShown,
      segmentation_platform::processing::ProcessedValue(signin_tip_shown));

  bool create_tab_groups_tip_shown = pref_service->GetBoolean(
      notifications::tips::prefs::kAndroidTipNotificationShownCreateTabGroups);
  input_context->metadata_args.emplace(
      segmentation_platform::kCreateTabGroupsTipShown,
      segmentation_platform::processing::ProcessedValue(
          create_tab_groups_tip_shown));

  bool customize_mvt_tip_shown = pref_service->GetBoolean(
      notifications::tips::prefs::kAndroidTipNotificationShownCustomizeMVT);
  input_context->metadata_args.emplace(
      segmentation_platform::kCustomizeMVTTipShown,
      segmentation_platform::processing::ProcessedValue(
          customize_mvt_tip_shown));

  bool recent_tabs_tip_shown = pref_service->GetBoolean(
      notifications::tips::prefs::kAndroidTipNotificationShownRecentTabs);
  input_context->metadata_args.emplace(
      segmentation_platform::kRecentTabsTipShown,
      segmentation_platform::processing::ProcessedValue(recent_tabs_tip_shown));

  segmentation_platform_service->GetClassificationResult(
      segmentation_platform::kTipsNotificationsRankerKey, prediction_options,
      input_context,
      base::BindOnce(&TipsAgentAndroid::RunGetClassificationResultCallback,
                     profile, base::Unretained(service)));
}

// static
void TipsAgentAndroid::OnGetClientOverview(
    Profile* profile,
    bool is_bottom_omnibox,
    notifications::NotificationScheduleService* service,
    notifications::ClientOverview overview) {
  // If there is a scheduled notification, reschedule it.
  if (!overview.scheduled_notifications.empty()) {
    // There will only ever be 1 notification scheduled at a time for tips.
    DCHECK_EQ(overview.scheduled_notifications.size(), 1u);
    const auto* entry = overview.scheduled_notifications[0];
    notifications::NotificationData data = entry->notification_data;

    // Use new ScheduleParams to reschedule based on the current time.
    notifications::ScheduleParams params = GetCurrentScheduleParams();

    // Wipe all pending notifications before rescheduling.
    service->DeleteNotifications(notifications::SchedulerClientType::kTips);
    service->Schedule(std::make_unique<notifications::NotificationParams>(
        notifications::SchedulerClientType::kTips, std::move(data),
        std::move(params)));
  } else {
    TipsAgentAndroid::ScheduleNewNotification(profile, is_bottom_omnibox,
                                              service);
  }
}

TipsAgentAndroid::TipsAgentAndroid() = default;

TipsAgentAndroid::~TipsAgentAndroid() = default;

void TipsAgentAndroid::ShowTipsPromo(
    notifications::TipsNotificationsFeatureType feature_type) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_TipsAgent_showTipsPromo(env, static_cast<int32_t>(feature_type));
}

static void JNI_TipsAgent_MaybeScheduleNotification(JNIEnv* env,
                                                    Profile* profile,
                                                    bool j_is_bottom_omnibox) {
  if (!profile) {
    return;
  }

  bool is_bottom_omnibox = j_is_bottom_omnibox;

  // Check the cached pending notifications in the ClientOverview.
  notifications::NotificationScheduleService* service =
      NotificationScheduleServiceFactory::GetForKey(profile->GetProfileKey());
  service->GetClientOverview(
      notifications::SchedulerClientType::kTips,
      base::BindOnce(
          [](Profile* profile, bool is_bottom_omnibox,
             notifications::NotificationScheduleService* service,
             notifications::ClientOverview overview) {
            TipsAgentAndroid::OnGetClientOverview(profile, is_bottom_omnibox,
                                                  service, std::move(overview));
          },
          profile, is_bottom_omnibox, base::Unretained(service)));
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
