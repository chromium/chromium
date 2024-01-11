// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_IDENTITY_DEVICE_OAUTH2_TOKEN_SERVICE_H_
#define CHROME_BROWSER_DEVICE_IDENTITY_DEVICE_OAUTH2_TOKEN_SERVICE_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/device_identity/device_oauth2_token_store.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

namespace network {
class SharedURLLoaderFactory;
}

class OAuth2AccessTokenFetcher;
class OAuth2AccessTokenConsumer;

// DeviceOAuth2TokenService retrieves OAuth2 access tokens for a given
// set of scopes using the device-level OAuth2 any-api refresh token
// obtained during enterprise device enrollment.
//
// Note that requests must be made from the UI thread.
class DeviceOAuth2TokenService : public OAuth2AccessTokenManager::Delegate,
                                 public gaia::GaiaOAuthClient::Delegate,
                                 public DeviceOAuth2TokenStore::Observer {
 public:
  using RefreshTokenAvailableCallback = base::RepeatingClosure;
  using StatusCallback = base::OnceCallback<void(bool)>;

  DeviceOAuth2TokenService(const DeviceOAuth2TokenService&) = delete;
  DeviceOAuth2TokenService& operator=(const DeviceOAuth2TokenService&) = delete;

  // Persist the given refresh token on the device. Overwrites any previous
  // value. Should only be called during initial device setup. Signals
  // completion via the given callback, passing true if the operation succeeded.
  void SetAndSaveRefreshToken(const std::string& refresh_token,
                              StatusCallback callback);

  // Pull the robot account ID from device policy.
  CoreAccountId GetRobotAccountId() const;

  // If set, this callback will be invoked when a new refresh token is
  // available.
  void SetRefreshTokenAvailableCallback(RefreshTokenAvailableCallback callback);

  // Returns true if the refresh token is available and if the clients of this
  // class may start fetching access tokens.
  bool RefreshTokenIsAvailable() const;

  // Checks in the cache for a valid access token for a specified |account_id|
  // and |scopes|, and if not found starts a request for an OAuth2 access token
  // using the OAuth2 refresh token maintained by this instance for that
  // |account_id|. The caller owns the returned Request.
  // |scopes| is the set of scopes to get an access token for, |consumer| is
  // the object that will be called back with results if the returned request
  // is not deleted.
  std::unique_ptr<OAuth2AccessTokenManager::Request> StartAccessTokenRequest(
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      OAuth2AccessTokenManager::Consumer* consumer);

  // Mark an OAuth2 |access_token| issued for |account_id| and |scopes| as
  // invalid. This should be done if the token was received from this class,
  // but was not accepted by the server (e.g., the server returned
  // 401 Unauthorized). The token will be removed from the cache for the given
  // scopes.
  void InvalidateAccessToken(const OAuth2AccessTokenManager::ScopeSet& scopes,
                             const std::string& access_token);

  OAuth2AccessTokenManager* GetAccessTokenManager();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Used on non-ChromeOS platforms to set the email associated with the
  // current service account. On ChromeOS, this function isn't used because
  // the service account identity comes from CrosSettings.
  void SetServiceAccountEmail(const std::string& account_email);
#endif

  // Can be used to override the robot account ID for testing purposes. Most
  // common use case is to easily inject a non-empty account ID to make the
  // refresh token for the robot account visible via GetAccounts() and
  // RefreshTokenIsAvailable().
  void set_robot_account_id_for_testing(const CoreAccountId& account_id);

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

  // gaia::GaiaOAuthClient::Delegate implementation.
  void OnRefreshTokenResponse(const std::string& access_token,
                              int expires_in_seconds) override;
  void OnGetTokenInfoResponse(const base::Value::Dict& token_info) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

  // DeviceOAuth2TokenStore callbacks:
  void OnInitComplete(bool init_result, bool validation_required);
  void OnPrepareTrustedAccountIdFinished(const CoreAccountId& gaia_robot_id,
                                         bool check_passed);

  // DeviceOAuth2TokenStore::Observer:
  void OnRefreshTokenAvailable() override;

  // OAuth2AccessTokenManager::Delegate:
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer,
      const std::string& token_binding_challenge) override;
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

  void FireRefreshTokenAvailable();

  // Use DeviceOAuth2TokenServiceFactory to get an instance of this class.
  explicit DeviceOAuth2TokenService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<DeviceOAuth2TokenStore> store);
  ~DeviceOAuth2TokenService() override;

  // Flushes |pending_requests_|, indicating the specified result.
  void FlushPendingRequests(bool token_is_valid,
                            GoogleServiceAuthError::State error);

  // Signals failure on the specified request, passing |error| as the reason.
  void FailRequest(OAuth2AccessTokenManager::RequestImpl* request,
                   GoogleServiceAuthError::State error);

  // Starts the token validation flow, i.e. token info fetch.
  void StartValidation();

  void RequestValidation();

  // Returns the refresh token for the robot account id.
  std::string GetRefreshToken() const;

  void ReportServiceError(GoogleServiceAuthError::State error);

  // Returns true if this object has already received the validation result for
  // the token, false otherwise.
  bool HasValidationResult() const;

  std::unique_ptr<OAuth2AccessTokenManager> token_manager_;

  // Currently open requests that are waiting while loading the system salt or
  // validating the token.
  std::vector<raw_ptr<PendingRequest, VectorExperimental>> pending_requests_;

  // Callbacks to invoke, if set, for refresh token-related events.
  RefreshTokenAvailableCallback on_refresh_token_available_callback_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Current operational state.
  State state_;

  int max_refresh_token_validation_retries_;

  // Flag to indicate whether there are pending requests.
  bool validation_requested_;

  std::unique_ptr<gaia::GaiaOAuthClient> gaia_oauth_client_;

  CoreAccountId robot_account_id_for_testing_;

  std::unique_ptr<DeviceOAuth2TokenStore> store_;

  base::WeakPtrFactory<DeviceOAuth2TokenService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEVICE_IDENTITY_DEVICE_OAUTH2_TOKEN_SERVICE_H_
