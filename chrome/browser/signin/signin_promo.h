// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_H_

#include <string>

#include "build/build_config.h"
#include "components/signin/public/base/signin_metrics.h"

class GURL;

namespace user_prefs {
class PrefRegistrySyncable;
}

// Utility functions for sign in promos.
namespace signin {

extern const char kSignInPromoQueryKeyAccessPoint[];
extern const char kSignInPromoQueryKeyAutoClose[];
extern const char kSignInPromoQueryKeyForceKeepData[];
extern const char kSignInPromoQueryKeyReason[];

#if !defined(OS_CHROMEOS)
// These functions are only used to unlock the profile from the desktop user
// manager and the windows credential provider.

// Returns the sign in promo URL that can be used in a modal dialog with
// the given arguments in the query.
// |access_point| indicates where the sign in is being initiated.
// |reason| indicates the purpose of using this URL.
// |auto_close| whether to close the sign in promo automatically when done.
GURL GetEmbeddedPromoURL(signin_metrics::AccessPoint access_point,
                         signin_metrics::Reason reason,
                         bool auto_close);

// Returns a sign in promo URL specifically for reauthenticating |email| that
// can be used in a modal dialog.
GURL GetEmbeddedReauthURLWithEmail(signin_metrics::AccessPoint access_point,
                                   signin_metrics::Reason reason,
                                   const std::string& email);
#endif  // !defined(OS_CHROMEOS)

// Returns the URL to be used to signin and turn on Sync when DICE is enabled.
// If email is not empty, then it will pass email as hint to the page so that it
// will be autofilled by Gaia.
// If |continue_url| is empty, this may redirect to myaccount.
GURL GetChromeSyncURLForDice(const std::string& email,
                             const std::string& continue_url);

// Returns the URL to be used to add (secondary) account when DICE is enabled.
// If email is not empty, then it will pass email as hint to the page so that it
// will be autofilled by Gaia.
// If |continue_url| is empty, this may redirect to myaccount.
GURL GetAddAccountURLForDice(const std::string& email,
                             const std::string& continue_url);

// Gets the partition URL for the embedded sign in frame/webview.
GURL GetSigninPartitionURL();

// Gets the access point from the query portion of the sign in promo URL.
signin_metrics::AccessPoint GetAccessPointForEmbeddedPromoURL(const GURL& url);

// Gets the sign in reason from the query portion of the sign in promo URL.
signin_metrics::Reason GetSigninReasonForEmbeddedPromoURL(const GURL& url);

// Returns true if the auto_close parameter in the given URL is set to true.
bool IsAutoCloseEnabledInEmbeddedURL(const GURL& url);

// Registers the preferences the Sign In Promo needs.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_H_
