// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/scanning_enabled_provider.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"

namespace ash::quick_pair {

ScanningEnabledProvider::ScanningEnabledProvider(
    std::unique_ptr<BatterySaverActiveProvider> battery_saver_provider,
    std::unique_ptr<FastPairPrefEnabledProvider>
        fast_pair_pref_enabled_provider,
    std::unique_ptr<HardwareOffloadingSupportedProvider>
        hardware_offloading_provider,
    std::unique_ptr<PowerConnectedProvider> power_connected_provider)
    : battery_saver_provider_(std::move(battery_saver_provider)),
      fast_pair_pref_enabled_provider_(
          std::move(fast_pair_pref_enabled_provider)),
      hardware_offloading_provider_(std::move(hardware_offloading_provider)),
      power_connected_provider_(std::move(power_connected_provider)) {
  if (battery_saver_provider_) {
    battery_saver_provider_->SetCallback(base::BindRepeating(
        &ScanningEnabledProvider::UpdateEnabled, weak_factory_.GetWeakPtr()));
  }

  if (fast_pair_pref_enabled_provider_) {
    fast_pair_pref_enabled_provider_->SetCallback(base::BindRepeating(
        &ScanningEnabledProvider::UpdateEnabled, weak_factory_.GetWeakPtr()));
  }

  if (hardware_offloading_provider_) {
    hardware_offloading_provider_->SetCallback(base::BindRepeating(
        &ScanningEnabledProvider::UpdateEnabled, weak_factory_.GetWeakPtr()));
  }

  if (power_connected_provider_) {
    power_connected_provider_->SetCallback(base::BindRepeating(
        &ScanningEnabledProvider::UpdateEnabled, weak_factory_.GetWeakPtr()));
  }

  // Shell::Get() will never return a nullptr.
  Shell* shell = Shell::Get();
  CHECK(shell);
  PrefService* local_state = shell->local_state();

  // `local_state` may be null in unit tests.
  if (local_state) {
    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(local_state);
    pref_change_registrar_->Add(
        ash::prefs::kSoftwareScanningEnabled,
        base::BindRepeating(
            &ScanningEnabledProvider::OnSoftwareScanningStatusChanged,
            weak_factory_.GetWeakPtr()));
    OnSoftwareScanningStatusChanged();
  }
}

ScanningEnabledProvider::~ScanningEnabledProvider() = default;

void ScanningEnabledProvider::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  if (!registry) {
    return;
  }

  registry->RegisterIntegerPref(
      ash::prefs::kSoftwareScanningEnabled,
      static_cast<int>(SoftwareScanningStatus::kOnlyWhenCharging));
}

void ScanningEnabledProvider::OnSoftwareScanningStatusChanged() {
  PrefService* pref_service = pref_change_registrar_->prefs();
  CHECK(pref_service);

  int new_pref_val =
      pref_service->GetInteger(ash::prefs::kSoftwareScanningEnabled);
  CHECK(new_pref_val >= 0);
  CHECK(new_pref_val <= static_cast<int>(SoftwareScanningStatus::kMaxValue));

  SoftwareScanningStatus new_software_scanning_status =
      static_cast<SoftwareScanningStatus>(new_pref_val);

  if (new_software_scanning_status == software_scanning_status_) {
    return;
  }

  software_scanning_status_ = new_software_scanning_status;
  UpdateEnabled(/*subprovider_val=*/false);
}

bool ScanningEnabledProvider::IsSoftwareScanningStatusAlways() {
  return software_scanning_status_ == SoftwareScanningStatus::kAlways;
}

bool ScanningEnabledProvider::IsSoftwareScanningStatusWhenCharging() {
  return software_scanning_status_ == SoftwareScanningStatus::kOnlyWhenCharging;
}

bool ScanningEnabledProvider::IsHardwareOffloadingSupported() {
  return hardware_offloading_provider_ &&
         hardware_offloading_provider_->is_enabled();
}

bool ScanningEnabledProvider::IsFastPairPrefEnabled() {
  return fast_pair_pref_enabled_provider_ &&
         fast_pair_pref_enabled_provider_->is_enabled();
}

bool ScanningEnabledProvider::IsBatterySaverActive() {
  return battery_saver_provider_ && battery_saver_provider_->is_enabled();
}

bool ScanningEnabledProvider::IsPowerConnected() {
  return power_connected_provider_ && power_connected_provider_->is_enabled();
}

bool ScanningEnabledProvider::IsScanningEnabled() {
  if (IsHardwareOffloadingSupported()) {
    // Enable scanning based on Fast Pair boolean pref if hardware offloading is
    // supported.
    return IsFastPairPrefEnabled();
  }

  if (IsBatterySaverActive()) {
    return false;
  }

  if (IsSoftwareScanningStatusAlways()) {
    return true;
  }

  return IsSoftwareScanningStatusWhenCharging() && IsPowerConnected();
}

void ScanningEnabledProvider::UpdateEnabled(bool subprovider_val) {
  SetEnabledAndInvokeCallback(/*new_value=*/IsScanningEnabled());
}

}  // namespace ash::quick_pair
