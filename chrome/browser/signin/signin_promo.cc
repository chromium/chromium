// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace signin {

const char kSignInPromoQueryKeyAccessPoint[] = "access_point";
const char kSignInPromoQueryKeyAutoClose[] = "auto_close";
const char kSignInPromoQueryKeyForceKeepData[] = "force_keep_data";
const char kSignInPromoQueryKeyReason[] = "reason";

#if !defined(OS_CHROMEOS)
GURL GetEmbeddedPromoURL(signin_metrics::AccessPoint access_point,
                         signin_metrics::Reason reason,
                         bool auto_close) {
  CHECK_LT(static_cast<int>(access_point),
           static_cast<int>(signin_metrics::AccessPoint::ACCESS_POINT_MAX));
  CHECK_NE(static_cast<int>(access_point),
           static_cast<int>(signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN));
  CHECK_LT(static_cast<int>(reason),
           static_cast<int>(signin_metrics::Reason::REASON_MAX));
  CHECK_NE(static_cast<int>(reason),
           static_cast<int>(signin_metrics::Reason::REASON_UNKNOWN_REASON));

  GURL url(chrome::kChromeUIChromeSigninURL);
  url = net::AppendQueryParameter(
      url, signin::kSignInPromoQueryKeyAccessPoint,
      base::NumberToString(static_cast<int>(access_point)));
  url =
      net::AppendQueryParameter(url, signin::kSignInPromoQueryKeyReason,
                                base::NumberToString(static_cast<int>(reason)));
  if (auto_close) {
    url = net::AppendQueryParameter(url, signin::kSignInPromoQueryKeyAutoClose,
                                    "1");
  }
  return url;
}

GURL GetEmbeddedReauthURLWithEmail(signin_metrics::AccessPoint access_point,
                                   signin_metrics::Reason reason,
                                   const std::string& email) {
  GURL url = GetEmbeddedPromoURL(access_point, reason, /*auto_close=*/true);
  url = net::AppendQueryParameter(url, "email", email);
  url = net::AppendQueryParameter(url, "validateEmail", "1");
  return net::AppendQueryParameter(url, "readOnlyEmail", "1");
}
#endif  // !defined(OS_CHROMEOS)

GURL GetChromeSyncURLForDice(const std::string& email,
                             const std::string& continue_url) {
  GURL url = GaiaUrls::GetInstance()->signin_chrome_sync_dice();
  if (!email.empty())
    url = net::AppendQueryParameter(url, "email_hint", email);
  if (!continue_url.empty())
    url = net::AppendQueryParameter(url, "continue", continue_url);
  return url;
}

GURL GetAddAccountURLForDice(const std::string& email,
                             const std::string& continue_url) {
  GURL url = GaiaUrls::GetInstance()->add_account_url();
  if (!email.empty())
    url = net::AppendQueryParameter(url, "Email", email);
  if (!continue_url.empty())
    url = net::AppendQueryParameter(url, "continue", continue_url);
  return url;
}

GURL GetSigninPartitionURL() {
  return GURL("chrome-guest://chrome-signin/?");
}

signin_metrics::AccessPoint GetAccessPointForEmbeddedPromoURL(const GURL& url) {
  std::string value;
  if (!net::GetValueForKeyInQuery(url, kSignInPromoQueryKeyAccessPoint,
                                  &value)) {
    return signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN;
  }

  int access_point = -1;
  base::StringToInt(value, &access_point);
  if (access_point <
          static_cast<int>(
              signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE) ||
      access_point >=
          static_cast<int>(signin_metrics::AccessPoint::ACCESS_POINT_MAX)) {
    return signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN;
  }

  return static_cast<signin_metrics::AccessPoint>(access_point);
}

signin_metrics::Reason GetSigninReasonForEmbeddedPromoURL(const GURL& url) {
  std::string value;
  if (!net::GetValueForKeyInQuery(url, kSignInPromoQueryKeyReason, &value))
    return signin_metrics::Reason::REASON_UNKNOWN_REASON;

  int reason = -1;
  base::StringToInt(value, &reason);
  if (reason < static_cast<int>(
                   signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT) ||
      reason >= static_cast<int>(signin_metrics::Reason::REASON_MAX)) {
    return signin_metrics::Reason::REASON_UNKNOWN_REASON;
  }

  return static_cast<signin_metrics::Reason>(reason);
}

bool IsAutoCloseEnabledInEmbeddedURL(const GURL& url) {
  std::string value;
  if (net::GetValueForKeyInQuery(url, kSignInPromoQueryKeyAutoClose, &value)) {
    int enabled = 0;
    if (base::StringToInt(value, &enabled) && enabled == 1)
      return true;
  }
  return false;
}

void RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kDiceSigninUserMenuPromoCount, 0);
}

}  // namespace signin
