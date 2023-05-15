// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/cookie_access_filter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(CookieAccessType, BitwiseOrOperator) {
  ASSERT_EQ(SiteDataAccessType::kRead,
            SiteDataAccessType::kNone | SiteDataAccessType::kRead);

  ASSERT_EQ(SiteDataAccessType::kWrite,
            SiteDataAccessType::kNone | SiteDataAccessType::kWrite);

  ASSERT_EQ(SiteDataAccessType::kReadWrite,
            SiteDataAccessType::kRead | SiteDataAccessType::kWrite);

  ASSERT_EQ(SiteDataAccessType::kUnknown,
            SiteDataAccessType::kUnknown | SiteDataAccessType::kNone);

  ASSERT_EQ(SiteDataAccessType::kUnknown,
            SiteDataAccessType::kUnknown | SiteDataAccessType::kRead);

  ASSERT_EQ(SiteDataAccessType::kUnknown,
            SiteDataAccessType::kUnknown | SiteDataAccessType::kWrite);
}

TEST(CookieAccessFilter, NoAccesses) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;

  std::vector<SiteDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(SiteDataAccessType::kNone,
                                           SiteDataAccessType::kNone));
}

TEST(CookieAccessFilter, OneRead_Former) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kRead);

  std::vector<SiteDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(SiteDataAccessType::kRead,
                                           SiteDataAccessType::kNone));
}

TEST(CookieAccessFilter, OneRead_Latter) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<SiteDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(SiteDataAccessType::kNone,
                                           SiteDataAccessType::kRead));
}

TEST(CookieAccessFilter, OneWrite) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url2, CookieOperation::kChange);

  std::vector<SiteDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(SiteDataAccessType::kNone,
                                           SiteDataAccessType::kWrite));
}

TEST(CookieAccessFilter, UnexpectedURL) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(GURL("http://other.com"), CookieOperation::kRead);

  std::vector<SiteDataAccessType> result;
  ASSERT_FALSE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(SiteDataAccessType::kUnknown,
                                           SiteDataAccessType::kUnknown));
}

TEST(CookieAccessFilter, TwoReads) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<SiteDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(SiteDataAccessType::kRead,
                                           SiteDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceReadBeforeWrite) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<SiteDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(SiteDataAccessType::kReadWrite,
                                           SiteDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceReadBeforeWrite_Repeated) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<SiteDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(SiteDataAccessType::kReadWrite,
                                           SiteDataAccessType::kReadWrite,
                                           SiteDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceWrites) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<SiteDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(SiteDataAccessType::kWrite,
                                           SiteDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceWrites_Repeated) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<SiteDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(SiteDataAccessType::kWrite,
                                           SiteDataAccessType::kWrite,
                                           SiteDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceReads) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<SiteDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(SiteDataAccessType::kRead,
                                           SiteDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceReads_Repeated) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<SiteDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(SiteDataAccessType::kRead,
                                           SiteDataAccessType::kRead,
                                           SiteDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceWriteBeforeRead) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<SiteDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(SiteDataAccessType::kReadWrite,
                                           SiteDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceWriteBeforeRead_Repeated) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<SiteDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(SiteDataAccessType::kReadWrite,
                                           SiteDataAccessType::kReadWrite,
                                           SiteDataAccessType::kRead));
}

TEST(CookieAccessFilter, SameURLTwiceWithDifferentAccessTypes) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url2, CookieOperation::kRead);
  filter.AddAccess(url2, CookieOperation::kChange);
  filter.AddAccess(url1, CookieOperation::kRead);

  std::vector<SiteDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2, url1}, &result));
  EXPECT_THAT(result, testing::ElementsAre(SiteDataAccessType::kWrite,
                                           SiteDataAccessType::kReadWrite,
                                           SiteDataAccessType::kRead));
}
