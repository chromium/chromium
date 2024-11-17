// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/android//web_signin_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/signin/services/android/jni_headers/WebSigninBridge_jni.h"
#include "components/signin/public/android/jni_headers/GoogleServiceAuthError_jni.h"

using base::android::JavaParamRef;

void ForwardOnSigninCompletedToJava(
    const base::android::ScopedJavaGlobalRef<jobject>& j_listener,
    const GoogleServiceAuthError& error) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (error.state() == GoogleServiceAuthError::State::NONE) {
    Java_WebSigninBridge_onSigninSucceeded(env, j_listener);
  } else {
    base::android::ScopedJavaLocalRef<jobject> j_error =
        signin::Java_GoogleServiceAuthError_Constructor(env, error.state());
    Java_WebSigninBridge_onSigninFailed(env, j_listener, j_error);
  }
}

WebSigninBridge::WebSigninBridge(signin::IdentityManager* identity_manager,
                                 AccountReconcilor* account_reconcilor,
                                 CoreAccountInfo signin_account,
                                 OnSigninCompletedCallback on_signin_completed)
    : identity_manager_(identity_manager),
      account_reconcilor_(account_reconcilor),
      signin_account_(signin_account),
      on_signin_completed_(std::move(on_signin_completed)) {
  DCHECK(on_signin_completed_) << "Callback must be non-null!";

  identity_manager_->AddObserver(this);
  account_reconcilor_->AddObserver(this);

  signin::AccountsInCookieJarInfo info =
      identity_manager_->GetAccountsInCookieJar();
  if (info.AreAccountsFresh()) {
    OnAccountsInCookieUpdated(info, GoogleServiceAuthError::AuthErrorNone());
  }
}

WebSigninBridge::~WebSigninBridge() {
  identity_manager_->RemoveObserver(this);
  account_reconcilor_->RemoveObserver(this);
}

void WebSigninBridge::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  for (const auto& account :
       accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts()) {
    if (account.valid && account.gaia_id == signin_account_.gaia) {
      OnSigninCompleted(GoogleServiceAuthError());
      return;
    }
  }
}

void WebSigninBridge::OnStateChanged(
    signin_metrics::AccountReconcilorState state) {
  if (state != signin_metrics::AccountReconcilorState::kError) {
    return;
  }

  bool is_auth_error =
      identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          signin_account_.account_id);
  OnSigninCompleted(
      is_auth_error ? GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                              CREDENTIALS_REJECTED_BY_SERVER)
                    : GoogleServiceAuthError(
                          GoogleServiceAuthError::State::CONNECTION_FAILED));
}

void WebSigninBridge::OnSigninCompleted(const GoogleServiceAuthError& error) {
  on_signin_completed_.Run(error);
}

static jlong JNI_WebSigninBridge_Create(
    JNIEnv* env,
    Profile* profile,
    const JavaParamRef<jobject>& j_account,
    const JavaParamRef<jobject>& j_listener) {
  DCHECK(j_listener) << "Listener should be non-null";

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  AccountReconcilor* account_reconcilor =
      AccountReconcilorFactory::GetForProfile(profile);
  CoreAccountInfo signin_account =
      ConvertFromJavaCoreAccountInfo(env, j_account);
  base::RepeatingCallback<void(const GoogleServiceAuthError&)>
      on_signin_completed = base::BindRepeating(
          &ForwardOnSigninCompletedToJava,
          base::android::ScopedJavaGlobalRef<jobject>(j_listener));
  return reinterpret_cast<intptr_t>(
      new WebSigninBridge(identity_manager, account_reconcilor, signin_account,
                          std::move(on_signin_completed)));
}

static void JNI_WebSigninBridge_Destroy(JNIEnv* env, jlong web_signin_bridge) {
  delete reinterpret_cast<WebSigninBridge*>(web_signin_bridge);
}
