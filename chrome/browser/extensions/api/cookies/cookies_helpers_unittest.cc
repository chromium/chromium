// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cookies/cookies_helpers.h"

#include <limits>
#include <memory>

#include "net/cookies/canonical_cookie.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {
std::optional<extensions::api::cookies::CookiePartitionKey>
BuildApiPartitionKey(const std::optional<std::string>& top_level_site,
                     const std::optional<bool>& has_cross_site_ancestor) {
  base::Value::Dict partition_key_vals;

  if (top_level_site.has_value()) {
    partition_key_vals.Set("topLevelSite", top_level_site.value());
  }
  if (has_cross_site_ancestor.has_value()) {
    partition_key_vals.Set("hasCrossSiteAncestor",
                           has_cross_site_ancestor.value());
  }

  auto partition_key = extensions::api::cookies::CookiePartitionKey::FromValue(
      partition_key_vals);
  CHECK(partition_key.has_value());
  return partition_key;
}
}  // namespace

// Tests that cookies with an expiration date too far in the future to represent
// with base::Time serialize gracefully.
// Regression test for https://crbug.com/848221.
TEST(CookiesHelperUnittest, CookieConversionWithInfiniteExpirationDate) {
  // Set a cookie to expire at base::Time::Max(). This can happen when the
  // cookie is set to expire farther in the future than we can accurately
  // represent with base::Time(). Note that, in practice, this is really only
  // applicable on 32-bit machines, but we can fake it a bit for cross-platform
  // testing by just setting the expiration date directly.
  const base::Time kExpirationDate = base::Time::Max();
  auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "cookiename", "cookievalue", "example.com", "/", base::Time::Now(),
      kExpirationDate, base::Time(), base::Time(), false, false,
      net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT);

  // Serialize the cookie to JSON. We need to gracefully handle the infinite
  // expiration date, which should be converted to the maximum value.
  api::cookies::Cookie serialized_cookie =
      cookies_helpers::CreateCookie(*cookie, "1");
  base::Value::Dict value_cookie = serialized_cookie.ToValue();
  std::optional<double> expiration_time =
      value_cookie.FindDouble("expirationDate");
  ASSERT_TRUE(expiration_time);
  EXPECT_EQ(std::numeric_limits<double>::max(), *expiration_time);
}

TEST(CookiesHelperUnittest, ValidateCrossSiteAncestorErrorCases) {
  std::string site = "https://example.com";
  std::string invalid_site = "invalid";
  struct {
    std::string description;
    std::string url;
    std::optional<std::string> top_level_site;
    std::optional<bool> has_cross_site_ancestor;
    std::string error_str;
  } error_cases[] = {
      {/*description=*/"No top_level_site and a has_cross_site_ancestor",
       /*url=*/site, /*top_level_site=*/std::nullopt,
       /*has_cross_site_ancestor=*/true,
       /*error_str=*/
       "CookiePartitionKey.topLevelSite is not present when "
       "CookiePartitionKey.hasCrossSiteAncestor is present."},
      {/*description=*/"Empty string for top_level_site and a "
                       "has_cross_site_ancestor` value of true",
       /*url=*/site, /*top_level_site=*/"", /*has_cross_site_ancestor=*/true,
       /*error_str=*/"CookiePartitionKey.hasCrossSiteAncestor is invalid."},
      {/*description=*/"Invalid url",
       /*url=*/invalid_site, /*top_level_site=*/site,
       /*has_cross_site_ancestor=*/true, /*error_str=*/"Invalid url_string."},
      {/*description=*/"Invalid top_level_site",
       /*url=*/site, /*top_level_site=*/invalid_site,
       /*has_cross_site_ancestor=*/true,
       /*error_str=*/"Invalid value for CookiePartitionKey.topLevelSite."},
      {/*description=*/"has_cross_site_ancestor can not be true if url and "
                       "top_level_site` aren't first party.",
       /*url=*/site, /*top_level_site=*/invalid_site,
       /*has_cross_site_ancestor=*/false,
       /*error_str=*/"Invalid value for CookiePartitionKey.topLevelSite."},
      {/*description=*/"has_cross_site_ancestor must be populated for "
                       "validation.",
       /*url=*/site, /*top_level_site=*/site,
       /*has_cross_site_ancestor=*/std::nullopt,
       /*error_str=*/
       "Can not validate an empty value "
       "for hasCrossSiteAncestor."},
  };

  for (const auto& tc : error_cases) {
    SCOPED_TRACE(tc.description);

    auto partition_key =
        BuildApiPartitionKey(tc.top_level_site, tc.has_cross_site_ancestor);

    std::string error;
    EXPECT_FALSE(cookies_helpers::ValidateCrossSiteAncestor(
        tc.url, partition_key, &error));
    EXPECT_EQ(tc.error_str, error);
  }
}

