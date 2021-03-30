// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/arc_optin_uma.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/arc/session/arc_provisioning_result.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/metrics/stability_metrics_manager.h"
#include "components/arc/mojom/app.mojom.h"
#include "components/arc/mojom/auth.mojom.h"

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
  base::UmaHistogramEnumeration("Arc.OptInAction", type);
}

void UpdateOptInCancelUMA(OptInCancelReason reason) {
  base::UmaHistogramEnumeration("Arc.OptInCancel", reason);
}

void UpdateOptInFlowResultUMA(OptInFlowResult result) {
  base::UmaHistogramEnumeration("Arc.OptInResult", result);
}

void UpdateProvisioningStatusUMA(ProvisioningStatus status,
                                 const Profile* profile) {
  DCHECK_NE(status, ProvisioningStatus::CHROME_SERVER_COMMUNICATION_ERROR);
  base::UmaHistogramEnumeration(
      GetHistogramNameByUserType("Arc.Provisioning.Status", profile), status);
}

void UpdateCloudProvisionFlowErrorUMA(mojom::CloudProvisionFlowError error,
                                      const Profile* profile) {
  base::UmaHistogramEnumeration(
      GetHistogramNameByUserType("Arc.Provisioning.CloudFlowError", profile),
      error);
}

void UpdateGMSSignInErrorUMA(mojom::GMSSignInError error,
                             const Profile* profile) {
  base::UmaHistogramEnumeration(
      GetHistogramNameByUserType("Arc.Provisioning.SignInError", profile),
      error);
}

void UpdateGMSCheckInErrorUMA(mojom::GMSCheckInError error,
                              const Profile* profile) {
  base::UmaHistogramEnumeration(
      GetHistogramNameByUserType("Arc.Provisioning.CheckInError", profile),
      error);
}

