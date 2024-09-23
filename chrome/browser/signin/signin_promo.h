// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_H_

#include <string>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"

class GURL;

namespace content {
class BrowserContext;
class StoragePartition;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}

// Utility functions for sign in promos.
namespace signin {

extern const char kSignInPromoQueryKeyAccessPoint[];
// TODO(crbug.com/40764426): Auto close is unused. Remove it.
extern const char kSignInPromoQueryKeyAutoClose[];
extern const char kSignInPromoQueryKeyForceKeepData[];
extern const char kSignInPromoQueryKeyReason[];

#if !BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Controls the information displayed around the Gaia Sign In page via the
// "flow" url parameter.
enum class Flow {
  // No value set for the "flow" parameter.
  NONE,
  // The "promo" flow indicates to the user that they are signing in to Chrome
  // but unlike the default dice sign-in page, they don't mention sync
  // benefits.
  PROMO,
  // The "embedded_promo" flow has the same effect as `PROMO` with the addition
  // of providing a page with no outbound links, in order not to be able to open
  // browser page during the signin flow.
  EMBEDDED_PROMO
};

// Wraps arguments for `GetChromeSyncURLForDice()`. They are all optional.
struct ChromeSyncUrlArgs {
  // If not empty, will be passed as hint to the page so that it will be
  // autofilled by Gaia.
  const std::string email;
  // If empty, after login, Gaia may redirect to myaccount.
  const GURL continue_url;
  // If true, the dark mode version of the page will be requested.
  bool request_dark_scheme = false;
  // Sets the "flow" parameter in the gaia sign in url.
  Flow flow = Flow::NONE;
};

// Returns the URL to be used to signin and turn on Sync when DICE is enabled.
// See `ChromeSyncUrlArgs` docs for details on the arguments.
GURL GetChromeSyncURLForDice(ChromeSyncUrlArgs args);

// Returns the URL to be used to reauth.
// As part of `args` only `email` and `continue_url` are used:
// `email` is used to be able to preview the URL with the appropriate email:
// - if the value is empty: the regular sign in page is opened with no prefill.
// - if the value is set and correspond to an existing account used within the
// profile previously: the "Verify it's you" page is opened with the preselected
// account on the next page requesting the authentication. Note: the email can
// still be modified by the user and does not guarantee that the reauth attempt
// will be done on this email/account.
// - if the value is set but the email does not correspond to an account
// previously used within the profile: the regular sign in gaia page is
// displayed with the prefilled email.
// `continue_url` is used to redirect to the given url in case of successful
// reauth.
GURL GetChromeReauthURL(ChromeSyncUrlArgs args);

// Returns the URL to be used to add (secondary) account when DICE is enabled.
// If email is not empty, then it will pass email as hint to the page so that it
// will be autofilled by Gaia.
// If |continue_url| is empty, this may redirect to myaccount.
GURL GetAddAccountURLForDice(const std::string& email,
                             const GURL& continue_url);

// Gets the partition for the embedded sign in frame/webview.
content::StoragePartition* GetSigninPartition(
    content::BrowserContext* browser_context);

// Gets the access point from the query portion of the sign in promo URL.
signin_metrics::AccessPoint GetAccessPointForEmbeddedPromoURL(const GURL& url);

// Gets the sign in reason from the query portion of the sign in promo URL.
signin_metrics::Reason GetSigninReasonForEmbeddedPromoURL(const GURL& url);

// Registers the preferences the Sign In Promo needs.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_PROMO_H_
