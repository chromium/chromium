// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cbcm_invalidations_initializer.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/device_account_initializer.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

// A helper class to make the appropriate calls into the device account
// initializer and manage the ChromeBrowserCloudManagementRegistrar callback's
// lifetime.
class CBCMInvalidationsInitializer::MachineLevelDeviceAccountInitializerHelper
    : public DeviceAccountInitializer::Delegate {
 public:
  using Callback = base::OnceCallback<void(bool)>;

  // |policy_client| should be registered and outlive this object.
  MachineLevelDeviceAccountInitializerHelper(
      const std::string& service_account_email,
      policy::CloudPolicyClient* policy_client,
      Callback callback,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : service_account_email_(service_account_email),
        policy_client_(policy_client),
        callback_(std::move(callback)),
        url_loader_factory_(url_loader_factory) {
    DCHECK(url_loader_factory_);

    device_account_initializer_ =
        std::make_unique<DeviceAccountInitializer>(policy_client_, this);
    device_account_initializer_->FetchToken();
  }

  MachineLevelDeviceAccountInitializerHelper& operator=(
      MachineLevelDeviceAccountInitializerHelper&) = delete;
  MachineLevelDeviceAccountInitializerHelper(
      MachineLevelDeviceAccountInitializerHelper&) = delete;
  MachineLevelDeviceAccountInitializerHelper(
      MachineLevelDeviceAccountInitializerHelper&&) = delete;

  ~MachineLevelDeviceAccountInitializerHelper() override = default;

  // DeviceAccountInitializer::Delegate:
  void OnDeviceAccountTokenFetched(bool empty_token) override {
    if (empty_token) {
      // Not being able to obtain a token isn't a showstopper for machine
      // level policies: the browser will fallback to fetching policies on a
      // regular schedule and won't support remote commands. Getting a refresh
      // token will be reattempted on the next successful policy fetch.
      std::move(callback_).Run(false);
      return;
    }

    device_account_initializer_->StoreToken();
  }

  void OnDeviceAccountTokenStored() override {
    // When the token is stored, the account init procedure is complete and
    // it's now time to save the associated email address.
    DeviceOAuth2TokenServiceFactory::Get()->SetServiceAccountEmail(
        service_account_email_);
    std::move(callback_).Run(true);
  }

  void OnDeviceAccountTokenFetchError(
      std::optional<DeviceManagementStatus> /*dm_status*/) override {
    std::move(callback_).Run(false);
  }

  void OnDeviceAccountTokenStoreError() override {
    std::move(callback_).Run(false);
  }

  void OnDeviceAccountClientError(DeviceManagementStatus status) override {
    std::move(callback_).Run(false);
  }

  enterprise_management::DeviceServiceApiAccessRequest::DeviceType
  GetRobotAuthCodeDeviceType() override {
    return enterprise_management::DeviceServiceApiAccessRequest::CHROME_BROWSER;
  }

  std::set<std::string> GetRobotOAuthScopes() override {
    return {
        GaiaConstants::kGoogleUserInfoEmail,
        GaiaConstants::kFCMOAuthScope,
    };
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return url_loader_factory_;
  }

  std::string service_account_email_;
  raw_ptr<policy::CloudPolicyClient> policy_client_;
  std::unique_ptr<DeviceAccountInitializer> device_account_initializer_;
  Callback callback_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

CBCMInvalidationsInitializer::CBCMInvalidationsInitializer(Delegate* delegate)
    : delegate_(delegate) {}

CBCMInvalidationsInitializer::~CBCMInvalidationsInitializer() = default;

void CBCMInvalidationsInitializer::OnServiceAccountSet(
    CloudPolicyClient* client,
    const std::string& account_email) {
  // If there's no invalidations service active yet, now's the time to start it.
  // It will be notified when the service account for is ready to be used.
  if (!delegate_->IsInvalidationsServiceStarted())
    delegate_->StartInvalidations();

  // If there's no refresh token when a policy has a service account, or the
  // service account in the policy doesn't match the one in the token service,
  // the service account has to be initialized to the one in the policy.
  if (!DeviceOAuth2TokenServiceFactory::Get()->RefreshTokenIsAvailable() ||
      DeviceOAuth2TokenServiceFactory::Get()->GetRobotAccountId() !=
          CoreAccountId::FromRobotEmail(account_email)) {
    // Initialize the device service account and fetch auth codes to exchange
    // for a refresh token. Creating this object starts that process and the
    // callback will be called from it whether it succeeds or not.
    account_initializer_helper_ =
        std::make_unique<MachineLevelDeviceAccountInitializerHelper>(
            account_email, client,
            base::BindOnce(&CBCMInvalidationsInitializer::AccountInitCallback,
                           base::Unretained(this), account_email),
            delegate_->GetURLLoaderFactory()
                ? delegate_->GetURLLoaderFactory()
                : g_browser_process->system_network_context_manager()
                      ->GetSharedURLLoaderFactory());
  }
}

void CBCMInvalidationsInitializer::AccountInitCallback(
    const std::string& account_email,
    bool success) {
  account_initializer_helper_.reset();
  if (!success) {
    DVLOG(1)
        << "There was an error initializing the service account with email: "
        << account_email;
  }
}

}  // namespace policy
