// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/background_sync_controller_impl.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/background_sync_context.h"
#include "content/public/browser/background_sync_controller.h"
#include "content/public/browser/background_sync_parameters.h"
#include "content/public/browser/background_sync_registration.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/background_sync_launcher_android.h"
#endif

// static
const char BackgroundSyncControllerImpl::kFieldTrialName[] = "BackgroundSync";
const char BackgroundSyncControllerImpl::kDisabledParameterName[] = "disabled";
#if defined(OS_ANDROID)
const char BackgroundSyncControllerImpl::kRelyOnAndroidNetworkDetection[] =
    "rely_on_android_network_detection";
#endif
const char BackgroundSyncControllerImpl::kKeepBrowserAwakeParameterName[] =
    "keep_browser_awake_till_events_complete";
const char BackgroundSyncControllerImpl::kMaxAttemptsParameterName[] =
    "max_sync_attempts";
const char BackgroundSyncControllerImpl::
    kMaxAttemptsWithNotificationPermissionParameterName[] =
        "max_sync_attempts_with_notification_permission";
const char BackgroundSyncControllerImpl::kInitialRetryParameterName[] =
    "initial_retry_delay_sec";
const char BackgroundSyncControllerImpl::kRetryDelayFactorParameterName[] =
    "retry_delay_factor";
const char BackgroundSyncControllerImpl::kMinSyncRecoveryTimeName[] =
    "min_recovery_time_sec";
const char BackgroundSyncControllerImpl::kMaxSyncEventDurationName[] =
    "max_sync_event_duration_sec";
const char BackgroundSyncControllerImpl::kMinPeriodicSyncEventsInterval[] =
    "min_periodic_sync_events_interval_sec";

BackgroundSyncControllerImpl::BackgroundSyncControllerImpl(Profile* profile)
    : SiteEngagementObserver(SiteEngagementService::Get(profile)),
      profile_(profile),
      site_engagement_service_(SiteEngagementService::Get(profile)),
      background_sync_metrics_(
          ukm::UkmBackgroundRecorderFactory::GetForProfile(profile_)) {
  DCHECK(profile_);
  DCHECK(site_engagement_service_);
}

BackgroundSyncControllerImpl::~BackgroundSyncControllerImpl() = default;

void BackgroundSyncControllerImpl::OnEngagementEvent(
    content::WebContents* web_contents,
    const GURL& url,
    double score,
    SiteEngagementService::EngagementType engagement_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (score == 0.0)
    return;

  auto origin = url::Origin::Create(url);
  auto iter = suspended_periodic_sync_origins_.find(origin);
  if (iter == suspended_periodic_sync_origins_.end())
    return;

  suspended_periodic_sync_origins_.erase(iter);

  auto* storage_partition = content::BrowserContext::GetStoragePartitionForSite(
      profile_, url, /* can_create= */ false);
  if (!storage_partition)
    return;

  auto* background_sync_context = storage_partition->GetBackgroundSyncContext();
  if (!background_sync_context)
    return;

  background_sync_context->RevivePeriodicBackgroundSyncRegistrations(
      std::move(origin));
}

void BackgroundSyncControllerImpl::GetParameterOverrides(
    content::BackgroundSyncParameters* parameters) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if defined(OS_ANDROID)
  if (BackgroundSyncLauncherAndroid::ShouldDisableBackgroundSync()) {
    parameters->disable = true;
  }
