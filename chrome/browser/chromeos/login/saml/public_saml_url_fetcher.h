// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SAML_PUBLIC_SAML_URL_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SAML_PUBLIC_SAML_URL_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"

namespace enterprise_management {
class DeviceManagementResponse;
}  // namespace enterprise_management

namespace chromeos {

// This class handles sending request for public SAML session URL to DM
// server, waits for the response and retrieves the redirect URL from it.
class PublicSamlUrlFetcher {
 public:
  explicit PublicSamlUrlFetcher(AccountId account_id);
  ~PublicSamlUrlFetcher();

  // Sends request to the DM server, gets and checks the response and
  // calls the callback.
  void Fetch(base::OnceClosure callback);
  std::string GetRedirectUrl();
  bool FetchSucceeded();

 private:
  // Response from DM server. Calls the stored FetchCallback or initiates the
  // SAML flow.
  void OnPublicSamlUrlReceived(
      policy::DeviceManagementService::Job* job,
      policy::DeviceManagementStatus dm_status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Account ID, added to the DM server request.
  std::string account_id_;

  // Indicates whether fetching the redirect URL was successful.
  bool fetch_succeeded_ = false;

  // Job that sends request to the DM server.
  std::unique_ptr<policy::DeviceManagementService::Job> fetch_request_job_;

  // The redirect URL.
  std::string redirect_url_;

  // Called at the end of Fetch().
  base::OnceClosure callback_;
  base::WeakPtrFactory<PublicSamlUrlFetcher> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PublicSamlUrlFetcher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SAML_PUBLIC_SAML_URL_FETCHER_H_
