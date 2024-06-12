// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_request_throttled_handler_browser_impl.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/common/renderer_configuration.mojom-shared.h"

BoundSessionRequestThrottledHandlerBrowserImpl::
    BoundSessionRequestThrottledHandlerBrowserImpl(
        BoundSessionCookieRefreshService& cookie_refresh_service)
    : cookie_refresh_service_(cookie_refresh_service.GetWeakPtr()) {}

BoundSessionRequestThrottledHandlerBrowserImpl::
    ~BoundSessionRequestThrottledHandlerBrowserImpl() = default;

void BoundSessionRequestThrottledHandlerBrowserImpl::
    HandleRequestBlockedOnCookie(
        const GURL& untrusted_request_url,
        ResumeOrCancelThrottledRequestCallback callback) {
  if (cookie_refresh_service_) {
    cookie_refresh_service_->HandleRequestBlockedOnCookie(
        untrusted_request_url,
        base::BindOnce(std::move(callback), UnblockAction::kResume));
  } else {
    // The service has been shutdown.
    // During shutdown, cancel the request to mitigate the risk of sending a
    // request missing the required cookie. Otherwise, the server might kill the
    // session upon receiving an unauthenticated request and the user might be
    // signed out on next startup.
    std::move(callback).Run(UnblockAction::kCancel,
                            chrome::mojom::ResumeBlockedRequestsTrigger::
                                kShutdownOrSessionTermination);
  }
}
