// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_

#include "base/functional/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"

// BoundSessionCookieRefreshService is responsible for maintaining cookies
// associated with bound sessions. This class does the following:
// - Tracks bound sessions
// - Provides bound session params to renderers
// - Monitors cookie changes and update renderers
// - Preemptively refreshes bound session cookies
class BoundSessionCookieRefreshService : public KeyedService {
 public:
  BoundSessionCookieRefreshService() = default;

  BoundSessionCookieRefreshService(const BoundSessionCookieRefreshService&) =
      delete;
  BoundSessionCookieRefreshService& operator=(
      const BoundSessionCookieRefreshService&) = delete;

  virtual void Initialize() = 0;

  // Returns true if session is bound.
  virtual bool IsBoundSession() const = 0;

  // Called when a network request requires a fresh SIDTS cookie. This function
  // is intended to be called by network requests throttlers.
  // The callback will be called once the cookie is fresh or the session is
  // terminated. Note: The callback might be called synchronously if the
  // previous conditions apply.
  virtual void OnRequestBlockedOnCookie(
      base::OnceClosure resume_blocked_request) = 0;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_
