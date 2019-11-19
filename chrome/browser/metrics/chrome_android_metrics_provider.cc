// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_android_metrics_provider.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/android/chrome_jni_headers/NotificationSystemStatusUtil_jni.h"
#include "chrome/browser/android/feature_utilities.h"
#include "chrome/browser/android/locale/locale_manager.h"
#include "chrome/browser/android/metrics/uma_session_stats.h"

namespace {

// TODO(crbug.com/1007308): Enable this by default after confirming the results.
const base::Feature kLogCustomTabStateOnLogStart = {
    "LogCustomTabStateOnLogStart", base::FEATURE_DISABLED_BY_DEFAULT};

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

ChromeAndroidMetricsProvider::ChromeAndroidMetricsProvider() {}

ChromeAndroidMetricsProvider::~ChromeAndroidMetricsProvider() {}

void ChromeAndroidMetricsProvider::OnDidCreateMetricsLog() {
  if (base::FeatureList::IsEnabled(kLogCustomTabStateOnLogStart)) {
    UMA_HISTOGRAM_ENUMERATION("CustomTabs.Visible",
                              chrome::android::GetCustomTabsVisibleValue(),
                              chrome::android::CUSTOM_TABS_VISIBILITY_MAX);
  }
}

void ChromeAndroidMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  if (!base::FeatureList::IsEnabled(kLogCustomTabStateOnLogStart)) {
    UMA_HISTOGRAM_ENUMERATION("CustomTabs.Visible",
                              chrome::android::GetCustomTabsVisibleValue(),
                              chrome::android::CUSTOM_TABS_VISIBILITY_MAX);
  }
  UMA_HISTOGRAM_BOOLEAN("Android.MultiWindowMode.Active",
                        chrome::android::GetIsInMultiWindowModeValue());
  UmaSessionStats::GetInstance()->ProvideCurrentSessionData();
  EmitAppNotificationStatusHistogram();
  LocaleManager::RecordUserTypeMetrics();
}
