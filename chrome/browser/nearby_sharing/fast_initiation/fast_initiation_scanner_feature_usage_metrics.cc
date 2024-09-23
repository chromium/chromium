// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/fast_initiation/fast_initiation_scanner_feature_usage_metrics.h"

#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/fast_initiation/fast_initiation_scanner.h"
#include "chrome/browser/nearby_sharing/nearby_share_feature_status.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "components/prefs/pref_service.h"

namespace {
const char kFeatureName[] = "NearbyShareBackgroundScanning";
}  // namespace

FastInitiationScannerFeatureUsageMetrics::
    FastInitiationScannerFeatureUsageMetrics(PrefService* pref_service)
    : pref_service_(pref_service), feature_usage_metrics_(kFeatureName, this) {}

FastInitiationScannerFeatureUsageMetrics::
    ~FastInitiationScannerFeatureUsageMetrics() = default;

bool FastInitiationScannerFeatureUsageMetrics::IsEligible() const {
  // Because the adapter and hardware support status may not be available right
  // away, there is a possibility here that we'll log "not eligible" on a device
  // that has hardware support. It's okay if a device goes from "not eligible"
  // to "eligible", and edge cases around this are accounted for by SFUL.
  if (!bluetooth_adapter_)
    return false;

  return FastInitiationScanner::Factory::IsHardwareSupportAvailable(
      bluetooth_adapter_.get());
}

bool FastInitiationScannerFeatureUsageMetrics::IsEnabled() const {
  return IsEligible() &&
         GetNearbyShareEnabledState(pref_service_) !=
             NearbyShareEnabledState::kDisallowedByPolicy &&
         static_cast<nearby_share::mojom::FastInitiationNotificationState>(
             pref_service_->GetInteger(
                 prefs::
                     kNearbySharingFastInitiationNotificationStatePrefName)) ==
             nearby_share::mojom::FastInitiationNotificationState::kEnabled;
}

void FastInitiationScannerFeatureUsageMetrics::SetBluetoothAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = std::move(adapter);
}

void FastInitiationScannerFeatureUsageMetrics::RecordUsage(bool success) {
  feature_usage_metrics_.RecordUsage(success);
}
