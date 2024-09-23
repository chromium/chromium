// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_SCANNING_ENABLED_PROVIDER_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_SCANNING_ENABLED_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"

#include "ash/quick_pair/feature_status_tracker/base_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/battery_saver_active_provider.h"
#include "ash/quick_pair/feature_status_tracker/fast_pair_pref_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/hardware_offloading_supported_provider.h"
#include "ash/quick_pair/feature_status_tracker/power_connected_provider.h"

namespace ash::quick_pair {

class ScanningEnabledProvider : public BaseEnabledProvider {
 public:
  // Options for the kSoftwareScanningEnabled pref to be set to.
  enum class SoftwareScanningStatus {
    kNever = 0,
    kAlways = 1,
    kOnlyWhenCharging = 2,
    kMaxValue = kOnlyWhenCharging,
  };

  explicit ScanningEnabledProvider(
      std::unique_ptr<BatterySaverActiveProvider> battery_saver_provider,
      std::unique_ptr<FastPairPrefEnabledProvider>
          fast_pair_pref_enabled_provider,
      std::unique_ptr<HardwareOffloadingSupportedProvider>
          hardware_offloading_provider,
      std::unique_ptr<PowerConnectedProvider> power_connected_provider);
  ~ScanningEnabledProvider() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

 private:
  friend class ScanningEnabledProviderTestBase;

  void OnSoftwareScanningStatusChanged();

  bool IsSoftwareScanningStatusAlways();
  bool IsSoftwareScanningStatusWhenCharging();
  bool IsHardwareOffloadingSupported();
  bool IsFastPairPrefEnabled();
  bool IsBatterySaverActive();
  bool IsPowerConnected();
  bool IsScanningEnabled();

  void UpdateEnabled(bool subprovider_val);

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  SoftwareScanningStatus software_scanning_status_ =
      SoftwareScanningStatus::kNever;

  std::unique_ptr<BatterySaverActiveProvider> battery_saver_provider_;
  std::unique_ptr<FastPairPrefEnabledProvider> fast_pair_pref_enabled_provider_;
  std::unique_ptr<HardwareOffloadingSupportedProvider>
      hardware_offloading_provider_;
  std::unique_ptr<PowerConnectedProvider> power_connected_provider_;
  base::WeakPtrFactory<ScanningEnabledProvider> weak_factory_{this};
};

}  // namespace ash::quick_pair

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_SCANNING_ENABLED_PROVIDER_H_
