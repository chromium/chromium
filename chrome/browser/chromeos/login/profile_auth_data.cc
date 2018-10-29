// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/profile_auth_data.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/google/core/common/google_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace chromeos {

namespace {

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

class ProfileAuthDataTransferer
    : public base::RefCountedDeleteOnSequence<ProfileAuthDataTransferer> {
 public:
  ProfileAuthDataTransferer(content::StoragePartition* from_partition,
                            content::StoragePartition* to_partition,
                            bool transfer_auth_cookies_on_first_login,
                            bool transfer_saml_auth_cookies_on_subsequent_login,
                            const base::Closure& completion_callback);

  void BeginTransfer();

 private:
  friend class RefCountedDeleteOnSequence<ProfileAuthDataTransferer>;
  friend class base::DeleteHelper<ProfileAuthDataTransferer>;

  ~ProfileAuthDataTransferer();

  // Transfer the proxy auth cache from |from_context_| to |to_context_|. If
  // the user was required to authenticate with a proxy during login, this
  // authentication information will be transferred into the user's session.
  void TransferProxyAuthCache();

  // Callback that receives the content of |to_partition_|'s cookie jar. Checks
  // whether this is the user's first login, based on the state of the cookie
  // jar, and starts retrieval of the data that should be transfered.
  void OnTargetCookieJarContentsRetrieved(
      const net::CookieList& target_cookies);

  // Retrieve the contents of |from_partition_|'s cookie jar. When the retrieval
  // finishes, OnCookiesToTransferRetrieved will be called with the result.
  void RetrieveCookiesToTransfer();

  // Callback that receives the contents of |from_partition_|'s cookie jar.
  // Transfers the necessary cookies to |to_partition_|'s cookie jar.
  void OnCookiesToTransferRetrieved(const net::CookieList& cookies_to_transfer);

  // Imports |cookies| into |to_partition_|'s cookie jar. |cookie.IsCanonical()|
  // must be true for all cookies in |cookies|.
  void ImportCookies(const net::CookieList& cookies);
  void OnCookieSet(bool result);

  content::StoragePartition* from_partition_;
  scoped_refptr<net::URLRequestContextGetter> from_context_;
  content::StoragePartition* to_partition_;
  scoped_refptr<net::URLRequestContextGetter> to_context_;
  bool transfer_auth_cookies_on_first_login_;
  bool transfer_saml_auth_cookies_on_subsequent_login_;
  base::OnceClosure completion_callback_;

  bool first_login_ = false;
};

ProfileAuthDataTransferer::ProfileAuthDataTransferer(
    content::StoragePartition* from_partition,
    content::StoragePartition* to_partition,
    bool transfer_auth_cookies_on_first_login,
    bool transfer_saml_auth_cookies_on_subsequent_login,
    const base::Closure& completion_callback)
    : RefCountedDeleteOnSequence<ProfileAuthDataTransferer>(
          base::ThreadTaskRunnerHandle::Get()),
      from_partition_(from_partition),
      from_context_(from_partition->GetURLRequestContext()),
      to_partition_(to_partition),
      to_context_(to_partition->GetURLRequestContext()),
      transfer_auth_cookies_on_first_login_(
          transfer_auth_cookies_on_first_login),
      transfer_saml_auth_cookies_on_subsequent_login_(
          transfer_saml_auth_cookies_on_subsequent_login),
      completion_callback_(completion_callback) {}

ProfileAuthDataTransferer::~ProfileAuthDataTransferer() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!completion_callback_.is_null())
    std::move(completion_callback_).Run();
}

void ProfileAuthDataTransferer::BeginTransfer() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&ProfileAuthDataTransferer::TransferProxyAuthCache, this));

  if (transfer_auth_cookies_on_first_login_ ||
      transfer_saml_auth_cookies_on_subsequent_login_) {
    // Retrieve the contents of |to_partition_|'s cookie jar.
    network::mojom::CookieManager* to_manager =
        to_partition_->GetCookieManagerForBrowserProcess();
    to_manager->GetAllCookies(base::BindOnce(
        &ProfileAuthDataTransferer::OnTargetCookieJarContentsRetrieved, this));
  }
}

void ProfileAuthDataTransferer::TransferProxyAuthCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::HttpAuthCache* new_cache = to_context_->GetURLRequestContext()
                                      ->http_transaction_factory()
                                      ->GetSession()
                                      ->http_auth_cache();
  new_cache->UpdateAllFrom(*from_context_->GetURLRequestContext()
                                ->http_transaction_factory()
                                ->GetSession()
                                ->http_auth_cache());
}

void ProfileAuthDataTransferer::OnTargetCookieJarContentsRetrieved(
    const net::CookieList& target_cookies) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool transfer_auth_cookies = false;

  first_login_ = target_cookies.empty();
  if (first_login_) {
    // On first login, transfer all auth cookies if
    // |transfer_auth_cookies_on_first_login_| is true.
    transfer_auth_cookies = transfer_auth_cookies_on_first_login_;
  } else {
    // On subsequent login, transfer auth cookies set by the SAML IdP if
    // |transfer_saml_auth_cookies_on_subsequent_login_| is true.
    transfer_auth_cookies = transfer_saml_auth_cookies_on_subsequent_login_;
  }

  if (transfer_auth_cookies)
    RetrieveCookiesToTransfer();
}

void ProfileAuthDataTransferer::RetrieveCookiesToTransfer() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  network::mojom::CookieManager* from_manager =
      from_partition_->GetCookieManagerForBrowserProcess();
  from_manager->GetAllCookies(base::BindOnce(
      &ProfileAuthDataTransferer::OnCookiesToTransferRetrieved, this));
}

void ProfileAuthDataTransferer::OnCookiesToTransferRetrieved(
    const net::CookieList& cookies) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (first_login_) {
    ImportCookies(cookies);
  } else {
    net::CookieList non_gaia_cookies;
    for (const auto& cookie : cookies) {
      if (!IsGAIACookie(cookie))
        non_gaia_cookies.push_back(cookie);
    }
    ImportCookies(non_gaia_cookies);
  }
}

void ProfileAuthDataTransferer::ImportCookies(const net::CookieList& cookies) {
  network::mojom::CookieManager* cookie_manager =
      to_partition_->GetCookieManagerForBrowserProcess();

  for (const auto& cookie : cookies) {
    // Assume secure_source - since the cookies are being restored from
    // another store, they have already gone through the strict secure check.
    DCHECK(cookie.IsCanonical());
    cookie_manager->SetCanonicalCookie(
        cookie, true /*secure_source*/, true /*modify_http_only*/,
        base::BindOnce(&ProfileAuthDataTransferer::OnCookieSet, this));
  }
}

void ProfileAuthDataTransferer::OnCookieSet(bool result) {
  // This function does nothing but extend the lifetime of |this| until after
  // all cookies have been transferred.
}

}  // namespace

void ProfileAuthData::Transfer(
    content::StoragePartition* from_partition,
    content::StoragePartition* to_partition,
    bool transfer_auth_cookies_on_first_login,
    bool transfer_saml_auth_cookies_on_subsequent_login,
    const base::Closure& completion_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::MakeRefCounted<ProfileAuthDataTransferer>(
      from_partition, to_partition, transfer_auth_cookies_on_first_login,
      transfer_saml_auth_cookies_on_subsequent_login, completion_callback)
      ->BeginTransfer();
}

}  // namespace chromeos
