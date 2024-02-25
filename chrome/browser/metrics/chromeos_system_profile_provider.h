// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROMEOS_SYSTEM_PROFILE_PROVIDER_H_
#define CHROME_BROWSER_METRICS_CHROMEOS_SYSTEM_PROFILE_PROVIDER_H_

#include <memory>

#include "ash/components/arc/arc_features_parser.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/metrics/cached_metrics_profile.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "components/metrics/metrics_provider.h"

// Provides the SystemProfile from ChromeOS to different metrics.
class ChromeOSSystemProfileProvider : public metrics::MetricsProvider {
 public:
  ChromeOSSystemProfileProvider();

  ~ChromeOSSystemProfileProvider() override;

  ChromeOSSystemProfileProvider(const ChromeOSSystemProfileProvider&) = delete;
  ChromeOSSystemProfileProvider& operator=(
      const ChromeOSSystemProfileProvider&) = delete;

  // metrics::MetricsProvider:
  void OnDidCreateMetricsLog() override;
  void AsyncInit(base::OnceClosure callback) override;
  void ProvideSystemProfileMetrics(metrics::SystemProfileProto* proto) override;

 private:
  void WriteLinkedAndroidPhoneProto(
      metrics::SystemProfileProto* system_profile_proto);

  // Update the number of users logged into a multi-profile session.
  // If the number of users change while the log is open, the call invalidates
  // the user count value.
  void UpdateMultiProfileUserCount(
      metrics::SystemProfileProto* system_profile_proto);

  void WriteDemoModeDimensionMetrics(
      metrics::SystemProfileProto* system_profile_proto);

  // Loads hardware class information. When this task is complete, |callback|
  // is run.
  void InitTaskGetFullHardwareClass(base::OnceClosure callback);

  // Retrieves ARC features using ArcFeaturesParser. When this task is complete,
  // |callback| is run.
  void InitTaskGetArcFeatures(base::OnceClosure callback);

  // Retrieves TPM firmware version using TpmManagerClient. When this task is
  // complete, |callback| is run.
  void InitTaskGetTpmFirmwareVersion(base::OnceClosure callback);

  // Loads the cellular device variant. When this task is complete, |callback|
  // is run.
  void InitTaskGetCellularDeviceVariant(base::OnceClosure callback);

  // Invoked when StatisticsProvider finishes loading to read the full hardware
  // class from StatisticsProvider and calls the callback.
  void OnMachineStatisticsLoaded(base::OnceClosure callback);

  // Updates ARC-related system profile fields, then calls the callback.
  void OnArcFeaturesParsed(base::OnceClosure callback,
                           std::optional<arc::ArcFeatures> features);

  // Sets the TPM RW firmware version, then calls the callback.
  void OnTpmManagerGetRwVersionInfo(
      base::OnceClosure callback,
      const tpm_manager::GetVersionInfoReply& reply);

  // Sets the cellular device variant, then calls the callback.
  void SetCellularDeviceVariant(base::OnceClosure callback,
                                std::string cellular_device_variant);

  // Use the first signed-in profile for profile-dependent metrics.
  std::unique_ptr<metrics::CachedMetricsProfile> cached_profile_;

  // Whether the user count was registered at the last log initialization.
  bool registered_user_count_at_log_initialization_ = false;

  // The user count at the time that a log was last initialized. Contains a
  // valid value only if |registered_user_count_at_log_initialization_| is
  // true.
  uint64_t user_count_at_log_initialization_ = 0;

  // Hardware class (e.g., hardware qualification ID). This value identifies
  // the configured system components such as CPU, WiFi adapter, etc.
  std::string full_hardware_class_;

  // Cellular device variant for Chrome OS devices with cellular support.
  std::string cellular_device_variant_;

  // ARC release version obtained from build properties.
  std::optional<std::string> arc_release_;

  // The RW firmware version of the TPM (go/trusted-platform-module).
  std::optional<std::string> tpm_rw_firmware_version_;

  base::WeakPtrFactory<ChromeOSSystemProfileProvider> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_METRICS_CHROMEOS_SYSTEM_PROFILE_PROVIDER_H_
