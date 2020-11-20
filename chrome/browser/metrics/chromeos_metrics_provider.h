// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROMEOS_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_CHROMEOS_METRICS_PROVIDER_H_

#include <stdint.h>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/metrics/perf/profile_provider_chromeos.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_provider.h"

namespace arc {
struct ArcFeatures;
}

namespace metrics {
class CachedMetricsProfile;
class ChromeUserMetricsExtension;
}

enum class EnrollmentStatus;
class PrefRegistrySimple;

// Performs ChromeOS specific metrics logging.
class ChromeOSMetricsProvider : public metrics::MetricsProvider {
 public:
  explicit ChromeOSMetricsProvider(
      metrics::MetricsLogUploader::MetricServiceType service_type);
  ~ChromeOSMetricsProvider() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Records a crash.
  static void LogCrash(const std::string& crash_type);

  // Returns Enterprise Enrollment status.
  static EnrollmentStatus GetEnrollmentStatus();

  // Loads hardware class information. When this task is complete, |callback|
  // is run.
  void InitTaskGetFullHardwareClass(base::OnceClosure callback);

  // Retrieves ARC features using ArcFeaturesParser. When this task is complete,
  // |callback| is run.
  void InitTaskGetArcFeatures(base::OnceClosure callback);

  // metrics::MetricsProvider:
  void Init() override;
  void AsyncInit(base::OnceClosure done_callback) override;
  void OnDidCreateMetricsLog() override;
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;
  void ProvideStabilityMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  void ProvideAccessibilityMetrics();
  void ProvideSuggestedContentMetrics();

  // Update the number of users logged into a multi-profile session.
  // If the number of users change while the log is open, the call invalidates
  // the user count value.
  void UpdateMultiProfileUserCount(
      metrics::SystemProfileProto* system_profile_proto);

  // Sets the full hardware class, then calls the callback.
  void SetFullHardwareClass(base::OnceClosure callback,
                            std::string full_hardware_class);

  // Updates ARC-related system profile fields, then calls the callback.
  void OnArcFeaturesParsed(base::OnceClosure callback,
                           base::Optional<arc::ArcFeatures> features);

  // Called from the ProvideCurrentSessionData(...) to record UserType.
  void UpdateUserTypeUMA();

  // Writes info about the linked Android phone if there is one.
  void WriteLinkedAndroidPhoneProto(
      metrics::SystemProfileProto* system_profile_proto);

  // For collecting systemwide performance data via the UMA channel.
  std::unique_ptr<metrics::ProfileProvider> profile_provider_;

  // Use the first signed-in profile for profile-dependent metrics.
  std::unique_ptr<metrics::CachedMetricsProfile> cached_profile_;

  // Whether the user count was registered at the last log initialization.
  bool registered_user_count_at_log_initialization_;

  // The user count at the time that a log was last initialized. Contains a
  // valid value only if |registered_user_count_at_log_initialization_| is
  // true.
  uint64_t user_count_at_log_initialization_;

  // Hardware class (e.g., hardware qualification ID). This value identifies
  // the configured system components such as CPU, WiFi adapter, etc.
  std::string full_hardware_class_;

  // ARC release version obtained from build properties.
  base::Optional<std::string> arc_release_ = base::nullopt;

  base::WeakPtrFactory<ChromeOSMetricsProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeOSMetricsProvider);
};

#endif  // CHROME_BROWSER_METRICS_CHROMEOS_METRICS_PROVIDER_H_
