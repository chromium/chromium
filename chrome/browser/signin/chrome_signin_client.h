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
#include "components/signin/public/base/signin_client.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class WaitForNetworkCallbackHelper;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
class ForceSigninVerifier;
#endif
class PrefRegistrySimple;
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
  bool IsClearPrimaryAccountAllowed() const override;

  // TODO(crbug.com/40240844): Remove revoke sync restriction when allowing
  // enterprise users to revoke sync fully launches.
  bool IsRevokeSyncConsentAllowed() const override;
  void PreSignOut(
      base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
      signin_metrics::ProfileSignout signout_source_metric) override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  network::mojom::CookieManager* GetCookieManager() override;
  network::mojom::DeviceBoundSessionManager* GetDeviceBoundSessionManager()
      const override;
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

  std::unique_ptr<signin::BoundSessionOAuthMultiLoginDelegate>
  CreateBoundSessionOAuthMultiloginDelegate() const override;
  signin::OAuthConsumer GetOAuthConsumerFromId(
      signin::OAuthConsumerId oauth_consumer_id) const override;

  // Adds the users to a synthetic field trial for user that were shown the
  // Bookmarks Bubble sign in/sync promo. Only adds user that are part of the
  // experiment associated with `switches::kSyncEnableBookmarksInTransportMode`.
  // Called when the promo is shown to the user.
  static void MaybeAddUserToBookmarksBubblePromoShownSyntheticFieldTrial();

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Used in tests to override the URLLoaderFactory returned by
  // GetURLLoaderFactory().
  void SetURLLoaderFactoryForTest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 protected:
  virtual void ShowUserManager(const base::FilePath& profile_path);
  virtual void LockForceSigninProfile(const base::FilePath& profile_path);

 private:
  // Returns what kind of signout is possible given the optional
  // `signout_source`. If `signout_source` is provided, it will be check against
  // some sources that must always allow signout regardless of any restriction,
  // otherwise the decision is made based on the profile's status.
  SigninClient::SignoutDecision GetSignoutDecision(
      const std::optional<signin_metrics::ProfileSignout> signout_source) const;
  void VerifySyncToken();
  void OnCloseBrowsersSuccess(
      const signin_metrics::ProfileSignout signout_source_metric,
      bool should_sign_out,
      const base::FilePath& profile_path);
  void OnCloseBrowsersAborted(const base::FilePath& profile_path);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  // Used as the `on_token_fetch_complete` callback in the
  // `ForceSigninVerifier`.
  void OnTokenFetchComplete(bool token_is_valid);
#endif

#if !BUILDFLAG(IS_CHROMEOS)
  void RecordOpenTabCount(signin_metrics::AccessPoint access_point,
                          signin::ConsentLevel consent_level);
#endif

  // Adds the user to a synthetic field trial based on the pref that it is
  // associated with. The pref is then read on startup to ensure stickiness on
  // session restart. Only adds user that are part of the experiment associated
  // with `switches::kSyncEnableBookmarksInTransportMode` from which the group
  // of the Synthetic Field trials are deduced.
  static void MaybeAddUserToUnoBookmarksSyntheticFieldTrial(
      std::string_view synthetic_field_trial_group_pref);

  // Reads the group associated with the Synthetic field trial from prefs and
  // registers it. Only registers the group if it was previously set in the
  // pref.
  static void RegisterSyntheticTrialsFromPrefs();

  const std::unique_ptr<WaitForNetworkCallbackHelper>
      wait_for_network_callback_helper_;
  raw_ptr<Profile, DanglingUntriaged> profile_;

  // Stored callback from PreSignOut();
  base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached_;

  bool should_display_user_manager_ = true;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<ForceSigninVerifier> force_signin_verifier_;
#endif

  scoped_refptr<network::SharedURLLoaderFactory>
      url_loader_factory_for_testing_;

  // Used to convert OAuthConsumerIds to OAuthConsumers.
  std::unique_ptr<signin::OAuthConsumerRegistry> oauth_consumer_registry_;
};

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_
