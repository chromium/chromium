// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/arc_optin_uma.h"

#include <string>

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/metrics/stability_metrics_manager.h"
#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/mojom/auth.mojom.h"
#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/arc/session/arc_provisioning_result.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user_manager.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {

namespace {

// Logs UMA enum values to facilitate finding feedback reports in Xamine.
template <typename T>
void LogStabilityUmaEnum(const std::string& name, T sample) {
  base::UmaHistogramEnumeration(name, sample);
  VLOG(1) << name << ": " << static_cast<std::underlying_type_t<T>>(sample);
}

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
  auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  // Don't record UMA if there is no primary user.
  if (!primary_user) {
    return;
  }

  const Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(primary_user));
  // Don't record UMA if the primary user profile is not loaded.
  if (!profile) {
    return;
  }

  // Don't record UMA if we're currently in guest session.
  if (profile->IsGuestSession()) {
    return;
  }

  std::optional<bool> enabled_state;
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
  LogStabilityUmaEnum("Arc.OptInResult", result);
}

void UpdateOptInNetworkErrorActionUMA(OptInNetworkErrorActionType type) {
  base::UmaHistogramEnumeration("Arc.OptInNetworkErrorAction", type);
}

void UpdateOptinTosLoadResultUMA(bool success) {
  base::UmaHistogramBoolean("Arc.OptinTosLoadResult", success);
}

void UpdateProvisioningStatusUMA(ProvisioningStatus status,
                                 const Profile* profile) {
  DCHECK_NE(status, ProvisioningStatus::CHROME_SERVER_COMMUNICATION_ERROR);
  LogStabilityUmaEnum(
      GetHistogramNameByUserType("Arc.Provisioning.Status", profile), status);
}

void UpdateProvisioningDpcResultUMA(ArcProvisioningDpcResult result,
                                    const Profile* profile) {
  LogStabilityUmaEnum(
      GetHistogramNameByUserType("Arc.Provisioning.DpcResult", profile),
      result);
}

void UpdateProvisioningSigninResultUMA(ArcProvisioningSigninResult result,
                                       const Profile* profile) {
  LogStabilityUmaEnum(
      GetHistogramNameByUserType("Arc.Provisioning.SigninResult", profile),
      result);
}

void UpdateProvisioningCheckinResultUMA(ArcProvisioningCheckinResult result,
                                        const Profile* profile) {
  LogStabilityUmaEnum(
      GetHistogramNameByUserType("Arc.Provisioning.CheckinResult", profile),
      result);
}

void UpdateSecondarySigninResultUMA(ProvisioningStatus status) {
  LogStabilityUmaEnum("Arc.Secondary.Signin.Result", status);
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
      base::Seconds(1), base::Minutes(6), 50);
}

void UpdateReauthorizationResultUMA(ProvisioningStatus status,
                                    const Profile* profile) {
  LogStabilityUmaEnum(
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
      elapsed_time, base::Seconds(1), base::Minutes(10), 50);
}

void UpdateArcUiAvailableTime(const base::TimeDelta& elapsed_time,
                              const std::string& mode,
                              const Profile* profile) {
  if (ash::Shell::HasInstance()) {
    ash::Shell::Get()
        ->login_unlock_throughput_recorder()
        ->ArcUiAvailableAfterLogin();
  }
  base::UmaHistogramCustomTimes(
      GetHistogramNameByUserType("Arc.UiAvailable." + mode + ".TimeDelta",
                                 profile),
      elapsed_time, base::Seconds(1), base::Minutes(5), 50);

  // This is local test-only histogram.
  LOCAL_HISTOGRAM_CUSTOM_TIMES("Arc.Tast.UiAvailable.TimeDelta", elapsed_time,
                               base::Seconds(1), base::Minutes(5), 50);
}

void UpdatePlayStoreLaunchTime(const base::TimeDelta& elapsed_time) {
  base::UmaHistogramCustomTimes("Arc.PlayStoreLaunch.TimeDelta", elapsed_time,
                                base::Milliseconds(10), base::Seconds(20), 50);
}

