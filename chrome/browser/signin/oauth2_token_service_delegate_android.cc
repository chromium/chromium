// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/oauth2_token_service_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_android.h"
#include "components/signin/core/browser/account_info.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "jni/OAuth2TokenService_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace {

// Callback from FetchOAuth2TokenWithUsername().
// Arguments:
// - the error, or NONE if the token fetch was successful.
// - the OAuth2 access token.
// - the expiry time of the token (may be null, indicating that the expiry
//   time is unknown.
typedef base::Callback<void(const GoogleServiceAuthError&,
                            const std::string&,
                            const base::Time&)> FetchOAuth2TokenCallback;

class AndroidAccessTokenFetcher : public OAuth2AccessTokenFetcher {
 public:
  AndroidAccessTokenFetcher(OAuth2AccessTokenConsumer* consumer,
                            const std::string& account_id);
  ~AndroidAccessTokenFetcher() override;

  // Overrides from OAuth2AccessTokenFetcher:
  void Start(const std::string& client_id,
             const std::string& client_secret,
             const std::vector<std::string>& scopes) override;
  void CancelRequest() override;

  // Handles an access token response.
  void OnAccessTokenResponse(const GoogleServiceAuthError& error,
                             const std::string& access_token,
                             const base::Time& expiration_time);

 private:
  std::string CombineScopes(const std::vector<std::string>& scopes);

  std::string account_id_;
  bool request_was_cancelled_;
  base::WeakPtrFactory<AndroidAccessTokenFetcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AndroidAccessTokenFetcher);
};

AndroidAccessTokenFetcher::AndroidAccessTokenFetcher(
    OAuth2AccessTokenConsumer* consumer,
    const std::string& account_id)
    : OAuth2AccessTokenFetcher(consumer),
      account_id_(account_id),
      request_was_cancelled_(false),
      weak_factory_(this) {
}

AndroidAccessTokenFetcher::~AndroidAccessTokenFetcher() {
}

void AndroidAccessTokenFetcher::Start(const std::string& client_id,
                                      const std::string& client_secret,
                                      const std::vector<std::string>& scopes) {
  JNIEnv* env = AttachCurrentThread();
  std::string scope = CombineScopes(scopes);
  ScopedJavaLocalRef<jstring> j_username =
      ConvertUTF8ToJavaString(env, account_id_);
  ScopedJavaLocalRef<jstring> j_scope = ConvertUTF8ToJavaString(env, scope);
  std::unique_ptr<FetchOAuth2TokenCallback> heap_callback(
      new FetchOAuth2TokenCallback(
          base::Bind(&AndroidAccessTokenFetcher::OnAccessTokenResponse,
                     weak_factory_.GetWeakPtr())));

  // Call into Java to get a new token.
  Java_OAuth2TokenService_getOAuth2AuthToken(
      env, j_username, j_scope,
      reinterpret_cast<intptr_t>(heap_callback.release()));
}

void AndroidAccessTokenFetcher::CancelRequest() {
  request_was_cancelled_ = true;
}

void AndroidAccessTokenFetcher::OnAccessTokenResponse(
    const GoogleServiceAuthError& error,
    const std::string& access_token,
    const base::Time& expiration_time) {
  if (request_was_cancelled_) {
    // Ignore the callback if the request was cancelled.
    return;
  }
  if (error.state() == GoogleServiceAuthError::NONE) {
    FireOnGetTokenSuccess(OAuth2AccessTokenConsumer::TokenResponse(
        access_token, expiration_time, std::string()));
  } else {
    FireOnGetTokenFailure(error);
  }
}

// static
std::string AndroidAccessTokenFetcher::CombineScopes(
    const std::vector<std::string>& scopes) {
  // The Android AccountManager supports multiple scopes separated by a space:
  // https://code.google.com/p/google-api-java-client/wiki/OAuth2#Android
  std::string scope;
  for (std::vector<std::string>::const_iterator it = scopes.begin();
       it != scopes.end(); ++it) {
    if (!scope.empty())
      scope += " ";
    scope += *it;
  }
  return scope;
}

}  // namespace

