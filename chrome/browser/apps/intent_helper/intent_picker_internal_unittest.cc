// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_picker_internal.h"

#include "base/test/gtest_util.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace apps {

TEST(IntentPickersInternalTest, TestIsGoogleRedirectorUrl) {
  // Test that redirect urls with different TLDs are still recognized.
  EXPECT_TRUE(IsGoogleRedirectorUrlForTesting(
      GURL("https://www.google.com.au/url?q=wathever")));
  EXPECT_TRUE(IsGoogleRedirectorUrlForTesting(
      GURL("https://www.google.com.mx/url?q=hotpot")));
  EXPECT_TRUE(IsGoogleRedirectorUrlForTesting(
      GURL("https://www.google.co/url?q=query")));

  // Non-google domains shouldn't be used as valid redirect links.
  EXPECT_FALSE(IsGoogleRedirectorUrlForTesting(
      GURL("https://www.not-google.com/url?q=query")));
  EXPECT_FALSE(IsGoogleRedirectorUrlForTesting(
      GURL("https://www.gooogle.com/url?q=legit_query")));

  // This method only takes "/url" as a valid path, it needs to contain a query,
  // we don't analyze that query as it will expand later on in the same
  // throttle.
  EXPECT_TRUE(IsGoogleRedirectorUrlForTesting(
      GURL("https://www.google.com/url?q=who_dis")));
  EXPECT_TRUE(IsGoogleRedirectorUrlForTesting(
      GURL("http://www.google.com/url?q=who_dis")));
  EXPECT_FALSE(
      IsGoogleRedirectorUrlForTesting(GURL("https://www.google.com/url")));
  EXPECT_FALSE(IsGoogleRedirectorUrlForTesting(
      GURL("https://www.google.com/link?q=query")));
  EXPECT_FALSE(
      IsGoogleRedirectorUrlForTesting(GURL("https://www.google.com/link")));
}

TEST(IntentPickersInternalTest, TestShouldOverrideUrlLoading) {
  // If either of two parameters is empty, the function should return false.
  EXPECT_FALSE(ShouldOverrideUrlLoading(GURL(), GURL("http://a.google.com/")));
  EXPECT_FALSE(ShouldOverrideUrlLoading(GURL("http://a.google.com/"), GURL()));
  EXPECT_FALSE(ShouldOverrideUrlLoading(GURL(), GURL()));

  // A navigation to an a url that is neither an http nor https scheme cannot be
  // override.
  EXPECT_FALSE(ShouldOverrideUrlLoading(
      GURL("http://www.a.com"), GURL("chrome-extension://fake_document")));
  EXPECT_FALSE(ShouldOverrideUrlLoading(
      GURL("https://www.a.com"), GURL("chrome-extension://fake_document")));
  EXPECT_FALSE(ShouldOverrideUrlLoading(GURL("http://www.a.com"),
                                        GURL("chrome://fake_document")));
  EXPECT_FALSE(ShouldOverrideUrlLoading(GURL("http://www.a.com"),
                                        GURL("file://fake_document")));
  EXPECT_FALSE(ShouldOverrideUrlLoading(GURL("https://www.a.com"),
                                        GURL("chrome://fake_document")));
  EXPECT_FALSE(ShouldOverrideUrlLoading(GURL("https://www.a.com"),
                                        GURL("file://fake_document")));

  // A navigation from chrome-extension scheme cannot be overridden.
  EXPECT_FALSE(ShouldOverrideUrlLoading(
      GURL("chrome-extension://fake_document"), GURL("http://www.a.com")));
  EXPECT_FALSE(ShouldOverrideUrlLoading(
      GURL("chrome-extension://fake_document"), GURL("https://www.a.com")));
  EXPECT_FALSE(ShouldOverrideUrlLoading(GURL("chrome-extension://fake_a"),
                                        GURL("chrome-extension://fake_b")));

  // Other navigations can be overridden.
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("http://www.google.com"),
                                       GURL("http://www.not-google.com/")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("http://www.not-google.com"),
                                       GURL("http://www.google.com/")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("http://www.google.com"),
                                       GURL("http://www.google.com/")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("http://a.google.com"),
                                       GURL("http://b.google.com/")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("http://a.not-google.com"),
                                       GURL("http://b.not-google.com")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("chrome://fake_document"),
                                       GURL("http://www.a.com")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("file://fake_document"),
                                       GURL("http://www.a.com")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("chrome://fake_document"),
                                       GURL("https://www.a.com")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("file://fake_document"),
                                       GURL("https://www.a.com")));

  // A navigation going to a redirect url cannot be overridden, unless there's
  // no query or the path is not valid.
  EXPECT_FALSE(ShouldOverrideUrlLoading(
      GURL("http://www.google.com"), GURL("https://www.google.com/url?q=b")));
  EXPECT_FALSE(ShouldOverrideUrlLoading(
      GURL("https://www.a.com"), GURL("https://www.google.com/url?q=a")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(GURL("https://www.a.com"),
                                       GURL("https://www.google.com/url")));
  EXPECT_TRUE(ShouldOverrideUrlLoading(
      GURL("https://www.a.com"), GURL("https://www.google.com/link?q=a")));
}

}  // namespace apps
