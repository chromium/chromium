// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

// This service is responsible for the following:
// - Receive requests to refresh the SIDTS cookie
// - Request a signature from the [future] token binding service
// - Create a fetcher to do the network refresh request
// - Runs callbacks to resume blocked requests when the cookie is set in the
//   cookie Jar
// - Monitor cookie changes and update the renderers
// This class is still work in progress.
class BoundSessionCookieRefreshService : public KeyedService {
 public:
  BoundSessionCookieRefreshService();
  ~BoundSessionCookieRefreshService() override;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_
