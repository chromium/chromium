// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

using signin::IdentityManager;

// This service is responsible for the following:
// - Receives requests to refresh the SIDTS cookie
// - Requests a signature from the [future] token binding service
// - Creates a fetcher to do the network refresh request
// - Runs callbacks to resume blocked requests when the cookie is set in the
//   cookie Jar
// - Monitors cookie changes and update the renderers
// This class is still work in progress.
class BoundSessionCookieRefreshService : public KeyedService {
 public:
  explicit BoundSessionCookieRefreshService(IdentityManager* identity_manager);
  ~BoundSessionCookieRefreshService() override;

  // Returns true if session is bound.
  bool IsBoundSession() const;

 private:
  class BoundSessionStateTracker;

  void OnBoundSessionUpdated(bool is_bound_session);

  // TODO: Add implementation
  void UpdateAllRenderers() {}
  void ResumeBlockedRequestsIfAny() {}
  void CancelCookieRefreshIfAny() {}

  std::unique_ptr<BoundSessionStateTracker> bound_session_tracker_;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_
