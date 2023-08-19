// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_OBSERVER_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_OBSERVER_H_

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller_impl.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace content {
class StoragePartition;
}

class GURL;

// This class monitors a cookie and notifies a callback whenever the expiration
// of the cookie changes.
class BoundSessionCookieObserver : public network::mojom::CookieChangeListener {
 public:
  // Returns the expected expiration date of the observed cookie or
  // `base::Time()` if the cookie was removed.
  using CookieExpirationDateUpdate =
      base::RepeatingCallback<void(const std::string&, base::Time)>;

  // `storage_partition_` must outlive `this`.
  BoundSessionCookieObserver(content::StoragePartition* storage_partion_,
                             const GURL& url,
                             const std::string& cookie_name,
                             CookieExpirationDateUpdate callback);

  ~BoundSessionCookieObserver() override;

 private:
  friend class BoundSessionCookieControllerImplTest;

  void AddCookieChangeListener();
  void OnCookieChangeListenerConnectionError();

  // network::mojom::CookieChangeListener
  void OnCookieChange(const net::CookieChangeInfo& change) override;

  void StartGetCookieList();
  void OnGetCookieList(const net::CookieAccessResultList& cookie_list,
                       const net::CookieAccessResultList& excluded_cookies);

  // Connection to the CookieManager that signals when the cookie changes.
  mojo::Receiver<network::mojom::CookieChangeListener>
      cookie_listener_receiver_{this};

  const raw_ptr<content::StoragePartition> storage_partition_;
  const GURL url_;
  const std::string cookie_name_;

  // Called on startup if the cookie exists and whenever the cookie changes.
  CookieExpirationDateUpdate callback_;

  base::WeakPtrFactory<BoundSessionCookieObserver> weak_ptr_factory_{this};
};
#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_OBSERVER_H_
