// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_cookie_listener.h"

#include "base/check.h"
#include "base/functional/callback.h"

// static
std::unique_ptr<PrefetchProxyCookieListener>
PrefetchProxyCookieListener::MakeAndRegister(
    const GURL& url,
    base::OnceCallback<void(const GURL&)> on_cookie_change_callback,
    network::mojom::CookieManager* cookie_manager) {
  DCHECK(cookie_manager);

  std::unique_ptr<PrefetchProxyCookieListener> listener =
      std::make_unique<PrefetchProxyCookieListener>(
          url, std::move(on_cookie_change_callback));

  // |listener| will get updates whenever host cookies for |url| or
  // domain cookies that match |url| are changed.
  cookie_manager->AddCookieChangeListener(
      url, absl::nullopt,
      listener->cookie_listener_receiver_.BindNewPipeAndPassRemote());

  return listener;
}

PrefetchProxyCookieListener::PrefetchProxyCookieListener(
    const GURL& url,
    base::OnceCallback<void(const GURL&)> on_cookie_change_callback)
    : url_(url),
      on_cookie_change_callback_(std::move(on_cookie_change_callback)) {}

PrefetchProxyCookieListener::~PrefetchProxyCookieListener() = default;

void PrefetchProxyCookieListener::StopListening() {
  cookie_listener_receiver_.reset();
}

void PrefetchProxyCookieListener::OnCookieChange(
    const net::CookieChangeInfo& change) {
  DCHECK(url_.DomainIs(change.cookie.DomainWithoutDot()));
  have_cookies_changed_ = true;

  if (on_cookie_change_callback_)
    std::move(on_cookie_change_callback_).Run(url_);

  // Once we record one change to the cookies associated with |url_|, we don't
  // care about any subsequent changes.
  StopListening();
}
