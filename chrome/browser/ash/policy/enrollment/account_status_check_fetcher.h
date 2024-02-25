// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ACCOUNT_STATUS_CHECK_FETCHER_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ACCOUNT_STATUS_CHECK_FETCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/cloud/device_management_service.h"

namespace policy {

struct DMServerJobResult;

// This enum is tied directly to the UMA `EnrollmentNudgePolicyFetchResult` enum
// defined in //tools/metrics/histograms/enums.xml, and should always reflect it
// (do not change one without changing the other). Entries should be never
// modified or deleted. Only additions possible.
enum class EnrollmentNudgePolicyFetchResult {
  kUnknown = 0,
  kNoPolicyInResponse = 1,
  kEnrollmentRequired = 2,
  kAllowConsumerSignIn = 3,
  kMaxValue = kAllowConsumerSignIn
};

struct AccountStatus {
  // This enum is tied directly to a UMA enum `EnterpriseAccountStatus` defined
  // in //tools/metrics/histograms/enums.xml, and should always reflect it (do
  // not change one without changing the other). Entries should be never
  // modified or deleted. Only additions possible.
  enum class Type {
    kUnknown = 0,
    kConsumerWithConsumerDomain = 1,
    kConsumerWithBusinessDomain = 2,
    kOrganisationalAccountUnverified = 3,
    kOrganisationalAccountVerified = 4,
    kDasher = 5,
    kMaxValue = kDasher
  };

  Type type = Type::kUnknown;
  bool enrollment_required = false;
};

bool operator==(const AccountStatus&, const AccountStatus&);
bool operator!=(const AccountStatus&, const AccountStatus&);

// This class handles sending request to check account to DM server,
// waits for the response and retrieves the account status from it.
// Provided email should be canonicalized.
class AccountStatusCheckFetcher {
 public:
  // Provided email should be canonicalized.
  explicit AccountStatusCheckFetcher(const std::string& canonicalized_email);
  // Provided email should be canonicalized.
  AccountStatusCheckFetcher(
      const std::string& canonicalized_email,
      DeviceManagementService* service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  AccountStatusCheckFetcher(const AccountStatusCheckFetcher&) = delete;
  AccountStatusCheckFetcher& operator=(const AccountStatusCheckFetcher&) =
      delete;
  ~AccountStatusCheckFetcher();

  using FetchCallback =
      base::OnceCallback<void(bool fetch_succeeded, AccountStatus status)>;

  // Sends request to the DM server, gets and checks the response and
  // calls the callback.
  void Fetch(FetchCallback callback, bool fetch_enrollment_nudge_policy);

 private:
  // Response from DM server.
  void OnAccountStatusCheckReceived(DMServerJobResult result);

  // Account ID, added to the DM server request.
  std::string email_;

  // Job that sends request to the DM server.
  std::unique_ptr<DeviceManagementService::Job> fetch_request_job_;

  raw_ptr<DeviceManagementService, DanglingUntriaged> service_ = nullptr;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Randomly generated device id for the request to make sure request won't
  // send private device information. It's important because request could be
  // sent from not-enrolled device. Google shouldn't know identifiers of
  // not-enrolled devices.
  std::string random_device_id_;

  // Called at the end of Fetch().
  FetchCallback callback_;

  // Indicates whether `AccountStatusCheckFetcher` is currently being used to
  // fetch the value of Enrollment Nudge policy.
  bool is_fetching_enrollment_nudge_policy_ = false;

  base::WeakPtrFactory<AccountStatusCheckFetcher> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ACCOUNT_STATUS_CHECK_FETCHER_H_
