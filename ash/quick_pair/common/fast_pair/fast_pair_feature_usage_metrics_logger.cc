// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/fast_pair/fast_pair_feature_usage_metrics_logger.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace {

const char kFastPairUmaFeatureName[] = "FastPair";

}  // namespace

namespace ash {
namespace quick_pair {

FastPairFeatureUsageMetricsLogger::FastPairFeatureUsageMetricsLogger()
    : feature_usage_metrics_(kFastPairUmaFeatureName, this) {
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&FastPairFeatureUsageMetricsLogger::OnGetAdapter,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairFeatureUsageMetricsLogger::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
}

FastPairFeatureUsageMetricsLogger::~FastPairFeatureUsageMetricsLogger() =
    default;

void FastPairFeatureUsageMetricsLogger::RecordUsage(bool success) {
  feature_usage_metrics_.RecordUsage(success);
}

bool FastPairFeatureUsageMetricsLogger::IsEligible() const {
  // IsEligible reflects the hardware filtering support on the Chromebook.
  // Devices that do not have hardware filtering support are not eligible for
  // Fast Pair.
  return bluetooth_adapter_.get() && bluetooth_adapter_->IsPresent() &&
         bluetooth_adapter_
                 ->GetLowEnergyScanSessionHardwareOffloadingStatus() ==
             device::BluetoothAdapter::
                 LowEnergyScanSessionHardwareOffloadingStatus::kSupported;
}

std::optional<bool> FastPairFeatureUsageMetricsLogger::IsAccessible() const {
  if (!IsEligible())
    return false;

  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();

  if (!pref_service)
    return false;

  // |PrefService::IsManagedPreference()| determines if a feature is set by
  // enterprise policy. If Fast Pair is not controlled by enterprise policy,
  // then the feature is accessible.
  if (!pref_service->IsManagedPreference(ash::prefs::kFastPairEnabled))
    return true;

  // If the feature is controlled by enterprise policy, then we match
  // |IsAccessible| to whether the feature is enabled or disabled by policy.
  return pref_service->GetBoolean(ash::prefs::kFastPairEnabled);
}

bool FastPairFeatureUsageMetricsLogger::IsEnabled() const {
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  return IsEligible() && pref_service &&
         pref_service->GetBoolean(ash::prefs::kFastPairEnabled);
}

}  // namespace quick_pair
}  // namespace ash
