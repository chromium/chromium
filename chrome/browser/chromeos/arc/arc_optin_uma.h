// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_OPTIN_UMA_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_OPTIN_UMA_H_

#include <ostream>

#include "components/arc/mojom/app.mojom.h"
#include "components/arc/mojom/auth.mojom.h"

class Profile;

namespace base {
class TimeDelta;
}

namespace arc {

// These enums are used to define the buckets for an enumerated UMA histogram
// and need to be synced with tools/metrics/histograms/enums.xml. Note that
// values 0, 1, 2, 3 and 4 are now deprecated.
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

  // Cloud provision flow failed.
  CLOUD_PROVISION_FLOW_FAIL = 6,

  // Android management is required for user.
  ANDROID_MANAGEMENT_REQUIRED = 7,

  // Cannot start ARC because it is busy.
  SESSION_BUSY = 8,

  kMaxValue = SESSION_BUSY,
};

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

  kMaxValue = NO_AUTH_CODE_IN_RESPONSE,
};

// The values should be listed in ascending order. They are also persisted to
// logs, and their values should therefore never be renumbered nor reused. For
// detailed meaning, please consult auth.mojom.
enum class ProvisioningResult : int {
  // Provisioning was successful. Note, SUCCESS_ALREADY_PROVISIONED is also
  // successful state.
  SUCCESS = 0,

  // Unclassified failure.
  UNKNOWN_ERROR = 1,

  // GMS errors. More errors defined below.
  GMS_NETWORK_ERROR = 2,
  GMS_SERVICE_UNAVAILABLE = 3,
  GMS_BAD_AUTHENTICATION = 4,

  // Check in error. More errors defined below.
  DEVICE_CHECK_IN_FAILED = 5,

  // Cloud provision error. More errors defined below.
  CLOUD_PROVISION_FLOW_FAILED = 6,

  // Mojo errors.
  MOJO_VERSION_MISMATCH = 7,
  MOJO_CALL_TIMEOUT = 8,

  // Check in error.
  DEVICE_CHECK_IN_TIMEOUT = 9,
  DEVICE_CHECK_IN_INTERNAL_ERROR = 10,

  // GMS errors:
  GMS_SIGN_IN_FAILED = 11,
  GMS_SIGN_IN_TIMEOUT = 12,
  GMS_SIGN_IN_INTERNAL_ERROR = 13,

  // Cloud provision error:
  CLOUD_PROVISION_FLOW_TIMEOUT = 14,
  CLOUD_PROVISION_FLOW_INTERNAL_ERROR = 15,

  // ARC instance is stopped during the sign in procedure.
  ARC_STOPPED = 16,

  // ARC instance did not report sign in status within a reasonable amount of
  // time.
  OVERALL_SIGN_IN_TIMEOUT = 17,

  // In Chrome, server communication error occurs.
  // For backward compatibility, the UMA is handled differently. Please see
  // ArcSessionManager::OnProvisioningFinished for details.
  CHROME_SERVER_COMMUNICATION_ERROR = 18,

  // Network connection is unavailable in ARC.
  NO_NETWORK_CONNECTION = 19,

  // ARC is not enabled.
  ARC_DISABLED = 20,

  // Device was already provisioned.
  SUCCESS_ALREADY_PROVISIONED = 21,

  // Account type is not supported for authorization.
  UNSUPPORTED_ACCOUNT_TYPE = 22,

  // Account is not present in Chrome OS Account Manager.
  CHROME_ACCOUNT_NOT_FOUND = 23,

  kMaxValue = CHROME_ACCOUNT_NOT_FOUND,
};

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
void UpdateProvisioningResultUMA(ProvisioningResult result,
                                 const Profile* profile);
void UpdateSecondarySigninResultUMA(ProvisioningResult result);
void UpdateProvisioningTiming(const base::TimeDelta& elapsed_time,
                              bool success,
                              const Profile* profile);
void UpdateReauthorizationResultUMA(ProvisioningResult result,
                                    const Profile* profile);
void UpdatePlayAutoInstallRequestState(mojom::PaiFlowState state,
                                       const Profile* profile);
void UpdatePlayAutoInstallRequestTime(const base::TimeDelta& elapsed_time,
                                      const Profile* profile);
void UpdateArcUiAvailableTime(const base::TimeDelta& elapsed_time,
                              const std::string& mode,
                              const Profile* profile);
void UpdatePlayStoreLaunchTime(const base::TimeDelta& elapsed_time);
// TODO(khmel): Remove this in favor of UpdateArcUiAvailableTime once it is
// rolled and has confirmed usability.
void UpdatePlayStoreShownTimeDeprecated(const base::TimeDelta& elapsed_time,
                                        const Profile* profile);
void UpdateSilentAuthCodeUMA(OptInSilentAuthCode state);
void UpdateSupervisionTransitionResultUMA(
    mojom::SupervisionChangeStatus result);
void UpdateReauthorizationSilentAuthCodeUMA(OptInSilentAuthCode state);
void UpdateSecondaryAccountSilentAuthCodeUMA(OptInSilentAuthCode state);
void UpdateAuthTiming(const char* histogram_name, base::TimeDelta elapsed_time);
void UpdateAuthCheckinAttempts(int32_t num_attempts);
void UpdateAuthAccountCheckStatus(mojom::AccountCheckStatus status);
void UpdateMainAccountResolutionStatus(
    const Profile* profile,
    mojom::MainAccountResolutionStatus status);

// Outputs the stringified |result| to |os|. This is only for logging purposes.
std::ostream& operator<<(std::ostream& os, const ProvisioningResult& result);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_OPTIN_UMA_H_
