// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"

#include "base/test/protobuf_matchers.h"
#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "google_apis/gaia/register_bound_session_payload.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace bound_session_credentials {

using ::testing::Optional;
using ::testing::UnorderedPointwise;

namespace {

BoundSessionParams CreateBoundSessionParamsWithNoCookies(
    const std::string& session_id,
    const std::string& site = "https://google.com/",
    const std::string& refresh_url = "https://google.com/rotate") {
  BoundSessionParams params;
  params.set_session_id(session_id);
  params.set_site(site);
  params.set_wrapped_key("456");
  params.set_refresh_url(refresh_url);
  return params;
}

Credential CreateCookieCredential(const std::string& name,
                                  const std::string& domain = ".google.com",
                                  const std::string& path = "/") {
  Credential credential;
  CookieCredential* cookie = credential.mutable_cookie_credential();
  cookie->set_name(name);
  cookie->set_domain(domain);
  cookie->set_path(path);
  return credential;
}

BoundSessionParams CreateValidBoundSessionParams() {
  BoundSessionParams params = CreateBoundSessionParamsWithNoCookies("123");
  *params.add_credentials() = CreateCookieCredential("auth_cookie");
  return params;
}

void UpdateAllCookieCredentialsDomains(BoundSessionParams& params,
                                       const std::string& domain) {
  for (Credential& credential : *params.mutable_credentials()) {
    credential.mutable_cookie_credential()->set_domain(domain);
  }
}

void UpdateAllDomains(BoundSessionParams& params, const std::string& domain) {
  for (Credential& credential : *params.mutable_credentials()) {
    credential.mutable_cookie_credential()->set_domain(domain);
  }

  GURL refresh_url(params.refresh_url());
  GURL::Replacements replacements;
  replacements.SetHostStr(domain);
  refresh_url = refresh_url.ReplaceComponents(replacements);
  params.set_refresh_url(refresh_url.spec());
}

RegisterBoundSessionPayload::Credential
CreateRegisterBoundSessionPayloadCredential(std::string_view name,
                                            std::string_view domain,
                                            std::string_view path) {
  RegisterBoundSessionPayload::Credential credential;
  credential.name = name;
  credential.scope = RegisterBoundSessionPayload::Scope{
      .domain = std::string(domain), .path = std::string(path)};
  return credential;
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

TEST(BoundSessionParamsUtilTest, ParamsValidYoutube) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  params.set_site("https://youtube.com/");
  UpdateAllDomains(params, "youtube.com");
  EXPECT_TRUE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, ParamsValidWithRefreshUrlSameSite) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  params.set_refresh_url("https://accounts.google.com/rotate");
  EXPECT_TRUE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, ParamsValidWithCookieDomainWithLeadingDot) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  UpdateAllCookieCredentialsDomains(params, ".google.com");
  EXPECT_TRUE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, ParamsValidWithCookieDomainWithoutLeadingDot) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  UpdateAllCookieCredentialsDomains(params, "google.com");
  EXPECT_TRUE(AreParamsValid(params));
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

TEST(BoundSessionParamsUtilTest, ParamsInvalidSiteInvalid) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  params.set_site("http//google.com");
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, ParamsInvalidSiteNotCanonical) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  params.set_site("https://google.com");
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, ParamsInvalidSiteNotGoogle) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  params.set_site("https://example.org");
  UpdateAllDomains(params, "example.org");
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, ParamsInvalidMissingRefreshUrl) {
  bound_session_credentials::BoundSessionParams params =
      CreateValidBoundSessionParams();
  params.set_refresh_url("");
  EXPECT_FALSE(AreParamsValid(params));

  params.clear_refresh_url();
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, ParamsInvalidRefreshUrlInvalid) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  params.set_refresh_url("http//not-a-url.com");
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, ParamsInvalidRefreshUrlSiteDoesNotMatch) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  params.set_refresh_url("https://example.com/rotate");
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, ParamsInvalidMissingCredentials) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  params.clear_credentials();
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, ParamsInvalidCookieCredentialEmptyCookieName) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  params.mutable_credentials(0)->mutable_cookie_credential()->set_name("");
  EXPECT_FALSE(AreParamsValid(params));

  params.mutable_credentials(0)->mutable_cookie_credential()->clear_name();
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, ParamsInvalidCookieCredentialEmptyDomain) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  UpdateAllCookieCredentialsDomains(params, "");
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, ParamsInvalidCookieCredentialInvalidDomain) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  // Domain isn't part of `params.site()`.
  UpdateAllCookieCredentialsDomains(params, "goole.com");
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, GetBoundSessionKey) {
  const GURL kUrl("https://google.com");
  const std::string kSessionId = "123456";
  BoundSessionParams params = CreateValidBoundSessionParams();
  params.set_site(kUrl.spec());
  params.set_session_id(kSessionId);
  BoundSessionKey key = GetBoundSessionKey(params);
  EXPECT_EQ(key.site, kUrl);
  EXPECT_EQ(key.session_id, kSessionId);
}

