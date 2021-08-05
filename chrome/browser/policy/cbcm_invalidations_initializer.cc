// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cbcm_invalidations_initializer.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/device_account_initializer.h"
#include "components/policy/core/common/features.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

namespace {

// A helper class to make the appropriate calls into the device account
// initializer and manage the ChromeBrowserCloudManagementRegistrar callback's
// lifetime.
class MachineLevelDeviceAccountInitializerHelper
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
    DCHECK(base::FeatureList::IsEnabled(
        policy::features::kCBCMPolicyInvalidations));

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
    DCHECK(base::FeatureList::IsEnabled(
        policy::features::kCBCMPolicyInvalidations))
        << "DeviceAccountInitializer is active but CBCM service accounts "
           "are not enabled.";
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
    DCHECK(base::FeatureList::IsEnabled(
        policy::features::kCBCMPolicyInvalidations))
        << "DeviceAccountInitializer is active but CBCM service accounts "
           "are not enabled.";
    // When the token is stored, the account init procedure is complete and
    // it's now time to save the associated email address.
    DeviceOAuth2TokenServiceFactory::Get()->SetServiceAccountEmail(
        service_account_email_);
    std::move(callback_).Run(true);
  }

  void OnDeviceAccountTokenError(EnrollmentStatus status) override {
    DCHECK(base::FeatureList::IsEnabled(
        policy::features::kCBCMPolicyInvalidations))
        << "DeviceAccountInitializer is active but CBCM service accounts "
           "are not enabled.";
    std::move(callback_).Run(false);
  }

  void OnDeviceAccountClientError(DeviceManagementStatus status) override {
    DCHECK(base::FeatureList::IsEnabled(
        policy::features::kCBCMPolicyInvalidations))
        << "DeviceAccountInitializer is active but CBCM service accounts "
           "are not enabled.";
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
  policy::CloudPolicyClient* policy_client_;
  std::unique_ptr<DeviceAccountInitializer> device_account_initializer_;
  Callback callback_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace

CBCMInvalidationsInitializer::CBCMInvalidationsInitializer(Delegate* delegate)
    : delegate_(delegate) {}

CBCMInvalidationsInitializer::~CBCMInvalidationsInitializer() = default;

void CBCMInvalidationsInitializer::OnServiceAccountSet(
    CloudPolicyClient* client,
    const std::string& account_email) {
  if (!base::FeatureList::IsEnabled(
          policy::features::kCBCMPolicyInvalidations)) {
    return;
  }

  // No need to get a refresh token if there is one present already.
  if (!DeviceOAuth2TokenServiceFactory::Get()->RefreshTokenIsAvailable()) {
    // If this feature is enabled, we need to ensure the device service
    // account is initialized and fetch auth codes to exchange for a refresh
    // token. Creating this object starts that process and the callback will
    // be called from it whether it succeeds or not.
    account_initializer_helper_ =
        std::make_unique<MachineLevelDeviceAccountInitializerHelper>(
            account_email, client,
            base::BindOnce(&CBCMInvalidationsInitializer::AccountInitCallback,
                           base::Unretained(this), account_email),
            delegate_->GetURLLoaderFactory()
                ? delegate_->GetURLLoaderFactory()
                : g_browser_process->system_network_context_manager()
                      ->GetSharedURLLoaderFactory());
  } else if (!delegate_->IsInvalidationsServiceStarted()) {
    // There's already a refresh token available but invalidations aren't
    // running yet which means this is browser startup and the refresh token was
    // retrieved from local storage. It's OK to start invalidations now.
    delegate_->StartInvalidations();
  }
}

void CBCMInvalidationsInitializer::AccountInitCallback(
    const std::string& account_email,
    bool success) {
  account_initializer_helper_.reset();
  if (success)
    delegate_->StartInvalidations();
}

}  // namespace policy
