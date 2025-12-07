// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOBSTER_LOBSTER_ENUMS_H_
#define ASH_PUBLIC_CPP_LOBSTER_LOBSTER_ENUMS_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

enum class ASH_PUBLIC_EXPORT LobsterMode {
  kInsert,
  kDownload,
};

enum class ASH_PUBLIC_EXPORT LobsterConsentStatus {
  // Users have neither approved nor declined the Lobster consent.
  kUnset,
  // Users have approved the Lobster consent.
  kApproved,
  // Users have declined the Lobster consent.
  kDeclined,
};

enum class ASH_PUBLIC_EXPORT LobsterStatus {
  // The feature requires user consent before use.
  kConsentNeeded,
  // The feature is enabled for use.
  kEnabled,
  // The feature is blocked from use.
  kBlocked,
};

enum class ASH_PUBLIC_EXPORT LobsterEnterprisePolicyValue : int {
  // The policy allows the feature to run with model improvement.
  kAllowedWithModelImprovement = 0,
  // The policy allows the feature
  kAllowedWithoutModelImprovement = 1,
  kDisabled = 2,
};

enum class ASH_PUBLIC_EXPORT LobsterSystemCheck {
  kMinValue,
  kInvalidConsent,
  kInvalidAccountCapabilities,
  kInvalidAccountType,
  kInvalidRegion,
  kInvalidInputField,
  kSettingsOff,
  kNoInternetConnection,
  kInvalidInputMethod,
  kInvalidFeatureFlags,  // The feature flag disabled.
  kUnsupportedHardware,
  kUnsupportedInKioskMode,  // In Kiosk mode.
  kUnsupportedFormFactor,
  kUnsupportedPolicy,
  kForcedDisabledOnManagedUsers,
  kMaxValue = kForcedDisabledOnManagedUsers,
};

enum class ASH_PUBLIC_EXPORT LobsterErrorCode {
  kNoInternetConnection,
  kBlockedOutputs,
  kUnknown,
  kResourceExhausted,
  kInvalidArgument,
  kBackendFailure,
  kUnsupportedLanguage,
  kRestrictedRegion,
  kContainsPeople,
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOBSTER_LOBSTER_ENUMS_H_
