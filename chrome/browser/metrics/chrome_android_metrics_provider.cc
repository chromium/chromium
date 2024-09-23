// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_android_metrics_provider.h"

#include <optional>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/android/customtabs/custom_tab_session_state_tracker.h"
#include "chrome/browser/android/metrics/uma_session_stats.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#include "components/metrics/android_metrics_helper.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "system_profile.pb.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/android/metrics/jni_headers/AppUpdateInfoUtils_jni.h"
#include "chrome/browser/notifications/jni_headers/NotificationSystemStatusUtil_jni.h"

namespace {

// Corresponds to APP_NOTIFICATIONS_STATUS_BOUNDARY in
// NotificationSystemStatusUtil.java
const int kAppNotificationStatusBoundary = 3;

void EmitAppNotificationStatusHistogram() {
  auto status = Java_NotificationSystemStatusUtil_getAppNotificationStatus(
      jni_zero::AttachCurrentThread());
  UMA_HISTOGRAM_ENUMERATION("Android.AppNotificationStatus", status,
                            kAppNotificationStatusBoundary);
}

void EmitMultipleUserProfilesHistogram() {
  const chrome::android::MultipleUserProfilesState
      multiple_user_profiles_state =
          chrome::android::GetMultipleUserProfilesState();
  base::UmaHistogramEnumeration("Android.MultipleUserProfilesState",
                                multiple_user_profiles_state);
}

void GetAppUpdateData() {
  Java_AppUpdateInfoUtils_emitToHistogram(jni_zero::AttachCurrentThread());
}

metrics::SystemProfileProto::OS::DarkModeState ToProtoDarkModeState(
    chrome::android::DarkModeState state) {
  switch (state) {
    case chrome::android::DarkModeState::kDarkModeSystem:
      return metrics::SystemProfileProto::OS::DARK_MODE_SYSTEM;
    case chrome::android::DarkModeState::kDarkModeApp:
      return metrics::SystemProfileProto::OS::DARK_MODE_APP;
    case chrome::android::DarkModeState::kLightModeSystem:
      return metrics::SystemProfileProto::OS::LIGHT_MODE_SYSTEM;
    case chrome::android::DarkModeState::kLightModeApp:
      return metrics::SystemProfileProto::OS::LIGHT_MODE_APP;
    case chrome::android::DarkModeState::kUnknown:
      return metrics::SystemProfileProto::OS::UNKNOWN;
  }
}

}  // namespace

ChromeAndroidMetricsProvider::ChromeAndroidMetricsProvider(
    PrefService* local_state)
    : local_state_(local_state) {}

ChromeAndroidMetricsProvider::~ChromeAndroidMetricsProvider() {}

// static
void ChromeAndroidMetricsProvider::RegisterPrefs(PrefRegistrySimple* registry) {
  chrome::android::RegisterActivityTypePrefs(registry);
  metrics::AndroidMetricsHelper::RegisterPrefs(registry);
}

void ChromeAndroidMetricsProvider::OnDidCreateMetricsLog() {
  const auto type = chrome::android::GetActivityType();

  // Determine and emit to histogram if AppUpdate is available.
  base::ThreadPool::PostTask(FROM_HERE, base::BindOnce(&GetAppUpdateData));

  // All records should be created with an activity type, even if no activity
  // type has yet been declared. If an activity type is declared before the UMA
  // record is closed, a second set of ActivityType histograms can be emitted.
  // The processing pipeline can handle multiple samples which mix undeclared
  // and a concrete activity type.
  chrome::android::EmitActivityTypeHistograms(type);

  // Save the current activity type to local state. If the browser is terminated
  // before the new metrics log record can be uploaded, Chrome may be able to
  // recover it for upload on restart.
  chrome::android::SaveActivityTypeToLocalState(local_state_, type);

  EmitMultipleUserProfilesHistogram();

  metrics::AndroidMetricsHelper::GetInstance()->EmitHistograms(
      local_state_,
      /*on_did_create_metrics_log=*/true);
}

void ChromeAndroidMetricsProvider::ProvidePreviousSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  auto activity_type =
      chrome::android::GetActivityTypeFromLocalState(local_state_);
  if (activity_type.has_value())
    chrome::android::EmitActivityTypeHistograms(activity_type.value());

  // Save whether multiple user profiles are present in Android. This is
  // unlikely to change across sessions.
  EmitMultipleUserProfilesHistogram();

  metrics::AndroidMetricsHelper::GetInstance()->EmitHistograms(
      local_state_,
      /*on_did_create_metrics_log=*/false);
}

void ChromeAndroidMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  UMA_HISTOGRAM_BOOLEAN("Android.MultiWindowMode.Active",
                        chrome::android::GetIsInMultiWindowModeValue());

  metrics::SystemProfileProto::OS* os_proto =
      uma_proto->mutable_system_profile()->mutable_os();

  os_proto->set_dark_mode_state(
      ToProtoDarkModeState(chrome::android::GetDarkModeState()));

  if (chrome::android::CustomTabSessionStateTracker::GetInstance()
          .HasCustomTabSessionState()) {
    uma_proto->mutable_custom_tab_session()->Swap(
        chrome::android::CustomTabSessionStateTracker::GetInstance()
            .GetSession()
            .get());
  }

  UmaSessionStats::GetInstance()->ProvideCurrentSessionData();
  EmitAppNotificationStatusHistogram();
}

// static
void ChromeAndroidMetricsProvider::ResetGlobalStateForTesting() {
  metrics::AndroidMetricsHelper::ResetGlobalStateForTesting();
}