bool OAuth2TokenServiceDelegateAndroid::is_testing_profile_ = false;

OAuth2TokenServiceDelegateAndroid::OAuth2TokenServiceDelegateAndroid(
    AccountTrackerService* account_tracker_service)
    : account_tracker_service_(account_tracker_service),
      fire_refresh_token_loaded_(RT_LOAD_NOT_START) {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::ctor";
  DCHECK(account_tracker_service_);
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> local_java_ref =
      Java_OAuth2TokenService_create(env, reinterpret_cast<intptr_t>(this));
  java_ref_.Reset(env, local_java_ref.obj());

  if (account_tracker_service_->GetMigrationState() ==
      AccountTrackerService::MIGRATION_IN_PROGRESS) {
    std::vector<std::string> accounts = GetAccounts();
    std::vector<std::string> accounts_id;
    for (auto account_name : accounts) {
      AccountInfo account_info =
          account_tracker_service_->FindAccountInfoByEmail(account_name);
      DCHECK(!account_info.gaia.empty());
      accounts_id.push_back(account_info.gaia);
    }
    ScopedJavaLocalRef<jobjectArray> java_accounts(
        base::android::ToJavaArrayOfStrings(env, accounts_id));
    Java_OAuth2TokenService_saveStoredAccounts(env, java_accounts);
  }

  if (!is_testing_profile_) {
    Java_OAuth2TokenService_validateAccounts(AttachCurrentThread(), java_ref_,
                                             JNI_TRUE);
  }
}

OAuth2TokenServiceDelegateAndroid::~OAuth2TokenServiceDelegateAndroid() {
}

// static
ScopedJavaLocalRef<jobject> OAuth2TokenServiceDelegateAndroid::GetForProfile(
    JNIEnv* env,
    const JavaRef<jobject>& j_profile_android) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile_android);
  ProfileOAuth2TokenService* service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  OAuth2TokenServiceDelegate* delegate = service->GetDelegate();
  return ScopedJavaLocalRef<jobject>(
      static_cast<OAuth2TokenServiceDelegateAndroid*>(delegate)->java_ref_);
}

static ScopedJavaLocalRef<jobject> JNI_OAuth2TokenService_GetForProfile(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& j_profile_android) {
  return OAuth2TokenServiceDelegateAndroid::GetForProfile(env,
                                                          j_profile_android);
}

bool OAuth2TokenServiceDelegateAndroid::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::RefreshTokenIsAvailable"
           << " account= " << account_id;
  std::string account_name = MapAccountIdToAccountName(account_id);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_account_id =
      ConvertUTF8ToJavaString(env, account_name);
  jboolean refresh_token_is_available =
      Java_OAuth2TokenService_hasOAuth2RefreshToken(env, j_account_id);
  return refresh_token_is_available == JNI_TRUE;
}

GoogleServiceAuthError OAuth2TokenServiceDelegateAndroid::GetAuthError(
    const std::string& account_id) const {
  auto it = errors_.find(account_id);
  return (it == errors_.end()) ? GoogleServiceAuthError::AuthErrorNone()
                               : it->second;
}

void OAuth2TokenServiceDelegateAndroid::UpdateAuthError(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::UpdateAuthError"
           << " account=" << account_id
           << " error=" << error.ToString();

  if (error.IsTransientError())
    return;

  auto it = errors_.find(account_id);
  if (error.state() == GoogleServiceAuthError::NONE) {
    if (it == errors_.end())
      return;
    errors_.erase(it);
  } else {
    if (it != errors_.end() && it->second == error)
      return;
    errors_[account_id] = error;
  }
  FireAuthErrorChanged(account_id, error);
}

std::vector<std::string> OAuth2TokenServiceDelegateAndroid::GetAccounts() {
  std::vector<std::string> accounts;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_accounts =
      Java_OAuth2TokenService_getAccounts(env);
  // TODO(fgorski): We may decide to filter out some of the accounts.
  base::android::AppendJavaStringArrayToStringVector(env, j_accounts,
                                                     &accounts);
  return accounts;
}