void UpdateDeferredLaunchTime(const base::TimeDelta& elapsed_time) {
  base::UmaHistogramCustomTimes(
      "Arc.FirstAppLaunchDelay.TimeDeltaUntilAppLaunch", elapsed_time,
      base::Milliseconds(10), base::Seconds(60), 50);
}

void UpdateAuthTiming(const char* histogram_name,
                      base::TimeDelta elapsed_time,
                      const Profile* profile) {
  base::UmaHistogramCustomTimes(
      GetHistogramNameByUserType(histogram_name, profile), elapsed_time,
      base::Seconds(1) /* minimum */, base::Minutes(3) /* maximum */,
      50 /* bucket_count */);
}

void UpdateAuthCheckinAttempts(int32_t num_attempts, const Profile* profile) {
  base::UmaHistogramExactLinear(
      GetHistogramNameByUserType("Arc.Auth.Checkin.Attempts", profile),
      num_attempts, 11 /* exclusive_max */);
}

void UpdateAuthAccountCheckStatus(mojom::AccountCheckStatus status,
                                  const Profile* profile) {
  DCHECK_LE(status, mojom::AccountCheckStatus::CHECK_FAILED);
  LogStabilityUmaEnum(
      GetHistogramNameByUserType("Arc.Auth.AccountCheck.Status", profile),
      status);
}

void UpdateAccountReauthReason(mojom::ReauthReason reason,
                               const Profile* profile) {
  LogStabilityUmaEnum(
      GetHistogramNameByUserType("Arc.Auth.Reauth.Reason", profile), reason);
}

void UpdateMainAccountResolutionStatus(
    const Profile* profile,
    mojom::MainAccountResolutionStatus status) {
  DCHECK(mojom::IsKnownEnumValue(status));
  LogStabilityUmaEnum(GetHistogramNameByUserType(
                          "Arc.Auth.MainAccountResolution.Status", profile),
                      status);
}

void UpdateSilentAuthCodeUMA(OptInSilentAuthCode state) {
  LogStabilityUmaEnum("Arc.OptInSilentAuthCode", state);
}

// TODO(tantoshchuk): rename UMA histogram to "Arc.Management.Transition.Result"
void UpdateSupervisionTransitionResultUMA(
    mojom::ManagementChangeStatus result) {
  LogStabilityUmaEnum("Arc.Supervision.Transition.Result", result);
}

void UpdateReauthorizationSilentAuthCodeUMA(OptInSilentAuthCode state) {
  LogStabilityUmaEnum("Arc.OptInSilentAuthCode.Reauthorization", state);
}

void UpdateSecondaryAccountSilentAuthCodeUMA(OptInSilentAuthCode state) {
  LogStabilityUmaEnum("Arc.OptInSilentAuthCode.SecondaryAccount", state);
}

