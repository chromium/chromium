// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/cookie_access_filter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(CookieAccessFilter, NoAccesses) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;

  std::vector<size_t> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre());
}

TEST(CookieAccessFilter, OneRead_Former) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieAccessFilter::Type::kRead);

  std::vector<size_t> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(0));
}

TEST(CookieAccessFilter, OneRead_Latter) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url2, CookieAccessFilter::Type::kRead);

  std::vector<size_t> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(1));
}

TEST(CookieAccessFilter, OneWrite) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url2, CookieAccessFilter::Type::kChange);

  std::vector<size_t> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(1));
}

TEST(CookieAccessFilter, UnexpectedURL) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(GURL("http://other.com"), CookieAccessFilter::Type::kRead);

  std::vector<size_t> result;
  ASSERT_FALSE(filter.Filter({url1, url2}, &result));
}

TEST(CookieAccessFilter, TwoReads) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieAccessFilter::Type::kRead);
  filter.AddAccess(url2, CookieAccessFilter::Type::kRead);

  std::vector<size_t> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(0, 1));
}

TEST(CookieAccessFilter, CoalesceReadAndWrite) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieAccessFilter::Type::kRead);
  filter.AddAccess(url1, CookieAccessFilter::Type::kChange);
  filter.AddAccess(url2, CookieAccessFilter::Type::kRead);

  std::vector<size_t> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(0, 1));
}

TEST(CookieAccessFilter, CantCoalesceMultipleWrites) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieAccessFilter::Type::kChange);
  filter.AddAccess(url1, CookieAccessFilter::Type::kChange);
  filter.AddAccess(url2, CookieAccessFilter::Type::kRead);

  std::vector<size_t> result;
  ASSERT_FALSE(filter.Filter({url1, url2}, &result));
}

TEST(CookieAccessFilter, CantCoalesceMultipleReads) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieAccessFilter::Type::kRead);
  filter.AddAccess(url1, CookieAccessFilter::Type::kRead);
  filter.AddAccess(url2, CookieAccessFilter::Type::kRead);

  std::vector<size_t> result;
  ASSERT_FALSE(filter.Filter({url1, url2}, &result));
}

TEST(CookieAccessFilter, CantCoalesceWriteBeforeRead) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieAccessFilter::Type::kChange);
  filter.AddAccess(url1, CookieAccessFilter::Type::kRead);
  filter.AddAccess(url2, CookieAccessFilter::Type::kRead);

  std::vector<size_t> result;
  ASSERT_FALSE(filter.Filter({url1, url2}, &result));
}