std::vector<std::string>
OAuth2TokenServiceDelegateAndroid::GetSystemAccountNames() {
  std::vector<std::string> account_names;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_accounts =
      Java_OAuth2TokenService_getSystemAccountNames(env);
  base::android::AppendJavaStringArrayToStringVector(env, j_accounts,
                                                     &account_names);
  return account_names;
}

OAuth2AccessTokenFetcher*
OAuth2TokenServiceDelegateAndroid::CreateAccessTokenFetcher(
    const std::string& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_factory,
    OAuth2AccessTokenConsumer* consumer) {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::CreateAccessTokenFetcher"
           << " account= " << account_id;
  ValidateAccountId(account_id);
  return new AndroidAccessTokenFetcher(consumer,
                                       MapAccountIdToAccountName(account_id));
}

void OAuth2TokenServiceDelegateAndroid::InvalidateAccessToken(
    const std::string& account_id,
    const std::string& client_id,
    const OAuth2TokenService::ScopeSet& scopes,
    const std::string& access_token) {
  ValidateAccountId(account_id);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_access_token =
      ConvertUTF8ToJavaString(env, access_token);
  Java_OAuth2TokenService_invalidateOAuth2AuthToken(env, j_access_token);
}

void OAuth2TokenServiceDelegateAndroid::ValidateAccounts(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_current_acc,
    jboolean j_force_notifications) {
  std::string signed_in_account_name;
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::ValidateAccounts from java";
  if (j_current_acc)
    signed_in_account_name = ConvertJavaStringToUTF8(env, j_current_acc);
  if (!signed_in_account_name.empty())
    signed_in_account_name = gaia::CanonicalizeEmail(signed_in_account_name);

  // Clear any auth errors so that client can retry to get access tokens.
  errors_.clear();

  ValidateAccounts(MapAccountNameToAccountId(signed_in_account_name),
                   j_force_notifications != JNI_FALSE);
}

void OAuth2TokenServiceDelegateAndroid::ValidateAccounts(
    const std::string& signed_in_account_id,
    bool force_notifications) {
  std::vector<std::string> curr_ids;
  for (const std::string& curr_name : GetSystemAccountNames()) {
    std::string curr_id(MapAccountNameToAccountId(curr_name));
    if (!curr_id.empty())
      curr_ids.push_back(curr_id);
  }

  std::vector<std::string> prev_ids;
  for (const std::string& prev_id : GetAccounts()) {
    if (ValidateAccountId(prev_id))
      prev_ids.push_back(prev_id);
  }

  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::ValidateAccounts:"
           << " sigined_in_account_id=" << signed_in_account_id
           << " prev_ids=" << prev_ids.size() << " curr_ids=" << curr_ids.size()
           << " force=" << (force_notifications ? "true" : "false");

  std::vector<std::string> refreshed_ids;
  std::vector<std::string> revoked_ids;
  bool currently_signed_in =
      ValidateAccounts(signed_in_account_id, prev_ids, curr_ids, &refreshed_ids,
                       &revoked_ids, force_notifications);

  ScopedBatchChange batch(this);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> java_accounts;
  if (currently_signed_in) {
    java_accounts = base::android::ToJavaArrayOfStrings(env, curr_ids);
  } else {
    java_accounts =
        base::android::ToJavaArrayOfStrings(env, std::vector<std::string>());
  }

  // Save the current accounts in the token service before calling
  // FireRefreshToken* methods.
  Java_OAuth2TokenService_saveStoredAccounts(env, java_accounts);

  for (const std::string& refreshed_id : refreshed_ids)
    FireRefreshTokenAvailable(refreshed_id);
  for (const std::string& revoked_id : revoked_ids)
    FireRefreshTokenRevoked(revoked_id);
  if (fire_refresh_token_loaded_ == RT_WAIT_FOR_VALIDATION) {
    fire_refresh_token_loaded_ = RT_LOADED;
    FireRefreshTokensLoaded();
  } else if (fire_refresh_token_loaded_ == RT_LOAD_NOT_START) {
    fire_refresh_token_loaded_ = RT_HAS_BEEN_VALIDATED;
  }

  // Clear accounts no longer exist on device from AccountTrackerService.
  std::vector<AccountInfo> accounts_info =
      account_tracker_service_->GetAccounts();
  for (const AccountInfo& info : accounts_info) {
    if (!base::ContainsValue(curr_ids, info.account_id))
      account_tracker_service_->RemoveAccount(info.account_id);
  }

  // No need to wait for SigninManager to finish migration if not signed in.
  if (account_tracker_service_->GetMigrationState() ==
          AccountTrackerService::MIGRATION_IN_PROGRESS &&
      signed_in_account_id.empty()) {
    account_tracker_service_->SetMigrationDone();
  }
}