ArcProvisioningDpcResult GetDpcErrorResult(
    mojom::CloudProvisionFlowError error) {
  switch (error) {
    case mojom::CloudProvisionFlowError::ERROR_ENROLLMENT_TOKEN_INVALID:
      return ArcProvisioningDpcResult::kInvalidToken;
    case mojom::CloudProvisionFlowError::ERROR_ADD_ACCOUNT_FAILED:
      return ArcProvisioningDpcResult::kAccountAddFail;
    case mojom::CloudProvisionFlowError::ERROR_TIMEOUT:
      return ArcProvisioningDpcResult::kTimeout;
    case mojom::CloudProvisionFlowError::ERROR_NETWORK_UNAVAILABLE:
      return ArcProvisioningDpcResult::kNetworkError;
    case mojom::CloudProvisionFlowError::
        ERROR_OAUTH_TOKEN_AUTHENTICATOR_EXCEPTION:
      return ArcProvisioningDpcResult::kOAuthAuthException;
    case mojom::CloudProvisionFlowError::ERROR_OAUTH_TOKEN_IO_EXCEPTION:
      return ArcProvisioningDpcResult::kOAuthIOException;
    case mojom::CloudProvisionFlowError::ERROR_OTHER:
    case mojom::CloudProvisionFlowError::ERROR_ENTERPRISE_INVALID:
    case mojom::CloudProvisionFlowError::ERROR_USER_CANCEL:
    case mojom::CloudProvisionFlowError::ERROR_NO_ACCOUNT_IN_WORK_PROFILE:
    case mojom::CloudProvisionFlowError::ERROR_INVALID_POLICY_STATE:
    case mojom::CloudProvisionFlowError::ERROR_DPC_SUPPORT:
    case mojom::CloudProvisionFlowError::ERROR_ACCOUNT_NOT_READY:
    case mojom::CloudProvisionFlowError::ERROR_CHECKIN_FAILED:
    case mojom::CloudProvisionFlowError::ERROR_ACCOUNT_NOT_ALLOWLISTED:
    case mojom::CloudProvisionFlowError::ERROR_JSON:
    case mojom::CloudProvisionFlowError::ERROR_MANAGED_PROVISIONING_FAILED:
    case mojom::CloudProvisionFlowError::ERROR_INVALID_SETUP_ACTION:
    case mojom::CloudProvisionFlowError::ERROR_SERVER:
    case mojom::CloudProvisionFlowError::ERROR_REMOVE_ACCOUNT_FAILED:
    case mojom::CloudProvisionFlowError::ERROR_OAUTH_TOKEN:
    case mojom::CloudProvisionFlowError::ERROR_ACCOUNT_OTHER:
    case mojom::CloudProvisionFlowError::ERROR_QUARANTINE:
    case mojom::CloudProvisionFlowError::ERROR_DEVICE_QUOTA_EXCEEDED:
    case mojom::CloudProvisionFlowError::ERROR_SERVER_TRANSIENT_ERROR:
    case mojom::CloudProvisionFlowError::
        ERROR_OAUTH_TOKEN_OPERATION_CANCELED_EXCEPTION:
    case mojom::CloudProvisionFlowError::ERROR_REQUEST_ANDROID_ID_FAILED:
      VLOG(1) << "ArcProvisioningDpcResult[kUnknownError]="
              << static_cast<
                     std::underlying_type_t<mojom::CloudProvisionFlowError>>(
                     error);
      return ArcProvisioningDpcResult::kUnknownError;
  }
}

ArcProvisioningSigninResult GetSigninErrorResult(mojom::GMSSignInError error) {
  switch (error) {
    case mojom::GMSSignInError::GMS_SIGN_IN_NETWORK_ERROR:
      return ArcProvisioningSigninResult::kNetworkError;
    case mojom::GMSSignInError::GMS_SIGN_IN_SERVICE_UNAVAILABLE:
      return ArcProvisioningSigninResult::kServiceUnavailable;
    case mojom::GMSSignInError::GMS_SIGN_IN_BAD_AUTHENTICATION:
      return ArcProvisioningSigninResult::kAuthFailure;
    case mojom::GMSSignInError::GMS_SIGN_IN_TIMEOUT:
      return ArcProvisioningSigninResult::kTimeout;
    case mojom::GMSSignInError::GMS_SIGN_IN_FAILED:
    case mojom::GMSSignInError::GMS_SIGN_IN_INTERNAL_ERROR:
      VLOG(1) << "ArcProvisioningSigninResult[kUnknownError]="
              << static_cast<std::underlying_type_t<mojom::GMSSignInError>>(
                     error);
      return ArcProvisioningSigninResult::kUnknownError;
  }
}

ArcProvisioningCheckinResult GetCheckinErrorResult(
    mojom::GMSCheckInError error) {
  switch (error) {
    case mojom::GMSCheckInError::GMS_CHECK_IN_TIMEOUT:
      return ArcProvisioningCheckinResult::kTimeout;
    case mojom::GMSCheckInError::GMS_CHECK_IN_FAILED:
    case mojom::GMSCheckInError::GMS_CHECK_IN_INTERNAL_ERROR:
      VLOG(1) << "ArcProvisioningCheckinResult[kUnknownError]="
              << static_cast<std::underlying_type_t<mojom::GMSCheckInError>>(
                     error);
      return ArcProvisioningCheckinResult::kUnknownError;
  }
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

  NOTREACHED_IN_MIGRATION() << "unexpected provisioning result";
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
  NOTREACHED_IN_MIGRATION() << "Invalid value " << static_cast<int>(status);
  return os;
}

}  // namespace arc
