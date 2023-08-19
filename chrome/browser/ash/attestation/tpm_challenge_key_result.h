// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_TPM_CHALLENGE_KEY_RESULT_H_
#define CHROME_BROWSER_ASH_ATTESTATION_TPM_CHALLENGE_KEY_RESULT_H_

#include <ostream>
#include <string>

namespace ash {
namespace attestation {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TpmChallengeKeyResultCode {
  // Not a result code. Convenient for UMA metrics.
  kSuccess = 0,
  kDevicePolicyDisabledError = 1,
  kSignChallengeFailedError = 2,
  kUserNotManagedError = 3,
  kKeyRegistrationFailedError = 4,
  kUserKeyNotAvailableError = 5,
  kUserPolicyDisabledError = 6,
  kNonEnterpriseDeviceError = 7,
  kDbusError = 8,
  kUserRejectedError = 9,
  kGetCertificateFailedError = 10,
  kResetRequiredError = 11,
  kAttestationUnsupportedError = 12,
  kTimeoutError = 13,
  kDeviceWebBasedAttestationUrlError = 14,
  kExtensionNotAllowedError = 15,
  kChallengeBadBase64Error = 16,
  kDeviceWebBasedAttestationNotOobeError = 17,
  kGetPublicKeyFailedError = 18,
  kMarkCorporateKeyFailedError = 19,
  kAttestationServiceInternalError = 20,
  kUploadCertificateFailedError = 21,
  kDeviceTrustURLConflictError = 22,
  kVerifiedAccessFlowUnsupportedError = 23,
  kMaxValue = kVerifiedAccessFlowUnsupportedError,
};

// If |IsSuccess| returns false, |result_code| contains error code and
// |public_key| and |challenge_response| are empty.
// Otherwise, if the object was returned from
// |TpmChallengeKeySubtle::PrepareKey|, the |public_key| is filled. If the
// object was returned from |TpmChallengeKey::BuildResponse| or
// |TpmChallengeKeySubtle::SignChallenge|, the |challenge_response| is filled.
struct TpmChallengeKeyResult {
  static const char kDevicePolicyDisabledErrorMsg[];
  static const char kSignChallengeFailedErrorMsg[];
  static const char kUserNotManagedErrorMsg[];
  static const char kKeyRegistrationFailedErrorMsg[];
  static const char kUserPolicyDisabledErrorMsg[];
  static const char kUserKeyNotAvailableErrorMsg[];
  static const char kNonEnterpriseDeviceErrorMsg[];
  static const char kDbusErrorMsg[];
  static const char kUserRejectedErrorMsg[];
  static const char kGetCertificateFailedErrorMsg[];
  static const char kResetRequiredErrorMsg[];
  static const char kAttestationUnsupportedErrorMsg[];
  static const char kTimeoutErrorMsg[];
  static const char kDeviceWebBasedAttestationUrlErrorMsg[];
  static const char kExtensionNotAllowedErrorMsg[];
  static const char kChallengeBadBase64ErrorMsg[];
  static const char kDeviceWebBasedAttestationNotOobeErrorMsg[];
  static const char kGetPublicKeyFailedErrorMsg[];
  static const char kMarkCorporateKeyFailedErrorMsg[];
  static const char kAttestationServiceInternalErrorMsg[];
  static const char kUploadCertificateFailedErrorMsg[];
  static const char kDeviceTrustURLConflictError[];
  static const char kVerifiedAccessFlowUnsupportedErrorMsg[];

  static TpmChallengeKeyResult MakeChallengeResponse(
      const std::string& challenge_response);
  static TpmChallengeKeyResult MakePublicKey(const std::string& public_key);
  static TpmChallengeKeyResult MakeSuccess();
  static TpmChallengeKeyResult MakeError(TpmChallengeKeyResultCode error_code);

  const char* GetErrorMessage() const;
  bool IsSuccess() const;

  bool operator==(const TpmChallengeKeyResult& other) const;
  bool operator!=(const TpmChallengeKeyResult& other) const;

  TpmChallengeKeyResultCode result_code = TpmChallengeKeyResultCode::kSuccess;
  std::string public_key;
  std::string challenge_response;
};

// For unit tests and debugging.
std::ostream& operator<<(std::ostream& os, const TpmChallengeKeyResult& result);

}  // namespace attestation
}  // namespace ash

#endif  //  CHROME_BROWSER_ASH_ATTESTATION_TPM_CHALLENGE_KEY_RESULT_H_
