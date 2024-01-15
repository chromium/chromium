// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_observer.h"

#include "content/public/browser/storage_partition.h"
#include "net/cookies/canonical_cookie.h"

namespace {
std::optional<const net::CanonicalCookie> GetCookie(
    const net::CookieAccessResultList& cookie_list,
    const std::string& cookie_name) {
  auto it = base::ranges::find_if(
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
}  // namespace

BoundSessionCookieObserver::BoundSessionCookieObserver(
    content::StoragePartition* storage_partion,
    const GURL& url,
    const std::string& cookie_name,
    CookieExpirationDateUpdate callback)
    : storage_partition_(storage_partion),
      url_(url),
      cookie_name_(cookie_name),
      callback_(std::move(callback)) {
  AddCookieChangeListener();
  StartGetCookieList();
}

BoundSessionCookieObserver::~BoundSessionCookieObserver() = default;

void BoundSessionCookieObserver::StartGetCookieList() {
  network::mojom::CookieManager* cookie_manager =
      storage_partition_->GetCookieManagerForBrowserProcess();
  if (!cookie_manager) {
    return;
  }

  cookie_manager->GetCookieList(
      url_, net::CookieOptions::MakeAllInclusive(),
      net::CookiePartitionKeyCollection::Todo(),
      base::BindOnce(&BoundSessionCookieObserver::OnGetCookieList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BoundSessionCookieObserver::OnGetCookieList(
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_cookies) {
  std::optional<const net::CanonicalCookie> cookie =
      GetCookie(cookie_list, cookie_name_);
  DCHECK(!GetCookie(excluded_cookies, cookie_name_).has_value())
      << "BSC cookie should not be excluded!";
  callback_.Run(cookie_name_,
                cookie.has_value() ? cookie->ExpiryDate() : base::Time());
}

void BoundSessionCookieObserver::OnCookieChange(
    const net::CookieChangeInfo& change) {
  DCHECK_EQ(change.cookie.Name(), cookie_name_);
  DCHECK(change.cookie.IsDomainMatch(url_.host()));
  switch (change.cause) {
    // The cookie was inserted.
    case net::CookieChangeCause::INSERTED:
      callback_.Run(cookie_name_, change.cookie.ExpiryDate());
      break;

    // The cookie was automatically removed due to an insert operation that
    // overwrote it. Replacing an existing cookie is actually a two-phase
    // delete + set operation, so we get an extra notification.
    case net::CookieChangeCause::OVERWRITE:
      // Skip the notification as `change.value` contains the old cookie value.
      break;

    // Cookie removed/expired.
    // The cookie was deleted, but no more details are known.
    case net::CookieChangeCause::UNKNOWN_DELETION:
    // The cookie was changed directly by a consumer's action.
    case net::CookieChangeCause::EXPLICIT:
    // The cookie was automatically evicted during garbage collection.
    case net::CookieChangeCause::EVICTED:
    // The cookie was overwritten with an already-expired expiration date.
    case net::CookieChangeCause::EXPIRED_OVERWRITE:
      DCHECK(net::CookieChangeCauseIsDeletion(change.cause));
      callback_.Run(cookie_name_, base::Time());
      break;

    // The cookie was automatically removed as it expired.
    case net::CookieChangeCause::EXPIRED:
      DCHECK(net::CookieChangeCauseIsDeletion(change.cause));
      DCHECK(change.cookie.ExpiryDate() < base::Time::Now());
      callback_.Run(cookie_name_, change.cookie.ExpiryDate());
  }
}

void BoundSessionCookieObserver::AddCookieChangeListener() {
  DCHECK(!cookie_listener_receiver_.is_bound());
  network::mojom::CookieManager* cookie_manager =
      storage_partition_->GetCookieManagerForBrowserProcess();
  // NOTE: `cookie_manager` can be nullptr when TestSigninClient is used in
  // testing contexts.
  if (!cookie_manager) {
    return;
  }

  cookie_manager->AddCookieChangeListener(
      url_, cookie_name_, cookie_listener_receiver_.BindNewPipeAndPassRemote());
  // Unretained is safe as `this` owns `cookie_listener_receiver_`.
  cookie_listener_receiver_.set_disconnect_handler(base::BindOnce(
      &BoundSessionCookieObserver::OnCookieChangeListenerConnectionError,
      base::Unretained(this)));
}

void BoundSessionCookieObserver::OnCookieChangeListenerConnectionError() {
  // A connection error from the CookieManager likely means that the network
  // service process has crashed. Try again to set up a listener.
  cookie_listener_receiver_.reset();
  AddCookieChangeListener();
  StartGetCookieList();
}
