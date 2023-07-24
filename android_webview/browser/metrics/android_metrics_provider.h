// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_ANDROID_METRICS_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_ANDROID_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace android_webview {

namespace prefs {
constexpr char kPrimaryCpuAbiBitnessPref[] =
    "android_system_info.primary_cpu_abi_bitness";
}

// AndroidMetricsProvider is responsible for logging information related to
// system-level information about the Android device as well as the process.
class AndroidMetricsProvider : public metrics::MetricsProvider {
 public:
  explicit AndroidMetricsProvider(PrefService* local_state)
      : local_state_(local_state) {}
  ~AndroidMetricsProvider() override = default;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  void OnDidCreateMetricsLog() override;
  void ProvidePreviousSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

  AndroidMetricsProvider(const AndroidMetricsProvider&) = delete;
  AndroidMetricsProvider& operator=(const AndroidMetricsProvider&) = delete;

  static void ResetGlobalStateForTesting();

 private:
  raw_ptr<PrefService> local_state_;

  static inline bool local_state_saved_ = false;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_ANDROID_METRICS_PROVIDER_H_