bool OAuth2TokenServiceDelegateAndroid::ValidateAccounts(
    const std::string& signed_in_id,
    const std::vector<std::string>& prev_ids,
    const std::vector<std::string>& curr_ids,
    std::vector<std::string>* refreshed_ids,
    std::vector<std::string>* revoked_ids,
    bool force_notifications) {
  bool currently_signed_in = base::ContainsValue(curr_ids, signed_in_id);
  if (currently_signed_in) {
    // Revoke token for ids that have been removed from the device.
    for (const std::string& prev_id : prev_ids) {
      if (prev_id == signed_in_id)
        continue;
      if (!base::ContainsValue(curr_ids, prev_id)) {
        DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::ValidateAccounts:"
                 << "revoked=" << prev_id;
        revoked_ids->push_back(prev_id);
      }
    }

    // Refresh token for new ids or all ids if |force_notifications|.
    if (force_notifications || !base::ContainsValue(prev_ids, signed_in_id)) {
      // Always fire the primary signed in account first.
      DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::ValidateAccounts:"
               << "refreshed=" << signed_in_id;
      refreshed_ids->push_back(signed_in_id);
    }
    for (const std::string& curr_id : curr_ids) {
      if (curr_id == signed_in_id)
        continue;
      if (force_notifications || !base::ContainsValue(prev_ids, curr_id)) {
        DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::ValidateAccounts:"
                 << "refreshed=" << curr_id;
        refreshed_ids->push_back(curr_id);
      }
    }
  } else {
    if (base::ContainsValue(prev_ids, signed_in_id)) {
      DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::ValidateAccounts:"
               << "revoked=" << signed_in_id;
      revoked_ids->push_back(signed_in_id);
    }
    for (const std::string& prev_id : prev_ids) {
      if (prev_id == signed_in_id)
        continue;
      DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::ValidateAccounts:"
               << "revoked=" << prev_id;
      revoked_ids->push_back(prev_id);
    }
  }
  return currently_signed_in;
}

void OAuth2TokenServiceDelegateAndroid::FireRefreshTokenAvailable(
    const std::string& account_id) {
  DCHECK(!account_id.empty());
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::FireRefreshTokenAvailable id="
           << account_id;
  std::string account_name = MapAccountIdToAccountName(account_id);
  DCHECK(!account_name.empty());
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_account_name =
      ConvertUTF8ToJavaString(env, account_name);
  Java_OAuth2TokenService_notifyRefreshTokenAvailable(env, java_ref_,
                                                      j_account_name);
  OAuth2TokenServiceDelegate::FireRefreshTokenAvailable(account_id);
}

void OAuth2TokenServiceDelegateAndroid::FireRefreshTokenRevoked(
    const std::string& account_id) {
  DCHECK(!account_id.empty());
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::FireRefreshTokenRevoked id="
           << account_id;
  std::string account_name = MapAccountIdToAccountName(account_id);
  if (!account_name.empty()) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jstring> j_account_name =
        ConvertUTF8ToJavaString(env, account_name);
    Java_OAuth2TokenService_notifyRefreshTokenRevoked(env, java_ref_,
                                                      j_account_name);
  } else {
    // Current prognosis is that we have an unmigrated account which is due for
    // deletion. Record a histogram to debug this.
    UMA_HISTOGRAM_ENUMERATION("OAuth2Login.AccountRevoked.MigrationState",
                              account_tracker_service_->GetMigrationState(),
                              AccountTrackerService::NUM_MIGRATION_STATES);
    bool is_email_id = account_id.find('@') != std::string::npos;
    UMA_HISTOGRAM_BOOLEAN("OAuth2Login.AccountRevoked.IsEmailId", is_email_id);
  }
  OAuth2TokenServiceDelegate::FireRefreshTokenRevoked(account_id);
}

