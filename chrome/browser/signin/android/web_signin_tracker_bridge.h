// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ANDROID_WEB_SIGNIN_TRACKER_BRIDGE_H_
#define CHROME_BROWSER_SIGNIN_ANDROID_WEB_SIGNIN_TRACKER_BRIDGE_H_

#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/browser/web_signin_tracker.h"
#include "components/signin/public/identity_manager/identity_manager.h"

// Glue class to expose `WebSigninTracker` to Java.
class WebSigninTrackerBridge {
 public:
  // The sign-in isn't tracked until `Start()` is called, so
  // `on_signin_completed` is never run from the constructor.
  explicit WebSigninTrackerBridge(
      signin::IdentityManager* identity_manager,
      AccountReconcilor* account_reconcilor,
      std::variant<CoreAccountId, std::string> signin_account,
      base::OnceCallback<void(signin::WebSigninTracker::Result)>
          on_signin_completed);

  WebSigninTrackerBridge(const WebSigninTrackerBridge&) = delete;
  WebSigninTrackerBridge& operator=(const WebSigninTrackerBridge&) = delete;

  ~WebSigninTrackerBridge();

  // Starts tracking the sign-in. Must be called at most once. If the result is
  // already known (e.g. the account is already in cookies),
  // `on_signin_completed` is run synchronously and may destroy `this` before
  // this method returns.
  void Start();

 private:
  signin::WebSigninTracker tracker_;
};

#endif  // CHROME_BROWSER_SIGNIN_ANDROID_WEB_SIGNIN_TRACKER_BRIDGE_H_
