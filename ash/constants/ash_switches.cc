// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"

#include <algorithm>
#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/hash/sha1.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"

namespace ash::switches {

namespace {

// Max and min number of seconds that must pass between showing user contextual
// nudges when override switch is set.
constexpr base::TimeDelta kAshContextualNudgesMinInterval = base::Seconds(0);
constexpr base::TimeDelta kAshContextualNudgesMaxInterval = base::Seconds(60);

}  // namespace

bool IsAuthSessionCryptohomeEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kCryptohomeUseAuthSession);
}

bool IsCellularFirstDevice() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kCellularFirst);
}

bool IsRevenBranding() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kRevenBranding);
}

bool ShouldTetherHostScansIgnoreWiredConnections() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kTetherHostScansIgnoreWiredConnections);
}

bool ShouldSkipNewUserCheckForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kOobeSkipNewUserCheckForTesting);
}

bool ShouldSkipSplitModifierCheckForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kOobeSkipSplitModifierCheckForTesting);
}

bool ShouldSkipOobePostLogin() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kOobeSkipPostLogin);
}

bool ShouldDisablePreConsentMetricsForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kOobeDisablePreConsentMetricsForTesting);
}

bool ShouldShowAccessibilityButtonOnMarketingOptInForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kOobeShowAccessibilityButtonOnMarketingOptInForTesting);
}

bool IsTabletFormFactor() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableTabletFormFactor);
}

bool ShouldMultideviceScreenBeSkippedForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kSkipMultideviceScreenForTesting);
}

bool IsGaiaServicesDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableGaiaServices);
}

bool IsArcCpuRestrictionDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableArcCpuRestriction);
}

bool IsTpmDynamic() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kTpmIsDynamic);
}

bool IsUnfilteredBluetoothDevicesEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kUnfilteredBluetoothDevices);
}

bool ShouldOobeUseTabletModeFirstRun() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kOobeForceTabletFirstRun);
}

bool ShouldScaleOobe() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kOobeLargeScreenSpecialScaling);
}

bool IsAueReachedForUpdateRequiredForTest() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kUpdateRequiredAueForTest);
}

bool AreEmptyPasswordsAllowedForForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kTemporaryAllowEmptyPasswordsInTests);
}

bool IsOOBEChromeVoxHintTimerDisabledForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableOOBEChromeVoxHintTimerForTesting);
}

bool IsOOBENetworkSetupSkippedForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kOOBESkipNetworkSetupForTesting);
}

bool IsOOBENetworkScreenSkippingDisabledForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableOOBENetworkScreenSkippingForTesting);
}

bool IsOOBEChromeVoxHintEnabledForDevMode() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableOOBEChromeVoxHintForDevMode);
}

bool IsOverviewButtonEnabledForTests() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kOverviewButtonForTests);
}

bool IsDeviceRequisitionConfigurable() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableRequisitionEdits);
}

bool IsOsInstallAllowed() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kAllowOsInstall);
}

std::optional<base::TimeDelta> ContextualNudgesInterval() {
  int numeric_cooldown_time;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kAshContextualNudgesInterval) &&
      base::StringToInt(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              kAshContextualNudgesInterval),
          &numeric_cooldown_time)) {
    base::TimeDelta cooldown_time = base::Seconds(numeric_cooldown_time);
    cooldown_time = std::clamp(cooldown_time, kAshContextualNudgesMinInterval,
                               kAshContextualNudgesMaxInterval);
    return std::optional<base::TimeDelta>(cooldown_time);
  }
  return std::nullopt;
}

bool ContextualNudgesResetShownCount() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kAshContextualNudgesResetShownCount);
}

bool IsUsingShelfAutoDim() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kEnableDimShelf);
}

bool HasHps() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kHasHps);
}

bool IsSkipRecorderNudgeShowThresholdDurationEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kSkipReorderNudgeShowThresholdDurationForTest);
}

bool IsStabilizeTimeDependentViewForTestsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kStabilizeTimeDependentViewForTests);
}

bool UseFakeCrasAudioClientForDBus() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kUseFakeCrasAudioClientForDBus);
}

bool ShouldAllowDefaultShelfPinLayoutIgnoringSync() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kAllowDefaultShelfPinLayoutIgnoringSync);
}

bool IsPerUserTimezoneEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisablePerUserTimezone);
}

bool IsFineGrainedTimeZoneDetectionEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      kDisableFineGrainedTimeZoneDetection);
}

}  // namespace ash::switches
