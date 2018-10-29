// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_OAUTH2_TOKEN_SERVICE_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SIGNIN_OAUTH2_TOKEN_SERVICE_DELEGATE_ANDROID_H_

#include <map>
#include <memory>
#include <string>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"

// A specialization of OAuth2TokenServiceDelegate that will be returned by
// OAuth2TokenServiceDelegateFactory for OS_ANDROID.  This instance uses
// native Android features to lookup OAuth2 tokens.
//
// See |OAuth2TokenServiceDelegate| for usage details.
//
// Note: requests should be started from the UI thread. To start a
// request from other thread, please use OAuth2TokenServiceRequest.
class OAuth2TokenServiceDelegateAndroid : public OAuth2TokenServiceDelegate {
 public:
  OAuth2TokenServiceDelegateAndroid(
      AccountTrackerService* account_tracker_service);
  ~OAuth2TokenServiceDelegateAndroid() override;

  // Creates a new instance of the OAuth2TokenServiceDelegateAndroid.
  static OAuth2TokenServiceDelegateAndroid* Create();

  // Returns a reference to the Java instance of this service.
  static base::android::ScopedJavaLocalRef<jobject> GetForProfile(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& j_profile_android);

  // Called by the TestingProfile class to disable account validation in
  // tests.  This prevents the token service from trying to look up system
  // accounts which requires special permission.
  static void set_is_testing_profile() { is_testing_profile_ = true; }

  // OAuth2TokenServiceDelegate overrides:
  bool RefreshTokenIsAvailable(const std::string& account_id) const override;
  GoogleServiceAuthError GetAuthError(
      const std::string& account_id) const override;
  void UpdateAuthError(const std::string& account_id,
                       const GoogleServiceAuthError& error) override;
  std::vector<std::string> GetAccounts() override;

  // Lists account names at the OS level.
  std::vector<std::string> GetSystemAccountNames();

  void ValidateAccounts(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& current_account,
      jboolean force_notifications);

  // Takes a the signed in sync account as well as all the other
  // android account ids and check the token status of each.  If
  // |force_notifications| is true, TokenAvailable notifications will
  // be sent anyway, even if the account was already known.
  void ValidateAccounts(const std::string& signed_in_account_id,
                        bool force_notifications);

  // Overridden from OAuth2TokenService to complete signout of all
  // OA2TService aware accounts.
  void RevokeAllCredentials() override;

  void LoadCredentials(const std::string& primary_account_id) override;

 protected:
  OAuth2AccessTokenFetcher* CreateAccessTokenFetcher(
      const std::string& account_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_factory,
      OAuth2AccessTokenConsumer* consumer) override;

  // Overridden from OAuth2TokenService to intercept token fetch requests and
  // redirect them to the Account Manager.
  void InvalidateAccessToken(const std::string& account_id,
                             const std::string& client_id,
                             const OAuth2TokenService::ScopeSet& scopes,
                             const std::string& access_token) override;

  // Called to notify observers when a refresh token is available.
  void FireRefreshTokenAvailable(const std::string& account_id) override;
  // Called to notify observers when a refresh token has been revoked.
  void FireRefreshTokenRevoked(const std::string& account_id) override;
  // Called to notify observers when refresh tokans have been loaded.
  void FireRefreshTokensLoaded() override;

 private:
  std::string MapAccountIdToAccountName(const std::string& account_id) const;
  std::string MapAccountNameToAccountId(const std::string& account_name) const;

  enum RefreshTokenLoadStatus {
    RT_LOAD_NOT_START,
    RT_WAIT_FOR_VALIDATION,
    RT_HAS_BEEN_VALIDATED,
    RT_LOADED
  };

  // Return whether |signed_in_id| is valid and we have access
  // to all the tokens in |curr_ids|. If |force_notifications| is true,
  // TokenAvailable notifications will be sent anyway, even if the account was
  // already known.
  bool ValidateAccounts(const std::string& signed_in_id,
                        const std::vector<std::string>& prev_ids,
                        const std::vector<std::string>& curr_ids,
                        std::vector<std::string>* refreshed_ids,
                        std::vector<std::string>* revoked_ids,
                        bool force_notifications);

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  // Maps account_id to the last error for that account.
  std::map<std::string, GoogleServiceAuthError> errors_;

  AccountTrackerService* account_tracker_service_;
  RefreshTokenLoadStatus fire_refresh_token_loaded_;

  static bool is_testing_profile_;

  DISALLOW_COPY_AND_ASSIGN(OAuth2TokenServiceDelegateAndroid);
};

#endif  // CHROME_BROWSER_SIGNIN_OAUTH2_TOKEN_SERVICE_DELEGATE_ANDROID_H_
