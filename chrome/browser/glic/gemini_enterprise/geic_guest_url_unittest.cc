// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_guest_url.h"

#include <string_view>

#include "base/check.h"
#include "base/types/expected.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace geic {
namespace {

GURL SkillsUrlForSpec(std::string_view spec) {
  base::expected<GeicGuestUrl, GuestUrlError> guest_url =
      GeicGuestUrl::FromSwitch(spec, /*allow_any_http_url=*/true);
  CHECK(guest_url.has_value()) << spec;
  return guest_url->SkillsUrl();
}

TEST(GeicGuestUrlTest, ConfigIdQuery) {
  // Extracts `configId` into `/home/cid/<configId>/r/skills`, strips
  // `configId`, `location`, `hl`, and `idp`, and preserves other query
  // parameters.
  EXPECT_EQ(
      SkillsUrlForSpec(
          "https://business.gemini.google/side-panel?"
          "configId=abc123&location=global&hl=en&mods=widget_staging_api_mod"),
      GURL("https://business.gemini.google/home/cid/abc123/r/skills?"
           "mods=widget_staging_api_mod"));
  EXPECT_EQ(
      SkillsUrlForSpec("https://vertexaisearch.cloud.google/side-panel?"
                       "configId=abc123&idp=locations%2Fglobal%2FworkforcePools"
                       "%2Fmy-pool-1%2Fproviders%2Fmy-idp"),
      GURL("https://vertexaisearch.cloud.google/home/cid/abc123/r/skills"));
}

TEST(GeicGuestUrlTest, CidInPath) {
  // Gemini at Work URLs carry the cid in the path: keep everything up to
  // `/cid/<configId>` and append `/customize/skills`.
  EXPECT_EQ(
      SkillsUrlForSpec("https://gemini.google.com/work/cid/abc123/side-panel"),
      GURL("https://gemini.google.com/work/cid/abc123/customize/skills"));
  EXPECT_EQ(
      SkillsUrlForSpec("https://gemini.google.com/work/u/1/cid/abc123"),
      GURL("https://gemini.google.com/work/u/1/cid/abc123/customize/skills"));
  EXPECT_EQ(SkillsUrlForSpec(
                "https://gemini.cloud.google/cid/abc123/side-panel?hl=en"),
            GURL("https://gemini.cloud.google/cid/abc123/customize/skills"));
  EXPECT_EQ(SkillsUrlForSpec(
                "https://gemini.cloud.google/work/us/cid/abc123/side-panel?"
                "mods=x"),
            GURL("https://gemini.cloud.google/work/us/cid/abc123/customize/"
                 "skills?mods=x"));
}

TEST(GeicGuestUrlTest, MountRootWithoutCid) {
  // Gemini at Work paths without a cid resolve to the mount root (the server
  // redirects it to the user's instance).
  EXPECT_EQ(SkillsUrlForSpec("https://gemini.google.com/work"),
            GURL("https://gemini.google.com/work"));
  EXPECT_EQ(SkillsUrlForSpec("https://gemini.google.com/work/side-panel"),
            GURL("https://gemini.google.com/work"));
  EXPECT_EQ(SkillsUrlForSpec("https://gemini.google.com/work/u/2/side-panel"),
            GURL("https://gemini.google.com/work/u/2"));
  EXPECT_EQ(SkillsUrlForSpec("https://gemini.cloud.google/side-panel"),
            GURL("https://gemini.cloud.google/"));
  EXPECT_EQ(SkillsUrlForSpec("https://gemini.cloud.google/work/side-panel"),
            GURL("https://gemini.cloud.google/work"));
  EXPECT_EQ(SkillsUrlForSpec("https://gemini.cloud.google/u/1/side-panel"),
            GURL("https://gemini.cloud.google/u/1"));
}

TEST(GeicGuestUrlTest, OtherUrls) {
  // Other URLs without `configId` fall back to `/r/skills`.
  EXPECT_EQ(SkillsUrlForSpec(
                "https://business.gemini.google/side-panel?location=us&hl=fr"),
            GURL("https://business.gemini.google/r/skills"));
}

TEST(GeicGuestUrlTest, EmptySpecReturnsUnset) {
  EXPECT_EQ(GeicGuestUrl::FromPolicy(""),
            base::unexpected(GuestUrlError::kUnset));
  EXPECT_EQ(GeicGuestUrl::FromFinch(""),
            base::unexpected(GuestUrlError::kUnset));
  EXPECT_EQ(GeicGuestUrl::FromSwitch("", /*allow_any_http_url=*/false),
            base::unexpected(GuestUrlError::kUnset));
  EXPECT_EQ(GeicGuestUrl::FromSwitch("", /*allow_any_http_url=*/true),
            base::unexpected(GuestUrlError::kUnset));
}

TEST(GeicGuestUrlTest, PolicyAcceptsOnlyProductionHosts) {
  base::expected<GeicGuestUrl, GuestUrlError> prod =
      GeicGuestUrl::FromPolicy("https://business.gemini.google/side-panel");
  ASSERT_TRUE(prod.has_value());
  EXPECT_EQ(prod->url(), GURL("https://business.gemini.google/side-panel"));
  EXPECT_EQ(prod->source(), GuestUrlSource::kPolicy);

  const char* const kAllowed[] = {
      "https://business.gemini.google/",
      "https://enterprise.gemini.google.com/",
      "https://enterprise.gemini.cloud.google/",
      "https://enterprise.gemini.cloud.google.com/",
      "https://gemini.cloud.google/",
      "https://vertexaisearch.cloud.google/",
      "https://vertexaisearch.cloud.google.com/",
      "https://vertexaisearch.gemini-us.google/",
      "https://ucs-widget.corp.google.com/side-panel",
  };
  for (const char* spec : kAllowed) {
    base::expected<GeicGuestUrl, GuestUrlError> result =
        GeicGuestUrl::FromPolicy(spec);
    ASSERT_TRUE(result.has_value()) << spec;
    EXPECT_EQ(result->url(), GURL(spec));
  }

  const char* const kDisallowed[] = {
      "not a url",
      "https://evil.com/side-panel",
      "https://ucs-widget-staging.corp.google.com/",
      "https://ucs-app-autopush.corp.google.com/side-panel",
      "https://business-staging.gemini.google/side-panel",
      "https://gemini.google.com/",
      "https://cloud.google.com.evil.com/",
      "https://vertexaisearch.cloud.google.evil.com/",
      "https://notcloud.google/",
      "https://notgemini.google.com/",
      "https://console.cloud.google.com/home",
      "https://storage.cloud.google/",
      "https://foo.corp.google.com/",
      "https://custom-host.corp.google.com/side-panel",
      "https://sub.business.gemini.google/",
      "https://sub.gemini.google.com/",
      "https://gemini.google/",
      "http://localhost:8080/side-panel",
      "https://localhost:8443/side-panel",
      "https://localhost.corp.google.com:10443/side-panel",
  };
  for (const char* spec : kDisallowed) {
    EXPECT_EQ(GeicGuestUrl::FromPolicy(spec),
              base::unexpected(GuestUrlError::kInvalid))
        << spec;
  }
}

TEST(GeicGuestUrlTest, FinchAcceptsPreProductionHosts) {
  base::expected<GeicGuestUrl, GuestUrlError> preprod =
      GeicGuestUrl::FromFinch("https://ucs-widget-staging.corp.google.com/");
  ASSERT_TRUE(preprod.has_value());
  EXPECT_EQ(preprod->url(),
            GURL("https://ucs-widget-staging.corp.google.com/"));
  EXPECT_EQ(preprod->source(), GuestUrlSource::kFinch);

  base::expected<GeicGuestUrl, GuestUrlError> autopush =
      GeicGuestUrl::FromFinch(
          "https://ucs-app-autopush.corp.google.com/side-panel");
  ASSERT_TRUE(autopush.has_value());
  EXPECT_EQ(autopush->url(),
            GURL("https://ucs-app-autopush.corp.google.com/side-panel"));

  base::expected<GeicGuestUrl, GuestUrlError> prod =
      GeicGuestUrl::FromFinch("https://business.gemini.google/side-panel");
  ASSERT_TRUE(prod.has_value());
  EXPECT_EQ(prod->source(), GuestUrlSource::kFinch);

  const char* const kDisallowed[] = {
      "https://evil.com/side-panel",
      "https://foo.corp.google.com/side-panel",
      "https://custom-host.corp.google.com/side-panel",
      "https://a.business-staging.gemini.google/side-panel",
      "http://business-staging.gemini.google/side-panel",
      "https://user:pass@business-staging.gemini.google/side-panel",
      "https://localhost.corp.google.com:10443/side-panel",
  };
  for (const char* spec : kDisallowed) {
    EXPECT_EQ(GeicGuestUrl::FromFinch(spec),
              base::unexpected(GuestUrlError::kInvalid))
        << spec;
  }
}

TEST(GeicGuestUrlTest, RejectsEmbeddedCredentialsAndInsecureSchemes) {
  const char* const kWithCreds[] = {
      "https://user:pw@business.gemini.google/",
      "https://user@business.gemini.google/side-panel",
      "https://:pass@business.gemini.google/side-panel",
  };
  for (const char* spec : kWithCreds) {
    EXPECT_EQ(GeicGuestUrl::FromPolicy(spec),
              base::unexpected(GuestUrlError::kInvalid))
        << spec;
    EXPECT_EQ(GeicGuestUrl::FromFinch(spec),
              base::unexpected(GuestUrlError::kInvalid))
        << spec;
    EXPECT_EQ(GeicGuestUrl::FromSwitch(spec, /*allow_any_http_url=*/false),
              base::unexpected(GuestUrlError::kInvalid))
        << spec;
    EXPECT_EQ(GeicGuestUrl::FromSwitch(spec, /*allow_any_http_url=*/true),
              base::unexpected(GuestUrlError::kInvalid))
        << spec;
  }

  constexpr char kHttp[] = "http://business.gemini.google/side-panel";
  EXPECT_EQ(GeicGuestUrl::FromPolicy(kHttp),
            base::unexpected(GuestUrlError::kInvalid));
  EXPECT_EQ(GeicGuestUrl::FromFinch(kHttp),
            base::unexpected(GuestUrlError::kInvalid));
  EXPECT_EQ(GeicGuestUrl::FromSwitch(kHttp, /*allow_any_http_url=*/false),
            base::unexpected(GuestUrlError::kInvalid));
}

TEST(GeicGuestUrlTest, SwitchAllowAnyHttpUrl) {
  const char* const kNonAllowlisted[] = {
      "http://localhost:8001/",
      "http://127.0.0.1:8080/side-panel",
      "https://localhost:8443/side-panel",
      "https://localhost.corp.google.com:10443/side-panel",
      "https://custom-dev-host.internal:8443/side-panel",
  };
  for (const char* spec : kNonAllowlisted) {
    EXPECT_EQ(GeicGuestUrl::FromSwitch(spec, /*allow_any_http_url=*/false),
              base::unexpected(GuestUrlError::kInvalid))
        << spec;
  }

  base::expected<GeicGuestUrl, GuestUrlError> local =
      GeicGuestUrl::FromSwitch("http://localhost:8001/",
                               /*allow_any_http_url=*/true);
  ASSERT_TRUE(local.has_value());
  EXPECT_EQ(local->url(), GURL("http://localhost:8001/"));
  EXPECT_EQ(local->source(), GuestUrlSource::kSwitch);

  base::expected<GeicGuestUrl, GuestUrlError> prod =
      GeicGuestUrl::FromSwitch("https://business.gemini.google/side-panel",
                               /*allow_any_http_url=*/false);
  ASSERT_TRUE(prod.has_value());
  EXPECT_EQ(prod->url(), GURL("https://business.gemini.google/side-panel"));
  EXPECT_EQ(prod->source(), GuestUrlSource::kSwitch);

  base::expected<GeicGuestUrl, GuestUrlError> preprod =
      GeicGuestUrl::FromSwitch("https://ucs-widget-staging.corp.google.com/",
                               /*allow_any_http_url=*/false);
  ASSERT_TRUE(preprod.has_value());
  EXPECT_EQ(preprod->source(), GuestUrlSource::kSwitch);

  base::expected<GeicGuestUrl, GuestUrlError> staging =
      GeicGuestUrl::FromSwitch(
          "https://business-staging.gemini.google/side-panel",
          /*allow_any_http_url=*/false);
  ASSERT_TRUE(staging.has_value());
  EXPECT_EQ(staging->url(),
            GURL("https://business-staging.gemini.google/side-panel"));
  EXPECT_EQ(staging->source(), GuestUrlSource::kSwitch);

  EXPECT_EQ(GeicGuestUrl::FromSwitch("not a url", /*allow_any_http_url=*/true),
            base::unexpected(GuestUrlError::kInvalid));
  EXPECT_EQ(GeicGuestUrl::FromSwitch("ftp://localhost:8001/",
                                     /*allow_any_http_url=*/true),
            base::unexpected(GuestUrlError::kInvalid));
  EXPECT_EQ(GeicGuestUrl::FromSwitch("http://user:pw@localhost:8001/",
                                     /*allow_any_http_url=*/true),
            base::unexpected(GuestUrlError::kInvalid));
}

TEST(GeicGuestUrlTest, CanonicalizesHomeCidAndDerivesSkillsUrl) {
  base::expected<GeicGuestUrl, GuestUrlError> url =
      GeicGuestUrl::FromPolicy("https://business.gemini.google/home/cid/123");
  ASSERT_TRUE(url.has_value());
  EXPECT_EQ(url->url(),
            GURL("https://business.gemini.google/side-panel?configId=123"));
  EXPECT_EQ(url->SkillsUrl(),
            GURL("https://business.gemini.google/home/cid/123/r/skills"));

  base::expected<GeicGuestUrl, GuestUrlError> regional =
      GeicGuestUrl::FromPolicy(
          "https://vertexaisearch.cloud.google/us/home/cid/123");
  ASSERT_TRUE(regional.has_value());
  EXPECT_EQ(regional->url(),
            GURL("https://vertexaisearch.cloud.google/side-panel?"
                 "configId=123&location=us"));
  EXPECT_EQ(regional->SkillsUrl(),
            GURL("https://vertexaisearch.cloud.google/home/cid/123/r/skills"));

  struct CanonicalizationCase {
    const char* input;
    const char* expected;
  };
  const CanonicalizationCase kCases[] = {
      {"https://business.gemini.google/home/cid/abc123/",
       "https://business.gemini.google/side-panel?configId=abc123"},
      {"https://business.gemini.google/home/cid/abc?foo=bar",
       "https://business.gemini.google/side-panel?foo=bar&configId=abc"},
      {"https://business.gemini.google/home/cid/abc?configId=other",
       "https://business.gemini.google/side-panel?configId=abc"},
      {"https://business.gemini.google/home/cid/abc&foo=bar",
       "https://business.gemini.google/side-panel?configId=abc%26foo%3Dbar"},
      {"https://business.gemini.google/home/cid/abc#section",
       "https://business.gemini.google/side-panel?configId=abc#section"},
      {"https://vertexaisearch.cloud.google/eu/home/cid/abc123/",
       "https://vertexaisearch.cloud.google/side-panel?"
       "configId=abc123&location=eu"},
      {"https://vertexaisearch.cloud.google/eu/home/cid/abc?foo=bar",
       "https://vertexaisearch.cloud.google/side-panel?"
       "foo=bar&configId=abc&location=eu"},
      {"https://vertexaisearch.cloud.google/eu/home/cid/abc#section",
       "https://vertexaisearch.cloud.google/side-panel?"
       "configId=abc&location=eu#section"},
  };
  for (const CanonicalizationCase& test_case : kCases) {
    base::expected<GeicGuestUrl, GuestUrlError> canonical =
        GeicGuestUrl::FromPolicy(test_case.input);
    ASSERT_TRUE(canonical.has_value()) << test_case.input;
    EXPECT_EQ(canonical->url(), GURL(test_case.expected)) << test_case.input;
  }
}

TEST(GeicGuestUrlTest, LeavesNonHomeCidPathsAlone) {
  const char* const kUnchanged[] = {
      "https://business.gemini.google/side-panel?configId=abc",
      "https://business.gemini.google/home/cid/",
      "https://business.gemini.google/home/other/abc",
      "https://vertexaisearch.cloud.google/us/home/cid/",
      "https://vertexaisearch.cloud.google/us/home/other/abc",
      "https://vertexaisearch.cloud.google/a/b/home/cid/abc",
  };
  for (const char* spec : kUnchanged) {
    base::expected<GeicGuestUrl, GuestUrlError> result =
        GeicGuestUrl::FromPolicy(spec);
    ASSERT_TRUE(result.has_value()) << spec;
    EXPECT_EQ(result->url(), GURL(spec)) << spec;
  }
}

TEST(GeicGuestUrlTest, UnwrapsWorkforceAuthPortalUrls) {
  base::expected<GeicGuestUrl, GuestUrlError> prod_portal =
      GeicGuestUrl::FromPolicy(
          "https://auth.cloud.google/signin/locations/global/workforcePools/"
          "my-pool-1/providers/my-idp?continueUrl=https%3A%2F%2F"
          "vertexaisearch.cloud.google%2Fhome%2Fcid%2F12345");
  ASSERT_TRUE(prod_portal.has_value());
  EXPECT_EQ(prod_portal->url(),
            GURL("https://vertexaisearch.cloud.google/side-panel?configId=12345"
                 "&idp=locations%2Fglobal%2FworkforcePools%2Fmy-pool-1%2F"
                 "providers%2Fmy-idp"));
  EXPECT_EQ(
      prod_portal->SkillsUrl(),
      GURL("https://vertexaisearch.cloud.google/home/cid/12345/r/skills"));

  struct PortalCase {
    const char* input;
    const char* expected;
  };
  const PortalCase kPortalCases[] = {
      {"https://auth.cloud.google/signin/locations/global/workforcePools/"
       "my-pool-1/providers/my-idp?continueUrl=https%3A%2F%2F"
       "vertexaisearch.cloud.google%2Fus%2Fhome%2Fcid%2F12345",
       "https://vertexaisearch.cloud.google/side-panel?configId=12345&"
       "location=us&idp=locations%2Fglobal%2FworkforcePools%2Fmy-pool-1%2F"
       "providers%2Fmy-idp"},
      {"https://auth.cloud.google/signin/?continueUrl=https%3A%2F%2F"
       "vertexaisearch.cloud.google%2Fhome%2Fcid%2F12345%3Fidp%3Dstale-idp",
       "https://vertexaisearch.cloud.google/side-panel?configId=12345"},
      {"https://auth.business.gemini.google/signin/locations/global/"
       "workforcePools/biz-pool/providers/biz-idp/?continueUrl=https%3A%2F%2F"
       "business.gemini.google%2Fhome%2Fcid%2F12345",
       "https://business.gemini.google/side-panel?configId=12345&"
       "idp=locations%2Fglobal%2FworkforcePools%2Fbiz-pool%2Fproviders%2F"
       "biz-idp"},
      {"https://auth.gemini-us.google/signin/locations/global/workforcePools/"
       "il5-pool/providers/il5-idp?continueUrl=https%3A%2F%2F"
       "vertexaisearch.gemini-us.google%2Fhome%2Fcid%2F12345",
       "https://vertexaisearch.gemini-us.google/side-panel?configId=12345&"
       "idp=locations%2Fglobal%2FworkforcePools%2Fil5-pool%2Fproviders%2F"
       "il5-idp"},
      {"https://auth.cloud.google/signin/locations/global/workforcePools/"
       "my-pool-1/providers/new-idp?continueUrl=https%3A%2F%2F"
       "vertexaisearch.cloud.google%2Fside-panel%3FconfigId%3D123%26"
       "idp%3Dold-idp",
       "https://vertexaisearch.cloud.google/side-panel?configId=123&"
       "idp=locations%2Fglobal%2FworkforcePools%2Fmy-pool-1%2Fproviders%2F"
       "new-idp"},
  };
  for (const PortalCase& test_case : kPortalCases) {
    base::expected<GeicGuestUrl, GuestUrlError> unwrapped =
        GeicGuestUrl::FromPolicy(test_case.input);
    ASSERT_TRUE(unwrapped.has_value()) << test_case.input;
    EXPECT_EQ(unwrapped->url(), GURL(test_case.expected)) << test_case.input;
  }

  constexpr char kPreProdPortal[] =
      "https://auth-autopush.corp.cloud.google/signin/locations/global/"
      "workforcePools/my-pool-1/providers/my-idp?continueUrl=https%3A%2F%2F"
      "ucs-app-autopush.corp.cloud.google%2Fhome%2Fcid%2F12345";
  constexpr char kExpectedPreProdUrl[] =
      "https://ucs-app-autopush.corp.cloud.google/side-panel?"
      "configId=12345&idp=locations%2Fglobal%2FworkforcePools%2F"
      "my-pool-1%2Fproviders%2Fmy-idp";
  EXPECT_EQ(GeicGuestUrl::FromPolicy(kPreProdPortal),
            base::unexpected(GuestUrlError::kInvalid));

  base::expected<GeicGuestUrl, GuestUrlError> finch_portal =
      GeicGuestUrl::FromFinch(kPreProdPortal);
  ASSERT_TRUE(finch_portal.has_value());
  EXPECT_EQ(finch_portal->url(), GURL(kExpectedPreProdUrl));

  base::expected<GeicGuestUrl, GuestUrlError> switch_portal =
      GeicGuestUrl::FromSwitch(kPreProdPortal, /*allow_any_http_url=*/false);
  ASSERT_TRUE(switch_portal.has_value());
  EXPECT_EQ(switch_portal->url(), GURL(kExpectedPreProdUrl));
}

TEST(GeicGuestUrlTest, Equality) {
  constexpr char kSpec[] = "https://business.gemini.google/side-panel";
  base::expected<GeicGuestUrl, GuestUrlError> policy =
      GeicGuestUrl::FromPolicy(kSpec);
  base::expected<GeicGuestUrl, GuestUrlError> finch =
      GeicGuestUrl::FromFinch(kSpec);
  ASSERT_TRUE(policy.has_value());
  ASSERT_TRUE(finch.has_value());

  GeicGuestUrl copy = *policy;
  EXPECT_EQ(copy, *policy);
  EXPECT_EQ(*policy, *GeicGuestUrl::FromPolicy(kSpec));
  // Same URL from a different source compares unequal.
  EXPECT_EQ(policy->url(), finch->url());
  EXPECT_NE(*policy, *finch);
}

}  // namespace
}  // namespace geic
