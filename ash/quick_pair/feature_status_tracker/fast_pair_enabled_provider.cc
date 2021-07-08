// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/fast_pair_enabled_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/feature_status_tracker/base_enabled_provider.h"
#include "ash/quick_pair/feature_status_tracker/bluetooth_enabled_provider.h"
#include "base/bind.h"
#include "base/feature_list.h"

namespace ash {
namespace quick_pair {

FastPairEnabledProvider::FastPairEnabledProvider(
    std::unique_ptr<BluetoothEnabledProvider> bluetooth_enabled_provider,
    std::unique_ptr<GoogleApiKeyAvailabilityProvider>
        google_api_key_availability_provider)
    : bluetooth_enabled_provider_(std::move(bluetooth_enabled_provider)),
      google_api_key_availability_provider_(
          std::move(google_api_key_availability_provider)) {
  // If the flag isn't enabled or if the API keys aren't available,
  // Fast Pair will never be enabled so don't hook up any callbacks.
  if (features::IsFastPairEnabled() &&
      google_api_key_availability_provider_->is_enabled()) {
    bluetooth_enabled_provider_->SetCallback(base::BindRepeating(
        &FastPairEnabledProvider::OnSubProviderEnabledChanged,
        weak_factory_.GetWeakPtr()));

    SetEnabledAndInvokeCallback(AreSubProvidersEnabled());
  }
}

FastPairEnabledProvider::~FastPairEnabledProvider() = default;

bool FastPairEnabledProvider::AreSubProvidersEnabled() {
  return base::FeatureList::IsEnabled(features::kFastPair) &&
         google_api_key_availability_provider_->is_enabled() &&
         bluetooth_enabled_provider_->is_enabled();
}

void FastPairEnabledProvider::OnSubProviderEnabledChanged(bool) {
  SetEnabledAndInvokeCallback(AreSubProvidersEnabled());
}

}  // namespace quick_pair
}  // namespace ash
