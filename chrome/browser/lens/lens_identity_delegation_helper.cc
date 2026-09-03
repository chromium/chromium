// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/lens_identity_delegation_helper.h"

#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
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

// Callback for CookieManager::GetCookieList.
void OnCookiesFetched(
    const std::string& email,
    const std::string& origin,
    size_t account_index,
    base::OnceCallback<void(std::vector<std::string>)> callback,
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_cookies) {
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
  int result =
      generate_func(email.c_str(), sapisid_cookie.c_str(), origin.c_str(),
                    timestamp.InMillisecondsSinceUnixEpoch(), &out_hash);
  if (result != 0 || !out_hash) {
    if (out_hash) {
      free_func(out_hash);
    }
    return std::nullopt;
  }

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
  if (!profile || !identity_manager) {
    std::move(callback).Run({});
    return;
  }

  std::string canonical_origin =
      origin.empty() ? "" : url::Origin::Create(GURL(origin)).Serialize();

  signin::AccountsInCookieJarInfo cookie_jar_info =
      identity_manager->GetAccountsInCookieJar();

  // Find a valid signed-in account.
  const std::vector<gaia::ListedAccount>& accounts =
      cookie_jar_info.GetValidSignedInAccounts();

  if (accounts.empty()) {
    // Signed-out case: return only Origin if present.
    std::vector<std::string> headers;
    if (!canonical_origin.empty()) {
      headers.push_back("Origin");
      headers.push_back(canonical_origin);
    }
    std::move(callback).Run(headers);
    return;
  }

  const std::vector<gaia::ListedAccount>& all_accounts =
      cookie_jar_info.GetAllAccounts();

  size_t true_authuser_index = 0;
  bool found_account = false;
  if (authuser_index.has_value() &&
      authuser_index.value() < all_accounts.size()) {
    true_authuser_index = authuser_index.value();
    found_account = true;
  } else {
    CoreAccountInfo primary_account =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

    if (primary_account.IsEmpty() ||
        identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
            primary_account.account_id)) {
      std::move(callback).Run({});
      return;
    }

    for (size_t i = 0; i < all_accounts.size(); ++i) {
      if (all_accounts[i].id == primary_account.account_id) {
        true_authuser_index = i;
        found_account = true;
        break;
      }
    }

    if (!found_account) {
      std::move(callback).Run({});
      return;
    }
  }

  const gaia::ListedAccount& selected_account =
      all_accounts[true_authuser_index];

  // Fetch cookies for google.com to get SAPISID.
  network::mojom::CookieManager* cookie_manager =
      profile->GetDefaultStoragePartition()
          ->GetCookieManagerForBrowserProcess();
  if (!cookie_manager) {
    std::vector<std::string> headers;
    if (!canonical_origin.empty()) {
      headers.push_back("Origin");
      headers.push_back(canonical_origin);
    }
    std::move(callback).Run(headers);
    return;
  }

  // Use google.com as the GURL for cookie retrieval.
  GURL google_url("https://google.com");
  // We MUST use the selected account's email to generate the SAPISIDHASH.
  // The X-Goog-AuthUser header tells the backend which identity's email should
  // be used to verify the signature against the SAPISID cookie.
  cookie_manager->GetCookieList(
      google_url, net::CookieOptions::MakeAllInclusive(),
      net::CookiePartitionKeyCollection(),
      base::BindOnce(&OnCookiesFetched, selected_account.raw_email,
                     canonical_origin, true_authuser_index,
                     std::move(callback)));
}

}  // namespace lens
