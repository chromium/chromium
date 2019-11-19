// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_optin_uma.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_util.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/metrics/stability_metrics_manager.h"

namespace arc {

namespace {

ArcEnabledState ComputeEnabledState(bool enabled, const Profile* profile) {
  if (!IsArcAllowedForProfile(profile)) {
    return enabled ? ArcEnabledState::ENABLED_NOT_ALLOWED
                   : ArcEnabledState::DISABLED_NOT_ALLOWED;
  }

  if (!IsArcPlayStoreEnabledPreferenceManagedForProfile(profile)) {
    return enabled ? ArcEnabledState::ENABLED_NOT_MANAGED
                   : ArcEnabledState::DISABLED_NOT_MANAGED;
  }

  if (IsArcPlayStoreEnabledForProfile(profile)) {
    return enabled ? ArcEnabledState::ENABLED_MANAGED_ON
                   : ArcEnabledState::DISABLED_MANAGED_ON;
  }

  DCHECK(!enabled);
  return ArcEnabledState::DISABLED_MANAGED_OFF;
}

}  // namespace

void UpdateEnabledStateByUserTypeUMA() {
  const Profile* profile = ProfileManager::GetPrimaryUserProfile();

  // Don't record UMA if current primary user profile should be ignored in the
  // first place, or we're currently in guest session.
  if (!IsRealUserProfile(profile) || profile->IsGuestSession())
    return;

  base::Optional<bool> enabled_state;
  if (auto* stability_metrics_manager = StabilityMetricsManager::Get())
    enabled_state = stability_metrics_manager->GetArcEnabledState();

  base::UmaHistogramEnumeration(
      GetHistogramNameByUserType("Arc.StateByUserType", profile),
      ComputeEnabledState(enabled_state.value_or(false), profile));
}

void UpdateOptInActionUMA(OptInActionType type) {
  UMA_HISTOGRAM_ENUMERATION("Arc.OptInAction", type);
}

void UpdateOptInCancelUMA(OptInCancelReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Arc.OptInCancel", reason);
}

void UpdateOptInFlowResultUMA(OptInFlowResult result) {
  UMA_HISTOGRAM_ENUMERATION("Arc.OptInResult", result);
}

void UpdateProvisioningResultUMA(ProvisioningResult result,
                                 const Profile* profile) {
  DCHECK_NE(result, ProvisioningResult::CHROME_SERVER_COMMUNICATION_ERROR);
  base::UmaHistogramEnumeration(
      GetHistogramNameByUserType("Arc.Provisioning.Result", profile), result);
}

void UpdateSecondarySigninResultUMA(ProvisioningResult result) {
  UMA_HISTOGRAM_ENUMERATION("Arc.Secondary.Signin.Result", result);
}

void UpdateProvisioningTiming(const base::TimeDelta& elapsed_time,
                              bool success,
                              const Profile* profile) {
  std::string histogram_name = "Arc.Provisioning.TimeDelta";
  histogram_name += success ? ".Success" : ".Failure";
  // The macro UMA_HISTOGRAM_CUSTOM_TIMES expects a constant string, but since
  // this measurement happens very infrequently, we do not need to use a macro
  // here.
  base::UmaHistogramCustomTimes(
      GetHistogramNameByUserType(histogram_name, profile), elapsed_time,
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromMinutes(6), 50);
}

void UpdateReauthorizationResultUMA(ProvisioningResult result,
                                    const Profile* profile) {
  base::UmaHistogramEnumeration(
      GetHistogramNameByUserType("Arc.Reauthorization.Result", profile),
      result);
}

void UpdatePlayAutoInstallRequestState(mojom::PaiFlowState state,
                                       const Profile* profile) {
  base::UmaHistogramEnumeration(
      GetHistogramNameByUserType("Arc.PlayAutoInstallRequest.State", profile),
      state);
}

void UpdatePlayAutoInstallRequestTime(const base::TimeDelta& elapsed_time,
                                      const Profile* profile) {
  base::UmaHistogramCustomTimes(
      GetHistogramNameByUserType("Arc.PlayAutoInstallRequest.TimeDelta",
                                 profile),
      elapsed_time, base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromMinutes(10), 50);
}

void UpdateArcUiAvailableTime(const base::TimeDelta& elapsed_time,
                              const std::string& mode,
                              const Profile* profile) {
  base::UmaHistogramCustomTimes(
      GetHistogramNameByUserType("Arc.UiAvailable." + mode + ".TimeDelta",
                                 profile),
      elapsed_time, base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromMinutes(5), 50);
}

void UpdatePlayStoreLaunchTime(const base::TimeDelta& elapsed_time) {
  base::UmaHistogramCustomTimes("Arc.PlayStoreLaunch.TimeDelta", elapsed_time,
                                base::TimeDelta::FromMilliseconds(10),
                                base::TimeDelta::FromSeconds(20), 50);
}

void UpdatePlayStoreShownTimeDeprecated(const base::TimeDelta& elapsed_time,
                                        const Profile* profile) {
  base::UmaHistogramCustomTimes(
      GetHistogramNameByUserType("Arc.PlayStoreShown.TimeDelta", profile),
      elapsed_time, base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromMinutes(10), 50);
}

void UpdateAuthTiming(const char* histogram_name,
                      base::TimeDelta elapsed_time) {
  base::UmaHistogramCustomTimes(histogram_name, elapsed_time,
                                base::TimeDelta::FromSeconds(1) /* minimum */,
                                base::TimeDelta::FromMinutes(3) /* maximum */,
                                50 /* bucket_count */);
}

void UpdateAuthCheckinAttempts(int32_t num_attempts) {
  base::UmaHistogramSparse("ArcAuth.CheckinAttempts", num_attempts);
}

void UpdateAuthAccountCheckStatus(mojom::AccountCheckStatus status) {
  DCHECK_LE(status, mojom::AccountCheckStatus::CHECK_FAILED);
  UMA_HISTOGRAM_ENUMERATION(
      "ArcAuth.AccountCheckStatus", static_cast<int>(status),
      static_cast<int>(mojom::AccountCheckStatus::CHECK_FAILED) + 1);
}

void UpdateMainAccountResolutionStatus(
    const Profile* profile,
    mojom::MainAccountResolutionStatus status) {
  DCHECK(mojom::IsKnownEnumValue(status));
  base::UmaHistogramEnumeration(
      GetHistogramNameByUserType("ArcAuth.MainAccountResolutionStatus",
                                 profile),
      status);
}

void UpdateSilentAuthCodeUMA(OptInSilentAuthCode state) {
  base::UmaHistogramSparse("Arc.OptInSilentAuthCode", static_cast<int>(state));
}

void UpdateSupervisionTransitionResultUMA(
    mojom::SupervisionChangeStatus result) {
  UMA_HISTOGRAM_ENUMERATION("Arc.Supervision.Transition.Result", result);
}

void UpdateReauthorizationSilentAuthCodeUMA(OptInSilentAuthCode state) {
  base::UmaHistogramSparse("Arc.OptInSilentAuthCode.Reauthorization",
                           static_cast<int>(state));
}

void UpdateSecondaryAccountSilentAuthCodeUMA(OptInSilentAuthCode state) {
  base::UmaHistogramSparse("Arc.OptInSilentAuthCode.SecondaryAccount",
                           static_cast<int>(state));
}

std::ostream& operator<<(std::ostream& os, const ProvisioningResult& result) {
#define MAP_PROVISIONING_RESULT(name) \
  case ProvisioningResult::name:      \
    return os << #name

  switch (result) {
    MAP_PROVISIONING_RESULT(SUCCESS);
    MAP_PROVISIONING_RESULT(UNKNOWN_ERROR);
    MAP_PROVISIONING_RESULT(GMS_NETWORK_ERROR);
    MAP_PROVISIONING_RESULT(GMS_SERVICE_UNAVAILABLE);
    MAP_PROVISIONING_RESULT(GMS_BAD_AUTHENTICATION);
    MAP_PROVISIONING_RESULT(DEVICE_CHECK_IN_FAILED);
    MAP_PROVISIONING_RESULT(CLOUD_PROVISION_FLOW_FAILED);
    MAP_PROVISIONING_RESULT(MOJO_VERSION_MISMATCH);
    MAP_PROVISIONING_RESULT(MOJO_CALL_TIMEOUT);
    MAP_PROVISIONING_RESULT(DEVICE_CHECK_IN_TIMEOUT);
    MAP_PROVISIONING_RESULT(DEVICE_CHECK_IN_INTERNAL_ERROR);
    MAP_PROVISIONING_RESULT(GMS_SIGN_IN_FAILED);
    MAP_PROVISIONING_RESULT(GMS_SIGN_IN_TIMEOUT);
    MAP_PROVISIONING_RESULT(GMS_SIGN_IN_INTERNAL_ERROR);
    MAP_PROVISIONING_RESULT(CLOUD_PROVISION_FLOW_TIMEOUT);
    MAP_PROVISIONING_RESULT(CLOUD_PROVISION_FLOW_INTERNAL_ERROR);
    MAP_PROVISIONING_RESULT(ARC_STOPPED);
    MAP_PROVISIONING_RESULT(OVERALL_SIGN_IN_TIMEOUT);
    MAP_PROVISIONING_RESULT(CHROME_SERVER_COMMUNICATION_ERROR);
    MAP_PROVISIONING_RESULT(NO_NETWORK_CONNECTION);
    MAP_PROVISIONING_RESULT(ARC_DISABLED);
    MAP_PROVISIONING_RESULT(SUCCESS_ALREADY_PROVISIONED);
    MAP_PROVISIONING_RESULT(UNSUPPORTED_ACCOUNT_TYPE);
    MAP_PROVISIONING_RESULT(CHROME_ACCOUNT_NOT_FOUND);
  }

#undef MAP_PROVISIONING_RESULT

  // Some compilers report an error even if all values of an enum-class are
  // covered exhaustively in a switch statement.
  NOTREACHED() << "Invalid value " << static_cast<int>(result);
  return os;
}

}  // namespace arc
