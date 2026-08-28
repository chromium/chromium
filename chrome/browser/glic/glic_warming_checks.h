// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_WARMING_CHECKS_H_
#define CHROME_BROWSER_GLIC_GLIC_WARMING_CHECKS_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "net/base/network_change_notifier.h"

class Profile;

namespace glic {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This enum should be kept in sync with
// GlicPrewarmingChecksResult in enums.xml.
// LINT.IfChange(GlicPrewarmingChecksResult)
enum class GlicPrewarmingChecksResult {
  // Preloading is happening.
  kSuccess = 0,

  // Warming was disabled by the feature configuration.
  kWarmingDisabled = 1,

  // The profile doesn't exist or is marked for deletion.
  kProfileGone = 2,

  // The profile is not ready for Glic, for an unknown reason.
  kProfileNotReadyUnknown = 3,

  // The account state is paused, and requires sign in.
  kProfileRequiresSignIn = 4,

  // The profile is not eligible for Glic.
  kProfileNotEligible = 5,

  // Glic is not rolled out to the user.
  kProfileNotRolledOut = 6,

  // The profile is disallowed by admin policy.
  kProfileDisallowedByAdmin = 7,

  // The profile is not enabled for Glic for some other reason.
  kProfileNotEnabledOther = 8,

  // The profile is already the last loaded profile.
  kProfileIsLastLoaded = 9,

  // The profile is already the last active profile.
  kProfileIsLastActive = 10,

  // Preloading is blocked because another Glic is already showing.
  kBlockedByShownGlic = 11,

  // The system is under memory pressure.
  kUnderMemoryPressure = 12,

  // The device has a cellular connection.
  kCellularConnection = 13,

  // The browser is being shutdown.
  kBrowserShuttingDown = 14,

  // Deprecated.
  kUserAlreadyWentTroughFre = 15,

  // Used by tests to prevent premature preloading. Not a valid value for
  // production code.
  kPrewarmingDisabledForTesting = 16,

  // Glic is not pinned to the tabstrip.
  kNotPinnedToTabstrip = 17,

  // The profile is not eligible to use Glic due to a country location mismatch.
  kProfileNotEligibleLocationMismatch = 18,

  // The profile is not eligible to use Glic due to account capability
  // restrictions.
  kProfileNotEligibleAccountCapabilities = 19,

  // The device has less than the minimum required memory for warming.
  kDeviceLowMemory = 20,

  kMaxValue = kDeviceLowMemory,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicPrewarmingChecksResult)

// Identifies what triggered prewarming of the Glic WebContents.
enum class GlicWarmingTrigger {
  kStartup = 0,
  kNudge = 1,
  kIph = 2,
  kMaxValue = kIph,
};

using ShouldPreloadCallback =
    base::OnceCallback<void(GlicPrewarmingChecksResult)>;

// Evaluates profile readiness, enablement, device memory constraints, and
// network meter status to determine if preloading is permitted for the given
// profile and trigger.
void ShouldPreloadForProfile(Profile* profile,
                             GlicWarmingTrigger trigger,
                             ShouldPreloadCallback callback);

// Testing helpers to override prewarming behavior and network connection type.
void SetPrewarmingEnabledForTesting(bool enabled);
void ForceConnectionTypeForTesting(
    std::optional<net::NetworkChangeNotifier::ConnectionType> type);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_WARMING_CHECKS_H_
