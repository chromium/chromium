// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_url_allowlist.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace geic {
namespace {

class GeicUrlAllowlistTest : public testing::Test {};

TEST_F(GeicUrlAllowlistTest, AllowedHosts) {
  // Representative production hosts.
  EXPECT_TRUE(IsAllowedGeminiEnterpriseHost(
      GURL("https://business.gemini.google/home")));
  EXPECT_TRUE(IsAllowedGeminiEnterpriseHost(
      GURL("https://vertexaisearch.cloud.google.com/home")));
  // Gemini Enterprise for Googlers.
  EXPECT_TRUE(IsAllowedGeminiEnterpriseHost(
      GURL("https://ucs-widget.corp.google.com/home")));
  // It intentionally also matches the pre-production `ucs-` pattern, so it can
  // be set from policy, Finch or the command line.
  EXPECT_TRUE(IsAllowedGeminiEnterprisePreProductionHost(
      GURL("https://ucs-widget.corp.google.com/home")));
}

TEST_F(GeicUrlAllowlistTest, PreProductionHostsAreNotPolicyHosts) {
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(
      GURL("https://business-staging.gemini.google/home")));
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(
      GURL("https://ucs-widget-staging.corp.google.com/home")));
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(
      GURL("https://ucs-app-autopush.corp.google.com/home")));
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(
      GURL("https://ucs-widget.corp.cloud.google/home")));
}

TEST_F(GeicUrlAllowlistTest, PreProductionHosts) {
  const char* const kAllowed[] = {
      "https://business-staging.gemini.google/home",
      "https://enterprise-autopush.gemini.google.com/home",
      "https://vertexaisearch-staging.gemini-us.google/home",
      "https://ucs-widget-autopush.corp.google.com/home",
      "https://ucs-app-autopush.corp.cloud.google/home",
      // A new environment label needs no code change.
      "https://business-preprod.gemini.google/home",
  };
  for (const char* url : kAllowed) {
    EXPECT_TRUE(IsAllowedGeminiEnterprisePreProductionHost(GURL(url))) << url;
  }
}

TEST_F(GeicUrlAllowlistTest, DisallowedPreProductionHosts) {
  const char* const kDisallowed[] = {
      // Production hosts are not pre-production hosts.
      "https://business.gemini.google/home",
      "https://enterprise.gemini.google.com/home",
      // Other products on the same parent domains.
      "https://gemini.google.com/home",
      "https://foo.gemini.google.com/home",
      "https://foo.corp.google.com/home",
      // Prefix without an environment label.
      "https://business-.gemini.google/home",
      "https://ucs-.corp.google.com/home",
      // Nested labels.
      "https://a.business-staging.gemini.google/home",
      "https://business-a.b.gemini.google/home",
      // Product prefix on the wrong parent domain.
      "https://business-staging.gemini.google.com/home",
      "https://ucs-widget.google.com/home",
      // Lookalike suffixes.
      "https://business-staging.gemini.google.evil.com/home",
      "https://ucs-widget.corp.google.com.evil.com/home",
      "https://localhost/home",
  };
  for (const char* url : kDisallowed) {
    EXPECT_FALSE(IsAllowedGeminiEnterprisePreProductionHost(GURL(url))) << url;
  }
}

TEST_F(GeicUrlAllowlistTest, DisallowedHosts) {
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(GURL("https://example.com")));
  EXPECT_FALSE(
      IsAllowedGeminiEnterpriseHost(GURL("https://gemini.google.com/home")));
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(GURL("https://localhost")));
  EXPECT_FALSE(
      IsAllowedGeminiEnterpriseHost(GURL("https://gemini.google/home")));
  EXPECT_FALSE(
      IsAllowedGeminiEnterpriseHost(GURL("https://sub.gemini.google/home")));
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(
      GURL("https://sub.business.gemini.google/home")));
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(
      GURL("https://sub.gemini.google.com/home")));
  EXPECT_FALSE(
      IsAllowedGeminiEnterpriseHost(GURL("https://cloud.google.com/home")));
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(
      GURL("https://console.cloud.google.com/home")));
  EXPECT_FALSE(
      IsAllowedGeminiEnterpriseHost(GURL("https://foo.corp.google.com/")));
}

