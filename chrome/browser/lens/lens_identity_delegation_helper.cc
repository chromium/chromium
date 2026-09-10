// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/lens_identity_delegation_helper.h"

#include "base/compiler_specific.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/optimization_guide_library_holder.h"
#include "components/optimization_guide/optimization_guide_buildflags.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

namespace {

// Helper to find a cookie by name in the CookieAccessResultList.
std::optional<net::CanonicalCookie> GetCookie(
    const net::CookieAccessResultList& cookie_list,
    const std::string& cookie_name) {
  auto it = std::ranges::find_if(
      cookie_list,
      [&cookie_name](
          const net::CookieWithAccessResult& cookie_with_access_result) {
        return cookie_with_access_result.cookie.Name() == cookie_name;
      });

  if (it != cookie_list.end()) {
    return it->cookie;
  }
  return std::nullopt;
}

void RecordFetchHeadersMetrics(lens::LensIdentityDelegationFetchStatus status,
                               base::TimeDelta duration) {
  base::UmaHistogramEnumeration("Lens.IdentityDelegation.FetchHeadersStatus",
                                status);
  base::UmaHistogramTimes("Lens.IdentityDelegation.TimeToFetchHeaders",
                          duration);
  std::string_view status_str;
  switch (status) {
    case lens::LensIdentityDelegationFetchStatus::kSuccess:
      status_str = "Success";
      break;
    case lens::LensIdentityDelegationFetchStatus::kSignedOut:
      status_str = "SignedOut";
      break;
    case lens::LensIdentityDelegationFetchStatus::kAccountError:
      status_str = "AccountError";
      break;
    case lens::LensIdentityDelegationFetchStatus::kNoSapisidCookie:
      status_str = "NoSapisidCookie";
      break;
    case lens::LensIdentityDelegationFetchStatus::kHashFailed:
      status_str = "HashFailed";
      break;
    case lens::LensIdentityDelegationFetchStatus::kNoCookieManager:
      status_str = "NoCookieManager";
      break;
  }
  base::UmaHistogramTimes(
      base::StrCat({"Lens.IdentityDelegation.TimeToFetchHeaders.", status_str}),
      duration);
}

// Callback for CookieManager::GetCookieList.
void OnCookiesFetched(
    const std::string& email,
    const std::string& origin,
    size_t account_index,
    base::TimeTicks fetch_start_time,
    base::TimeTicks cookie_fetch_start_time,
    base::OnceCallback<void(std::vector<std::string>)> callback,
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_cookies) {
  base::UmaHistogramTimes("Lens.IdentityDelegation.TimeToFetchCookies",
                          base::TimeTicks::Now() - cookie_fetch_start_time);

  base::TimeDelta fetch_duration = base::TimeTicks::Now() - fetch_start_time;

  std::optional<net::CanonicalCookie> sapisid_cookie =
      GetCookie(cookie_list, "SAPISID");

  std::vector<std::string> headers;
  if (!origin.empty()) {
    headers.push_back("Origin");
    headers.push_back(origin);
  }

  if (!sapisid_cookie.has_value()) {
    // If no SAPISID cookie, return only the Origin header (signed-out
    // behavior).
    RecordFetchHeadersMetrics(
        lens::LensIdentityDelegationFetchStatus::kNoSapisidCookie,
        fetch_duration);
    std::move(callback).Run(headers);
    return;
  }

  base::Time now = base::Time::Now();
  std::optional<std::string> auth_header =
      lens::GenerateSapisidHash(email, sapisid_cookie->Value(), origin, now);

  if (auth_header.has_value()) {
    headers.push_back("Authorization");
    headers.push_back(auth_header.value());
    headers.push_back("X-Goog-AuthUser");
    headers.push_back(base::NumberToString(account_index));
    RecordFetchHeadersMetrics(lens::LensIdentityDelegationFetchStatus::kSuccess,
                              fetch_duration);
  } else {
    RecordFetchHeadersMetrics(
        lens::LensIdentityDelegationFetchStatus::kHashFailed, fetch_duration);
  }

  std::move(callback).Run(headers);
}

}  // namespace

namespace lens {

DISABLE_CFI_DLSYM
std::optional<std::string> GenerateSapisidHash(
    const std::string& email,
    const std::string& sapisid_cookie,
    const std::string& origin,
    base::Time timestamp) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
  optimization_guide::OptimizationGuideLibraryHolder* loader =
      optimization_guide::OptimizationGuideLibraryHolder::GetInstance();
  if (!loader) {
    return std::nullopt;
  }
  typedef int (*GenerateFunc)(const char*, const char*, const char*, int64_t,
                              char**);
  typedef void (*FreeFunc)(char*);

