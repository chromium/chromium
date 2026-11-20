// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/auxiliary_search/auxiliary_search_provider.h"

#include <stdint.h>

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// The expected visit IDs below are persisted as AppSearch document IDs, so they
// must not change. They are also larger than INT32_MAX, which verifies that the
// visit IDs aren't truncated to 32 bits.

TEST(AuxiliarySearchProviderTest, GetCustomTabVisitId) {
  const std::string url_key = "https://www.example.com/home";
  EXPECT_EQ(INT64_C(8420259065473389252),
            AuxiliarySearchProvider::GetCustomTabVisitId(url_key));
  EXPECT_NE(AuxiliarySearchProvider::GetCustomTabVisitId(url_key),
            AuxiliarySearchProvider::GetCustomTabVisitId(
                "https://www.example.com/login"));
}

TEST(AuxiliarySearchProviderTest, GetTopSiteVisitId) {
  const GURL url("https://www.example.com/home");
  const std::u16string title = u"Example Page";
  EXPECT_EQ(INT64_C(4855622617872007849),
            AuxiliarySearchProvider::GetTopSiteVisitId(url, title));
  EXPECT_NE(AuxiliarySearchProvider::GetTopSiteVisitId(url, title),
            AuxiliarySearchProvider::GetTopSiteVisitId(url, u"Other Page"));

  // The UTF-16 code unit U+6261 has the same bytes as "ab", so without the
  // length prefixes both entries below would hash the same byte sequence.
  EXPECT_NE(AuxiliarySearchProvider::GetTopSiteVisitId(
                GURL("https://example.com/ab"), std::u16string()),
            AuxiliarySearchProvider::GetTopSiteVisitId(
                GURL("https://example.com/"), u"\u6261"));
}
