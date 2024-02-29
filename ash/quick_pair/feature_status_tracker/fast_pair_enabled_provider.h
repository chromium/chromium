// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_FAST_PAIR_ENABLED_PROVIDER_H_
#define ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_FAST_PAIR_ENABLED_PROVIDER_H_

#include "ash/quick_pair/feature_status_tracker/base_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/bluetooth_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/fast_pair_pref_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/google_api_key_availability_provider.h"
#include "ash/quick_pair/feature_status_tracker/logged_in_user_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/scanning_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/screen_state_enabled_provider.h"
#include "base/memory/weak_ptr.h"

namespace ash::quick_pair {

// Exposes an |is_enabled()| method and callback to query and observe when the
// Fast Pair feature is enabled/disabled.
class FastPairEnabledProvider : public BaseEnabledProvider {
 public:
  explicit FastPairEnabledProvider(
      std::unique_ptr<BluetoothEnabledProvider> bluetooth_enabled_provider,
      std::unique_ptr<FastPairPrefEnabledProvider>
          fast_pair_pref_enabled_provider,
      std::unique_ptr<LoggedInUserEnabledProvider>
          logged_in_user_enabled_provider,
      std::unique_ptr<ScreenStateEnabledProvider> screen_state_enabled_provider,
      std::unique_ptr<GoogleApiKeyAvailabilityProvider>
          google_api_key_availability_provider,
      std::unique_ptr<ScanningEnabledProvider> scanning_enabled_provider);
  ~FastPairEnabledProvider() override;

 private:
  bool IsBluetoothEnabled();
  bool AreAPIKeysAvailable();
  bool IsFastPairPrefEnabled();
  bool IsUserLoggedIn();
  bool IsDisplayScreenOn();
  bool IsScanningEnabled();

  bool AreSubProvidersEnabled();
  void OnSubProviderEnabledChanged(bool);

  std::unique_ptr<BluetoothEnabledProvider> bluetooth_enabled_provider_;
  std::unique_ptr<FastPairPrefEnabledProvider> fast_pair_pref_enabled_provider_;
  std::unique_ptr<LoggedInUserEnabledProvider> logged_in_user_enabled_provider_;
  std::unique_ptr<ScreenStateEnabledProvider> screen_state_enabled_provider_;
  std::unique_ptr<GoogleApiKeyAvailabilityProvider>
      google_api_key_availability_provider_;
  std::unique_ptr<ScanningEnabledProvider> scanning_enabled_provider_;
  base::WeakPtrFactory<FastPairEnabledProvider> weak_factory_{this};
};

}  // namespace ash::quick_pair

#endif  // ASH_QUICK_PAIR_FEATURE_STATUS_TRACKER_FAST_PAIR_ENABLED_PROVIDER_H_
