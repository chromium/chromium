// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ARC_OPTIN_UMA_H_
#define CHROME_BROWSER_ASH_ARC_ARC_OPTIN_UMA_H_

#include <ostream>

#include "ash/components/arc/mojom/app.mojom-forward.h"
#include "ash/components/arc/mojom/auth.mojom-forward.h"

class Profile;

namespace base {
class TimeDelta;
}

namespace arc {

class ArcProvisioningResult;

// These enums are used to define the buckets for an enumerated UMA histogram
// and need to be synced with tools/metrics/histograms/enums.xml. Note that
// values 0, 1, 2, 3 and 4 are now deprecated.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OptInActionType : int {
  // User asked to retry OptIn.
  RETRY = 5,

  // ARC was opted in by user from OOBE flow.
  OOBE_OPTED_IN = 6,

  // ARC was opted out by user from OOBE flow.
  OOBE_OPTED_OUT = 7,

  // ARC was opted in by user from session.
  SESSION_OPTED_IN = 8,

  // ARC was opted out by user from session.
  SESSION_OPTED_OUT = 9,

  // ARC was opted in due to configuration in OOBE flow.
  OOBE_OPTED_IN_CONFIGURATION = 10,

  kMaxValue = OOBE_OPTED_IN_CONFIGURATION,
};

enum class OptInCancelReason {
  // Canceled by user.
  USER_CANCEL = 0,

  // Unclassified failure.
  UNKNOWN_ERROR = 1,

  // Network failure.
  NETWORK_ERROR = 2,

  DEPRECATED_SERVICE_UNAVAILABLE = 3,
  DEPRECATED_BAD_AUTHENTICATION = 4,
  DEPRECATED_GMS_CORE_NOT_AVAILABLE = 5,

  // Provision failed.
  PROVISIONING_FAILED = 6,

  // Android management is required for user.
  ANDROID_MANAGEMENT_REQUIRED = 7,

  // Cannot start ARC because it is busy.
  SESSION_BUSY = 8,

