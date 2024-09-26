// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ANDROID_WEB_SIGNIN_BRIDGE_H_
#define CHROME_BROWSER_SIGNIN_ANDROID_WEB_SIGNIN_BRIDGE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"

// The glue for Java-side implementation of WebSigninBridge.
class WebSigninBridge : public signin::IdentityManager::Observer,
                        public AccountReconcilor::Observer {
 public:
  using OnSigninCompletedCallback =
      base::RepeatingCallback<void(const GoogleServiceAuthError&)>;

  explicit WebSigninBridge(signin::IdentityManager* identity_manager,
                           AccountReconcilor* account_reconcilor,
                           CoreAccountInfo signin_account,
                           OnSigninCompletedCallback on_signin_completed);

  WebSigninBridge(const WebSigninBridge&) = delete;
  WebSigninBridge& operator=(const WebSigninBridge&) = delete;

  ~WebSigninBridge() override;

  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  void OnStateChanged(signin_metrics::AccountReconcilorState state) override;

 private:
  void OnSigninCompleted(const GoogleServiceAuthError& error);

  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<AccountReconcilor> account_reconcilor_;
  CoreAccountInfo signin_account_;
  OnSigninCompletedCallback on_signin_completed_;
};

#endif  // CHROME_BROWSER_SIGNIN_ANDROID_WEB_SIGNIN_BRIDGE_H_