#endif

  std::map<std::string, std::string> field_params;
  if (!variations::GetVariationParams(kFieldTrialName, &field_params))
    return;

  if (base::LowerCaseEqualsASCII(field_params[kDisabledParameterName],
                                 "true")) {
    parameters->disable = true;
  }

  if (base::LowerCaseEqualsASCII(field_params[kKeepBrowserAwakeParameterName],
                                 "true")) {
    parameters->keep_browser_awake_till_events_complete = true;
  }

  if (base::Contains(field_params,
                     kMaxAttemptsWithNotificationPermissionParameterName)) {
    int max_attempts;
    if (base::StringToInt(
            field_params[kMaxAttemptsWithNotificationPermissionParameterName],
            &max_attempts)) {
      parameters->max_sync_attempts_with_notification_permission = max_attempts;
    }
  }

  if (base::Contains(field_params, kMaxAttemptsParameterName)) {
    int max_attempts;
    if (base::StringToInt(field_params[kMaxAttemptsParameterName],
                          &max_attempts)) {
      parameters->max_sync_attempts = max_attempts;
    }
  }

  if (base::Contains(field_params, kInitialRetryParameterName)) {
    int initial_retry_delay_sec;
    if (base::StringToInt(field_params[kInitialRetryParameterName],
                          &initial_retry_delay_sec)) {
      parameters->initial_retry_delay =
          base::TimeDelta::FromSeconds(initial_retry_delay_sec);
    }
  }

  if (base::Contains(field_params, kRetryDelayFactorParameterName)) {
    int retry_delay_factor;
    if (base::StringToInt(field_params[kRetryDelayFactorParameterName],
                          &retry_delay_factor)) {
      parameters->retry_delay_factor = retry_delay_factor;
    }
  }

  if (base::Contains(field_params, kMinSyncRecoveryTimeName)) {
    int min_sync_recovery_time_sec;
    if (base::StringToInt(field_params[kMinSyncRecoveryTimeName],
                          &min_sync_recovery_time_sec)) {
      parameters->min_sync_recovery_time =
          base::TimeDelta::FromSeconds(min_sync_recovery_time_sec);
    }
  }

  if (base::Contains(field_params, kMaxSyncEventDurationName)) {
    int max_sync_event_duration_sec;
    if (base::StringToInt(field_params[kMaxSyncEventDurationName],
                          &max_sync_event_duration_sec)) {
      parameters->max_sync_event_duration =
          base::TimeDelta::FromSeconds(max_sync_event_duration_sec);
    }
  }

  if (base::Contains(field_params, kMinPeriodicSyncEventsInterval)) {
    int min_periodic_sync_events_interval_sec;
    if (base::StringToInt(field_params[kMinPeriodicSyncEventsInterval],
                          &min_periodic_sync_events_interval_sec)) {
      parameters->min_periodic_sync_events_interval =
          base::TimeDelta::FromSeconds(min_periodic_sync_events_interval_sec);
    }
  }

  return;
}

void BackgroundSyncControllerImpl::NotifyOneShotBackgroundSyncRegistered(
    const url::Origin& origin,
    bool can_fire,
    bool is_reregistered) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  background_sync_metrics_.MaybeRecordOneShotSyncRegistrationEvent(
      origin, can_fire, is_reregistered);
}

void BackgroundSyncControllerImpl::NotifyPeriodicBackgroundSyncRegistered(
    const url::Origin& origin,
    int min_interval,
    bool is_reregistered) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  background_sync_metrics_.MaybeRecordPeriodicSyncRegistrationEvent(
      origin, min_interval, is_reregistered);
}

void BackgroundSyncControllerImpl::NotifyOneShotBackgroundSyncCompleted(
    const url::Origin& origin,
    blink::ServiceWorkerStatusCode status_code,
    int num_attempts,
    int max_attempts) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  background_sync_metrics_.MaybeRecordOneShotSyncCompletionEvent(
      origin, status_code, num_attempts, max_attempts);
}

void BackgroundSyncControllerImpl::NotifyPeriodicBackgroundSyncCompleted(
    const url::Origin& origin,
    blink::ServiceWorkerStatusCode status_code,
    int num_attempts,
    int max_attempts) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  background_sync_metrics_.MaybeRecordPeriodicSyncEventCompletion(
      origin, status_code, num_attempts, max_attempts);
}

void BackgroundSyncControllerImpl::ScheduleBrowserWakeUpWithDelay(
    blink::mojom::BackgroundSyncType sync_type,
    base::TimeDelta delay) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (profile_->IsOffTheRecord())
    return;

#if defined(OS_ANDROID)
  BackgroundSyncLauncherAndroid::ScheduleBrowserWakeUpWithDelay(sync_type,
                                                                delay);
#endif
}

void BackgroundSyncControllerImpl::CancelBrowserWakeup(
    blink::mojom::BackgroundSyncType sync_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (profile_->IsOffTheRecord())
    return;

#if defined(OS_ANDROID)
  BackgroundSyncLauncherAndroid::CancelBrowserWakeup(sync_type);
#endif
}

int BackgroundSyncControllerImpl::GetSiteEngagementPenalty(const GURL& url) {
  blink::mojom::EngagementLevel engagement_level =
      site_engagement_service_->GetEngagementLevel(url);
  if (engagement_level == blink::mojom::EngagementLevel::NONE) {
    suspended_periodic_sync_origins_.insert(
        url::Origin::Create(url.GetOrigin()));
  }

  switch (engagement_level) {
    case blink::mojom::EngagementLevel::NONE:
      // Suspend registration until site_engagement improves.
      return kEngagementLevelNonePenalty;
    case blink::mojom::EngagementLevel::MINIMAL:
      return kEngagementLevelMinimalPenalty;
    case blink::mojom::EngagementLevel::LOW:
    case blink::mojom::EngagementLevel::MEDIUM:
      return kEngagementLevelLowOrMediumPenalty;
    case blink::mojom::EngagementLevel::HIGH:
    case blink::mojom::EngagementLevel::MAX:
      // Very few sites reach max engagement level.
      return kEngagementLevelHighOrMaxPenalty;
  }

  NOTREACHED();
  return kEngagementLevelNonePenalty;
}