  kMaxValue = SESSION_BUSY,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OptInNetworkErrorActionType {
  // User closed the optin dialog.
  WINDOW_CLOSED = 0,

  // User asked to retry optin.
  RETRY = 1,

  // User asked to send feedback.
  SEND_FEEDBACK = 2,

  // User wants to diagnose network.
  CHECK_NETWORK = 3,

  // Network error page was shown. This bucket encompasses all others and works
  // as a total count for network-related errors (instead of the histogram's
  // total sample count which doesn't reflect that information). This enum
  // value was added on 2023 Dec 21. The data before this date is missing.
  ERROR_SHOWN = 4,

  kMaxValue = ERROR_SHOWN,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OptInSilentAuthCode {
  // Silent auth code feature is disabled.
  DISABLED = 0,

  // Silent auth code fetched normally.
  SUCCESS = 1,

  // HTTP Context cannot be prepared.
  CONTEXT_NOT_READY = 2,

  // No LST token is available.
  NO_LST_TOKEN = 3,

  // Silent auth code failed due sever HTTP error 5XX.
  HTTP_SERVER_FAILURE = 4,

  // Silent auth code failed due client HTTP error 4XX.
  HTTP_CLIENT_FAILURE = 5,

  // Silent auth code failed due unknown HTTP error.
  HTTP_UNKNOWN_FAILURE = 6,

  // Cannot parse HTTP response.
  RESPONSE_PARSE_FAILURE = 7,

  // No Auth code in response.
  NO_AUTH_CODE_IN_RESPONSE = 8,

  // The network was configured with a mandatory PAC script that could not be
  // fetched, parsed or executed.
  MANDATORY_PROXY_CONFIGURATION_FAILED = 9,

  kMaxValue = MANDATORY_PROXY_CONFIGURATION_FAILED,
};

// The values should be listed in ascending order. They are also persisted to
// logs, and their values should therefore never be renumbered nor reused.
enum class ProvisioningStatus {
  // Provisioning was successful.
  SUCCESS = 0,

  // Unclassified failure.
  UNKNOWN_ERROR = 1,

  // Unmanaged sign-in error.
  GMS_SIGN_IN_ERROR = 2,

  // Check in error.
  GMS_CHECK_IN_ERROR = 3,

  // Managed sign-in error.
  CLOUD_PROVISION_FLOW_ERROR = 4,

  // Mojo errors.
  MOJO_VERSION_MISMATCH = 5,

  // ARC did not finish provisioning within a reasonable amount of time.
  GENERIC_PROVISIONING_TIMEOUT = 6,

  // ARC instance did not report provisioning status within a reasonable amount
  // of time.
  CHROME_PROVISIONING_TIMEOUT = 7,

  // ARC instance is stopped during the sign in procedure.
  ARC_STOPPED = 8,

  // ARC is not enabled.
  ARC_DISABLED = 9,

  // In Chrome, server communication error occurs.
  // For backward compatibility, the UMA is handled differently. Please see
  // ArcSessionManager::OnProvisioningFinished for details.
  CHROME_SERVER_COMMUNICATION_ERROR = 10,

  // Network connection is unavailable in ARC.
  NO_NETWORK_CONNECTION = 11,

  // Account type is not supported for authorization.
  UNSUPPORTED_ACCOUNT_TYPE = 12,

  // Account is not present in Chrome OS Account Manager.
  CHROME_ACCOUNT_NOT_FOUND = 13,

  kMaxValue = CHROME_ACCOUNT_NOT_FOUND,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OptInFlowResult : int {
  // OptIn has started.
  STARTED = 0,

  // OptIn has been succeeded, this also includes succeeded with error cases.
  SUCCEEDED = 1,

  // OptIn has been succeeded but with retry after an error.
  SUCCEEDED_AFTER_RETRY = 2,

  // OptIn has been canceled, this also includes canceled after error cases.
  CANCELED = 3,

  // OptIn has been canceled after an error was reported.
  CANCELED_AFTER_ERROR = 4,

  kMaxValue = CANCELED_AFTER_ERROR,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ArcProvisioningCheckinResult {
  kSuccess = 0,
  kUnknownError = 1,
  kTimeout = 2,
  kMaxValue = kTimeout,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ArcProvisioningDpcResult {
  kSuccess = 0,
  kUnknownError = 1,
  kInvalidToken = 2,
  kAccountAddFail = 3,
  kTimeout = 4,
  kNetworkError = 5,
  kOAuthAuthException = 6,
  kOAuthIOException = 7,
  kMaxValue = kOAuthIOException,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ArcProvisioningSigninResult {
  kSuccess = 0,
  kUnknownError = 1,
  kNetworkError = 2,
  kServiceUnavailable = 3,
  kAuthFailure = 4,
  kTimeout = 5,
  kMaxValue = kTimeout,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ArcEnabledState {
  // ARC++ is enabled for non-managed case.
  ENABLED_NOT_MANAGED = 0,

  // ARC++ is disabled for non-managed case.
  DISABLED_NOT_MANAGED = 1,

  // ARC++ is enabled for managed case when ARC++ is forced on.
  ENABLED_MANAGED_ON = 2,

  // ARC++ is disabled for managed case when ARC++ is forced on. This can happen
  // when user declines ToS even if ARC++ is forced on.
  DISABLED_MANAGED_ON = 3,

  // ARC++ is disabled for managed case when ARC++ is forced off.
  DISABLED_MANAGED_OFF = 4,

  // ARC++ is enabled in case ARC++ is not allowed for the device. This can
  // happen for ARC++ kiosk mode.
  ENABLED_NOT_ALLOWED = 5,

  // ARC++ is disabled and ARC++ is not allowed for the device.
  DISABLED_NOT_ALLOWED = 6,

  kMaxValue = DISABLED_NOT_ALLOWED,
};

// Called from the Chrome OS metrics provider to record Arc.StateByUserType
// strictly once per every metrics recording interval. This way they are in
// every record uploaded to the server and therefore can be used to split and
// compare analysis data for all other metrics.
// TODO(shaochuan): Decouple profile-related logic and move recording to
// components/arc.
void UpdateEnabledStateByUserTypeUMA();

void UpdateOptInActionUMA(OptInActionType type);
void UpdateOptInCancelUMA(OptInCancelReason reason);
void UpdateOptInFlowResultUMA(OptInFlowResult result);
void UpdateOptInNetworkErrorActionUMA(OptInNetworkErrorActionType type);
void UpdateProvisioningStatusUMA(ProvisioningStatus status,
                                 const Profile* profile);
void UpdateProvisioningDpcResultUMA(ArcProvisioningDpcResult result,
                                    const Profile* profile);
void UpdateProvisioningSigninResultUMA(ArcProvisioningSigninResult result,
                                       const Profile* profile);
void UpdateProvisioningCheckinResultUMA(ArcProvisioningCheckinResult result,
                                        const Profile* profile);
void UpdateSecondarySigninResultUMA(ProvisioningStatus status);
void UpdateProvisioningTiming(const base::TimeDelta& elapsed_time,
                              bool success,
                              const Profile* profile);
void UpdateReauthorizationResultUMA(ProvisioningStatus status,
                                    const Profile* profile);
void UpdatePlayAutoInstallRequestState(mojom::PaiFlowState state,
                                       const Profile* profile);
void UpdatePlayAutoInstallRequestTime(const base::TimeDelta& elapsed_time,
                                      const Profile* profile);
void UpdateArcUiAvailableTime(const base::TimeDelta& elapsed_time,
                              const std::string& mode,
                              const Profile* profile);
void UpdatePlayStoreLaunchTime(const base::TimeDelta& elapsed_time);
void UpdateDeferredLaunchTime(const base::TimeDelta& elapsed_time);
void UpdateOptinTosLoadResultUMA(bool success);
void UpdateSilentAuthCodeUMA(OptInSilentAuthCode state);
void UpdateSupervisionTransitionResultUMA(mojom::ManagementChangeStatus result);
void UpdateReauthorizationSilentAuthCodeUMA(OptInSilentAuthCode state);
void UpdateSecondaryAccountSilentAuthCodeUMA(OptInSilentAuthCode state);
void UpdateAuthTiming(const char* histogram_name,
                      base::TimeDelta elapsed_time,
                      const Profile* profile);
void UpdateAuthCheckinAttempts(int32_t num_attempts, const Profile* profile);
void UpdateAuthAccountCheckStatus(mojom::AccountCheckStatus status,
                                  const Profile* profile);
void UpdateAccountReauthReason(mojom::ReauthReason reason,
                               const Profile* profile);
void UpdateMainAccountResolutionStatus(
    const Profile* profile,
    mojom::MainAccountResolutionStatus status);

// Returns the enum for use in UMA stat for reporting DPC errors.
ArcProvisioningDpcResult GetDpcErrorResult(
    mojom::CloudProvisionFlowError error);
// Returns the enum for use in UMA stat for reporting signin errors.
ArcProvisioningSigninResult GetSigninErrorResult(mojom::GMSSignInError error);
// Returns the enum for use in UMA stat for reporting checkin errors.
ArcProvisioningCheckinResult GetCheckinErrorResult(
    mojom::GMSCheckInError error);

// Returns the enum for use in UMA stat and displaying error code on the UI.
// This enum should not be used anywhere else. Please work with the object
// instead.
ProvisioningStatus GetProvisioningStatus(
    const ArcProvisioningResult& provisioning_result);

// Outputs the stringified |result| to |os|. This is only for logging purposes.
std::ostream& operator<<(std::ostream& os, const ProvisioningStatus& status);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ARC_OPTIN_UMA_H_
