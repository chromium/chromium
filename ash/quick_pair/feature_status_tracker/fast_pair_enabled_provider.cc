// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/fast_pair_enabled_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/feature_status_tracker/base_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/bluetooth_enabled_provider.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "components/cross_device/logging/logging.h"

namespace ash::quick_pair {

FastPairEnabledProvider::FastPairEnabledProvider(
    std::unique_ptr<BluetoothEnabledProvider> bluetooth_enabled_provider,
    std::unique_ptr<FastPairPrefEnabledProvider>
        fast_pair_pref_enabled_provider,
    std::unique_ptr<LoggedInUserEnabledProvider>
        logged_in_user_enabled_provider,
    std::unique_ptr<ScreenStateEnabledProvider> screen_state_enabled_provider,
    std::unique_ptr<GoogleApiKeyAvailabilityProvider>
        google_api_key_availability_provider,
    std::unique_ptr<ScanningEnabledProvider> scanning_enabled_provider)
    : bluetooth_enabled_provider_(std::move(bluetooth_enabled_provider)),
      fast_pair_pref_enabled_provider_(
          std::move(fast_pair_pref_enabled_provider)),
      logged_in_user_enabled_provider_(
          std::move(logged_in_user_enabled_provider)),
      screen_state_enabled_provider_(std::move(screen_state_enabled_provider)),
      google_api_key_availability_provider_(
          std::move(google_api_key_availability_provider)) {
  // If the flag isn't enabled or if the API keys aren't available,
  // Fast Pair will never be enabled so don't hook up any callbacks.
  if (features::IsFastPairEnabled() && AreAPIKeysAvailable()) {
    if (bluetooth_enabled_provider_) {
      bluetooth_enabled_provider_->SetCallback(base::BindRepeating(
          &FastPairEnabledProvider::OnSubProviderEnabledChanged,
          weak_factory_.GetWeakPtr()));
    }

    if (fast_pair_pref_enabled_provider_) {
      fast_pair_pref_enabled_provider_->SetCallback(base::BindRepeating(
          &FastPairEnabledProvider::OnSubProviderEnabledChanged,
          weak_factory_.GetWeakPtr()));
    }

    if (logged_in_user_enabled_provider_) {
      logged_in_user_enabled_provider_->SetCallback(base::BindRepeating(
          &FastPairEnabledProvider::OnSubProviderEnabledChanged,
          weak_factory_.GetWeakPtr()));
    }

    if (screen_state_enabled_provider_) {
      screen_state_enabled_provider_->SetCallback(base::BindRepeating(
          &FastPairEnabledProvider::OnSubProviderEnabledChanged,
          weak_factory_.GetWeakPtr()));
    }

    SetEnabledAndInvokeCallback(AreSubProvidersEnabled());
  }
}

FastPairEnabledProvider::~FastPairEnabledProvider() = default;

bool FastPairEnabledProvider::IsBluetoothEnabled() {
  return bluetooth_enabled_provider_ &&
         bluetooth_enabled_provider_->is_enabled();
}

bool FastPairEnabledProvider::AreAPIKeysAvailable() {
  return google_api_key_availability_provider_ &&
         google_api_key_availability_provider_->is_enabled();
}

bool FastPairEnabledProvider::IsFastPairPrefEnabled() {
  return fast_pair_pref_enabled_provider_ &&
         fast_pair_pref_enabled_provider_->is_enabled();
}

bool FastPairEnabledProvider::IsUserLoggedIn() {
  return logged_in_user_enabled_provider_ &&
         logged_in_user_enabled_provider_->is_enabled();
}

bool FastPairEnabledProvider::IsDisplayScreenOn() {
  return screen_state_enabled_provider_ &&
         screen_state_enabled_provider_->is_enabled();
}

bool FastPairEnabledProvider::IsScanningEnabled() {
  return scanning_enabled_provider_ && scanning_enabled_provider_->is_enabled();
}

bool FastPairEnabledProvider::AreSubProvidersEnabled() {
  CD_LOG(INFO, Feature::FP)
      << __func__
      << ": Flag:" << base::FeatureList::IsEnabled(features::kFastPair)
      << " Policy Pref:" << IsFastPairPrefEnabled()
      << " Google API Key:" << AreAPIKeysAvailable()
      << " Logged in User:" << IsUserLoggedIn()
      << " Screen State:" << IsDisplayScreenOn()
      << " Bluetooth:" << IsBluetoothEnabled();

  return base::FeatureList::IsEnabled(features::kFastPair) &&
         IsFastPairPrefEnabled() && AreAPIKeysAvailable() && IsUserLoggedIn() &&
         IsDisplayScreenOn() && IsBluetoothEnabled();
}

void FastPairEnabledProvider::OnSubProviderEnabledChanged(bool) {
  SetEnabledAndInvokeCallback(AreSubProvidersEnabled());
}

}  // namespace ash::quick_pair