TEST(BoundSessionParamsUtilTest, GetBoundSessionScopeValidMixedLeadingDot) {
  BoundSessionParams params = CreateBoundSessionParamsWithNoCookies(
      "test_session", "https://google.com/");
  *params.add_credentials() = CreateCookieCredential("cookie1", ".google.com");
  *params.add_credentials() = CreateCookieCredential("cookie2", "google.com");
  EXPECT_EQ(GetBoundSessionScope(params), GURL("https://google.com"));
  EXPECT_TRUE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, GetBoundSessionScopeValidSubdomain) {
  BoundSessionParams params = CreateBoundSessionParamsWithNoCookies(
      "test_session", "https://google.com/");
  *params.add_credentials() =
      CreateCookieCredential("cookie1", ".accounts.google.com");
  *params.add_credentials() =
      CreateCookieCredential("cookie2", ".accounts.google.com");
  EXPECT_EQ(GetBoundSessionScope(params), GURL("https://accounts.google.com"));
  EXPECT_TRUE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, GetBoundSessionScopeValidWithPath) {
  BoundSessionParams params = CreateBoundSessionParamsWithNoCookies(
      "test_session", "https://google.com/");
  *params.add_credentials() =
      CreateCookieCredential("cookie1", ".accounts.google.com", "/secure");
  *params.add_credentials() =
      CreateCookieCredential("cookie2", ".accounts.google.com", "/secure");
  EXPECT_EQ(GetBoundSessionScope(params),
            GURL("https://accounts.google.com/secure"));
  EXPECT_TRUE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, GetBoundSessionScopeInvalidWrongSite) {
  BoundSessionParams params = CreateBoundSessionParamsWithNoCookies(
      "test_session", "https://google.com/");
  *params.add_credentials() = CreateCookieCredential("cookie", ".youtube.com");
  EXPECT_EQ(GetBoundSessionScope(params), GURL());
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, GetBoundSessionScopeInvalidDifferentDomains) {
  BoundSessionParams params = CreateBoundSessionParamsWithNoCookies(
      "test_session", "https://google.com/");
  *params.add_credentials() = CreateCookieCredential("cookie1", ".google.com");
  *params.add_credentials() =
      CreateCookieCredential("cookie2", ".accounts.google.com");
  EXPECT_EQ(GetBoundSessionScope(params), GURL());
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest,
     GetBoundSessionScopeInvalidCookieDomainAllEmpty) {
  BoundSessionParams params = CreateBoundSessionParamsWithNoCookies(
      "test_session", "https://google.com/");
  *params.add_credentials() = CreateCookieCredential("cookie", "");
  EXPECT_EQ(GetBoundSessionScope(params), GURL());
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest,
     GetBoundSessionScopeValidCookieDomainsSomeEmpty) {
  BoundSessionParams params = CreateBoundSessionParamsWithNoCookies(
      "test_session", "https://google.com/");
  *params.add_credentials() =
      CreateCookieCredential("cookie1", ".accounts.google.com");
  *params.add_credentials() = CreateCookieCredential("cookie2", "");
  EXPECT_EQ(GetBoundSessionScope(params), GURL());
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, GetBoundSessionScopeInvalidDifferentPaths) {
  BoundSessionParams params = CreateBoundSessionParamsWithNoCookies(
      "test_session", "https://google.com/");
  *params.add_credentials() =
      CreateCookieCredential("cookie1", ".accounts.google.com", "/p1");
  *params.add_credentials() =
      CreateCookieCredential("cookie2", ".accounts.google.com", "/p2");
  EXPECT_EQ(GetBoundSessionScope(params), GURL());
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest,
     GetBoundSessionScopeInvalidDifferentPathsSomeEmpty) {
  BoundSessionParams params = CreateBoundSessionParamsWithNoCookies(
      "test_session", "https://google.com/");
  *params.add_credentials() =
      CreateCookieCredential("cookie1", ".accounts.google.com", "/p1");
  *params.add_credentials() =
      CreateCookieCredential("cookie2", ".accounts.google.com");
  EXPECT_EQ(GetBoundSessionScope(params), GURL());
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, GetBoundSessionScopeInvalidDomainInvalid) {
  BoundSessionParams params = CreateBoundSessionParamsWithNoCookies(
      "test_session", "https://google.com/");
  *params.add_credentials() =
      CreateCookieCredential("cookie", "google.com/path");
  EXPECT_EQ(GetBoundSessionScope(params), GURL());
  EXPECT_FALSE(AreParamsValid(params));
}

