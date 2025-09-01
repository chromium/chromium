// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_MAHI_AVAILABILITY_H_
#define CHROME_BROWSER_ASH_MAHI_MAHI_AVAILABILITY_H_

#include <optional>

#include "base/types/expected.h"

namespace ash::mahi_availability {

enum class Error { kMantaFeatureBitNotReady };

// Check whether Mahi is allowed. This function checks following restrictions:
//   * age: if not demo mode, the account must not hit minor restrictions
//   * country: the country code must be in the allow list.
//   * If not in demo mode, guest session is not allowed.
//
// This check reads a bit loaded as an async operation:
// `CanAccessMantaFeaturesWithoutMinorRestrictions`.
// `Error::kMantaFeatureBitNotRead` is returned if the bit is not ready yet.
base::expected<bool, Error> CanUseMahiService();

// Check if the mahi feature is available to use. It can be unavailable if the
// mahi feature flag is disabled, or the age and country requirements are not
// met.
//
// This check reads a bit loaded as an async operation via `CanUseMahiService`.
// `Error::kMantaFeatureBitNotReady` is returned if the bit is not ready yet.
base::expected<bool, Error> IsMahiAvailable();

}  // namespace ash::mahi_availability

#endif  // CHROME_BROWSER_ASH_MAHI_MAHI_AVAILABILITY_H_