TEST_F(GeicUrlAllowlistTest, FinchGuestUrlDoesNotExpandAllowlist) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kGeic, {{features::kGeicGuestURL.name,
                         "https://custom-host.corp.google.com/side-panel"}});

  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(
      GURL("https://custom-host.corp.google.com/side-panel")));
  EXPECT_FALSE(IsAllowedGeminiEnterprisePreProductionHost(
      GURL("https://custom-host.corp.google.com/side-panel")));
}

// Regression guard: `IsAllowedGeminiEnterpriseHost()` is called by policy
// handlers, which can run before the FeatureList is initialized, so it must
// never read features.
TEST_F(GeicUrlAllowlistTest, BeforeFeatureListInit) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithNullFeatureAndFieldTrialLists();
  base::FeatureList::FailOnFeatureAccessWithoutFeatureList();

  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(GURL("https://example.com/home")));
  EXPECT_TRUE(IsAllowedGeminiEnterpriseHost(
      GURL("https://business.gemini.google/home")));
  EXPECT_TRUE(IsAllowedGeminiEnterprisePreProductionHost(
      GURL("https://business-staging.gemini.google/home")));
  EXPECT_TRUE(
      ParseWorkforceAuthPortalUrl(
          GURL("https://auth.cloud.google/signin/locations/global/"
               "workforcePools/my-pool/providers/my-provider?continueUrl="
               "https%3A%2F%2Fvertexaisearch.cloud.google%2Fhome%2Fcid%2F123"))
          .has_value());
}

TEST_F(GeicUrlAllowlistTest, WorkforceAuthPortalHostsAreNeverDirectGeHosts) {
  EXPECT_FALSE(
      IsAllowedGeminiEnterpriseHost(GURL("https://auth.cloud.google/signin")));
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(
      GURL("https://auth.business.gemini.google/signin")));
  EXPECT_FALSE(IsAllowedGeminiEnterpriseHost(
      GURL("https://auth.gemini-us.google/signin")));
  EXPECT_FALSE(IsAllowedGeminiEnterprisePreProductionHost(
      GURL("https://auth-staging.cloud.google/signin")));
  EXPECT_FALSE(IsAllowedGeminiEnterprisePreProductionHost(
      GURL("https://auth.corp.cloud.google/signin")));
  EXPECT_FALSE(IsAllowedGeminiEnterprisePreProductionHost(
      GURL("https://auth-staging.business-staging.gemini.google/signin")));
}