  GenerateFunc generate_func = reinterpret_cast<GenerateFunc>(
      loader->GetFunctionPointer("GenerateSapisidHash"));
  FreeFunc free_func =
      reinterpret_cast<FreeFunc>(loader->GetFunctionPointer("FreeSapisidHash"));

  if (!generate_func || !free_func) {
    return std::nullopt;
  }

  char* out_hash = nullptr;
  base::TimeTicks start_time = base::TimeTicks::Now();
  int result =
      generate_func(email.c_str(), sapisid_cookie.c_str(), origin.c_str(),
                    timestamp.InMillisecondsSinceUnixEpoch(), &out_hash);
  if (result != 0 || !out_hash) {
    if (out_hash) {
      free_func(out_hash);
    }
    return std::nullopt;
  }

  base::UmaHistogramMicrosecondsTimes(
      "Lens.IdentityDelegation.TimeToGenerateSapisidHash",
      base::TimeTicks::Now() - start_time);
  std::string hash_str(out_hash);
  free_func(out_hash);
  return hash_str;
#else
  return std::nullopt;
#endif
}

void FetchIdentityDelegationHeaders(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    const std::string& origin,
    std::optional<size_t> authuser_index,
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  base::TimeTicks fetch_start_time = base::TimeTicks::Now();
  std::string canonical_origin =
      origin.empty() ? "" : url::Origin::Create(GURL(origin)).Serialize();

  auto return_signed_out_headers =
      [&canonical_origin, &callback,
       fetch_start_time](LensIdentityDelegationFetchStatus status) {
        RecordFetchHeadersMetrics(status,
                                  base::TimeTicks::Now() - fetch_start_time);
        std::vector<std::string> headers;
        if (!canonical_origin.empty()) {
          headers.push_back("Origin");
          headers.push_back(canonical_origin);
        }
        std::move(callback).Run(std::move(headers));
      };

  if (!profile || !identity_manager) {
    return_signed_out_headers(LensIdentityDelegationFetchStatus::kSignedOut);
    return;
  }

  signin::AccountsInCookieJarInfo cookie_jar_info =
      identity_manager->GetAccountsInCookieJar();

  // Find a valid signed-in account.
  const std::vector<gaia::ListedAccount>& accounts =
      cookie_jar_info.GetValidSignedInAccounts();

  if (accounts.empty()) {
    return_signed_out_headers(LensIdentityDelegationFetchStatus::kSignedOut);
    return;
  }

  const std::vector<gaia::ListedAccount>& all_accounts =
      cookie_jar_info.GetAllAccounts();

  size_t true_authuser_index = 0;
  bool found_account = false;
  if (authuser_index.has_value()) {
    if (authuser_index.value() < all_accounts.size()) {
      const gaia::ListedAccount& candidate =
          all_accounts[authuser_index.value()];
      if (candidate.valid && !candidate.signed_out &&
          !identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
              candidate.id)) {
        true_authuser_index = authuser_index.value();
        found_account = true;
      }
    }
  } else {
    CoreAccountInfo primary_account =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

    if (!primary_account.IsEmpty() &&
        !identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
            primary_account.account_id)) {
      for (size_t i = 0; i < all_accounts.size(); ++i) {
        if (all_accounts[i].id == primary_account.account_id &&
            all_accounts[i].valid && !all_accounts[i].signed_out) {
          true_authuser_index = i;
          found_account = true;
          break;
        }
      }
    }
  }

  if (!found_account) {
    return_signed_out_headers(LensIdentityDelegationFetchStatus::kAccountError);
    return;
  }

  const gaia::ListedAccount& selected_account =
      all_accounts[true_authuser_index];

  // Fetch cookies for google.com to get SAPISID.
  network::mojom::CookieManager* cookie_manager =
      profile->GetDefaultStoragePartition()
          ->GetCookieManagerForBrowserProcess();
  if (!cookie_manager) {
    return_signed_out_headers(
        LensIdentityDelegationFetchStatus::kNoCookieManager);
    return;
  }

  // Use google.com as the GURL for cookie retrieval.
  GURL google_url("https://google.com");
  base::TimeTicks cookie_fetch_start_time = base::TimeTicks::Now();
  cookie_manager->GetCookieList(
      google_url, net::CookieOptions::MakeAllInclusive(),
      net::CookiePartitionKeyCollection(),
      base::BindOnce(&OnCookiesFetched, selected_account.raw_email,
                     canonical_origin, true_authuser_index, fetch_start_time,
                     cookie_fetch_start_time, std::move(callback)));
}

}  // namespace lens
