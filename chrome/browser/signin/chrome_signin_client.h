// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_

#include <list>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/gaia_oauth_client.h"
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
      public gaia::GaiaOAuthClient::Delegate {
 public:
  explicit ChromeSigninClient(Profile* profile);
  ~ChromeSigninClient() override;

  void DoFinalInit() override;

  // Utility method.
  static bool ProfileAllowsSigninCookies(Profile* profile);

  // SigninClient implementation.
  PrefService* GetPrefs() override;
  void PreSignOut(
      base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
      signin_metrics::ProfileSignout signout_source_metric) override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  network::mojom::CookieManager* GetCookieManager() override;
  bool AreSigninCookiesAllowed() override;
  bool AreSigninCookiesDeletedOnExit() override;
  void AddContentSettingsObserver(
      content_settings::Observer* observer) override;
  void RemoveContentSettingsObserver(
      content_settings::Observer* observer) override;
  void DelayNetworkCall(base::OnceClosure callback) override;
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      gaia::GaiaSource source) override;
  bool IsNonEnterpriseUser(const std::string& username) override;

  // gaia::GaiaOAuthClient::Delegate implementation.
  void OnGetTokenInfoResponse(
      std::unique_ptr<base::DictionaryValue> token_info) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

#if !defined(OS_CHROMEOS)
  // network::NetworkConnectionTracker::NetworkConnectionObserver
  // implementation.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;
#endif

  void SetDiceMigrationCompleted() override;
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

  // signin::PrimaryAccountAccessTokenFetcher callback
  void OnAccessTokenAvailable(GoogleServiceAuthError error,
                              signin::AccessTokenInfo access_token_info);

  Profile* profile_;

  // Stored callback from PreSignOut();
  base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached_;

#if !defined(OS_CHROMEOS)
  std::list<base::OnceClosure> delayed_callbacks_;
#endif

  bool should_display_user_manager_ = true;
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  std::unique_ptr<ForceSigninVerifier> force_signin_verifier_;
#endif

  std::unique_ptr<gaia::GaiaOAuthClient> oauth_client_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  scoped_refptr<network::SharedURLLoaderFactory>
      url_loader_factory_for_testing_;

  base::WeakPtrFactory<ChromeSigninClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeSigninClient);
};

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_