void UpdateSecondarySigninResultUMA(ProvisioningStatus status) {
  base::UmaHistogramEnumeration("Arc.Secondary.Signin.Result", status);
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

void UpdateReauthorizationResultUMA(ProvisioningStatus status,
                                    const Profile* profile) {
  base::UmaHistogramEnumeration(
      GetHistogramNameByUserType("Arc.Reauthorization.Result", profile),
      status);
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
                      base::TimeDelta elapsed_time,
                      const Profile* profile) {
  base::UmaHistogramCustomTimes(
      GetHistogramNameByUserType(histogram_name, profile), elapsed_time,
      base::TimeDelta::FromSeconds(1) /* minimum */,
      base::TimeDelta::FromMinutes(3) /* maximum */, 50 /* bucket_count */);
}

void UpdateAuthCheckinAttempts(int32_t num_attempts, const Profile* profile) {
  base::UmaHistogramExactLinear(
      GetHistogramNameByUserType("Arc.Auth.Checkin.Attempts", profile),
      num_attempts, 11 /* exclusive_max */);
}

void UpdateAuthAccountCheckStatus(mojom::AccountCheckStatus status,
                                  const Profile* profile) {
  DCHECK_LE(status, mojom::AccountCheckStatus::CHECK_FAILED);
  base::UmaHistogramEnumeration(
      GetHistogramNameByUserType("Arc.Auth.AccountCheck.Status", profile),
      status);
}

void UpdateMainAccountResolutionStatus(
    const Profile* profile,
    mojom::MainAccountResolutionStatus status) {
  DCHECK(mojom::IsKnownEnumValue(status));
  base::UmaHistogramEnumeration(
      GetHistogramNameByUserType("Arc.Auth.MainAccountResolution.Status",
                                 profile),
      status);
}

void UpdateSilentAuthCodeUMA(OptInSilentAuthCode state) {
  base::UmaHistogramEnumeration("Arc.OptInSilentAuthCode", state);
}

void UpdateSupervisionTransitionResultUMA(
    mojom::SupervisionChangeStatus result) {
  base::UmaHistogramEnumeration("Arc.Supervision.Transition.Result", result);
}

void UpdateReauthorizationSilentAuthCodeUMA(OptInSilentAuthCode state) {
  base::UmaHistogramEnumeration("Arc.OptInSilentAuthCode.Reauthorization",
                                state);
}

void UpdateSecondaryAccountSilentAuthCodeUMA(OptInSilentAuthCode state) {
  base::UmaHistogramEnumeration("Arc.OptInSilentAuthCode.SecondaryAccount",
                                state);
}

ProvisioningStatus GetProvisioningStatus(
    const ArcProvisioningResult& provisioning_result) {
  if (provisioning_result.stop_reason())
    return ProvisioningStatus::ARC_STOPPED;

  if (provisioning_result.is_timedout())
    return ProvisioningStatus::CHROME_PROVISIONING_TIMEOUT;

  if (provisioning_result.is_success())
    return ProvisioningStatus::SUCCESS;

  if (provisioning_result.cloud_provision_flow_error())
    return ProvisioningStatus::CLOUD_PROVISION_FLOW_ERROR;

  if (provisioning_result.gms_check_in_error())
    return ProvisioningStatus::GMS_CHECK_IN_ERROR;

  if (provisioning_result.gms_sign_in_error())
    return ProvisioningStatus::GMS_SIGN_IN_ERROR;

  if (provisioning_result.general_error()) {
#define MAP_GENERAL_ERROR(name)         \
  case mojom::GeneralSignInError::name: \
    return ProvisioningStatus::name

    switch (provisioning_result.general_error().value()) {
      MAP_GENERAL_ERROR(UNKNOWN_ERROR);
      MAP_GENERAL_ERROR(MOJO_VERSION_MISMATCH);
      MAP_GENERAL_ERROR(GENERIC_PROVISIONING_TIMEOUT);
      MAP_GENERAL_ERROR(NO_NETWORK_CONNECTION);
      MAP_GENERAL_ERROR(CHROME_SERVER_COMMUNICATION_ERROR);
      MAP_GENERAL_ERROR(ARC_DISABLED);
      MAP_GENERAL_ERROR(UNSUPPORTED_ACCOUNT_TYPE);
      MAP_GENERAL_ERROR(CHROME_ACCOUNT_NOT_FOUND);
    }
#undef MAP_GENERAL_ERROR
  }

  NOTREACHED() << "unexpected provisioning result";
  return ProvisioningStatus::UNKNOWN_ERROR;
}

std::ostream& operator<<(std::ostream& os, const ProvisioningStatus& status) {
#define MAP_PROVISIONING_RESULT(name) \
  case ProvisioningStatus::name:      \
    return os << #name

  switch (status) {
    MAP_PROVISIONING_RESULT(SUCCESS);
    MAP_PROVISIONING_RESULT(UNKNOWN_ERROR);
    MAP_PROVISIONING_RESULT(GMS_SIGN_IN_ERROR);
    MAP_PROVISIONING_RESULT(GMS_CHECK_IN_ERROR);
    MAP_PROVISIONING_RESULT(CLOUD_PROVISION_FLOW_ERROR);
    MAP_PROVISIONING_RESULT(MOJO_VERSION_MISMATCH);
    MAP_PROVISIONING_RESULT(GENERIC_PROVISIONING_TIMEOUT);
    MAP_PROVISIONING_RESULT(CHROME_PROVISIONING_TIMEOUT);
    MAP_PROVISIONING_RESULT(ARC_STOPPED);
    MAP_PROVISIONING_RESULT(ARC_DISABLED);
    MAP_PROVISIONING_RESULT(CHROME_SERVER_COMMUNICATION_ERROR);
    MAP_PROVISIONING_RESULT(NO_NETWORK_CONNECTION);
    MAP_PROVISIONING_RESULT(UNSUPPORTED_ACCOUNT_TYPE);
    MAP_PROVISIONING_RESULT(CHROME_ACCOUNT_NOT_FOUND);
  }

#undef MAP_PROVISIONING_RESULT

  // Some compilers report an error even if all values of an enum-class are
  // covered exhaustively in a switch statement.
  NOTREACHED() << "Invalid value " << static_cast<int>(status);
  return os;
}

}  // namespace arc
