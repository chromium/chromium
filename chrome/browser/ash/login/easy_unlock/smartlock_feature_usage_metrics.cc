// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/smartlock_feature_usage_metrics.h"

#include "chromeos/components/feature_usage/feature_usage_metrics.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

namespace ash {
namespace {
using chromeos::multidevice_setup::mojom::Feature;
using chromeos::multidevice_setup::mojom::FeatureState;

const char kFeatureName[] = "SmartLock";
}  // namespace

SmartLockFeatureUsageMetrics::SmartLockFeatureUsageMetrics(
    chromeos::multidevice_setup::MultiDeviceSetupClient*
        multi_device_setup_client)
    : multi_device_setup_client_(multi_device_setup_client),
      feature_usage_metrics_(kFeatureName, this) {}

SmartLockFeatureUsageMetrics::~SmartLockFeatureUsageMetrics() = default;

void SmartLockFeatureUsageMetrics::RecordUsage(bool success) {
  feature_usage_metrics_.RecordUsage(success);
}

bool SmartLockFeatureUsageMetrics::IsEligible() const {
  if (!multi_device_setup_client_) {
    // EasyUnlockServiceSignin cannot determine Eligible/Enabled values, so
    // it injects a null MultiDeviceSetupClient reference. Therefore, a null
    // MultiDeviceSetupClient reference implies that Eligible/Enabled are always
    // true.
    return true;
  }

  switch (multi_device_setup_client_->GetFeatureState(Feature::kSmartLock)) {
    case FeatureState::kUnavailableNoVerifiedHost:
      FALLTHROUGH;
    case FeatureState::kNotSupportedByChromebook:
      FALLTHROUGH;
    case FeatureState::kNotSupportedByPhone:
      return false;

    case FeatureState::kProhibitedByPolicy:
      FALLTHROUGH;
    case FeatureState::kDisabledByUser:
      FALLTHROUGH;
    case FeatureState::kEnabledByUser:
      FALLTHROUGH;
    case FeatureState::kUnavailableInsufficientSecurity:
      FALLTHROUGH;
    case FeatureState::kUnavailableSuiteDisabled:
      FALLTHROUGH;
    case FeatureState::kFurtherSetupRequired:
      FALLTHROUGH;
    case FeatureState::kUnavailableTopLevelFeatureDisabled:
      return true;
  }
}

bool SmartLockFeatureUsageMetrics::IsEnabled() const {
  if (!multi_device_setup_client_) {
    // EasyUnlockServiceSignin cannot determine Eligible/Enabled values, so
    // it injects a null MultiDeviceSetupClient reference. Therefore, a null
    // MultiDeviceSetupClient reference implies that Eligible/Enabled are always
    // true.
    return true;
  }

  if (multi_device_setup_client_->GetFeatureState(Feature::kSmartLock) ==
      FeatureState::kEnabledByUser) {
    return true;
  }

  return false;
}

}  // namespace ash