base::TimeDelta BackgroundSyncControllerImpl::SnapToMaxOriginFrequency(
    int64_t min_interval,
    int64_t min_gap_for_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK_GE(min_gap_for_origin, 0);
  DCHECK_GE(min_interval, 0);

  if (min_interval < min_gap_for_origin)
    return base::TimeDelta::FromMilliseconds(min_gap_for_origin);
  if (min_interval % min_gap_for_origin == 0)
    return base::TimeDelta::FromMilliseconds(min_interval);
  return base::TimeDelta::FromMilliseconds(
      (min_interval / min_gap_for_origin + 1) * min_gap_for_origin);
}

base::TimeDelta BackgroundSyncControllerImpl::ApplyMinGapForOrigin(
    base::TimeDelta delay,
    base::TimeDelta time_till_next_scheduled_event_for_origin,
    base::TimeDelta min_gap_for_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (time_till_next_scheduled_event_for_origin.is_max())
    return delay;

  if (delay <= time_till_next_scheduled_event_for_origin - min_gap_for_origin)
    return delay;

  if (delay <= time_till_next_scheduled_event_for_origin)
    return time_till_next_scheduled_event_for_origin;

  if (delay <= time_till_next_scheduled_event_for_origin + min_gap_for_origin)
    return time_till_next_scheduled_event_for_origin + min_gap_for_origin;

  return delay;
}

base::TimeDelta BackgroundSyncControllerImpl::GetNextEventDelay(
    const content::BackgroundSyncRegistration& registration,
    content::BackgroundSyncParameters* parameters,
    base::TimeDelta time_till_soonest_scheduled_event_for_origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(parameters);

  int num_attempts = registration.num_attempts();

  if (!num_attempts) {
    // First attempt.
    switch (registration.sync_type()) {
      case blink::mojom::BackgroundSyncType::ONE_SHOT:
        return base::TimeDelta();
      case blink::mojom::BackgroundSyncType::PERIODIC:
        int site_engagement_factor =
            GetSiteEngagementPenalty(registration.origin().GetURL());
        if (!site_engagement_factor)
          return base::TimeDelta::Max();

        int64_t effective_gap_ms =
            site_engagement_factor *
            parameters->min_periodic_sync_events_interval.InMilliseconds();
        return ApplyMinGapForOrigin(
            SnapToMaxOriginFrequency(registration.options()->min_interval,
                                     effective_gap_ms),
            time_till_soonest_scheduled_event_for_origin,
            parameters->min_periodic_sync_events_interval);
    }
  }

  // After a sync event has been fired.
  DCHECK_LT(num_attempts, parameters->max_sync_attempts);
  return parameters->initial_retry_delay *
         pow(parameters->retry_delay_factor, num_attempts - 1);
}

std::unique_ptr<content::BackgroundSyncController::BackgroundSyncEventKeepAlive>
BackgroundSyncControllerImpl::CreateBackgroundSyncEventKeepAlive() {
#if !defined(OS_ANDROID)
  if (!KeepAliveRegistry::GetInstance()->IsShuttingDown())
    return std::make_unique<BackgroundSyncEventKeepAliveImpl>();
#endif
  return nullptr;
}

#if !defined(OS_ANDROID)
BackgroundSyncControllerImpl::BackgroundSyncEventKeepAliveImpl::
    BackgroundSyncEventKeepAliveImpl() {
  keepalive_ = std::unique_ptr<ScopedKeepAlive,
                               content::BrowserThread::DeleteOnUIThread>(
      new ScopedKeepAlive(KeepAliveOrigin::BACKGROUND_SYNC,
                          KeepAliveRestartOption::DISABLED));
}

BackgroundSyncControllerImpl::BackgroundSyncEventKeepAliveImpl::
    ~BackgroundSyncEventKeepAliveImpl() = default;
#endif

void BackgroundSyncControllerImpl::NoteSuspendedPeriodicSyncOrigins(
    std::set<url::Origin> suspended_origins) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (auto& origin : suspended_origins)
    suspended_periodic_sync_origins_.insert(std::move(origin));
}
