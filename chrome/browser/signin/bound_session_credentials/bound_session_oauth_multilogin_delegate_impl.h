// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_OAUTH_MULTILOGIN_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_OAUTH_MULTILOGIN_DELEGATE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "components/signin/public/base/bound_session_oauth_multilogin_delegate.h"

class BoundSessionCookieRefreshService;

class BoundSessionOAuthMultiLoginDelegateImpl
    : public signin::BoundSessionOAuthMultiLoginDelegate {
 public:
  explicit BoundSessionOAuthMultiLoginDelegateImpl(
      base::WeakPtr<BoundSessionCookieRefreshService>
          bound_session_cookie_refresh_service);

  ~BoundSessionOAuthMultiLoginDelegateImpl() override;

  // signin::BoundSessionOauthMultiLoginDelegate:
  void BeforeSetCookies(const OAuthMultiloginResult& result) override;
  void OnCookiesSet() override;

 private:
  base::WeakPtr<BoundSessionCookieRefreshService>
      bound_session_cookie_refresh_service_;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_OAUTH_MULTILOGIN_DELEGATE_IMPL_H_
