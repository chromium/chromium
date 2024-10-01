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
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/signin_client.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class WaitForNetworkCallbackHelper;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
class ForceSigninVerifier;
#endif
class Profile;

namespace version_info {
enum class Channel;
}

class ChromeSigninClient : public SigninClient {
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

  // TODO(crbug.com/40240844): Remove revoke sync restriction when allowing
  // enterprise users to revoke sync fully launches.
  bool IsRevokeSyncConsentAllowed() const override;
  void PreSignOut(
      base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
      signin_metrics::ProfileSignout signout_source_metric,
      bool has_sync_account) override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  network::mojom::CookieManager* GetCookieManager() override;
  network::mojom::NetworkContext* GetNetworkContext() override;
  bool AreSigninCookiesAllowed() override;
  bool AreSigninCookiesDeletedOnExit() override;
  void AddContentSettingsObserver(
      content_settings::Observer* observer) override;
  void RemoveContentSettingsObserver(
      content_settings::Observer* observer) override;
  bool AreNetworkCallsDelayed() override;
  void DelayNetworkCall(base::OnceClosure callback) override;
  std::unique_ptr<GaiaAuthFetcher> CreateGaiaAuthFetcher(
      GaiaAuthConsumer* consumer,
      gaia::GaiaSource source) override;
  version_info::Channel GetClientChannel() override;
  void OnPrimaryAccountChanged(
      signin::PrimaryAccountChangeEvent event_details) override;

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  std::unique_ptr<signin::BoundSessionOAuthMultiLoginDelegate>
  CreateBoundSessionOAuthMultiloginDelegate() const override;
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::optional<account_manager::Account> GetInitialPrimaryAccount() override;
  std::optional<bool> IsInitialPrimaryAccountChild() const override;
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
      const std::optional<signin_metrics::ProfileSignout> signout_source) const;
  void VerifySyncToken();
  void OnCloseBrowsersSuccess(
      const signin_metrics::ProfileSignout signout_source_metric,
      bool should_sign_out,
      bool has_sync_account,
      const base::FilePath& profile_path);
  void OnCloseBrowsersAborted(const base::FilePath& profile_path);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // Used as the `on_token_fetch_complete` callback in the
  // `ForceSigninVerifier`.
  void OnTokenFetchComplete(bool token_is_valid);
#endif

  // virtual for unit testing: cut down dependency on `BookmarkModel`.
  // The following two functions will return `std::nullopt` if the
  // `BookmarkModel` is nullptr.
  virtual std::optional<size_t> GetAllBookmarksCount();
  virtual std::optional<size_t> GetBookmarkBarBookmarksCount();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Returns `std::nullopt` if the `ExtensionRegistry` is nullptr.
  virtual std::optional<size_t> GetExtensionsCount();
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  void RecordOpenTabCount(signin_metrics::AccessPoint access_point,
                          signin::ConsentLevel consent_level);
#endif

  const std::unique_ptr<WaitForNetworkCallbackHelper>
      wait_for_network_callback_helper_;
  raw_ptr<Profile, DanglingUntriaged> profile_;

  // Stored callback from PreSignOut();
  base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached_;

  bool should_display_user_manager_ = true;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<ForceSigninVerifier> force_signin_verifier_;
#endif

  scoped_refptr<network::SharedURLLoaderFactory>
      url_loader_factory_for_testing_;
};

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_
