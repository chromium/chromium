// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace bound_session_credentials {
namespace {
Credential CreateValidCookieCredential() {
  Credential credential;
  CookieCredential* cookie = credential.mutable_cookie_credential();
  cookie->set_name("auth_cookie");
  cookie->set_domain(".example.org");
  cookie->set_path("/");
  return credential;
}

BoundSessionParams CreateValidBoundSessionParams() {
  BoundSessionParams params;
  params.set_session_id("123");
  params.set_site("https://example.org");
  params.set_wrapped_key("456");
  *params.add_credentials() = CreateValidCookieCredential();
  return params;
}
}  // namespace

TEST(BoundSessionParamsUtilTest, Timestamp) {
  base::Time time =
      base::Time::UnixEpoch() + base::Milliseconds(987984);  // arbitrary
  EXPECT_EQ(TimestampToTime(TimeToTimestamp(time)), time);
}

TEST(BoundSessionParamsUtilTest, ParamsValid) {
  EXPECT_TRUE(AreParamsValid(CreateValidBoundSessionParams()));
}

TEST(BoundSessionParamsUtilTest, ParamsInvalidMissingSessionId) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  params.set_session_id("");
  EXPECT_FALSE(AreParamsValid(params));

  params.clear_session_id();
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, ParamsInvalidMissingWrappedKey) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  params.set_wrapped_key("");
  EXPECT_FALSE(AreParamsValid(params));

  params.clear_wrapped_key();
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, ParamsInvalidMissingSite) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  params.set_site("");
  EXPECT_FALSE(AreParamsValid(params));

  params.clear_site();
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, ParamsInvalidSite) {
  bound_session_credentials::BoundSessionParams params =
      CreateValidBoundSessionParams();
  params.set_site("http//google.com");
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, ParamsInvalidMissingCredentials) {
  bound_session_credentials::BoundSessionParams params =
      CreateValidBoundSessionParams();
  params.clear_credentials();
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, ParamsInvalidCookieCredentialInvalid) {
  bound_session_credentials::BoundSessionParams params =
      CreateValidBoundSessionParams();

  // Add a second invalid cookie credential.
  // If any of cookie credentials is invalid, `AreParamsValid` is expected to
  // return false.
  bound_session_credentials::Credential credential =
      CreateValidCookieCredential();
  // Domain isn't part of `params.site()`.
  credential.mutable_cookie_credential()->set_domain("goole.com");
  *params.add_credentials() = std::move(credential);
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, CookieCredentialInvalidEmptyCookieName) {
  bound_session_credentials::Credential credential =
      CreateValidCookieCredential();
  credential.mutable_cookie_credential()->set_name("");
  EXPECT_FALSE(
      IsCookieCredentialValid(credential, GURL("https://example.org")));

  credential.mutable_cookie_credential()->clear_name();
  EXPECT_FALSE(
      IsCookieCredentialValid(credential, GURL("https://example.org")));
}

TEST(BoundSessionParamsUtilTest, CookieCredentialInvalidCookieDomainInvalid) {
  bound_session_credentials::Credential credential =
      CreateValidCookieCredential();
  EXPECT_FALSE(IsCookieCredentialValid(credential, GURL("https://google.com")));
}

TEST(BoundSessionParamsUtilTest,
     CookieCredentialValidCookieDomainWithLeadingDot) {
  bound_session_credentials::Credential credential =
      CreateValidCookieCredential();
  EXPECT_TRUE(IsCookieCredentialValid(credential,
                                      GURL("https://accounts.example.org")));
}

TEST(BoundSessionParamsUtilTest,
     CookieCredentialValidCookieDomainWithoutLeadingDot) {
  bound_session_credentials::Credential credential =
      CreateValidCookieCredential();
  credential.mutable_cookie_credential()->set_domain("example.org");
  EXPECT_TRUE(IsCookieCredentialValid(credential,
                                      GURL("https://accounts.example.org")));
}

TEST(BoundSessionParamsUtilTest, CookieCredentialValidCookieDomainEmpty) {
  bound_session_credentials::Credential credential =
      CreateValidCookieCredential();
  credential.mutable_cookie_credential()->set_domain("");
  EXPECT_TRUE(IsCookieCredentialValid(credential,
                                      GURL("https://accounts.example.org")));
}
}  // namespace bound_session_credentials
