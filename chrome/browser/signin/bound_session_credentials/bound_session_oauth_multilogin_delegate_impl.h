// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_OAUTH_MULTILOGIN_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_OAUTH_MULTILOGIN_DELEGATE_IMPL_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/public/base/bound_session_oauth_multilogin_delegate.h"

class BoundSessionCookieRefreshService;

namespace bound_session_credentials {
class BoundSessionParams;
}  // namespace bound_session_credentials

namespace signin {
class IdentityManager;
}  // namespace signin

class BoundSessionOAuthMultiLoginDelegateImpl
    : public signin::BoundSessionOAuthMultiLoginDelegate {
 public:
  explicit BoundSessionOAuthMultiLoginDelegateImpl(
      base::WeakPtr<BoundSessionCookieRefreshService>
          bound_session_cookie_refresh_service,
      const signin::IdentityManager* identity_manager);

  ~BoundSessionOAuthMultiLoginDelegateImpl() override;

  // signin::BoundSessionOauthMultiLoginDelegate:
  void BeforeSetCookies(const OAuthMultiloginResult& result) override;
  void OnCookiesSet() override;

 private:
  std::vector<bound_session_credentials::BoundSessionParams>
  CreateBoundSessionsParams(const OAuthMultiloginResult& result);

  // The bound sessions params to be registered. `std::nullopt` indicates that
  // `BeforeSetCookies` was not called.
  std::optional<std::vector<bound_session_credentials::BoundSessionParams>>
      bound_sessions_params_;

  const base::WeakPtr<BoundSessionCookieRefreshService>
      bound_session_cookie_refresh_service_;

  const raw_ref<const signin::IdentityManager> identity_manager_;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_OAUTH_MULTILOGIN_DELEGATE_IMPL_H_