TEST_F(GeicUrlAllowlistTest, ParseWorkforceAuthPortalUrlValid) {
  const base::expected<WorkforceAuthPortalInfo, WorkforceAuthPortalUrlError>
      classic = ParseWorkforceAuthPortalUrl(
          GURL("https://auth.cloud.google/signin/locations/global/"
               "workforcePools/my-pool-1/providers/okta-saml?continueUrl="
               "https%3A%2F%2Fvertexaisearch.cloud.google%2Fhome%2Fcid%2Fcfg-1"
               "&hl=en"));
  ASSERT_TRUE(classic.has_value());
  EXPECT_EQ(classic->inner_url,
            GURL("https://vertexaisearch.cloud.google/home/cid/cfg-1"));
  EXPECT_EQ(classic->workforce_provider,
            "locations/global/workforcePools/my-pool-1/providers/okta-saml");

  // Optional trailing slash on the provider path is normalized.
  const base::expected<WorkforceAuthPortalInfo, WorkforceAuthPortalUrlError>
      trailing_slash = ParseWorkforceAuthPortalUrl(
          GURL("https://auth.cloud.google/signin/locations/global/"
               "workforcePools/my-pool-1/providers/okta-saml/?continueUrl="
               "https%3A%2F%2Fenterprise.gemini.cloud.google%2Fhome%2Fcid%2F"
               "cfg-1&hl=en_US"));
  ASSERT_TRUE(trailing_slash.has_value());
  EXPECT_EQ(trailing_slash->inner_url,
            GURL("https://enterprise.gemini.cloud.google/home/cid/cfg-1"));
  EXPECT_EQ(trailing_slash->workforce_provider,
            "locations/global/workforcePools/my-pool-1/providers/okta-saml");

  const base::expected<WorkforceAuthPortalInfo, WorkforceAuthPortalUrlError>
      multi_provider = ParseWorkforceAuthPortalUrl(
          GURL("https://auth.cloud.google/signin/?continueUrl="
               "https%3A%2F%2Fgemini.cloud.google%2Fhome%2Fcid%2Fcfg-1"
               "%3Fidp%3Dstale-idp"));
  ASSERT_TRUE(multi_provider.has_value());
  EXPECT_EQ(multi_provider->inner_url,
            GURL("https://gemini.cloud.google/home/cid/cfg-1"));
  EXPECT_TRUE(multi_provider->workforce_provider.empty());

  const base::expected<WorkforceAuthPortalInfo, WorkforceAuthPortalUrlError>
      business = ParseWorkforceAuthPortalUrl(
          GURL("https://auth.business.gemini.google/signin/locations/global/"
               "workforcePools/biz-pool/providers/biz-idp?continueUrl="
               "https%3A%2F%2Fbusiness.gemini.google%2Fhome%2Fcid%2Fcfg-1"));
  ASSERT_TRUE(business.has_value());
  EXPECT_EQ(business->inner_url,
            GURL("https://business.gemini.google/home/cid/cfg-1"));
  EXPECT_EQ(business->workforce_provider,
            "locations/global/workforcePools/biz-pool/providers/biz-idp");

  const base::expected<WorkforceAuthPortalInfo, WorkforceAuthPortalUrlError>
      il5 = ParseWorkforceAuthPortalUrl(
          GURL("https://auth.gemini-us.google/signin/locations/global/"
               "workforcePools/il5-pool/providers/il5-idp?continueUrl="
               "https%3A%2F%2Fvertexaisearch.gemini-us.google%2Fhome%2Fcid%2F"
               "cfg-1"));
  ASSERT_TRUE(il5.has_value());
  EXPECT_EQ(il5->inner_url,
            GURL("https://vertexaisearch.gemini-us.google/home/cid/cfg-1"));
  EXPECT_EQ(il5->workforce_provider,
            "locations/global/workforcePools/il5-pool/providers/il5-idp");
}

TEST_F(GeicUrlAllowlistTest, ParsePreProductionWorkforceAuthPortalUrl) {
  const GURL corp_cloud(
      "https://auth-autopush.corp.cloud.google/signin/locations/global/"
      "workforcePools/my-pool-1/providers/my-idp?continueUrl="
      "https%3A%2F%2Fucs-app-autopush.corp.cloud.google%2Fhome%2Fcid%2Fcfg-1");
  const GURL staging_business(
      "https://auth-staging.business-staging.gemini.google/signin/locations/"
      "global/workforcePools/my-pool-1/providers/my-idp?continueUrl="
      "https%3A%2F%2Fbusiness-staging.gemini.google%2Fside-panel");
  const GURL staging_il5(
      "https://auth-staging.gemini-us.google/signin/locations/global/"
      "workforcePools/my-pool-1/providers/my-idp?continueUrl="
      "https%3A%2F%2Fvertexaisearch-staging.gemini-us.google%2Fside-panel");

  EXPECT_EQ(ParseWorkforceAuthPortalUrl(corp_cloud).error(),
            WorkforceAuthPortalUrlError::kNotWorkforceAuthPortal);
  EXPECT_EQ(ParseWorkforceAuthPortalUrl(staging_business).error(),
            WorkforceAuthPortalUrlError::kNotWorkforceAuthPortal);
  EXPECT_EQ(ParseWorkforceAuthPortalUrl(staging_il5).error(),
            WorkforceAuthPortalUrlError::kNotWorkforceAuthPortal);

  EXPECT_TRUE(ParsePreProductionWorkforceAuthPortalUrl(corp_cloud).has_value());
  EXPECT_TRUE(
      ParsePreProductionWorkforceAuthPortalUrl(staging_business).has_value());
  EXPECT_TRUE(
      ParsePreProductionWorkforceAuthPortalUrl(staging_il5).has_value());
}

