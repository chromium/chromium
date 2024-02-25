// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/smart_lock/smart_lock_feature_usage_metrics.h"

#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

namespace ash {

namespace {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;

const char kFeatureName[] = "SmartLock";
}  // namespace

SmartLockFeatureUsageMetrics::SmartLockFeatureUsageMetrics(
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
    : multidevice_setup_client_(multidevice_setup_client),
      feature_usage_metrics_(kFeatureName, this) {}

SmartLockFeatureUsageMetrics::~SmartLockFeatureUsageMetrics() = default;

void SmartLockFeatureUsageMetrics::RecordUsage(bool success) {
  feature_usage_metrics_.RecordUsage(success);
}

bool SmartLockFeatureUsageMetrics::IsEligible() const {
  switch (multidevice_setup_client_->GetFeatureState(Feature::kSmartLock)) {
    case FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts:
      [[fallthrough]];
    case FeatureState::kUnavailableNoVerifiedHost_ClientNotReady:
      [[fallthrough]];
    case FeatureState::kNotSupportedByChromebook:
      [[fallthrough]];
    case FeatureState::kNotSupportedByPhone:
      return false;

    case FeatureState::
        kUnavailableNoVerifiedHost_HostExistsButNotSetAndVerified:
      [[fallthrough]];
    case FeatureState::kProhibitedByPolicy:
      [[fallthrough]];
    case FeatureState::kDisabledByUser:
      [[fallthrough]];
    case FeatureState::kEnabledByUser:
      [[fallthrough]];
    case FeatureState::kUnavailableInsufficientSecurity:
      [[fallthrough]];
    case FeatureState::kUnavailableSuiteDisabled:
      [[fallthrough]];
    case FeatureState::kUnavailableTopLevelFeatureDisabled:
      return true;
  }
}

bool SmartLockFeatureUsageMetrics::IsEnabled() const {
  return multidevice_setup_client_->GetFeatureState(
             multidevice_setup::mojom::Feature::kSmartLock) ==
         multidevice_setup::mojom::FeatureState::kEnabledByUser;
}

}  // namespace ash
