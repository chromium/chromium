// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REQUEST_THROTTLED_HANDLER_BROWSER_IMPL_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REQUEST_THROTTLED_HANDLER_BROWSER_IMPL_H_

#include "chrome/common/bound_session_request_throttled_handler.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"

class BoundSessionRequestThrottledHandlerBrowserImpl
    : public BoundSessionRequestThrottledHandler {
 public:
  explicit BoundSessionRequestThrottledHandlerBrowserImpl(
      BoundSessionCookieRefreshService& cookie_refresh_service);

  ~BoundSessionRequestThrottledHandlerBrowserImpl() override;

  BoundSessionRequestThrottledHandlerBrowserImpl(
      const BoundSessionRequestThrottledHandlerBrowserImpl&) = delete;
  BoundSessionRequestThrottledHandlerBrowserImpl& operator=(
      const BoundSessionRequestThrottledHandlerBrowserImpl&) = delete;

  void HandleRequestBlockedOnCookie(
      const GURL& untrusted_request_url,
      ResumeOrCancelThrottledRequestCallback callback) override;

 private:
  base::WeakPtr<BoundSessionCookieRefreshService> cookie_refresh_service_;
};
#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REQUEST_THROTTLED_HANDLER_BROWSER_IMPL_H_
