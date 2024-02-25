// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_LICENSE_CHECKER_H_
#define CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_LICENSE_CHECKER_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace plugin_vm {

// PluginVmLicenseChecker ensures that the user has a valid PluginVM License.
class PluginVmLicenseChecker {
 public:
  using LicenseCheckedCallback = base::OnceCallback<void(bool)>;

  explicit PluginVmLicenseChecker(Profile* profile);
  virtual ~PluginVmLicenseChecker();

  PluginVmLicenseChecker(const PluginVmLicenseChecker& other) = delete;
  PluginVmLicenseChecker& operator=(const PluginVmLicenseChecker& other) =
      delete;

  // CheckLicense is an asynchronous call that validates the provided |profile|
  // has a valid license.
  void CheckLicense(LicenseCheckedCallback callback);

 private:
  std::unique_ptr<network::ResourceRequest> CreateResourceRequest(
      std::string_view access_token);

  void FetchAccessToken();
  void HandleStringResponse(std::unique_ptr<std::string> response_body);
  void CallEndpointWithAccessToken(GoogleServiceAuthError error,
                                   signin::AccessTokenInfo access_token_info);

  raw_ptr<Profile> profile_;
  GURL validation_url_;
  net::NetworkTrafficAnnotationTag traffic_annotation_;

  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  LicenseCheckedCallback callback_;

  base::WeakPtrFactory<PluginVmLicenseChecker> weak_ptr_factory_{this};
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_LICENSE_CHECKER_H_