TEST(BoundSessionParamsUtilTest, AreSameSessionParamsIdentical) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  EXPECT_TRUE(AreSameSessionParams(params, params));
}

TEST(BoundSessionParamsUtilTest, AreSameSessionParamsSameSiteAndSessionId) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  BoundSessionParams params2 = CreateValidBoundSessionParams();
  params2.set_wrapped_key("abcdef2");
  params2.mutable_credentials(0)->mutable_cookie_credential()->set_name(
      "cookie2");
  EXPECT_TRUE(AreSameSessionParams(params, params2));
}

TEST(BoundSessionParamsUtilTest, AreSameSessionParamsDifferentSite) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  BoundSessionParams params2 = CreateValidBoundSessionParams();
  params2.set_site("https://youtube.com/");
  UpdateAllDomains(params2, "youtube.com");
  EXPECT_FALSE(AreSameSessionParams(params, params2));
}

TEST(BoundSessionParamsUtilTest, AreSameSessionParamsDifferentSessionId) {
  BoundSessionParams params = CreateValidBoundSessionParams();
  BoundSessionParams params2 = CreateValidBoundSessionParams();
  params2.set_session_id("session_id2");
  EXPECT_FALSE(AreSameSessionParams(params, params2));
}

TEST(BoundSessionParamsUtilTest, ResolveEndpointPathRelative) {
  GURL resolved_url =
      ResolveEndpointPath(GURL("https://google.com/path1"), "/path2");
  EXPECT_EQ(resolved_url, GURL("https://google.com/path2"));
}

// This is an edge-case that is not banned by the standard (at least yet).
TEST(BoundSessionParamsUtilTest, ResolveEndpointPathRelativeEmpty) {
  GURL resolved_url = ResolveEndpointPath(GURL("https://google.com/path1"), "");
  EXPECT_EQ(resolved_url, GURL("https://google.com/path1"));
}

TEST(BoundSessionParamsUtilTest, ResolveEndpointPathRelativeEmptyPath) {
  GURL resolved_url =
      ResolveEndpointPath(GURL("https://google.com/path1"), "/");
  EXPECT_EQ(resolved_url, GURL("https://google.com/"));
}

TEST(BoundSessionParamsUtilTest, ResolveEndpointPathRelativeEmptyRequestPath) {
  GURL resolved_url = ResolveEndpointPath(GURL("https://google.com"), "/path1");
  EXPECT_EQ(resolved_url, GURL("https://google.com/path1"));
}

TEST(BoundSessionParamsUtilTest, ResolveEndpointPathAbsoluteSameDomain) {
  GURL resolved_url =
      ResolveEndpointPath(GURL("https://accounts.google.com/path1"),
                          "https://accounts.google.com/path2");
  EXPECT_EQ(resolved_url, GURL("https://accounts.google.com/path2"));
}

TEST(BoundSessionParamsUtilTest, ResolveEndpointPathAbsoluteSameSite) {
  GURL resolved_url = ResolveEndpointPath(GURL("https://mail.google.com/path1"),
                                          "https://accounts.google.com/path2");
  EXPECT_EQ(resolved_url, GURL("https://accounts.google.com/path2"));
}

TEST(BoundSessionParamsUtilTest, ResolveEndpointPathAbsoluteOtherSite) {
  GURL resolved_url = ResolveEndpointPath(GURL("https://mail.google.com/path1"),
                                          "https://accounts.other.com/path2");
  EXPECT_FALSE(resolved_url.is_valid());
}

TEST(BoundSessionParamsUtilTest, ResolveEndpointPathAbsoluteOtherScheme) {
  GURL resolved_url =
      ResolveEndpointPath(GURL("https://accounts.google.com/path1"),
                          "http://accounts.google.com/path2");
  EXPECT_FALSE(resolved_url.is_valid());
}

TEST(BoundSessionParamsUtilTest, ResolveEndpointPathInvalidRequestUrl) {
  GURL resolved_url = ResolveEndpointPath(GURL(), "https://google.com/path1");
  EXPECT_FALSE(resolved_url.is_valid());
}

class CreateBoundSessionsParamsFromRegistrationPayloadTest
    : public testing::TestWithParam<SessionOrigin> {};

