// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo.h"

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_pref_names.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition_config.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace signin {

const char kSignInPromoQueryKeyAccessPoint[] = "access_point";
const char kSignInPromoQueryKeyAutoClose[] = "auto_close";
const char kSignInPromoQueryKeyForceKeepData[] = "force_keep_data";
const char kSignInPromoQueryKeyReason[] = "reason";

#if !BUILDFLAG(IS_CHROMEOS_ASH)
GURL GetEmbeddedPromoURL(signin_metrics::AccessPoint access_point,
                         signin_metrics::Reason reason,
                         bool auto_close) {
  CHECK_LT(static_cast<int>(access_point),
           static_cast<int>(signin_metrics::AccessPoint::ACCESS_POINT_MAX));
  CHECK_NE(static_cast<int>(access_point),
           static_cast<int>(signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN));
  CHECK_LE(static_cast<int>(reason),
           static_cast<int>(signin_metrics::Reason::kMaxValue));
  CHECK_NE(static_cast<int>(reason),
           static_cast<int>(signin_metrics::Reason::kUnknownReason));

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
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

GURL GetChromeSyncURLForDice(ChromeSyncUrlArgs args) {
  GURL url = GaiaUrls::GetInstance()->signin_chrome_sync_dice();
  if (!args.email.empty()) {
    url = net::AppendQueryParameter(url, "email_hint", args.email);
  }
  if (!args.continue_url.is_empty()) {
    url = net::AppendQueryParameter(url, "continue", args.continue_url.spec());
  }
  if (args.request_dark_scheme) {
    url = net::AppendQueryParameter(url, "color_scheme", "dark");
  }
  switch (args.flow) {
    // Default behavior.
    case Flow::NONE:
      break;
    case Flow::PROMO:
      url = net::AppendQueryParameter(url, "flow", "promo");
      break;
    case Flow::EMBEDDED_PROMO:
      url = net::AppendQueryParameter(url, "flow", "embedded_promo");
      break;
  }
  return url;
}

GURL GetChromeReauthURL(ChromeSyncUrlArgs args) {
  GURL url = GaiaUrls::GetInstance()->reauth_chrome_dice();
  if (!args.email.empty()) {
    url = net::AppendQueryParameter(url, "Email", args.email);
  }
  if (!args.continue_url.is_empty()) {
    url = net::AppendQueryParameter(url, "continue", args.continue_url.spec());
  }
  return url;
}

GURL GetAddAccountURLForDice(const std::string& email,
                             const GURL& continue_url) {
  GURL url = GaiaUrls::GetInstance()->add_account_url();
  if (!email.empty())
    url = net::AppendQueryParameter(url, "Email", email);
  if (!continue_url.is_empty()) {
    url = net::AppendQueryParameter(url, "continue", continue_url.spec());
  }
  return url;
}

content::StoragePartition* GetSigninPartition(
    content::BrowserContext* browser_context) {
  const auto signin_partition_config = content::StoragePartitionConfig::Create(
      browser_context, "chrome-signin", /* partition_name= */ "",
      /* in_memory= */ true);
  return browser_context->GetStoragePartition(signin_partition_config);
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
    return signin_metrics::Reason::kUnknownReason;

  int reason = -1;
  base::StringToInt(value, &reason);
  if (reason <
          static_cast<int>(signin_metrics::Reason::kSigninPrimaryAccount) ||
      reason > static_cast<int>(signin_metrics::Reason::kMaxValue)) {
    return signin_metrics::Reason::kUnknownReason;
  }

  return static_cast<signin_metrics::Reason>(reason);
}

void RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kDiceSigninUserMenuPromoCount, 0);
  registry->RegisterIntegerPref(
      prefs::kAutofillSignInPromoDismissCountPerProfile, 0);
  registry->RegisterIntegerPref(prefs::kPasswordSignInPromoShownCountPerProfile,
                                0);
}

}  // namespace signin
