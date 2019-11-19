// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_ACTIVE_DIRECTORY_ENROLLMENT_TOKEN_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_ACTIVE_DIRECTORY_ENROLLMENT_TOKEN_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/chromeos/arc/auth/arc_fetcher_base.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"

namespace enterprise_management {
class DeviceManagementResponse;
}  // namespace enterprise_management

namespace policy {
class DMTokenStorage;
}  // namespace policy

namespace arc {

// Fetches an enrollment token and user id for a new managed Google Play account
// when using ARC with Active Directory.
class ArcActiveDirectoryEnrollmentTokenFetcher
    : public ArcFetcherBase,
      public ArcSupportHost::AuthDelegate {
 public:
  explicit ArcActiveDirectoryEnrollmentTokenFetcher(
      ArcSupportHost* support_host);
  ~ArcActiveDirectoryEnrollmentTokenFetcher() override;

  enum class Status {
    SUCCESS,       // The fetch was successful.
    FAILURE,       // The request failed.
    ARC_DISABLED,  // ARC is not enabled.
  };

  // Fetches the enrollment token and user id in the background and calls
  // |callback| when done. |status| indicates whether the operation was
  // successful. In case of success, |enrollment_token| and |user_id| are set to
  // the fetched values.
  // Fetch() should be called once per instance, and it is expected that the
  // inflight operation is cancelled without calling the |callback| when the
  // instance is deleted.
  using FetchCallback =
      base::OnceCallback<void(Status status,
                              const std::string& enrollment_token,
                              const std::string& user_id)>;
  void Fetch(FetchCallback callback);

 private:
  // Called when the |dm_token| is retrieved from policy::DMTokenStorage.
  // Triggers DoFetchEnrollmentToken().
  void OnDMTokenAvailable(const std::string& dm_token);

  // ArcSupportHost::AuthDelegate:
  void OnAuthSucceeded() override;
  void OnAuthFailed(const std::string& error_msg) override;
  void OnAuthRetryClicked() override;

  // Sends a request to fetch an enrollment token from DM server.
  void DoFetchEnrollmentToken();

  // Response from DM server. Calls the stored FetchCallback or initiates the
  // SAML flow.
  void OnEnrollmentTokenResponseReceived(
      policy::DeviceManagementService::Job* job,
      policy::DeviceManagementStatus dm_status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Sends |auth_redirect_url| to the ArcSupportHost instance, which displays
  // it in a web view and checks whether authentication succeeded. Calls
  // CancelSamlFlow() if the url is invalid.
  void InitiateSamlFlow(const std::string& auth_redirect_url);

  // Calls callback_ with an error status and resets state.
  void CancelSamlFlow();

  ArcSupportHost* const support_host_ = nullptr;  // Not owned.

  std::unique_ptr<policy::DeviceManagementService::Job> fetch_request_job_;
  std::unique_ptr<policy::DMTokenStorage> dm_token_storage_;
  FetchCallback callback_;

  std::string dm_token_;

  // Current SAML auth session id, stored during SAML authentication.
  std::string auth_session_id_;

  base::WeakPtrFactory<ArcActiveDirectoryEnrollmentTokenFetcher>
      weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcActiveDirectoryEnrollmentTokenFetcher);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_ACTIVE_DIRECTORY_ENROLLMENT_TOKEN_FETCHER_H_
