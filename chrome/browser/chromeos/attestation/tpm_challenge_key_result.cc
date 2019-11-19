// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/tpm_challenge_key_result.h"

#include "base/logging.h"

namespace chromeos {
namespace attestation {

// These messages are exposed to the extensions that using
// chrome.enterprise.platformKeys API. Someone can rely on exectly these
// strings. Should be changed carefully.
const char TpmChallengeKeyResult::kDevicePolicyDisabledErrorMsg[] =
    "Remote attestation is not enabled for your device.";
const char TpmChallengeKeyResult::kSignChallengeFailedErrorMsg[] =
    "Failed to sign the challenge.";
const char TpmChallengeKeyResult::kUserNotManagedErrorMsg[] =
    "The user account is not enterprise managed.";
const char TpmChallengeKeyResult::kKeyRegistrationFailedErrorMsg[] =
    "Key registration failed.";
const char TpmChallengeKeyResult::kUserPolicyDisabledErrorMsg[] =
    "Remote attestation is not enabled for your account.";
const char TpmChallengeKeyResult::kUserKeyNotAvailableErrorMsg[] =
    "User keys cannot be challenged in this profile.";
const char TpmChallengeKeyResult::kNonEnterpriseDeviceErrorMsg[] =
    "The device is not enterprise enrolled.";

const char TpmChallengeKeyResult::kDbusErrorMsg[] =
    "Failed to get Enterprise certificate. Error code = 1";
const char TpmChallengeKeyResult::kUserRejectedErrorMsg[] =
    "Failed to get Enterprise certificate. Error code = 2";
const char TpmChallengeKeyResult::kGetCertificateFailedErrorMsg[] =
    "Failed to get Enterprise certificate. Error code = 3";
const char TpmChallengeKeyResult::kResetRequiredErrorMsg[] =
    "Failed to get Enterprise certificate. Error code = 4";
const char TpmChallengeKeyResult::kAttestationUnsupportedErrorMsg[] =
    "Failed to get Enterprise certificate. Error code = 5";

const char TpmChallengeKeyResult::kTimeoutErrorMsg[] =
    "Device web based attestation failed with timeout error.";
const char TpmChallengeKeyResult::kDeviceWebBasedAttestationUrlErrorMsg[] =
    "Device web based attestation is not enabled for the provided URL.";
const char TpmChallengeKeyResult::kExtensionNotWhitelistedErrorMsg[] =
    "The extension does not have permission to call this function.";
const char TpmChallengeKeyResult::kChallengeBadBase64ErrorMsg[] =
    "Challenge is not base64 encoded.";
const char TpmChallengeKeyResult::kDeviceWebBasedAttestationNotOobeErrorMsg[] =
    "Device web based attestation is only available on the OOBE screen.";

// static
TpmChallengeKeyResult TpmChallengeKeyResult::MakeResult(
    const std::string& success_result) {
  return TpmChallengeKeyResult{
      /*result_code=*/TpmChallengeKeyResultCode::kSuccess,
      /*data=*/success_result};
}

// static
TpmChallengeKeyResult TpmChallengeKeyResult::MakeError(
    TpmChallengeKeyResultCode error_code) {
  return TpmChallengeKeyResult{/*result_code=*/error_code,
                               /*data=*/""};
}

const char* TpmChallengeKeyResult::GetErrorMessage() const {
  switch (result_code) {
    case TpmChallengeKeyResultCode::kDevicePolicyDisabledError:
      return kDevicePolicyDisabledErrorMsg;
    case TpmChallengeKeyResultCode::kSignChallengeFailedError:
      return kSignChallengeFailedErrorMsg;
    case TpmChallengeKeyResultCode::kUserNotManagedError:
      return kUserNotManagedErrorMsg;
    case TpmChallengeKeyResultCode::kKeyRegistrationFailedError:
      return kKeyRegistrationFailedErrorMsg;
    case TpmChallengeKeyResultCode::kUserKeyNotAvailableError:
      return kUserKeyNotAvailableErrorMsg;
    case TpmChallengeKeyResultCode::kUserPolicyDisabledError:
      return kUserPolicyDisabledErrorMsg;
    case TpmChallengeKeyResultCode::kNonEnterpriseDeviceError:
      return kNonEnterpriseDeviceErrorMsg;
    case TpmChallengeKeyResultCode::kDbusError:
      return kDbusErrorMsg;
    case TpmChallengeKeyResultCode::kUserRejectedError:
      return kUserRejectedErrorMsg;
    case TpmChallengeKeyResultCode::kGetCertificateFailedError:
      return kGetCertificateFailedErrorMsg;
    case TpmChallengeKeyResultCode::kResetRequiredError:
      return kResetRequiredErrorMsg;
    case TpmChallengeKeyResultCode::kAttestationUnsupportedError:
      return kAttestationUnsupportedErrorMsg;
    case TpmChallengeKeyResultCode::kTimeoutError:
      return kTimeoutErrorMsg;
    case TpmChallengeKeyResultCode::kDeviceWebBasedAttestationUrlError:
      return kDeviceWebBasedAttestationUrlErrorMsg;
    case TpmChallengeKeyResultCode::kExtensionNotWhitelistedError:
      return kExtensionNotWhitelistedErrorMsg;
    case TpmChallengeKeyResultCode::kChallengeBadBase64Error:
      return kChallengeBadBase64ErrorMsg;
    case TpmChallengeKeyResultCode::kDeviceWebBasedAttestationNotOobeError:
      return kDeviceWebBasedAttestationNotOobeErrorMsg;
    case TpmChallengeKeyResultCode::kSuccess:
      // Not an error message.
      NOTREACHED();
      return "";
  }
  NOTREACHED() << static_cast<int>(result_code);
}

bool TpmChallengeKeyResult::IsSuccess() const {
  return result_code == TpmChallengeKeyResultCode::kSuccess;
}

}  // namespace attestation
}  // namespace chromeos
