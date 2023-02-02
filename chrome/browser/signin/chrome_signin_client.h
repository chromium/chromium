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

  // Returns true if removing/changing a non empty primary account (signout)
  // from the profile is allowed. Returns false if signout is disallowed.
  // Signout is diallowed for:
  // - Cloud-managed enterprise accounts. Signout would require profile
  //   destruction (See ChromeSigninClient::PreSignOut(),
  //   PrimaryAccountPolicyManager::EnsurePrimaryAccountAllowedForProfile()).
  // - Supervised users on Android.IsRevokeSyncConsentAllowed
  // - Lacros main profile: the primary account
  //   must be the device account and can't be changed/cleared.
  bool IsClearPrimaryAccountAllowed(bool has_sync_account) const override;

  // TODO(crbug.com/1369980): Remove revoke sync restriction when allowing
  // enterprise users to revoke sync fully launches.
  bool IsRevokeSyncConsentAllowed() const override;
  void PreSignOut(
      base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
      signin_metrics::ProfileSignout signout_source_metric,
      bool has_sync_account) override;
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
  // Returns what kind of signout is possible given `has_sync_account` and the
  // optional `signout_source`. If `signout_source` is provided, it will be
  // check against some sources that must always allow signout regardless of any
  // restriction, otherwise the decision is made based on the profile's status.
  SigninClient::SignoutDecision GetSignoutDecision(
      bool has_sync_account,
      const absl::optional<signin_metrics::ProfileSignout> signout_source)
      const;
  void VerifySyncToken();
  void OnCloseBrowsersSuccess(
      const signin_metrics::ProfileSignout signout_source_metric,
      bool has_sync_account,
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
