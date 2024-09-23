// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOBSTER_LOBSTER_ENUMS_H_
#define ASH_PUBLIC_CPP_LOBSTER_LOBSTER_ENUMS_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

enum class ASH_PUBLIC_EXPORT LobsterStatus {
  kEnabled,
  kBlocked,
};

enum class ASH_PUBLIC_EXPORT LobsterSystemCheck {
  kMinValue,
  kInvalidConsent,
  kInvalidAccountCapabilities,
  kInvalidAccountType,
  kInvalidRegion,
  kMaxValue = kInvalidRegion,
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
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOBSTER_LOBSTER_ENUMS_H_
