// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_GET_AUTH_TOKEN_FUNCTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_GET_AUTH_TOKEN_FUNCTION_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/identity/gaia_remote_consent_flow.h"
#include "chrome/browser/extensions/api/identity/identity_mint_queue.h"
#include "chrome/common/extensions/api/identity.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/device_oauth2_token_service_ash.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/device_oauth2_token_service_lacros.h"
#endif

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

namespace extensions {
class IdentityGetAuthTokenError;

// Exposed for testing
inline constexpr base::TimeDelta kGetAuthTokenInactivityTime =
    base::Minutes(10);

// identity.getAuthToken fetches an OAuth 2 function for the
// caller. The request has three sub-flows: non-interactive,
// interactive, and sign-in.
//
// In the non-interactive flow, getAuthToken requests a token from
// GAIA. GAIA may respond with a token, an error, or "consent
// required". In the consent required cases, getAuthToken proceeds to
// the second, interactive phase.
//
// The interactive flow presents a scope approval dialog to the
// user. If the user approves the request, a grant will be recorded on
// the server, and an access token will be returned to the caller.
//
// In some cases we need to display a sign-in dialog. Normally the
// profile will be signed in already, but if it turns out we need a
// new login token, there is a sign-in flow. If that flow completes
// successfully, getAuthToken proceeds to the non-interactive flow.
class IdentityGetAuthTokenFunction : public ExtensionFunction,
                                     public GaiaRemoteConsentFlow::Delegate,
                                     public IdentityMintRequestQueue::Request,
                                     public signin::IdentityManager::Observer,
                                     public OAuth2MintTokenFlow::Delegate {
 public:
  // Interactive requests showing UIs (like a signin tab or a consent window)
  // may be rejected to avoid creating unwanted or out-of-context user
  // interruption.
  enum class InteractivityStatus {
    // Interactivity was not requested.
    kNotRequested = 0,
    // Interactivity was requested, but the user is inactive. Interactivity is
    // not allowed.
    kDisallowedIdle = 1,
    // Interactivity was requested, and the user is not idle, but signin is
    // disabled.
    kDisallowedSigninDisallowed = 2,
    // Interactivity was requested and allowed because there is a user gesture.
    kAllowedWithGesture = 3,
    // Interactivity was requested. There is no user gesture but it is allowed
    // because the user was recently active.
    kAllowedWithActivity = 4,
  };

  DECLARE_EXTENSION_FUNCTION("identity.getAuthToken",
                             EXPERIMENTAL_IDENTITY_GETAUTHTOKEN)

  IdentityGetAuthTokenFunction();

  const ExtensionTokenKey* GetExtensionTokenKeyForTest() { return &token_key_; }

  void OnIdentityAPIShutdown();

 protected:
  ~IdentityGetAuthTokenFunction() override;

  void SigninFailed();

  // GaiaRemoteConsentFlow::Delegate implementation:
  void OnGaiaRemoteConsentFlowFailed(
      GaiaRemoteConsentFlow::Failure failure) override;
  void OnGaiaRemoteConsentFlowApproved(const std::string& consent_result,
                                       const std::string& gaia_id) override;

  // Starts a login access token request.
  virtual void StartTokenKeyAccountAccessTokenRequest();

#if BUILDFLAG(IS_CHROMEOS)
  void OnAccessTokenForDeviceAccountFetchCompleted(
      crosapi::mojom::AccessTokenResultPtr result);
#endif

  void OnAccessTokenFetchCompleted(GoogleServiceAuthError error,
                                   signin::AccessTokenInfo access_token_info);

  // Invoked on completion of the access token fetcher.
  // Exposed for testing.
  void OnGetAccessTokenComplete(const std::optional<std::string>& access_token,
                                base::Time expiration_time,
                                const GoogleServiceAuthError& error);

  // Starts a mint token request to GAIA.
  // Exposed for testing.
  virtual void StartGaiaRequest(const std::string& login_access_token);

  // Caller owns the returned instance.
  // Exposed for testing.
  virtual std::unique_ptr<OAuth2MintTokenFlow> CreateMintTokenFlow();

  Profile* GetProfile() const;

  // Returns the gaia id of the account requested by or previously selected for
  // this extension if the account is available on the device. Otherwise,
  // returns an empty string.
  // Exposed for testing.
  std::string GetSelectedUserId() const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  using DeviceOAuth2TokenFetcher = crosapi::DeviceOAuth2TokenServiceAsh;
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  using DeviceOAuth2TokenFetcher = DeviceOAuth2TokenServiceLacros;
#endif

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<DeviceOAuth2TokenFetcher> device_oauth2_token_fetcher_;
#endif

  // Pending fetcher for an access token for |token_key_.account_id| (via
  // IdentityManager).
  std::unique_ptr<signin::AccessTokenFetcher>
      token_key_account_access_token_fetcher_;

  // Returns whether granular permissions will be requested.
  // Exposed for testing.
  bool enable_granular_permissions() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(GetAuthTokenFunctionTest,
                           ComponentWithChromeClientId);
  FRIEND_TEST_ALL_PREFIXES(GetAuthTokenFunctionTest,
                           ComponentWithNormalClientId);
  FRIEND_TEST_ALL_PREFIXES(GetAuthTokenFunctionTest, InteractiveQueueShutdown);
  FRIEND_TEST_ALL_PREFIXES(GetAuthTokenFunctionTest, NoninteractiveShutdown);

  enum class InteractionType { kSignin, kConsent };

  // If `gaia_id` is empty or the account is not present in Chrome, this will
  // use the primary account if it exists. Otherwise, interactive sign in flow
  // might be started.
  void GetAuthTokenForAccount(const std::string& gaia_id);

  // signin::IdentityManager::Observer implementation:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  // Attempts to show the signin UI after the service auth error if this error
  // isn't transient.
  // Returns true iff the signin flow was triggered.
  bool TryRecoverFromServiceAuthError(const GoogleServiceAuthError& error);

  // ExtensionFunction:
  ResponseAction Run() override;

  // Helpers to report async function results to the caller.
  void StartAsyncRun();
  void CompleteAsyncRun(ResponseValue response);
  void CompleteFunctionWithResult(const std::string& access_token,
                                  const std::set<std::string>& granted_scopes);
  void CompleteFunctionWithError(const IdentityGetAuthTokenError& error);

  // Whether a signin flow should be initiated in the user's current state.
  bool ShouldStartSigninFlow();

  // Initiate/complete the sub-flows.
  void StartSigninFlow();
  void StartMintTokenFlow(IdentityMintRequestQueue::MintType type);
  void CompleteMintTokenFlow();

  // IdentityMintRequestQueue::Request implementation:
  void StartMintToken(IdentityMintRequestQueue::MintType type) override;

  // OAuth2MintTokenFlow::Delegate implementation:
  void OnMintTokenSuccess(
      const OAuth2MintTokenFlow::MintTokenResult& result) override;
  void OnMintTokenFailure(const GoogleServiceAuthError& error) override;
  void OnRemoteConsentSuccess(
      const RemoteConsentResolutionData& resolution_data) override;

#if BUILDFLAG(IS_CHROMEOS)
  // Starts a login access token request for device robot account. This method
  // will be called only in Chrome OS for:
  // 1. Enterprise kiosk mode.
  // 2. Allowlisted first party apps in public session.
  virtual void StartDeviceAccessTokenRequest();
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // This dialog prompts the user to sign in with an account that is already
  // present in the identity manager. This is different from the signin dialog
  // shown when there are no accounts in the identity manager.
  void MaybeShowChromeSigninDialog();
  void OnChromeSigninDialogDestroyed();
#endif

  // Methods for invoking UI. Overridable for testing.
  virtual void ShowExtensionLoginPrompt();
  virtual void ShowRemoteConsentDialog(
      const RemoteConsentResolutionData& resolution_data);

  // Checks if there is a master login token to mint tokens for the extension.
  bool HasRefreshTokenForTokenKeyAccount() const;

  std::string GetOAuth2ClientId() const;

  // Returns true if extensions are restricted to the primary account.
  bool IsPrimaryAccountOnly() const;

  // Computes the interactivity status for consent and sign-in based on the
  // "interactive" parameter, the idle state and whether signin is allowed.
  void ComputeInteractivityStatus(
      const std::optional<api::identity::TokenDetails>& details);

  // Returns an error for cases when interactivity is not allowed. Must not be
  // called when interactivity is allowed.
  IdentityGetAuthTokenError GetErrorFromInteractivityStatus(
      InteractionType interaction_type) const;

  // For metrics.
  base::TimeDelta idle_time_;
  bool interactivity_metrics_recorded_ = false;

  InteractivityStatus interactivity_status_for_consent_ =
      InteractivityStatus::kNotRequested;
  IdentityMintRequestQueue::MintType mint_token_flow_type_;
  std::unique_ptr<OAuth2MintTokenFlow> mint_token_flow_;
  OAuth2MintTokenFlow::Mode gaia_mint_token_mode_;
  InteractivityStatus interactivity_status_for_signin_ =
      InteractivityStatus::kNotRequested;
  bool enable_granular_permissions_ = false;

  // The gaia id of the account requested by or previously selected for this
  // extension.
  std::string selected_gaia_id_;

  ExtensionTokenKey token_key_{/*extension_id=*/"",
                               /*account_info=*/CoreAccountInfo(),
                               /*scopes=*/{}};
  std::string oauth2_client_id_;
  // When launched in interactive mode, and if there is no existing grant,
  // a permissions prompt will be popped up to the user.
  RemoteConsentResolutionData resolution_data_;
  std::unique_ptr<GaiaRemoteConsentFlow> gaia_remote_consent_flow_;
  std::string consent_result_;
  // Added for debugging https://crbug.com/1091423.
  bool remote_consent_approved_ = false;

  // Invoked when IdentityAPI is shut down.
  base::CallbackListSubscription identity_api_shutdown_subscription_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_identity_manager_observation_{this};

  bool waiting_on_account_ = false;

  base::WeakPtrFactory<IdentityGetAuthTokenFunction> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_GET_AUTH_TOKEN_FUNCTION_H_
