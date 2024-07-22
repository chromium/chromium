// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_MOCK_OIDC_AUTHENTICATION_SIGNIN_INTERCEPTOR_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_MOCK_OIDC_AUTHENTICATION_SIGNIN_INTERCEPTOR_H_

#include "chrome/browser/enterprise/signin/oidc_authentication_signin_interceptor.h"
#include "testing/gmock/include/gmock/gmock.h"

// Mock version of OIDC interceptor for testing.
class MockOidcAuthenticationSigninInterceptor
    : public OidcAuthenticationSigninInterceptor {
 public:
  MockOidcAuthenticationSigninInterceptor(
      Profile* profile,
      std::unique_ptr<WebSigninInterceptor::Delegate> delegate);
  ~MockOidcAuthenticationSigninInterceptor() override;

  MockOidcAuthenticationSigninInterceptor(
      const MockOidcAuthenticationSigninInterceptor&) = delete;
  MockOidcAuthenticationSigninInterceptor& operator=(
      const MockOidcAuthenticationSigninInterceptor&) = delete;

  MOCK_METHOD(void,
              MaybeInterceptOidcAuthentication,
              (content::WebContents * intercepted_contents,
               const ProfileManagementOidcTokens& oidc_tokens,
               const std::string& issuer_id,
               const std::string& subject_id,
               OidcInterceptionCallback oidc_callback),
              (override));

  MOCK_METHOD(void, CreateBrowserAfterSigninInterception, (), (override));
};

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_MOCK_OIDC_AUTHENTICATION_SIGNIN_INTERCEPTOR_H_
