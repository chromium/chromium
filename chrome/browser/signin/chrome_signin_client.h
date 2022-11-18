// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_

#include <list>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/signin_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_change_manager.mojom-forward.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "services/network/public/cpp/network_connection_tracker.h"
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
class ForceSigninVerifier;
#endif
class Profile;

class ChromeSigninClient
    : public SigninClient
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    ,
      public network::NetworkConnectionTracker::NetworkConnectionObserver
#endif
{
 public:
  explicit ChromeSigninClient(Profile* profile);

  ChromeSigninClient(const ChromeSigninClient&) = delete;
  ChromeSigninClient& operator=(const ChromeSigninClient&) = delete;

  ~ChromeSigninClient() override;

  void DoFinalInit() override;

  // Utility method.
  static bool ProfileAllowsSigninCookies(Profile* profile);

  // SigninClient implementation.
  PrefService* GetPrefs() override;
  bool IsClearPrimaryAccountAllowed() const override;
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

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // network::NetworkConnectionTracker::NetworkConnectionObserver
  // implementation.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  absl::optional<account_manager::Account> GetInitialPrimaryAccount() override;
  absl::optional<bool> IsInitialPrimaryAccountChild() const override;
  void RemoveAccount(const account_manager::AccountKey& account_key) override;
  void RemoveAllAccounts() override;
#endif

  // Used in tests to override the URLLoaderFactory returned by
  // GetURLLoaderFactory().
  void SetURLLoaderFactoryForTest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 protected:
  virtual void ShowUserManager(const base::FilePath& profile_path);
  virtual void LockForceSigninProfile(const base::FilePath& profile_path);

 private:
  void VerifySyncToken();
  void OnCloseBrowsersSuccess(
      const signin_metrics::ProfileSignout signout_source_metric,
      const base::FilePath& profile_path);
  void OnCloseBrowsersAborted(const base::FilePath& profile_path);

  raw_ptr<Profile> profile_;

  // Stored callback from PreSignOut();
  base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached_;

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  std::list<base::OnceClosure> delayed_callbacks_;
#endif

  bool should_display_user_manager_ = true;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<ForceSigninVerifier> force_signin_verifier_;
#endif

  scoped_refptr<network::SharedURLLoaderFactory>
      url_loader_factory_for_testing_;

  base::WeakPtrFactory<ChromeSigninClient> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_
