// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_OAUTH2_TOKEN_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_OAUTH2_TOKEN_SERVICE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

namespace network {
class SharedURLLoaderFactory;
}

class OAuth2AccessTokenFetcher;
class OAuth2AccessTokenConsumer;
class PrefRegistrySimple;
class PrefService;

namespace chromeos {

// DeviceOAuth2TokenService retrieves OAuth2 access tokens for a given
// set of scopes using the device-level OAuth2 any-api refresh token
// obtained during enterprise device enrollment.
// When using DeviceOAuth2TokenService, a value of |GetRobotAccountId| should
// be used in places where API expects |account_id|.
//
// Note that requests must be made from the UI thread.
class DeviceOAuth2TokenService : public OAuth2AccessTokenManager::Delegate,
                                 public gaia::GaiaOAuthClient::Delegate {
 public:
  typedef base::RepeatingCallback<void(const CoreAccountId& /* account_id */)>
      RefreshTokenAvailableCallback;
  typedef base::RepeatingCallback<void(const CoreAccountId& /* account_id */)>
      RefreshTokenRevokedCallback;

  typedef base::Callback<void(bool)> StatusCallback;

  // Persist the given refresh token on the device. Overwrites any previous
  // value. Should only be called during initial device setup. Signals
  // completion via the given callback, passing true if the operation succeeded.
  void SetAndSaveRefreshToken(const std::string& refresh_token,
                              const StatusCallback& callback);

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Pull the robot account ID from device policy.
  CoreAccountId GetRobotAccountId() const;

  // Can be used to override the robot account ID for testing purposes. Most
  // common use case is to easily inject a non-empty account ID to make the
  // refresh token for the robot account visible via GetAccounts() and
  // RefreshTokenIsAvailable().
  void set_robot_account_id_for_testing(const CoreAccountId& account_id);

  // If set, this callback will be invoked when a new refresh token is
  // available.
  void SetRefreshTokenAvailableCallback(RefreshTokenAvailableCallback callback);

  // If set, this callback will be invoked when a refresh token is revoked.
  void SetRefreshTokenRevokedCallback(RefreshTokenRevokedCallback callback);

  // Checks in the cache for a valid access token for a specified |account_id|
  // and |scopes|, and if not found starts a request for an OAuth2 access token
  // using the OAuth2 refresh token maintained by this instance for that
  // |account_id|. The caller owns the returned Request.
  // |scopes| is the set of scopes to get an access token for, |consumer| is
  // the object that will be called back with results if the returned request
  // is not deleted.
  std::unique_ptr<OAuth2AccessTokenManager::Request> StartAccessTokenRequest(
      const CoreAccountId& account_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      OAuth2AccessTokenManager::Consumer* consumer);

  // Mark an OAuth2 |access_token| issued for |account_id| and |scopes| as
  // invalid. This should be done if the token was received from this class,
  // but was not accepted by the server (e.g., the server returned
  // 401 Unauthorized). The token will be removed from the cache for the given
  // scopes.
  void InvalidateAccessToken(const CoreAccountId& account_id,
                             const OAuth2AccessTokenManager::ScopeSet& scopes,
                             const std::string& access_token);

  bool RefreshTokenIsAvailable(const CoreAccountId& account_id) const;

  OAuth2AccessTokenManager* GetAccessTokenManager();

  // gaia::GaiaOAuthClient::Delegate implementation.
  void OnRefreshTokenResponse(const std::string& access_token,
                              int expires_in_seconds) override;
  void OnGetTokenInfoResponse(
      std::unique_ptr<base::DictionaryValue> token_info) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

 private:
  friend class DeviceOAuth2TokenServiceFactory;
  friend class DeviceOAuth2TokenServiceTest;
  struct PendingRequest;

  // Describes the operational state of this object.
  enum State {
    // Pending system salt / refresh token load.
    STATE_LOADING,
    // No token available.
    STATE_NO_TOKEN,
    // System salt loaded, validation not started yet.
    STATE_VALIDATION_PENDING,
    // Refresh token validation underway.
    STATE_VALIDATION_STARTED,
    // Token validation failed.
    STATE_TOKEN_INVALID,
    // Refresh token is valid.
    STATE_TOKEN_VALID,
  };

  // OAuth2AccessTokenManager::Delegate:
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer) override;
  bool HasRefreshToken(const CoreAccountId& account_id) const override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      const override;
  bool HandleAccessTokenFetch(
      OAuth2AccessTokenManager::RequestImpl* request,
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& client_id,
      const std::string& client_secret,
      const OAuth2AccessTokenManager::ScopeSet& scopes) override;

  void FireRefreshTokenAvailable(const CoreAccountId& account_id);
  void FireRefreshTokenRevoked(const CoreAccountId& account_id);

  // Use DeviceOAuth2TokenServiceFactory to get an instance of this class.
  explicit DeviceOAuth2TokenService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* local_state);
  ~DeviceOAuth2TokenService() override;

  // Flushes |pending_requests_|, indicating the specified result.
  void FlushPendingRequests(bool token_is_valid,
                            GoogleServiceAuthError::State error);

  // Signals failure on the specified request, passing |error| as the reason.
  void FailRequest(OAuth2AccessTokenManager::RequestImpl* request,
                   GoogleServiceAuthError::State error);

  // Returns a list of accounts based on |state_|.
  std::vector<CoreAccountId> GetAccounts() const;

  // Starts the token validation flow, i.e. token info fetch.
  void StartValidation();

  void RequestValidation();

  // Invoked by CrosSettings when the robot account ID becomes available.
  void OnServiceAccountIdentityChanged();

  // Checks whether |gaia_robot_id| matches the expected account ID indicated in
  // device settings.
  void CheckRobotAccountId(const CoreAccountId& gaia_robot_id);

  // Returns the refresh token for the robot account id.
  std::string GetRefreshToken() const;

  // Handles completion of the system salt input.
  void DidGetSystemSalt(const std::string& system_salt);

  // Encrypts and saves the refresh token. Should only be called when the system
  // salt is available.
  void EncryptAndSaveToken();

  // Flushes |token_save_callbacks_|, indicating the specified result.
  void FlushTokenSaveCallbacks(bool result);

  void ReportServiceError(GoogleServiceAuthError::State error);

  std::unique_ptr<OAuth2AccessTokenManager> token_manager_;

  // Currently open requests that are waiting while loading the system salt or
  // validating the token.
  std::vector<PendingRequest*> pending_requests_;

  // Callbacks to invoke, if set, for refresh token-related events.
  RefreshTokenAvailableCallback on_refresh_token_available_callback_;
  RefreshTokenRevokedCallback on_refresh_token_revoked_callback_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  PrefService* local_state_;

  // Current operational state.
  State state_;

  // Token save callbacks waiting to be completed.
  std::vector<StatusCallback> token_save_callbacks_;

  // The system salt for encrypting and decrypting the refresh token.
  std::string system_salt_;

  int max_refresh_token_validation_retries_;

  // Flag to indicate whether there are pending requests.
  bool validation_requested_;

  // Cache the decrypted refresh token, so we only decrypt once.
  std::string refresh_token_;

  std::unique_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;

  std::unique_ptr<CrosSettings::ObserverSubscription>
      service_account_identity_subscription_;

  CoreAccountId robot_account_id_for_testing_;

  base::WeakPtrFactory<DeviceOAuth2TokenService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceOAuth2TokenService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_OAUTH2_TOKEN_SERVICE_H_
