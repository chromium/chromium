// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lookalikes {

TEST(LookalikeUrlNavigationThrottleTest, IsEditDistanceAtMostOne) {
  const struct TestCase {
    const wchar_t* domain;
    const wchar_t* top_domain;
    bool expected;
  } kTestCases[] = {
      {L"", L"", true},
      {L"a", L"a", true},
      {L"a", L"", true},
      {L"", L"a", true},

      {L"", L"ab", false},
      {L"ab", L"", false},

      {L"ab", L"a", true},
      {L"a", L"ab", true},
      {L"ab", L"b", true},
      {L"b", L"ab", true},
      {L"ab", L"ab", true},

      {L"", L"ab", false},
      {L"ab", L"", false},
      {L"a", L"abc", false},
      {L"abc", L"a", false},

      {L"aba", L"ab", true},
      {L"ba", L"aba", true},
      {L"abc", L"ac", true},
      {L"ac", L"abc", true},

      // Same length.
      {L"xbc", L"ybc", true},
      {L"axc", L"ayc", true},
      {L"abx", L"aby", true},

      // Should also work for non-ASCII.
      {L"é", L"", true},
      {L"", L"é", true},
      {L"tést", L"test", true},
      {L"test", L"tést", true},
      {L"tés", L"test", false},
      {L"test", L"tés", false},

      // Real world test cases.
      {L"google.com", L"gooogle.com", true},
      {L"gogle.com", L"google.com", true},
      {L"googlé.com", L"google.com", true},
      {L"google.com", L"googlé.com", true},
      // Different by two characters.
      {L"google.com", L"goooglé.com", false},
  };
  for (const TestCase& test_case : kTestCases) {
    bool result =
        IsEditDistanceAtMostOne(base::WideToUTF16(test_case.domain),
                                base::WideToUTF16(test_case.top_domain));
    EXPECT_EQ(test_case.expected, result);
  }
}

// These redirects are safe:
// - http[s]://sité.test -> http[s]://site.test
// - http[s]://sité.test/path -> http[s]://site.test
// - http[s]://subdomain.sité.test -> http[s]://site.test
// - http[s]://random.test -> http[s]://sité.test -> http[s]://site.test
// - http://sité.test/path -> https://sité.test/path -> https://site.test ->
// <any_url>
// - "subdomain" on either side.
//
// These are not safe:
// - http[s]://[subdomain.]sité.test -> http[s]://[subdomain.]site.test/path
// because the redirected URL has a path.
TEST(LookalikeUrlNavigationThrottleTest, IsSafeRedirect) {
  EXPECT_TRUE(IsSafeRedirect(
      "example.com", {GURL("http://éxample.com"), GURL("http://example.com")}));
  EXPECT_TRUE(IsSafeRedirect(
      "example.com", {GURL("http://éxample.com"), GURL("http://example.com")}));
  EXPECT_TRUE(IsSafeRedirect(
      "example.com",
      {GURL("http://éxample.com"), GURL("http://subdomain.example.com")}));
  EXPECT_TRUE(IsSafeRedirect(
      "example.com", {GURL("http://éxample.com"), GURL("http://example.com"),
                      GURL("https://example.com")}));
  // Original site redirects to HTTPS.
  EXPECT_TRUE(IsSafeRedirect(
      "example.com", {GURL("http://éxample.com"), GURL("https://éxample.com"),
                      GURL("https://example.com")}));
  // Original site redirects to HTTPS which redirects to HTTP which redirects
  // back to HTTPS of the non-IDN version.
  EXPECT_TRUE(IsSafeRedirect(
      "example.com",
      {GURL("http://éxample.com/redir1"), GURL("https://éxample.com/redir1"),
       GURL("http://éxample.com/redir2"), GURL("https://example.com/")}));
  // Same as above, but there is another redirect at the end of the chain.
  EXPECT_TRUE(IsSafeRedirect(
      "example.com",
      {GURL("http://éxample.com/redir1"), GURL("https://éxample.com/redir1"),
       GURL("http://éxample.com/redir2"), GURL("https://example.com/"),
       GURL("https://totallydifferentsite.com/somepath")}));

  // Not a redirect, the chain is too short.
  EXPECT_FALSE(IsSafeRedirect("example.com", {GURL("http://éxample.com")}));
  // Not safe: Redirected site is not the same as the matched site.
  EXPECT_FALSE(IsSafeRedirect("example.com", {GURL("http://éxample.com"),
                                              GURL("http://other-site.com")}));
  // Not safe: Initial URL doesn't redirect to the root of the suggested domain.
  EXPECT_FALSE(IsSafeRedirect(
      "example.com",
      {GURL("http://éxample.com"), GURL("http://example.com/path")}));
  // Not safe: The first redirect away from éxample.com is not to the matching
  // non-IDN site.
  EXPECT_FALSE(IsSafeRedirect("example.com", {GURL("http://éxample.com"),
                                              GURL("http://intermediate.com"),
                                              GURL("http://example.com")}));
}

}  // namespace lookalikes
