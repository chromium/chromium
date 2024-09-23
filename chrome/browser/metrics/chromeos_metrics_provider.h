// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROMEOS_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_CHROMEOS_METRICS_PROVIDER_H_

#include <stdint.h>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/metrics/perf/profile_provider_chromeos.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_provider.h"

namespace metrics {
class ChromeUserMetricsExtension;
}  // namespace metrics

class ChromeOSSystemProfileProvider;
enum class EnrollmentStatus;
class PrefRegistrySimple;

// Performs ChromeOS specific metrics logging.
class ChromeOSMetricsProvider : public metrics::MetricsProvider {
 public:
  ChromeOSMetricsProvider(
      metrics::MetricsLogUploader::MetricServiceType service_type,
      ChromeOSSystemProfileProvider* system_profile_provider);

  ChromeOSMetricsProvider(const ChromeOSMetricsProvider&) = delete;
  ChromeOSMetricsProvider& operator=(const ChromeOSMetricsProvider&) = delete;

  ~ChromeOSMetricsProvider() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Records a crash.
  static void LogCrash(const std::string& crash_type, int num_samples);

  // Returns Enterprise Enrollment status.
  static EnrollmentStatus GetEnrollmentStatus();

  // metrics::MetricsProvider:
  void Init() override;
  void OnDidCreateMetricsLog() override;
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;
  void ProvideStabilityMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
  void ProvideCurrentSessionUKMData() override;

 private:
  void ProvideAccessibilityMetrics();
  void ProvideSuggestedContentMetrics();
  void ProvideMetrics(metrics::SystemProfileProto* system_profile_proto,
                      bool should_include_arc_metrics);

  void SetTpmType(metrics::SystemProfileProto* system_profile_proto);

  // Called from the ProvideCurrentSessionData(...) to record UserType.
  bool UpdateUserTypeUMA();

  // For collecting systemwide performance data via the UMA channel.
  std::unique_ptr<metrics::ProfileProvider> profile_provider_;

  // Interface for providing the SystemProfile to metrics.
  raw_ptr<ChromeOSSystemProfileProvider> cros_system_profile_provider_;

  base::WeakPtrFactory<ChromeOSMetricsProvider> weak_ptr_factory_{this};
};

// Provides *histograms* to UMA. Due to the below bug, this cannot be part of
// |ChromeOSMetricsProvider|.
// TODO(crbug.com/40899764): Allow this to be part of the above class.
class ChromeOSHistogramMetricsProvider : public metrics::MetricsProvider {
 public:
  ChromeOSHistogramMetricsProvider();

  ChromeOSHistogramMetricsProvider(const ChromeOSHistogramMetricsProvider&) =
      delete;
  ChromeOSHistogramMetricsProvider& operator=(
      const ChromeOSHistogramMetricsProvider&) = delete;

  ~ChromeOSHistogramMetricsProvider() override;

  // metrics::MetricsProvider:
  bool ProvideHistograms() override;
};

#endif  // CHROME_BROWSER_METRICS_CHROMEOS_METRICS_PROVIDER_H_
