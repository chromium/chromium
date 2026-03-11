// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SAML_PUBLIC_SAML_URL_FETCHER_H_
#define CHROME_BROWSER_ASH_LOGIN_SAML_PUBLIC_SAML_URL_FETCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/device_management_service.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {
class BrowserPolicyConnectorAsh;
struct DMServerJobResult;
}  // namespace policy

namespace ash {

// This class handles sending request for public SAML session URL to DM
// server, waits for the response and retrieves the redirect URL from it.
class PublicSamlUrlFetcher {
 public:
  // `browser_policy_connector_ash` must be non-null and must outlive `this`.
  // `shared_url_loader_factory` must be non-null.
  PublicSamlUrlFetcher(
      policy::BrowserPolicyConnectorAsh* browser_policy_connector_ash,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      AccountId account_id);

  PublicSamlUrlFetcher(const PublicSamlUrlFetcher&) = delete;
  PublicSamlUrlFetcher& operator=(const PublicSamlUrlFetcher&) = delete;

  ~PublicSamlUrlFetcher();

  // Sends request to the DM server, gets and checks the response and
  // calls the callback.
  void Fetch(base::OnceClosure callback);
  std::string GetRedirectUrl();
  bool FetchSucceeded();

 private:
  // Response from DM server. Calls the stored FetchCallback or initiates the
  // SAML flow.
  void OnPublicSamlUrlReceived(policy::DMServerJobResult result);

  const raw_ref<policy::BrowserPolicyConnectorAsh>
      browser_policy_connector_ash_;
  const scoped_refptr<network::SharedURLLoaderFactory>
      shared_url_loader_factory_;

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
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SAML_PUBLIC_SAML_URL_FETCHER_H_
