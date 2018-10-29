// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"

#if !defined(OS_CHROMEOS)
#include "services/network/public/cpp/network_connection_tracker.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
class ForceSigninVerifier;
#endif
class Profile;

class ChromeSigninClient
    : public SigninClient,
#if !defined(OS_CHROMEOS)
      public network::NetworkConnectionTracker::NetworkConnectionObserver,
#endif
      public SigninErrorController::Observer,
      public gaia::GaiaOAuthClient::Delegate,
      public OAuth2TokenService::Consumer {
 public:
  explicit ChromeSigninClient(
      Profile* profile, SigninErrorController* signin_error_controller);
  ~ChromeSigninClient() override;

  void DoFinalInit() override;

  // Utility method.
  static bool ProfileAllowsSigninCookies(Profile* profile);

  // SigninClient implementation.
  PrefService* GetPrefs() override;
  void PreSignOut(
      base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
      signin_metrics::ProfileSignout signout_source_metric) override;
  void OnSignedOut() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  network::mojom::CookieManager* GetCookieManager() override;
  bool IsFirstRun() const override;
  base::Time GetInstallDate() override;
  bool AreSigninCookiesAllowed() override;
  void AddContentSettingsObserver(
      content_settings::Observer* observer) override;
  void RemoveContentSettingsObserver(
      content_settings::Observer* observer) override;
  void DelayNetworkCall(const base::Closure& callback) override;
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      const std::string& source,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      override;

  // Returns a string describing the chrome version environment. Version format:
  // <Build Info> <OS> <Version number> (<Last change>)<channel or "-devel">
  // If version information is unavailable, returns "invalid."
  std::string GetProductVersion() override;
  void OnSignedIn(const std::string& account_id,
                  const std::string& gaia_id,
                  const std::string& username,
                  const std::string& password) override;
  void PostSignedIn(const std::string& account_id,
                    const std::string& username,
                    const std::string& password) override;

  // SigninErrorController::Observer implementation.
  void OnErrorChanged() override;

  // gaia::GaiaOAuthClient::Delegate implementation.
  void OnGetTokenInfoResponse(
      std::unique_ptr<base::DictionaryValue> token_info) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

  // OAuth2TokenService::Consumer implementation
  void OnGetTokenSuccess(
      const OAuth2TokenService::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

#if !defined(OS_CHROMEOS)
  // network::NetworkConnectionTracker::NetworkConnectionObserver
  // implementation.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;
#endif

  void AfterCredentialsCopied() override;
  void SetReadyForDiceMigration(bool is_ready) override;

  // Used in tests to override the URLLoaderFactory returned by
  // GetURLLoaderFactory().
  void SetURLLoaderFactoryForTest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 protected:
  virtual void ShowUserManager(const base::FilePath& profile_path);
  virtual void LockForceSigninProfile(const base::FilePath& profile_path);

 private:
  void MaybeFetchSigninTokenHandle();
  void VerifySyncToken();
  void OnCloseBrowsersSuccess(
      const signin_metrics::ProfileSignout signout_source_metric,
      const base::FilePath& profile_path);
  void OnCloseBrowsersAborted(const base::FilePath& profile_path);

  Profile* profile_;

  SigninErrorController* signin_error_controller_;

  // Stored callback from PreSignOut();
  base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached_;

#if !defined(OS_CHROMEOS)
  std::list<base::Closure> delayed_callbacks_;
#endif

  bool should_display_user_manager_ = true;
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  std::unique_ptr<ForceSigninVerifier> force_signin_verifier_;
#endif

  std::unique_ptr<gaia::GaiaOAuthClient> oauth_client_;
  std::unique_ptr<OAuth2TokenService::Request> oauth_request_;

  scoped_refptr<network::SharedURLLoaderFactory>
      url_loader_factory_for_testing_;

  base::WeakPtrFactory<ChromeSigninClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSigninClient);
};

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_
