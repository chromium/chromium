// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker_impl.h"

#include <memory>

#include "ash/quick_pair/feature_status_tracker/battery_saver_active_provider.h"
#include "ash/quick_pair/feature_status_tracker/bluetooth_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/fast_pair_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/fast_pair_pref_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/google_api_key_availability_provider.h"
#include "ash/quick_pair/feature_status_tracker/hardware_offloading_supported_provider.h"
#include "ash/quick_pair/feature_status_tracker/logged_in_user_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/power_connected_provider.h"
#include "ash/quick_pair/feature_status_tracker/screen_state_enabled_provider.h"
#include "base/functional/bind.h"

namespace ash {
namespace quick_pair {

FeatureStatusTrackerImpl::FeatureStatusTrackerImpl()
    : fast_pair_enabled_provider_(std::make_unique<FastPairEnabledProvider>(
          std::make_unique<BluetoothEnabledProvider>(),
          std::make_unique<FastPairPrefEnabledProvider>(),
          std::make_unique<LoggedInUserEnabledProvider>(),
          std::make_unique<ScreenStateEnabledProvider>(),
          std::make_unique<GoogleApiKeyAvailabilityProvider>(),
          std::make_unique<ScanningEnabledProvider>(
              std::make_unique<BatterySaverActiveProvider>(),
              std::make_unique<FastPairPrefEnabledProvider>(),
              std::make_unique<HardwareOffloadingSupportedProvider>(),
              std::make_unique<PowerConnectedProvider>()))) {
  fast_pair_enabled_provider_->SetCallback(
      base::BindRepeating(&FeatureStatusTrackerImpl::OnFastPairEnabledChanged,
                          weak_factory_.GetWeakPtr()));
}

FeatureStatusTrackerImpl::~FeatureStatusTrackerImpl() = default;

void FeatureStatusTrackerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FeatureStatusTrackerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FeatureStatusTrackerImpl::IsFastPairEnabled() {
  return fast_pair_enabled_provider_->is_enabled();
}

void FeatureStatusTrackerImpl::OnFastPairEnabledChanged(bool is_enabled) {
  for (auto& observer : observers_) {
    observer.OnFastPairEnabledChanged(is_enabled);
  }
}

}  // namespace quick_pair
}  // namespace ash