TEST_P(CreateBoundSessionsParamsFromRegistrationPayloadTest, Valid) {
  const SessionOrigin session_origin = GetParam();
  RegisterBoundSessionPayload payload;
  payload.session_id = "test_session_id";
  payload.refresh_url = "/rotate";
  payload.credentials = {CreateRegisterBoundSessionPayloadCredential(
                             /*name=*/"test_cookie_name_1",
                             /*domain=*/".google.com", /*path=*/"/"),
                         CreateRegisterBoundSessionPayloadCredential(
                             /*name=*/"test_cookie_name_2",
                             /*domain=*/".google.com", /*path=*/"/")};
  const BoundSessionParams params =
      CreateBoundSessionsParamsFromRegistrationPayload(
          payload, /*request_url=*/GURL("https://example.google.com/request"),
          /*site=*/GURL("https://google.com/"), /*wrapped_key=*/"secret",
          session_origin);

  ASSERT_TRUE(AreParamsValid(params));
  EXPECT_EQ(params.session_id(), "test_session_id");
  EXPECT_EQ(params.refresh_url(), "https://example.google.com/rotate");
  EXPECT_EQ(params.wrapped_key(), "secret");
  EXPECT_EQ(params.site(), "https://google.com/");
  EXPECT_EQ(params.session_origin(), session_origin);
  const std::vector<bound_session_credentials::Credential>
      expected_credentials = {
          CreateCookieCredential("test_cookie_name_1", ".google.com", "/"),
          CreateCookieCredential("test_cookie_name_2", ".google.com", "/")};
  EXPECT_THAT(
      params.credentials(),
      UnorderedPointwise(base::test::EqualsProto(), expected_credentials));
}

TEST_P(CreateBoundSessionsParamsFromRegistrationPayloadTest, InvalidSite) {
  RegisterBoundSessionPayload payload;
  payload.session_id = "test_session_id";
  payload.refresh_url = "/rotate";
  payload.credentials = {CreateRegisterBoundSessionPayloadCredential(
                             /*name=*/"test_cookie_name_1",
                             /*domain=*/".google.com", /*path=*/"/"),
                         CreateRegisterBoundSessionPayloadCredential(
                             /*name=*/"test_cookie_name_2",
                             /*domain=*/".google.com", /*path=*/"/")};

  const BoundSessionParams params =
      CreateBoundSessionsParamsFromRegistrationPayload(
          payload, /*request_url=*/GURL("https://example.google.com/request"),
          /*site=*/GURL(), /*wrapped_key=*/"secret", GetParam());

  EXPECT_FALSE(AreParamsValid(params));
}

TEST_P(CreateBoundSessionsParamsFromRegistrationPayloadTest,
       InvalidRequestUrl) {
  RegisterBoundSessionPayload payload;
  payload.session_id = "test_session_id";
  payload.refresh_url = "/rotate";
  payload.credentials = {CreateRegisterBoundSessionPayloadCredential(
                             /*name=*/"test_cookie_name_1",
                             /*domain=*/".google.com", /*path=*/"/"),
                         CreateRegisterBoundSessionPayloadCredential(
                             /*name=*/"test_cookie_name_2",
                             /*domain=*/".google.com", /*path=*/"/")};

  const BoundSessionParams params =
      CreateBoundSessionsParamsFromRegistrationPayload(
          payload, /*request_url=*/GURL(),
          /*site=*/GURL("https://google.com/"), /*wrapped_key=*/"secret",
          GetParam());

  EXPECT_FALSE(AreParamsValid(params));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    CreateBoundSessionsParamsFromRegistrationPayloadTest,
    testing::Values(SessionOrigin::SESSION_ORIGIN_REGISTRATION,
                    SessionOrigin::SESSION_ORIGIN_OAML));

TEST(GetSessionOriginHistogramSuffixTest, Registration) {
  EXPECT_THAT(GetSessionOriginHistogramSuffix(
                  SessionOrigin::SESSION_ORIGIN_REGISTRATION),
              testing::Optional(std::string_view(".FromRegistration")));
}

TEST(GetSessionOriginHistogramSuffixTest, OAuthMultiLogin) {
  EXPECT_THAT(
      GetSessionOriginHistogramSuffix(SessionOrigin::SESSION_ORIGIN_OAML),
      testing::Optional(std::string_view(".FromOAuthMultiLogin")));
}

TEST(GetSessionOriginHistogramSuffixTest, Unspecified) {
  EXPECT_EQ(GetSessionOriginHistogramSuffix(
                SessionOrigin::SESSION_ORIGIN_UNSPECIFIED),
            std::nullopt);
}

}  // namespace bound_session_credentials