TEST_F(GeicUrlAllowlistTest, ParseWorkforceAuthPortalUrlErrors) {
  const char* const kNotAuthPortal[] = {
      "https://evil.com/signin",
      "https://auth-staging.cloud.google/signin?continueUrl="
      "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel",
  };
  for (const char* raw_url : kNotAuthPortal) {
    EXPECT_EQ(ParseWorkforceAuthPortalUrl(GURL(raw_url)).error(),
              WorkforceAuthPortalUrlError::kNotWorkforceAuthPortal)
        << raw_url;
  }

  const char* const kInvalidUrls[] = {
      "http://auth.cloud.google/signin?continueUrl="
      "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel",
      "https://auth.cloud.google:8443/signin?continueUrl="
      "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel",
      "https://user:pass@auth.cloud.google/signin?continueUrl="
      "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel",
      "https://auth.cloud.google/select-session?continueUrl="
      "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel",
      "https://auth.cloud.google/signin//?continueUrl="
      "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel",
      "https://auth.cloud.google/signin/locations/global/workforcePools/"
      "my-pool-1/providers/my-idp//?continueUrl="
      "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel",
      "https://auth.cloud.google/signin/locations/us-central1/workforcePools/"
      "my-pool-1/providers/my-idp?continueUrl="
      "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel",
      "https://auth.cloud.google/signin/locations/global/workforcePools/"
      "short/providers/my-idp?continueUrl="
      "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel",
      "https://auth.cloud.google/signin/locations/global/workforcePools/"
      "1badpool/providers/my-idp?continueUrl="
      "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel",
      "https://auth.cloud.google/signin/locations/global/workforcePools/"
      "my-pool-1/providers/abc?continueUrl="
      "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel",
      "https://auth.cloud.google/signin/locations/global/workforcePools/"
      "my-pool-1/providers/MyIdp?continueUrl="
      "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel",
      "https://auth.cloud.google/signin",
      "https://auth.cloud.google/signin?continueUrl=",
      "https://auth.cloud.google/signin?continueUrl="
      "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel&continueUrl="
      "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel",
      "https://auth.cloud.google/signin?continueUrl="
      "https%3A%2F%2Fvertexaisearch.cloud.google%3A8443%2Fside-panel",
      "https://auth.cloud.google/signin?continueUrl="
      "https%3A%2F%2Fuser%3Apass%40vertexaisearch.cloud.google%2Fside-panel",
  };
  for (const char* raw_url : kInvalidUrls) {
    EXPECT_EQ(ParseWorkforceAuthPortalUrl(GURL(raw_url)).error(),
              WorkforceAuthPortalUrlError::kInvalidUrl)
        << raw_url;
  }

  EXPECT_EQ(ParseWorkforceAuthPortalUrl(
                GURL("https://auth.cloud.google/signin?continueUrl="
                     "http%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel"))
                .error(),
            WorkforceAuthPortalUrlError::kInnerUrlNotHttps);

  const char* const kDisallowedInner[] = {
      "https://auth.cloud.google/signin?continueUrl="
      "https%3A%2F%2Fevil.com%2Fside-panel",
      "https://auth.cloud.google/signin?continueUrl="
      "https%3A%2F%2Fauth.cloud.google%2Fsignin",
      "https://auth.cloud.google/signin?continueUrl="
      "https%3A%2F%2Fvertexaisearch.cloud.google.com%2Fside-panel",
      "https://auth.cloud.google/signin?continueUrl="
      "https%3A%2F%2Fbusiness.gemini.google%2Fside-panel",
      "https://auth.cloud.google/signin?continueUrl="
      "https%3A%2F%2Fvertexaisearch.gemini-us.google%2Fside-panel",
      "https://auth.business.gemini.google/signin?continueUrl="
      "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel",
      "https://auth.gemini-us.google/signin?continueUrl="
      "https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel",
      "https://auth.gemini-us.google/signin?continueUrl="
      "https%3A%2F%2Fvertexaisearch-staging.gemini-us.google%2Fside-panel",
  };
  for (const char* raw_url : kDisallowedInner) {
    EXPECT_EQ(ParseWorkforceAuthPortalUrl(GURL(raw_url)).error(),
              WorkforceAuthPortalUrlError::kDisallowedInnerHost)
        << raw_url;
  }
}

