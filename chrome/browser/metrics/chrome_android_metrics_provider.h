// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_ANDROID_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_CHROME_ANDROID_METRICS_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/metrics/metrics_provider.h"

namespace metrics {
class ChromeUserMetricsExtension;
}

class PrefService;
class PrefRegistrySimple;

// ChromeAndroidMetricsProvider provides Chrome-for-Android-specific stability
// metrics. Android-specific metrics which apply to lower layers should be
// implemented in metrics::AndroidMetricsProvider.
class ChromeAndroidMetricsProvider : public metrics::MetricsProvider {
 public:
  explicit ChromeAndroidMetricsProvider(PrefService* local_state);

  ChromeAndroidMetricsProvider(const ChromeAndroidMetricsProvider&) = delete;
  ChromeAndroidMetricsProvider& operator=(const ChromeAndroidMetricsProvider&) =
      delete;

  ~ChromeAndroidMetricsProvider() override;

  // Registers local state prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // metrics::MetricsProvider:
  void AsyncInit(base::OnceClosure done_callback) override;
  void OnDidCreateMetricsLog() override;
  void ProvidePreviousSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
  void ProvideSystemProfileMetrics(metrics::SystemProfileProto* proto) override;

  static void ResetGlobalStateForTesting();

 protected:
  // Returns the full hardware class of the Android device. Virtual for testing.
  virtual const std::string& GetHardwareClass() const;

 private:
  void OnAppUpdateCheckComplete();

  raw_ptr<PrefService> local_state_;
  // Stores the full hardware class of the Android device.
  std::string hardware_class_;

  bool app_update_check_in_progress_ = false;

  // The completion time of the last app update check during this run of
  // Chrome. It is a null TimeTicks if no check has completed yet during this
  // run (regardless of prior runs). While an update check is in progress, this
  // retains the completion time of the previous check (or null if none).
  base::TimeTicks last_app_update_check_time_;

  base::WeakPtrFactory<ChromeAndroidMetricsProvider> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_METRICS_CHROME_ANDROID_METRICS_PROVIDER_H_
