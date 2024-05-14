// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/tpm_challenge_key_result.h"

#include <ostream>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/values.h"

namespace ash {
namespace attestation {
namespace {
std::string Base64EncodeStr(const std::string& str) {
  return base::Base64Encode(str);
}
}  // namespace

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
const char TpmChallengeKeyResult::kExtensionNotAllowedErrorMsg[] =
    "The extension does not have permission to call this function.";
const char TpmChallengeKeyResult::kChallengeBadBase64ErrorMsg[] =
    "Challenge is not base64 encoded.";
const char TpmChallengeKeyResult::kDeviceWebBasedAttestationNotOobeErrorMsg[] =
    "Device web based attestation is only available on the OOBE screen.";
const char TpmChallengeKeyResult::kGetPublicKeyFailedErrorMsg[] =
    "Failed to get public key.";
const char TpmChallengeKeyResult::kMarkCorporateKeyFailedErrorMsg[] =
    "Failed to mark key as corporate.";
const char TpmChallengeKeyResult::kAttestationServiceInternalErrorMsg[] =
    "OS platform service internal error.";
const char TpmChallengeKeyResult::kUploadCertificateFailedErrorMsg[] =
    "Failed to upload machine certificate.";
const char TpmChallengeKeyResult::kDeviceTrustURLConflictError[] =
    "Both policies DeviceContextAwareAccessSignalsAllowlist and "
    "DeviceWebBasedAttestationAllowedUrls are enabled for this URL.";
const char TpmChallengeKeyResult::kVerifiedAccessFlowUnsupportedErrorMsg[] =
    "Verified Access flow type is not supported on ChromeOS.";

// static
TpmChallengeKeyResult TpmChallengeKeyResult::MakeChallengeResponse(
    const std::string& challenge_response) {
  return TpmChallengeKeyResult{
      /*result_code=*/TpmChallengeKeyResultCode::kSuccess,
      /*public_key=*/"",
      /*challenge_response=*/challenge_response};
}

// static
TpmChallengeKeyResult TpmChallengeKeyResult::MakePublicKey(
    const std::string& public_key) {
  return TpmChallengeKeyResult{
      /*result_code=*/TpmChallengeKeyResultCode::kSuccess,
      /*public_key=*/public_key,
      /*challenge_response=*/""};
}

// static
TpmChallengeKeyResult TpmChallengeKeyResult::MakeSuccess() {
  return TpmChallengeKeyResult{
      /*result_code=*/TpmChallengeKeyResultCode::kSuccess,
      /*public_key=*/"",
      /*challenge_response=*/""};
}

// static
TpmChallengeKeyResult TpmChallengeKeyResult::MakeError(
    TpmChallengeKeyResultCode error_code) {
  DCHECK_NE(error_code, TpmChallengeKeyResultCode::kSuccess);
  return TpmChallengeKeyResult{/*result_code=*/error_code,
                               /*public_key=*/"",
                               /*challenge_response=*/""};
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
    case TpmChallengeKeyResultCode::kExtensionNotAllowedError:
      return kExtensionNotAllowedErrorMsg;
    case TpmChallengeKeyResultCode::kChallengeBadBase64Error:
      return kChallengeBadBase64ErrorMsg;
    case TpmChallengeKeyResultCode::kDeviceWebBasedAttestationNotOobeError:
      return kDeviceWebBasedAttestationNotOobeErrorMsg;
    case TpmChallengeKeyResultCode::kGetPublicKeyFailedError:
      return kGetPublicKeyFailedErrorMsg;
    case TpmChallengeKeyResultCode::kMarkCorporateKeyFailedError:
      return kMarkCorporateKeyFailedErrorMsg;
    case TpmChallengeKeyResultCode::kAttestationServiceInternalError:
      return kAttestationServiceInternalErrorMsg;
    case TpmChallengeKeyResultCode::kUploadCertificateFailedError:
      return kUploadCertificateFailedErrorMsg;
    case TpmChallengeKeyResultCode::kDeviceTrustURLConflictError:
      return kDeviceTrustURLConflictError;
    case TpmChallengeKeyResultCode::kVerifiedAccessFlowUnsupportedError:
      return kVerifiedAccessFlowUnsupportedErrorMsg;
    case TpmChallengeKeyResultCode::kSuccess:
      // Not an error message.
      NOTREACHED_IN_MIGRATION();
      return "";
  }
  NOTREACHED_IN_MIGRATION() << static_cast<int>(result_code);
}

bool TpmChallengeKeyResult::IsSuccess() const {
  return result_code == TpmChallengeKeyResultCode::kSuccess;
}

bool TpmChallengeKeyResult::operator==(
    const TpmChallengeKeyResult& other) const {
  return ((result_code == other.result_code) &&
          (public_key == other.public_key) &&
          (challenge_response == other.challenge_response));
}

bool TpmChallengeKeyResult::operator!=(
    const TpmChallengeKeyResult& other) const {
  return !(*this == other);
}

std::ostream& operator<<(std::ostream& os,
                         const TpmChallengeKeyResult& result) {
  base::Value::Dict value;

  value.Set("result_code", static_cast<int>(result.result_code));
  if (!result.IsSuccess()) {
    value.Set("error_message", result.GetErrorMessage());
  }
  value.Set("public_key", Base64EncodeStr(result.public_key));
  value.Set("challenge_response", Base64EncodeStr(result.challenge_response));

  os << value;
  return os;
}

}  // namespace attestation
}  // namespace ash