TEST_F(GeicUrlAllowlistTest,
       IsPairedWorkforceAuthPortalOriginMatchesProdPortals) {
  EXPECT_TRUE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin::Create(GURL("https://auth.cloud.google")),
      GURL("https://vertexaisearch.cloud.google/side-panel?configId=123")));
  EXPECT_TRUE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin::Create(GURL("https://auth.cloud.google")),
      GURL("https://enterprise.gemini.cloud.google/side-panel")));
  EXPECT_TRUE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin::Create(GURL("https://auth.cloud.google")),
      GURL("https://gemini.cloud.google/side-panel")));
  EXPECT_TRUE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin::Create(GURL("https://auth.business.gemini.google")),
      GURL("https://business.gemini.google/side-panel?configId=123")));
  EXPECT_TRUE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin::Create(GURL("https://auth.gemini-us.google")),
      GURL("https://vertexaisearch.gemini-us.google/side-panel?configId=123")));
}

TEST_F(GeicUrlAllowlistTest,
       IsPairedWorkforceAuthPortalOriginRejectsCrossDomainAnd1P) {
  // Cross-domain portal mismatch.
  EXPECT_FALSE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin::Create(GURL("https://auth.cloud.google")),
      GURL("https://business.gemini.google/side-panel")));
  EXPECT_FALSE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin::Create(GURL("https://auth.business.gemini.google")),
      GURL("https://vertexaisearch.cloud.google/side-panel")));
  EXPECT_FALSE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin::Create(GURL("https://auth.gemini-us.google")),
      GURL("https://vertexaisearch.cloud.google/side-panel")));

  // 1P Gaia `.google.com` hosts do not use Workforce Identity Auth Portals.
  EXPECT_FALSE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin::Create(GURL("https://auth.cloud.google")),
      GURL("https://vertexaisearch.cloud.google.com/side-panel")));
  EXPECT_FALSE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin::Create(GURL("https://auth.cloud.google")),
      GURL("https://enterprise.gemini.google.com/side-panel")));

  // Non-HTTPS, custom ports, opaque origins, or disallowed guest hosts.
  EXPECT_FALSE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin::Create(GURL("http://auth.cloud.google")),
      GURL("https://vertexaisearch.cloud.google/side-panel")));
  EXPECT_FALSE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin::Create(GURL("https://auth.cloud.google:8443")),
      GURL("https://vertexaisearch.cloud.google/side-panel")));
  EXPECT_FALSE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin(), GURL("https://vertexaisearch.cloud.google/side-panel")));
  EXPECT_FALSE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin::Create(GURL("https://auth.cloud.google")),
      GURL("https://console.cloud.google/side-panel")));
  EXPECT_FALSE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin::Create(GURL("https://evil.com")),
      GURL("https://vertexaisearch.cloud.google/side-panel")));
}

TEST_F(GeicUrlAllowlistTest,
       IsPairedWorkforceAuthPortalOriginMatchesPreProdPortals) {
  EXPECT_TRUE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin::Create(GURL("https://auth-autopush.corp.cloud.google")),
      GURL("https://ucs-app-autopush.corp.cloud.google/side-panel")));
  EXPECT_TRUE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin::Create(
          GURL("https://auth-staging.business-staging.gemini.google")),
      GURL("https://business-staging.gemini.google/side-panel")));
  EXPECT_TRUE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin::Create(GURL("https://auth-staging.gemini-us.google")),
      GURL("https://vertexaisearch-staging.gemini-us.google/side-panel")));

  // Pre-prod portal does not match mismatched pre-prod environment label.
  EXPECT_FALSE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin::Create(
          GURL("https://auth-staging.business-staging.gemini.google")),
      GURL("https://business-autopush.gemini.google/side-panel")));
  // Pre-prod portal does not match a production guest host on the same parent
  // domain.
  EXPECT_FALSE(IsPairedWorkforceAuthPortalOrigin(
      url::Origin::Create(GURL("https://auth-staging.gemini-us.google")),
      GURL("https://vertexaisearch.gemini-us.google/side-panel")));
}

