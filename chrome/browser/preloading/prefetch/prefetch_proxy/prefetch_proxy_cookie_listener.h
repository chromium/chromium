// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_COOKIE_LISTENER_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_COOKIE_LISTENER_H_

#include <memory>

#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

// Listens for changes to the cookies for a specific URL. This includes both
// host and domain cookies. Keeps track of whether the cookies have changed or
// remained the same since the object is created until StopListening() is
// called.
class PrefetchProxyCookieListener
    : public network::mojom::CookieChangeListener {
 public:
  static std::unique_ptr<PrefetchProxyCookieListener> MakeAndRegister(
      const GURL& url,
      base::OnceCallback<void(const GURL&)> on_cookie_change_callback,
      network::mojom::CookieManager* cookie_manager);

  PrefetchProxyCookieListener(
      const GURL& url,
      base::OnceCallback<void(const GURL&)> on_cookie_change_callback);
  ~PrefetchProxyCookieListener() override;

  PrefetchProxyCookieListener(const PrefetchProxyCookieListener&) = delete;
  PrefetchProxyCookieListener& operator=(const PrefetchProxyCookieListener&) =
      delete;

  // Causes the Cookie Listener to stop listening to cookie changes to |url_|.
  // After this is called the value of |have_cookies_changed_| will no longer
  // change.
  void StopListening();

  // Gets whether the cookies of |url_| have changed between the creation of the
  // object and either the first time |StopListening| is called or now (if
  // |StopListening| has never been called).
  bool HaveCookiesChanged() const { return have_cookies_changed_; }

 private:
  // network::mojom::CookieChangeListener
  void OnCookieChange(const net::CookieChangeInfo& change) override;

  bool have_cookies_changed_ = false;
  GURL url_;

  base::OnceCallback<void(const GURL&)> on_cookie_change_callback_;

  mojo::Receiver<network::mojom::CookieChangeListener>
      cookie_listener_receiver_{this};
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_COOKIE_LISTENER_H_
