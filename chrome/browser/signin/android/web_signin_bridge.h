// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ANDROID_WEB_SIGNIN_BRIDGE_H_
#define CHROME_BROWSER_SIGNIN_ANDROID_WEB_SIGNIN_BRIDGE_H_

#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/browser/web_signin_tracker.h"
#include "components/signin/public/identity_manager/identity_manager.h"

// Glue class to expose `WebSigninTracker` to Java.
class WebSigninBridge {
 public:
  explicit WebSigninBridge(
      signin::IdentityManager* identity_manager,
      AccountReconcilor* account_reconcilor,
      CoreAccountId signin_account,
      base::OnceCallback<void(signin::WebSigninTracker::Result)>
          on_signin_completed);

  WebSigninBridge(const WebSigninBridge&) = delete;
  WebSigninBridge& operator=(const WebSigninBridge&) = delete;

  ~WebSigninBridge();

 private:
  signin::WebSigninTracker tracker_;
};

#endif  // CHROME_BROWSER_SIGNIN_ANDROID_WEB_SIGNIN_BRIDGE_H_