TEST_F(GeicUrlAllowlistTest,
       IsWorkforceAuthPortalInteractiveSignInPathClassifiesEndpoints) {
  EXPECT_TRUE(IsWorkforceAuthPortalInteractiveSignInPath("/signin"));
  EXPECT_TRUE(IsWorkforceAuthPortalInteractiveSignInPath("/signin/"));
  EXPECT_TRUE(IsWorkforceAuthPortalInteractiveSignInPath(
      "/signin/locations/global/workforcePools/p/providers/idp"));
  EXPECT_TRUE(IsWorkforceAuthPortalInteractiveSignInPath("/login"));
  EXPECT_TRUE(IsWorkforceAuthPortalInteractiveSignInPath("/login/federated"));
  EXPECT_TRUE(IsWorkforceAuthPortalInteractiveSignInPath("/reauth"));
  EXPECT_TRUE(IsWorkforceAuthPortalInteractiveSignInPath("/reauth/challenge"));

  // Silent 302 session-check/cookie-refresh endpoints are not interactive
  // sign-in paths.
  EXPECT_FALSE(IsWorkforceAuthPortalInteractiveSignInPath("/select-session"));
  EXPECT_FALSE(IsWorkforceAuthPortalInteractiveSignInPath("/account-chooser"));
  EXPECT_FALSE(IsWorkforceAuthPortalInteractiveSignInPath("/refreshcookies"));
  EXPECT_FALSE(IsWorkforceAuthPortalInteractiveSignInPath("/submit"));
  EXPECT_FALSE(IsWorkforceAuthPortalInteractiveSignInPath("/signin-other"));
}

