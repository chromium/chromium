// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_cic_proxy_loader.h"

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "content/public/browser/content_browser_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {
namespace {

using FactoryType = content::ContentBrowserClient::URLLoaderFactoryType;

constexpr char kProdOriginSpec[] = "https://glic-cic.apps.corp.google.com/";

TEST(GlicCicProxyLoaderTest, InvalidCicUrlParamFallsBackToProdOrigin) {
  const GURL prod_origin(kProdOriginSpec);

  for (const char* invalid_url : {
           "javascript:alert(1)",
           "http://example.com",
           "https://user:pass@glic-cic.apps.corp.google.com",
           "ftp://127.0.0.1:8080",
       }) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(features::kGlicCic,
                                                    {{"url", invalid_url}});
    EXPECT_EQ(prod_origin, GetGlicCicBaseUrl()) << "url=" << invalid_url;
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kGlicCic,
        {{"url", "http://127.0.0.1:8765/ignored/path?foo=bar"}});
    EXPECT_EQ(GURL("http://127.0.0.1:8765/"), GetGlicCicBaseUrl());
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kGlicCic,
        {{"url", "https://custom-cic.example.com:8443/prefix"}});
    EXPECT_EQ(GURL("https://custom-cic.example.com:8443/"),
              GetGlicCicBaseUrl());
  }
}

TEST(GlicCicProxyLoaderTest, RewritesMainFrameGlicNavigationPaths) {
  const url::Origin guest_origin =
      url::Origin::Create(GURL("https://gemini.google.com"));
  const GURL cic_base("https://glic-cic.apps.corp.google.com/");

  EXPECT_EQ(GURL("https://glic-cic.apps.corp.google.com/glic"),
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/glic"), guest_origin, "/glic",
                cic_base, FactoryType::kNavigation,
                /*is_primary_main_frame=*/true, "GET", std::nullopt));

  EXPECT_EQ(GURL("https://glic-cic.apps.corp.google.com/glic?hl=en"),
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/glic/?hl=en#frag"),
                guest_origin, "/glic", cic_base, FactoryType::kNavigation,
                /*is_primary_main_frame=*/true, "HEAD", std::nullopt));

  // Multi-login `/u/<N>/glic` prefix is stripped.
  EXPECT_EQ(GURL("https://glic-cic.apps.corp.google.com/glic?hl=ja"),
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/u/0/glic?hl=ja"), guest_origin,
                "/glic", cic_base, FactoryType::kNavigation,
                /*is_primary_main_frame=*/true, "GET", std::nullopt));

  // Custom `--glic-guest-url` document paths normalize to `/glic`.
  EXPECT_EQ(GURL("https://glic-cic.apps.corp.google.com/glic"),
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/custom/entry"), guest_origin,
                "/custom/entry", cic_base, FactoryType::kNavigation,
                /*is_primary_main_frame=*/true, "GET", std::nullopt));

  // `glic_passthrough=1` bypasses the proxy.
  EXPECT_EQ(std::nullopt,
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/glic?glic_passthrough=1"),
                guest_origin, "/glic", cic_base, FactoryType::kNavigation,
                /*is_primary_main_frame=*/true, "GET", std::nullopt));

  // Non-GET/HEAD navigations are not proxied.
  EXPECT_EQ(std::nullopt,
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/glic"), guest_origin, "/glic",
                cic_base, FactoryType::kNavigation,
                /*is_primary_main_frame=*/true, "POST", std::nullopt));

  // Subframe `/glic` navigations are not proxied.
  EXPECT_EQ(std::nullopt,
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/glic"), guest_origin, "/glic",
                cic_base, FactoryType::kNavigation,
                /*is_primary_main_frame=*/false, "GET", std::nullopt));
}

TEST(GlicCicProxyLoaderTest, RewritesCicPathsAndStripsMultiLoginPrefix) {
  const url::Origin guest_origin =
      url::Origin::Create(GURL("https://gemini.google.com"));
  const GURL cic_base("http://127.0.0.1:8765/");

  // Primary and multi-login `/cic/` subresources.
  EXPECT_EQ(
      GURL("http://127.0.0.1:8765/cic/api/healthz?v=1"),
      RewriteGlicCicTargetUrl(
          GURL("https://gemini.google.com/cic/api/healthz?v=1#ref"),
          guest_origin, "/glic", cic_base, FactoryType::kDocumentSubResource,
          /*is_primary_main_frame=*/true, "GET", guest_origin));
  EXPECT_EQ(
      GURL("http://127.0.0.1:8765/cic/api/healthz"),
      RewriteGlicCicTargetUrl(
          GURL("https://gemini.google.com/u/0/cic/api/healthz"), guest_origin,
          "/glic", cic_base, FactoryType::kDocumentSubResource,
          /*is_primary_main_frame=*/false, "POST", guest_origin));

  // Subframe `/cic/` navigation (e.g. sandbox iframe).
  EXPECT_EQ(GURL("http://127.0.0.1:8765/cic/sandbox.html"),
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/u/2/cic/sandbox.html"),
                guest_origin, "/glic", cic_base, FactoryType::kNavigation,
                /*is_primary_main_frame=*/false, "GET", std::nullopt));

  // Non-same-origin or missing initiator is rejected for subresources.
  EXPECT_EQ(std::nullopt,
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/cic/api/healthz"), guest_origin,
                "/glic", cic_base, FactoryType::kDocumentSubResource,
                /*is_primary_main_frame=*/true, "GET", std::nullopt));
  EXPECT_EQ(std::nullopt,
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/cic/api/healthz"), guest_origin,
                "/glic", cic_base, FactoryType::kDocumentSubResource,
                /*is_primary_main_frame=*/true, "GET",
                url::Origin::Create(GURL("https://evil.example.com"))));

  // Cross-origin request URL is rejected.
  EXPECT_EQ(std::nullopt,
            RewriteGlicCicTargetUrl(
                GURL("https://other.example.com/cic/api/healthz"), guest_origin,
                "/glic", cic_base, FactoryType::kDocumentSubResource,
                /*is_primary_main_frame=*/true, "GET", guest_origin));

  // Non-`/cic/` subresources stay on the guest origin.
  EXPECT_EQ(
      std::nullopt,
      RewriteGlicCicTargetUrl(
          GURL("https://gemini.google.com/chrome__glic_api.js"), guest_origin,
          "/glic", cic_base, FactoryType::kDocumentSubResource,
          /*is_primary_main_frame=*/true, "GET", guest_origin));
}

TEST(GlicCicProxyLoaderTest, RejectsPathTraversal) {
  const url::Origin guest_origin =
      url::Origin::Create(GURL("https://gemini.google.com"));
  const GURL cic_base("https://glic-cic.apps.corp.google.com/");

  for (const char* bad_url : {
           "https://gemini.google.com/cic/%2e%2e/secret",
           "https://gemini.google.com/cic/%2E%2Fsecret",
           "https://gemini.google.com/cic/a%5cb",
           "https://gemini.google.com/cic/a%00b",
           "https://gemini.google.com/u/0/cic/%2e%2e/secret",
       }) {
    EXPECT_EQ(std::nullopt,
              RewriteGlicCicTargetUrl(
                  GURL(bad_url), guest_origin, "/glic", cic_base,
                  FactoryType::kDocumentSubResource,
                  /*is_primary_main_frame=*/true, "GET", guest_origin))
        << "url=" << bad_url;
  }
}

}  // namespace
}  // namespace glic
