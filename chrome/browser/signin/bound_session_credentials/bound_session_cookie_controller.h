// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_CONTROLLER_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "url/gurl.h"

// This class is responsible for tracking a single bound session cookie:
// - It observers cookie changes
// - Caches cookie expiry date
// - Initiates a cookie refresh at creation time
// - Receives requests to refresh cookie when a request requires it [on demand
//   cookie refresh]
// - Proactively schedule cookie refresh before it expires
// - To execute a the refresh:
//      (1) It requests an async signature from the [future] token binding
//      service.
//      (2) After receiving the signature, it creates a
//      'BoundSessionRefreshCookieFetcher' to do the network refresh request.
// - It is responsible on resuming blocked request for the managed domain on
// cookie updates, persistent refresh errors or timeout.
// - Monitors cookie changes and update the renderers
class BoundSessionCookieController {
 public:
  class Delegate {
   public:
    // Called when the cookie tracked in this controller has a change in its
    // expiration date. Cookie deletion is considered as a change in the
    // expiration date to the null time.
    virtual void OnCookieExpirationDateChanged() = 0;
  };

  BoundSessionCookieController(const GURL& url,
                               const std::string& cookie_name,
                               Delegate* delegate);

  virtual ~BoundSessionCookieController();

  virtual void Initialize();

  // Called when a network request requires a fresh SIDTS cookie.
  // The callback will be called once the cookie is fresh or the session is
  // terminated. Note: The callback might be called synchronously if the
  // previous conditions apply.
  virtual void OnRequestBlockedOnCookie(
      base::OnceClosure resume_blocked_request) = 0;

  const GURL& url() const { return url_; }
  const std::string& cookie_name() const { return cookie_name_; }
  base::Time cookie_expiration_time() { return cookie_expiration_time_; }

 protected:
  const GURL url_;
  const std::string cookie_name_;
  base::Time cookie_expiration_time_;
  raw_ptr<Delegate> delegate_;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_CONTROLLER_H_