void OAuth2TokenServiceDelegateAndroid::FireRefreshTokensLoaded() {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::FireRefreshTokensLoaded";

  DCHECK_EQ(LOAD_CREDENTIALS_IN_PROGRESS, load_credentials_state());
  set_load_credentials_state(LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS);

  JNIEnv* env = AttachCurrentThread();
  Java_OAuth2TokenService_notifyRefreshTokensLoaded(env, java_ref_);
  OAuth2TokenServiceDelegate::FireRefreshTokensLoaded();
}

void OAuth2TokenServiceDelegateAndroid::RevokeAllCredentials() {
  DVLOG(1) << "OAuth2TokenServiceDelegateAndroid::RevokeAllCredentials";
  ScopedBatchChange batch(this);
  std::vector<std::string> accounts_to_revoke = GetAccounts();

  // Clear accounts in the token service before calling
  // |FireRefreshTokenRevoked|.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> java_accounts(
      base::android::ToJavaArrayOfStrings(env, std::vector<std::string>()));
  Java_OAuth2TokenService_saveStoredAccounts(env, java_accounts);

  for (const std::string& account : accounts_to_revoke)
    FireRefreshTokenRevoked(account);
}

void OAuth2TokenServiceDelegateAndroid::LoadCredentials(
    const std::string& primary_account_id) {
  DCHECK_EQ(LOAD_CREDENTIALS_NOT_STARTED, load_credentials_state());
  set_load_credentials_state(LOAD_CREDENTIALS_IN_PROGRESS);
  if (primary_account_id.empty()) {
    FireRefreshTokensLoaded();
    return;
  }
  if (fire_refresh_token_loaded_ == RT_HAS_BEEN_VALIDATED) {
    fire_refresh_token_loaded_ = RT_LOADED;
    FireRefreshTokensLoaded();
  } else if (fire_refresh_token_loaded_ == RT_LOAD_NOT_START) {
    fire_refresh_token_loaded_ = RT_WAIT_FOR_VALIDATION;
  }
}

std::string OAuth2TokenServiceDelegateAndroid::MapAccountIdToAccountName(
    const std::string& account_id) const {
  std::string account_name =
      account_tracker_service_->GetAccountInfo(account_id).email;
  DCHECK(!account_name.empty() || account_id.empty())
      << "Can't find account name, account_id=" << account_id;
  return account_name;
}

std::string OAuth2TokenServiceDelegateAndroid::MapAccountNameToAccountId(
    const std::string& account_name) const {
  std::string account_id =
      account_tracker_service_->FindAccountInfoByEmail(account_name).account_id;
  DCHECK(!account_id.empty() || account_name.empty())
      << "Can't find account id, account_name=" << account_name;
  return account_id;
}

// Called from Java when fetching of an OAuth2 token is finished. The
// |authToken| param is only valid when |result| is true.
void JNI_OAuth2TokenService_OAuth2TokenFetched(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& authToken,
    jboolean isTransientError,
    jlong nativeCallback) {
  std::string token;
  if (authToken)
    token = ConvertJavaStringToUTF8(env, authToken);
  std::unique_ptr<FetchOAuth2TokenCallback> heap_callback(
      reinterpret_cast<FetchOAuth2TokenCallback*>(nativeCallback));
  GoogleServiceAuthError err = GoogleServiceAuthError::AuthErrorNone();
  if (!authToken) {
    err =
        isTransientError
            ? GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED)
            : GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                  GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                      CREDENTIALS_REJECTED_BY_SERVER);
  }
  heap_callback->Run(err, token, base::Time());
}
