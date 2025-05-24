// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_CONTROLLER_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_CONTROLLER_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_key.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/rotation_debug_info.pb.h"
#include "chrome/common/renderer_configuration.mojom.h"
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
    // Called when the cookie refresh request results in a persistent error that
    // can't be fixed by retrying. `BoundSessionCookieController` is expected to
    // be deleted after this call.
    // `controller` points at the caller object.
    // `BoundSessionRefreshCookieFetcher::IsPersistentError(refresh_error)` is
    // guaranteed to be true.
    virtual void OnPersistentErrorEncountered(
        BoundSessionCookieController* controller,
        BoundSessionRefreshCookieFetcher::Result refresh_error) = 0;

    // Called when the bound session parameters change, for example the minimum
    // cookie expiration date changes. Cookie deletion is considered as a change
    // in the expiration date to the null time.
    virtual void OnBoundSessionThrottlerParamsChanged() = 0;
  };

  BoundSessionCookieController(
      const bound_session_credentials::BoundSessionParams& bound_session_params,
      Delegate* delegate);

  virtual ~BoundSessionCookieController();

  virtual void Initialize();

  // Called when a network request requires a fresh SIDTS cookie.
  // The callback will be called once the cookie is fresh or the session is
  // terminated. Note: The callback might be called synchronously if the
  // previous conditions apply.
  virtual void HandleRequestBlockedOnCookie(
      chrome::mojom::BoundSessionRequestThrottledHandler::
          HandleRequestBlockedOnCookieCallback resume_blocked_request) = 0;

  // URL determining the scope of the bound session. All requests that are
  // within the scope are subject to throttling.
  const GURL& scope_url() const { return scope_url_; }
  // Unique identifier of the session (within the same site).
  const std::string& session_id() const { return session_id_; }
  // Time at which the session was initially registered.
  base::Time session_creation_time() const { return session_creation_time_; }
  // URL for refreshing bound cookies.
  const GURL& refresh_url() const { return refresh_url_; }
  // eTLD+1 of the session serving as a hard session boundary. Session IDs must
  // be unique within a single site.
  const GURL& site() const { return site_; }
  // The earliest time at which one of the bound cookies expires.
  base::Time min_cookie_expiration_time() const;
  chrome::mojom::BoundSessionThrottlerParamsPtr bound_session_throttler_params()
      const;
  base::flat_set<std::string> bound_cookie_names() const;

  // Key that uniquely identifies the session across all sites.
  BoundSessionKey GetBoundSessionKey() const;

  // Extracts debug info information from the controller.
  virtual bound_session_credentials::RotationDebugInfo TakeDebugInfo() = 0;

  // Returns true in case of successive 5xx responses on the cookie rotation
  // endpoint which indicates the server might be experiencing an outage.
  // Throttling requests in this case might make the web (within the scope of
  // the bound session) unusable.
  virtual bool ShouldPauseThrottlingRequests() const = 0;

 protected:
  const GURL scope_url_;
  const std::string session_id_;
  const base::Time session_creation_time_;
  const GURL refresh_url_;
  const GURL site_;
  // Map from cookie name to cookie expiration time, it is expected to have two
  // elements the 1P and 3P cookies.
  // Cookie expiration time is reduced by threshold to guarantee cookie will be
  // fresh when cookies are added to the request, as URL Loader throttle(s)
  // attached to the request may decide to defer it.
  base::flat_map<std::string, base::Time> bound_cookies_info_;
  raw_ptr<Delegate> delegate_;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_CONTROLLER_H_
