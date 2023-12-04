// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_DICE_BOUND_SESSION_COOKIE_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_DICE_BOUND_SESSION_COOKIE_SERVICE_H_

#include <memory>

#include "base/scoped_observation.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace signin {
class IdentityManager;
}

// Supports cookie binding for DICe profiles including:
// - On bound session termination, expire short lived bound cookies if needed
// and trigger /ListAccounts to ensure account consistency is maintained.
// - TODO(b/312719798): Support setting a new bound session from OAuthMultiLogin
// response.
// Note: This work can't be done directly in `BoundSessionCookieRefreshService`
// as it produces circular dependency.
class DiceBoundSessionCookieService
    : public KeyedService,
      public BoundSessionCookieRefreshService::Observer {
 public:
  DiceBoundSessionCookieService(
      BoundSessionCookieRefreshService& bound_session_cookie_refresh_service,
      signin::IdentityManager& identity_manager);

  ~DiceBoundSessionCookieService() override;

  DiceBoundSessionCookieService(const DiceBoundSessionCookieService&) = delete;
  DiceBoundSessionCookieService& operator=(
      const DiceBoundSessionCookieService&) = delete;

  // BoundSessionCookieRefreshService::Observer:
  void OnBoundSessionTerminated() override;

 private:
  const raw_ref<signin::IdentityManager> identity_manager_;
  base::ScopedObservation<BoundSessionCookieRefreshService,
                          BoundSessionCookieRefreshService::Observer>
      bound_session_cookie_refresh_service_observer_{this};
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_DICE_BOUND_SESSION_COOKIE_SERVICE_H_