TEST_F(GeicUrlAllowlistTest,
       BuildWorkforceSignInTabUrlConstructsExpectedTarget) {
  const GURL guest_url(
      "https://vertexaisearch.cloud.google/side-panel?"
      "configId=cfg-1&idp=locations%2Fglobal%2FworkforcePools%2Fpool-1%2F"
      "providers%2Fprovider-1&csesidx=42&hl=en");

  // When intercepted on `/select-session` on the paired Auth Portal, updates
  // `continueUrl` (stripping stale `csesidx` and adding `auth_popout=1`) and
  // injects `provider_name` from the guest URL's `idp` param.
  const GURL intercepted_select_session(
      "https://auth.cloud.google/select-session?"
      "continueUrl=https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel");
  const GURL built =
      BuildWorkforceSignInTabUrl(guest_url, intercepted_select_session);
  EXPECT_EQ(
      built,
      GURL("https://auth.cloud.google/select-session?"
           "continueUrl=https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel"
           "%3FconfigId%3Dcfg-1%26idp%3Dlocations%252Fglobal%252FworkforcePools"
           "%252Fpool-1%252Fproviders%252Fprovider-1%26hl%3Den"
           "%26auth_popout%3D1&"
           "provider_name=locations%2Fglobal%2FworkforcePools%2Fpool-1%2F"
           "providers%2Fprovider-1"));

  // When intercepted on `/submit`, also injects `provider_name`.
  const GURL intercepted_submit(
      "https://auth.cloud.google/submit?"
      "continueUrl=https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel");
  EXPECT_EQ(
      BuildWorkforceSignInTabUrl(guest_url, intercepted_submit),
      GURL("https://auth.cloud.google/submit?"
           "continueUrl=https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel"
           "%3FconfigId%3Dcfg-1%26idp%3Dlocations%252Fglobal%252FworkforcePools"
           "%252Fpool-1%252Fproviders%252Fprovider-1%26hl%3Den"
           "%26auth_popout%3D1&"
           "provider_name=locations%2Fglobal%2FworkforcePools%2Fpool-1%2F"
           "providers%2Fprovider-1"));

  // When intercepted on `/signin` or `/signin/`, rewrites the path to
  // `/signin/<workforce_provider>`.
  const GURL intercepted_signin(
      "https://auth.cloud.google/signin?"
      "continueUrl=https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel");
  EXPECT_EQ(
      BuildWorkforceSignInTabUrl(guest_url, intercepted_signin),
      GURL("https://auth.cloud.google/signin/locations/global/workforcePools/"
           "pool-1/providers/provider-1?"
           "continueUrl=https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel"
           "%3FconfigId%3Dcfg-1%26idp%3Dlocations%252Fglobal%252FworkforcePools"
           "%252Fpool-1%252Fproviders%252Fprovider-1%26hl%3Den"
           "%26auth_popout%3D1"));

  // When intercepted on `/account-chooser` on Gemini Business, updates
  // `continueUrl` without appending `provider_name`.
  const GURL business_guest_url(
      "https://business.gemini.google/side-panel?"
      "configId=cfg-1&idp=locations%2Fglobal%2FworkforcePools%2Fpool-1%2F"
      "providers%2Fprovider-1&hl=en");
  const GURL intercepted_account_chooser(
      "https://auth.business.gemini.google/account-chooser?"
      "continueUrl=https%3A%2F%2Fbusiness.gemini.google%2Fside-panel");
  EXPECT_EQ(
      BuildWorkforceSignInTabUrl(business_guest_url,
                                 intercepted_account_chooser),
      GURL("https://auth.business.gemini.google/account-chooser?"
           "continueUrl=https%3A%2F%2Fbusiness.gemini.google%2Fside-panel"
           "%3FconfigId%3Dcfg-1%26idp%3Dlocations%252Fglobal%252FworkforcePools"
           "%252Fpool-1%252Fproviders%252Fprovider-1%26hl%3Den"
           "%26auth_popout%3D1"));

  // When `provider_name=` is present with an empty value, replaces it with
  // `workforce_provider`.
  const GURL intercepted_empty_provider(
      "https://auth.cloud.google/select-session?"
      "provider_name=&continueUrl=https%3A%2F%2Fvertexaisearch.cloud.google"
      "%2Fside-panel");
  EXPECT_EQ(
      BuildWorkforceSignInTabUrl(guest_url, intercepted_empty_provider),
      GURL("https://auth.cloud.google/select-session?"
           "provider_name=locations%2Fglobal%2FworkforcePools%2Fpool-1%2F"
           "providers%2Fprovider-1&"
           "continueUrl=https%3A%2F%2Fvertexaisearch.cloud.google%2Fside-panel"
           "%3FconfigId%3Dcfg-1%26idp%3Dlocations%252Fglobal%252FworkforcePools"
           "%252Fpool-1%252Fproviders%252Fprovider-1%26hl%3Den"
           "%26auth_popout%3D1"));

  // Nested non-HTTPS URLs (e.g. `blob:https://auth.cloud.google/...`) do not
  // match the Workforce Auth Portal and fall back to the guest URL.
  EXPECT_EQ(BuildWorkforceSignInTabUrl(
                guest_url, GURL("blob:https://auth.cloud.google/1234-5678")),
            GURL("https://vertexaisearch.cloud.google/side-panel?"
                 "configId=cfg-1&idp=locations%2Fglobal%2FworkforcePools%2F"
                 "pool-1%2Fproviders%2Fprovider-1&hl=en&auth_popout=1"));

  // When no intercepted URL (or an external 3P IdP URL) is present, falls back
  // to the guest URL with `auth_popout=1` and `csesidx` stripped.
  const GURL fallback = BuildWorkforceSignInTabUrl(
      guest_url, GURL("https://login.microsoftonline.com/common/oauth2"));
  EXPECT_EQ(fallback,
            GURL("https://vertexaisearch.cloud.google/side-panel?"
                 "configId=cfg-1&idp=locations%2Fglobal%2FworkforcePools%2F"
                 "pool-1%2Fproviders%2Fprovider-1&hl=en&auth_popout=1"));
}

}  // namespace

}  // namespace geic
