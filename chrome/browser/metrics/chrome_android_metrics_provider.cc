// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_android_metrics_provider.h"

#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "chrome/android/chrome_jni_headers/NotificationSystemStatusUtil_jni.h"
#include "chrome/browser/android/locale/locale_manager.h"
#include "chrome/browser/android/metrics/uma_session_stats.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace {

// Name of local state pref to persist the last |chrome::android::ActivityType|.
const char kLastActivityTypePref[] =
    "user_experience_metrics.last_activity_type";

base::Optional<chrome::android::ActivityType> GetActivityTypeFromLocalState(
    PrefService* local_state_) {
  auto value = local_state_->GetInteger(kLastActivityTypePref);
  if (value >= static_cast<int>(chrome::android::ActivityType::kTabbed) &&
      value <= static_cast<int>(chrome::android::ActivityType::kMaxValue)) {
    return static_cast<chrome::android::ActivityType>(value);
  }
  return base::nullopt;
}

void EmitActivityTypeHistograms(chrome::android::ActivityType type) {
  UMA_STABILITY_HISTOGRAM_ENUMERATION(
      "CustomTabs.Visible", chrome::android::GetCustomTabsVisibleValue(type));
  UMA_STABILITY_HISTOGRAM_ENUMERATION("Android.ChromeActivity.Type", type);
}

// Corresponds to APP_NOTIFICATIONS_STATUS_BOUNDARY in
// NotificationSystemStatusUtil.java
const int kAppNotificationStatusBoundary = 3;

void EmitAppNotificationStatusHistogram() {
  auto status = Java_NotificationSystemStatusUtil_getAppNotificationStatus(
      base::android::AttachCurrentThread());
  UMA_HISTOGRAM_ENUMERATION("Android.AppNotificationStatus", status,
                            kAppNotificationStatusBoundary);
}

}  // namespace

ChromeAndroidMetricsProvider::ChromeAndroidMetricsProvider(
    PrefService* local_state)
    : local_state_(local_state) {}

ChromeAndroidMetricsProvider::~ChromeAndroidMetricsProvider() {}

// static
void ChromeAndroidMetricsProvider::RegisterPrefs(PrefRegistrySimple* registry) {
  // Register with a default value of -1 which is not a valid enum.
  registry->RegisterIntegerPref(kLastActivityTypePref, -1);
}

void ChromeAndroidMetricsProvider::OnDidCreateMetricsLog() {
  auto type = chrome::android::GetActivityType();
  EmitActivityTypeHistograms(type);
  // Save the value off for reporting stability metrics.
  local_state_->SetInteger(kLastActivityTypePref, static_cast<int>(type));
}

void ChromeAndroidMetricsProvider::ProvidePreviousSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  auto activity_type = GetActivityTypeFromLocalState(local_state_);
  if (activity_type.has_value())
    EmitActivityTypeHistograms(activity_type.value());
}

void ChromeAndroidMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  UMA_HISTOGRAM_BOOLEAN("Android.MultiWindowMode.Active",
                        chrome::android::GetIsInMultiWindowModeValue());
  UmaSessionStats::GetInstance()->ProvideCurrentSessionData();
  EmitAppNotificationStatusHistogram();
  LocaleManager::RecordUserTypeMetrics();
}
