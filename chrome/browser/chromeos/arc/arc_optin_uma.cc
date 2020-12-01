// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_optin_uma.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_util.h"
#include "chrome/browser/chromeos/arc/session/arc_provisioning_result.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
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

void UpdateProvisioningResultUMA(ProvisioningResultUMA result,
                                 const Profile* profile) {
  DCHECK_NE(result, ProvisioningResultUMA::CHROME_SERVER_COMMUNICATION_ERROR);
  base::UmaHistogramEnumeration(
      GetHistogramNameByUserType("Arc.Provisioning.Result", profile), result);
}

void UpdateCloudProvisionFlowErrorUMA(mojom::CloudProvisionFlowError error,
                                      const Profile* profile) {
  base::UmaHistogramEnumeration(
      GetHistogramNameByUserType("Arc.Provisioning.CloudFlowError", profile),
      error);
}

void UpdateSecondarySigninResultUMA(ProvisioningResultUMA result) {
  base::UmaHistogramEnumeration("Arc.Secondary.Signin.Result", result);
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

void UpdateReauthorizationResultUMA(ProvisioningResultUMA result,
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
                      base::TimeDelta elapsed_time,
                      const Profile* profile) {
  base::UmaHistogramCustomTimes(
      GetHistogramNameByUserType(histogram_name, profile), elapsed_time,
      base::TimeDelta::FromSeconds(1) /* minimum */,
      base::TimeDelta::FromMinutes(3) /* maximum */, 50 /* bucket_count */);
}

void UpdateAuthCheckinAttempts(int32_t num_attempts, const Profile* profile) {
  base::UmaHistogramSparse(
      GetHistogramNameByUserType("Arc.Auth.Checkin.Attempts", profile),
      num_attempts);
}

void UpdateAuthAccountCheckStatus(mojom::AccountCheckStatus status,
                                  const Profile* profile) {
  DCHECK_LE(status, mojom::AccountCheckStatus::CHECK_FAILED);
  UMA_HISTOGRAM_ENUMERATION(
      GetHistogramNameByUserType("Arc.Auth.AccountCheck.Status", profile),
      static_cast<int>(status),
      static_cast<int>(mojom::AccountCheckStatus::CHECK_FAILED) + 1);
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
  base::UmaHistogramSparse("Arc.OptInSilentAuthCode", static_cast<int>(state));
}

void UpdateSupervisionTransitionResultUMA(
    mojom::SupervisionChangeStatus result) {
  base::UmaHistogramEnumeration("Arc.Supervision.Transition.Result", result);
}

void UpdateReauthorizationSilentAuthCodeUMA(OptInSilentAuthCode state) {
  base::UmaHistogramSparse("Arc.OptInSilentAuthCode.Reauthorization",
                           static_cast<int>(state));
}

void UpdateSecondaryAccountSilentAuthCodeUMA(OptInSilentAuthCode state) {
  base::UmaHistogramSparse("Arc.OptInSilentAuthCode.SecondaryAccount",
                           static_cast<int>(state));
}

ProvisioningResultUMA GetProvisioningResultUMA(
    const ArcProvisioningResult& provisioning_result) {
  if (provisioning_result.is_stopped())
    return ProvisioningResultUMA::ARC_STOPPED;

  if (provisioning_result.is_timedout())
    return ProvisioningResultUMA::OVERALL_SIGN_IN_TIMEOUT;

  const mojom::ArcSignInResult* result = provisioning_result.sign_in_result();
  if (result->is_success()) {
    if (result->get_success() == mojom::ArcSignInSuccess::SUCCESS)
      return ProvisioningResultUMA::SUCCESS;
    else
      return ProvisioningResultUMA::SUCCESS_ALREADY_PROVISIONED;
  }

  if (result->get_error()->is_cloud_provision_flow_error())
    return ProvisioningResultUMA::CLOUD_PROVISION_FLOW_ERROR;

  if (result->get_error()->is_general_error()) {
#define MAP_GENERAL_ERROR(name)         \
  case mojom::GeneralSignInError::name: \
    return ProvisioningResultUMA::name

    switch (result->get_error()->get_general_error()) {
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

  if (result->get_error()->is_check_in_error()) {
#define MAP_CHECKIN_ERROR(name)      \
  case mojom::GMSCheckInError::name: \
    return ProvisioningResultUMA::name

    switch (result->get_error()->get_check_in_error()) {
      MAP_CHECKIN_ERROR(GMS_CHECK_IN_FAILED);
      MAP_CHECKIN_ERROR(GMS_CHECK_IN_TIMEOUT);
      MAP_CHECKIN_ERROR(GMS_CHECK_IN_INTERNAL_ERROR);
    }
#undef MAP_CHECKIN_ERROR
  }

  if (result->get_error()->is_sign_in_error()) {
#define MAP_GMS_ERROR(name)         \
  case mojom::GMSSignInError::name: \
    return ProvisioningResultUMA::name

    switch (result->get_error()->get_sign_in_error()) {
      MAP_GMS_ERROR(GMS_SIGN_IN_NETWORK_ERROR);
      MAP_GMS_ERROR(GMS_SIGN_IN_SERVICE_UNAVAILABLE);
      MAP_GMS_ERROR(GMS_SIGN_IN_BAD_AUTHENTICATION);
      MAP_GMS_ERROR(GMS_SIGN_IN_FAILED);
      MAP_GMS_ERROR(GMS_SIGN_IN_TIMEOUT);
      MAP_GMS_ERROR(GMS_SIGN_IN_INTERNAL_ERROR);
    }
#undef MAP_GMS_ERROR
  }

  NOTREACHED() << "unknown sign result";
  return ProvisioningResultUMA::UNKNOWN_ERROR;
}

std::ostream& operator<<(std::ostream& os,
                         const ProvisioningResultUMA& result) {
#define MAP_PROVISIONING_RESULT(name) \
  case ProvisioningResultUMA::name:   \
    return os << #name

  switch (result) {
    MAP_PROVISIONING_RESULT(SUCCESS);
    MAP_PROVISIONING_RESULT(UNKNOWN_ERROR);
    MAP_PROVISIONING_RESULT(GMS_SIGN_IN_NETWORK_ERROR);
    MAP_PROVISIONING_RESULT(GMS_SIGN_IN_SERVICE_UNAVAILABLE);
    MAP_PROVISIONING_RESULT(GMS_SIGN_IN_BAD_AUTHENTICATION);
    MAP_PROVISIONING_RESULT(GMS_CHECK_IN_FAILED);
    MAP_PROVISIONING_RESULT(MOJO_VERSION_MISMATCH);
    MAP_PROVISIONING_RESULT(GENERIC_PROVISIONING_TIMEOUT);
    MAP_PROVISIONING_RESULT(GMS_CHECK_IN_TIMEOUT);
    MAP_PROVISIONING_RESULT(GMS_CHECK_IN_INTERNAL_ERROR);
    MAP_PROVISIONING_RESULT(GMS_SIGN_IN_FAILED);
    MAP_PROVISIONING_RESULT(GMS_SIGN_IN_TIMEOUT);
    MAP_PROVISIONING_RESULT(GMS_SIGN_IN_INTERNAL_ERROR);
    MAP_PROVISIONING_RESULT(ARC_STOPPED);
    MAP_PROVISIONING_RESULT(OVERALL_SIGN_IN_TIMEOUT);
    MAP_PROVISIONING_RESULT(CHROME_SERVER_COMMUNICATION_ERROR);
    MAP_PROVISIONING_RESULT(NO_NETWORK_CONNECTION);
    MAP_PROVISIONING_RESULT(ARC_DISABLED);
    MAP_PROVISIONING_RESULT(SUCCESS_ALREADY_PROVISIONED);
    MAP_PROVISIONING_RESULT(UNSUPPORTED_ACCOUNT_TYPE);
    MAP_PROVISIONING_RESULT(CHROME_ACCOUNT_NOT_FOUND);
    MAP_PROVISIONING_RESULT(CLOUD_PROVISION_FLOW_ERROR);
  }

#undef MAP_PROVISIONING_RESULT

  // Some compilers report an error even if all values of an enum-class are
  // covered exhaustively in a switch statement.
  NOTREACHED() << "Invalid value " << static_cast<int>(result);
  return os;
}

}  // namespace arc
