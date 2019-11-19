// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/profile_auth_data.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "components/google/core/common/google_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Callback that receives the key for from_partition's saved http auth cache
// proxy entries.
void OnTargetHttpAuthCacheProxyEntriesSaved(
    base::RepeatingClosure completion_callback,
    content::StoragePartition* to_partition,
    const base::UnguessableToken& cache_key) {
  to_partition->GetNetworkContext()->LoadHttpAuthCacheProxyEntries(
      cache_key, completion_callback);
}

// Starts tranferring |from_partition|'s http auth cache's proxy entries into
// |to_partition|.
void TransferHttpAuthCacheProxyEntries(
    base::RepeatingClosure completion_callback,
    content::StoragePartition* from_partition,
    content::StoragePartition* to_partition) {
  // |to_partition| will outlive the call to |completion_callback|.
  // See ProfileAuthData::Transfer.
  from_partition->GetNetworkContext()->SaveHttpAuthCacheProxyEntries(
      base::BindOnce(&OnTargetHttpAuthCacheProxyEntriesSaved,
                     completion_callback, base::Unretained(to_partition)));
}

// Given a |cookie| set during login, returns true if the cookie may have been
// set by GAIA. The main criterion is the |cookie|'s domain. If the domain
// is *google.<TLD> or *youtube.<TLD>, the cookie is considered to have been set
// by GAIA as well.
bool IsGAIACookie(const net::CanonicalCookie& cookie) {
  GURL cookie_url =
      net::cookie_util::CookieOriginToURL(cookie.Domain(), cookie.IsSecure());

  return google_util::IsGoogleDomainUrl(
             cookie_url, google_util::ALLOW_SUBDOMAIN,
             google_util::ALLOW_NON_STANDARD_PORTS) ||
         google_util::IsYoutubeDomainUrl(cookie_url,
                                         google_util::ALLOW_SUBDOMAIN,
                                         google_util::ALLOW_NON_STANDARD_PORTS);
}

void OnCookieSet(base::RepeatingClosure completion_callback,
                 net::CanonicalCookie::CookieInclusionStatus status) {
  completion_callback.Run();
}

// Imports |cookies| into |to_partition|'s cookie jar. |cookie.IsCanonical()|
// must be true for all cookies in |cookies|.
void ImportCookies(base::RepeatingClosure completion_callback,
                   content::StoragePartition* to_partition,
                   const net::CookieList& cookies) {
  if (cookies.empty()) {
    completion_callback.Run();
    return;
  }

  network::mojom::CookieManager* cookie_manager =
      to_partition->GetCookieManagerForBrowserProcess();

  base::RepeatingClosure cookie_completion_callback =
      base::BarrierClosure(cookies.size(), completion_callback);
  for (const auto& cookie : cookies) {
    // Assume secure_source - since the cookies are being restored from
    // another store, they have already gone through the strict secure check.
    // Likewise for permitting same-site marked cookies.
    DCHECK(cookie.IsCanonical());
    net::CookieOptions options;
    options.set_include_httponly();
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);
    cookie_manager->SetCanonicalCookie(
        cookie, "https", options,
        base::BindOnce(&OnCookieSet, cookie_completion_callback));
  }
}

// Callback that receives the contents of |from_partition|'s cookie jar.
// Transfers the necessary cookies to |to_partition|'s cookie jar.
void OnCookiesToTransferRetrieved(base::RepeatingClosure completion_callback,
                                  content::StoragePartition* to_partition,
                                  bool first_login,
                                  const net::CookieList& cookies) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (first_login) {
    ImportCookies(completion_callback, to_partition, cookies);
  } else {
    net::CookieList non_gaia_cookies;
    for (const auto& cookie : cookies) {
      if (!IsGAIACookie(cookie))
        non_gaia_cookies.push_back(cookie);
    }
    ImportCookies(completion_callback, to_partition, non_gaia_cookies);
  }
}

// Callback that receives the content of |to_partition|'s cookie jar. Checks
// whether this is the user's first login, based on the state of the cookie
// jar, and starts retrieval of the data that should be transfered.
void OnTargetCookieJarContentsRetrieved(
    base::RepeatingClosure completion_callback,
    content::StoragePartition* from_partition,
    content::StoragePartition* to_partition,
    bool transfer_auth_cookies_on_first_login,
    bool transfer_saml_auth_cookies_on_subsequent_login,
    const net::CookieList& target_cookies) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool transfer_auth_cookies;

  bool first_login = target_cookies.empty();
  if (first_login) {
    // On first login, transfer all auth cookies if
    // |transfer_auth_cookies_on_first_login| is true.
    transfer_auth_cookies = transfer_auth_cookies_on_first_login;
  } else {
    // On subsequent login, transfer auth cookies set by the SAML IdP if
    // |transfer_saml_auth_cookies_on_subsequent_login| is true.
    transfer_auth_cookies = transfer_saml_auth_cookies_on_subsequent_login;
  }

  if (!transfer_auth_cookies) {
    completion_callback.Run();
    return;
  }

  // Retrieve the contents of |from_partition|'s cookie jar. When the retrieval
  // finishes, OnCookiesToTransferRetrieved will be called with the result.
  network::mojom::CookieManager* from_manager =
      from_partition->GetCookieManagerForBrowserProcess();
  from_manager->GetAllCookies(
      base::BindOnce(&OnCookiesToTransferRetrieved, completion_callback,
                     base::Unretained(to_partition), first_login));
}

// Starts the process of transferring cookies from |from_partition| to
// |to_partition|.
void TransferCookies(base::RepeatingClosure completion_callback,
                     content::StoragePartition* from_partition,
                     content::StoragePartition* to_partition,
                     bool transfer_auth_cookies_on_first_login,
                     bool transfer_saml_auth_cookies_on_subsequent_login) {
  if (transfer_auth_cookies_on_first_login ||
      transfer_saml_auth_cookies_on_subsequent_login) {
    // Retrieve the contents of |to_partition_|'s cookie jar.
    network::mojom::CookieManager* to_manager =
        to_partition->GetCookieManagerForBrowserProcess();
    to_manager->GetAllCookies(base::BindOnce(
        &OnTargetCookieJarContentsRetrieved, completion_callback,
        base::Unretained(from_partition), base::Unretained(to_partition),
        transfer_auth_cookies_on_first_login,
        transfer_saml_auth_cookies_on_subsequent_login));
  } else {
    completion_callback.Run();
  }
}

}  // namespace

void ProfileAuthData::Transfer(
    content::StoragePartition* from_partition,
    content::StoragePartition* to_partition,
    bool transfer_auth_cookies_on_first_login,
    bool transfer_saml_auth_cookies_on_subsequent_login,
    base::OnceClosure completion_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The BarrierClosure will call |completion_callback| after the 2 async
  // transfers have finished.
  base::RepeatingClosure task_completion_callback =
      base::BarrierClosure(2, std::move(completion_callback));

  // Transfer the proxy auth cache entries from |from_context| to |to_context|.
  // If the user was required to authenticate with a proxy during login, this
  // authentication information will be transferred into the user's session.
  TransferHttpAuthCacheProxyEntries(task_completion_callback, from_partition,
                                    to_partition);

  TransferCookies(task_completion_callback, from_partition, to_partition,
                  transfer_auth_cookies_on_first_login,
                  transfer_saml_auth_cookies_on_subsequent_login);
}

}  // namespace chromeos
