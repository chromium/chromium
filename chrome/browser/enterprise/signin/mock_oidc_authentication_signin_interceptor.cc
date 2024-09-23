// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/mock_oidc_authentication_signin_interceptor.h"

MockOidcAuthenticationSigninInterceptor::
    MockOidcAuthenticationSigninInterceptor(
        Profile* profile,
        std::unique_ptr<WebSigninInterceptor::Delegate> delegate)
    : OidcAuthenticationSigninInterceptor(profile, std::move(delegate)) {}

MockOidcAuthenticationSigninInterceptor::
    ~MockOidcAuthenticationSigninInterceptor() = default;
