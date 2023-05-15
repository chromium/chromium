// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REQUEST_THROTTLED_LISTENER_BROWSER_IMPL_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REQUEST_THROTTLED_LISTENER_BROWSER_IMPL_H_

#include "chrome/common/bound_session_request_throttled_listener.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"

class BoundSessionRequestThrottledListenerBrowserImpl
    : public BoundSessionRequestThrottledListener {
 public:
  explicit BoundSessionRequestThrottledListenerBrowserImpl(
      BoundSessionCookieRefreshService& cookie_refresh_service);

  ~BoundSessionRequestThrottledListenerBrowserImpl() override;

  BoundSessionRequestThrottledListenerBrowserImpl(
      const BoundSessionRequestThrottledListenerBrowserImpl&) = delete;
  BoundSessionRequestThrottledListenerBrowserImpl& operator=(
      const BoundSessionRequestThrottledListenerBrowserImpl&) = delete;

  void OnRequestBlockedOnCookie(
      ResumeOrCancelThrottledRequestCallback callback) override;

 private:
  base::WeakPtr<BoundSessionCookieRefreshService> cookie_refresh_service_;
};
#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REQUEST_THROTTLED_LISTENER_BROWSER_IMPL_H_