TEST(CookiesHelperUnittest, CalculateHasCrossSiteAncestor) {
  std::string site = "https://example.com";
  std::string cross_site = "https://crosssite.com";
  std::string invalid_site = "invalid";
  struct {
    std::string description;
    std::string url;
    std::optional<std::string> top_level_site;
    std::optional<bool> has_cross_site_ancestor;
    base::expected<bool, std::string> result;
  } cases[] = {
      {/*description=*/"Valid same-site case with empty hasCrossSiteAncestor ",
       /*url=*/site,
       /*top_level_site=*/site,
       /*has_cross_site_ancestor=*/std::nullopt,
       /*result=*/false},
      {/*description=*/"Valid cross-site case with empty hasCrossSiteAncestor ",
       /*url=*/site,
       /*top_level_site=*/cross_site,
       /*has_cross_site_ancestor=*/std::nullopt,
       /*result=*/true},
      {
          /*description=*/"Return already set value",
          /*url=*/site,
          /*top_level_site=*/site,
          /*has_cross_site_ancestor=*/true,
          /*result=*/true,
      },
      {
          /*description=*/"Empty top_level_site indicates an unpartitioned "
                          "cookie which always has a hasCrossSiteAncestor of "
                          "false.",
          /*url=*/site,
          /*top_level_site=*/"",
          /*has_cross_site_ancestor=*/std::nullopt,
          /*result=*/false,
      },
      {/*description=*/"Invalid url_string",
       /*url=*/invalid_site,
       /*top_level_site=*/site,
       /*has_cross_site_ancestor=*/std::nullopt,
       /*result=*/base::unexpected("Invalid url_string.")},
      {/*description=*/"Invalid value for CookiePartitionKey.topLevelSite",
       /*url=*/site,
       /*top_level_site=*/invalid_site,
       /*has_cross_site_ancestor=*/std::nullopt,
       /*result=*/
       base::unexpected("Invalid value for CookiePartitionKey.topLevelSite.")},
  };
  for (const auto& tc : cases) {
    SCOPED_TRACE(tc.description);
    auto partition_key =
        BuildApiPartitionKey(tc.top_level_site, tc.has_cross_site_ancestor);

    base::expected<bool, std::string> result =
        cookies_helpers::CalculateHasCrossSiteAncestor(tc.url, partition_key);
    EXPECT_EQ(tc.result, result);
  }
}

TEST(CookiesHelperUnittest, ToNetCookiePartitionKey) {
  std::string site = "https://example.com";

  struct {
    std::string description;
    std::optional<std::string> top_level_site;
    std::optional<bool> has_cross_site_ancestor;
    base::expected<std::optional<net::CookiePartitionKey>, std::string>
        expected;
  } cases[] = {
      {/*description=*/"Valid unpartitioned case, no hasCrossSiteAncestor or "
                       "topLevelSite",
       /*top_level_site=*/std::nullopt,
       /*has_cross_site_ancestor=*/std::nullopt,
       /*expected=*/std::nullopt},
      {/*description=*/"Valid unpartitioned case, no hasCrossSiteAncestor and "
                       "empty topLevelSite",
       /*top_level_site=*/"",
       /*has_cross_site_ancestor=*/std::nullopt,
       /*expected=*/std::nullopt},
      {/*description=*/"Valid unpartitioned case, empty topLevelSite and "
                       "hasCrossSiteAncestor value of false",
       /*top_level_site=*/"",
       /*has_cross_site_ancestor=*/false,
       /*expected=*/std::nullopt},
      {/*description=*/"Valid case with hasCrossSiteAncestor false",
       /*top_level_site=*/site,
       /*has_cross_site_ancestor=*/false,
       /*expected=*/
       net::CookiePartitionKey::FromURLForTesting(
           GURL(site), net::CookiePartitionKey::AncestorChainBit::kSameSite)},
      {/*description=*/"Valid case with hasCrossSiteAncestor true",
       /*top_level_site=*/site,
       /*has_cross_site_ancestor=*/true,
       /*expected=*/net::CookiePartitionKey::FromURLForTesting(GURL(site))},
      {/*description=*/"No hasCrossSiteAncestor value default to true",
       /*top_level_site=*/site,
       /*has_cross_site_ancestor=*/std::nullopt,
       /*expected=*/net::CookiePartitionKey::FromURLForTesting(GURL(site))},
      {/*description=*/"HasCrossSiteAncestor populated but no topLevelSite",
       /*top_level_site=*/std::nullopt,
       /*has_cross_site_ancestor=*/true,
       /*expected=*/
       base::unexpected(
           "CookiePartitionKey.topLevelSite unexpectedly not present.")},
      {/*description=*/"Invalid topLevelSite",
       /*top_level_site=*/"INVALID",
       /*has_cross_site_ancestor=*/true,
       /*expected=*/
       base::unexpected(
           "Cannot deserialize opaque origin to CookiePartitionKey")},
  };

  for (const auto& tc : cases) {
    SCOPED_TRACE(tc.description);
    auto partition_key =
        BuildApiPartitionKey(tc.top_level_site, tc.has_cross_site_ancestor);
    if (!tc.expected.has_value()) {
      DLOG(ERROR)
          << cookies_helpers::ToNetCookiePartitionKey(partition_key).error();
    }

    EXPECT_EQ(tc.expected,
              cookies_helpers::ToNetCookiePartitionKey(partition_key));
  }
}
}  // namespace extensions
