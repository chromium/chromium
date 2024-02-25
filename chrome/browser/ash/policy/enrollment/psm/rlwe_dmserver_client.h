// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_RLWE_DMSERVER_CLIENT_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_RLWE_DMSERVER_CLIENT_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state.h"

namespace policy::psm {

// Indicates all possible PSM protocol results after it has executed
// successfully or terminated because of an error or timeout. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class RlweResult {
  kSuccessfulDetermination = 0,
  kCreateRlweClientLibraryError = 1,
  kCreateOprfRequestLibraryError = 2,
  kCreateQueryRequestLibraryError = 3,
  kProcessingQueryResponseLibraryError = 4,
  kEmptyOprfResponseError = 5,
  kEmptyQueryResponseError = 6,
  kConnectionError = 7,
  kServerError = 8,
  // [deleted] kTimeout = 9,
  kMaxValue = kServerError,
};

// Interface for the PSM RLWE Client which uses DMServer, allowing to replace
// the PSM RLWE DMServer client with a fake for tests.
class RlweDmserverClient {
 public:
  struct ResultHolder final {
    ResultHolder(bool membership_result,
                 base::Time membership_determination_time)
        : psm_result(RlweResult::kSuccessfulDetermination),
          membership_result(membership_result),
          membership_determination_time(membership_determination_time) {}

    explicit ResultHolder(AutoEnrollmentDMServerError dm_server_error)
        : psm_result(dm_server_error.network_error.has_value()
                         ? RlweResult::kConnectionError
                         : RlweResult::kServerError),
          dm_server_error(dm_server_error) {}

    explicit ResultHolder(RlweResult result) : psm_result(result) {
      CHECK_NE(result, RlweResult::kSuccessfulDetermination);
      CHECK_NE(result, RlweResult::kConnectionError);
      CHECK_NE(result, RlweResult::kServerError);
    }

    // Indicate whether an error occurred while executing the PSM protocol.
    bool IsError() const {
      return psm_result != RlweResult::kSuccessfulDetermination;
    }

    RlweResult psm_result;

    // The field has value iff `psm_result` value is
    // `kConnectionError` or `kServerError`.
    std::optional<AutoEnrollmentDMServerError> dm_server_error;

    // These fields have values iff `psm_result` value is
    // `kSuccessfulDetermination`.
    std::optional<bool> membership_result;
    std::optional<base::Time> membership_determination_time;
  };

  // Callback will be triggered after completing the protocol, in case of a
  // successful determination or stopping due to an error.
  using CompletionCallback =
      base::OnceCallback<void(ResultHolder result_holder)>;

  virtual ~RlweDmserverClient() = default;

  // Determines the membership. Then, will call `callback` on
  // successful result or error. Also, the `callback` has to be non-null.
  // Note: This method should be called only when there is no PSM request in
  // progress (i.e. `IsCheckMembershipInProgress` is false).
  virtual void CheckMembership(CompletionCallback callback) = 0;

  // Returns true if the PSM protocol is still running,
  // otherwise false.
  virtual bool IsCheckMembershipInProgress() const = 0;
};

}  // namespace policy::psm

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_RLWE_DMSERVER_CLIENT_H_
