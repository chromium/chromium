// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/site_pair_cache.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

constexpr char kHostA[] = "a.test";
constexpr char kHostB[] = "b.test";
constexpr char kHostC[] = "c.test";
constexpr char kHostASubdomain[] = "foo.a.test";
constexpr char kHostBSubdomain[] = "bar.b.test";
constexpr char kIPv4Addr[] = "127.0.0.1";
constexpr char kIPv6Addr[] = "::1";
constexpr char kLocalhost[] = "localhost";

url::Origin OriginFromHost(std::string_view host) {
  return url::Origin::Create(GURL(base::StrCat({"https://", host})));
}

}  // namespace

TEST(SitePairCacheTest, Insert) {
  url::Origin origin_a = OriginFromHost(kHostA);

  SitePairCache cache;
  EXPECT_TRUE(cache.Insert(origin_a, OriginFromHost(kHostB)));

  // A different <site, site> pair is not present yet.
  EXPECT_TRUE(cache.Insert(origin_a, OriginFromHost(kHostC)));
}

TEST(SitePairCacheTest, Insert_Subdomains) {
  url::Origin origin_a = OriginFromHost(kHostA);
  url::Origin origin_b = OriginFromHost(kHostB);
  url::Origin origin_a_sub = OriginFromHost(kHostASubdomain);
  url::Origin origin_b_sub = OriginFromHost(kHostBSubdomain);

  SitePairCache cache;
  EXPECT_TRUE(cache.Insert(origin_a, origin_b_sub));
  // All same-site variants of the <kHostA, kHostB> pair should already be
  // present in the cache.
  EXPECT_FALSE(cache.Insert(origin_a, origin_b_sub));
  EXPECT_FALSE(cache.Insert(origin_a, origin_b));
  EXPECT_FALSE(cache.Insert(origin_a_sub, origin_b));
  EXPECT_FALSE(cache.Insert(origin_a_sub, origin_b_sub));
}

TEST(SitePairCacheTest, Insert_Ordering) {
  url::Origin origin_a = OriginFromHost(kHostA);
  url::Origin origin_b = OriginFromHost(kHostB);

  SitePairCache cache;
  EXPECT_TRUE(cache.Insert(origin_a, origin_b));
  // Order of the keys matters:
  EXPECT_TRUE(cache.Insert(origin_b, origin_a));
}

TEST(SitePairCacheTest, Insert_Opaque) {
  url::Origin origin_a = OriginFromHost(kHostA);
  url::Origin opaque = url::Origin();

  SitePairCache cache;
  EXPECT_TRUE(cache.Insert(origin_a, opaque));
  EXPECT_FALSE(cache.Insert(origin_a, opaque));

  url::Origin origin_a_sub = OriginFromHost(kHostASubdomain);
  EXPECT_FALSE(cache.Insert(origin_a_sub, opaque));

  // Different opaque origins are handled independently.
  url::Origin opaque2 = url::Origin();
  EXPECT_TRUE(cache.Insert(origin_a, opaque2));
  EXPECT_FALSE(cache.Insert(origin_a, opaque2));
}

TEST(SitePairCacheTest, Insert_NoRegistrableDomain) {
  url::Origin v4 = OriginFromHost(kIPv4Addr);
  url::Origin v6 = OriginFromHost(kIPv6Addr);
  url::Origin localhost = OriginFromHost(kLocalhost);

  SitePairCache cache;
  EXPECT_TRUE(cache.Insert(v4, v6));
  EXPECT_FALSE(cache.Insert(v4, v6));

  EXPECT_TRUE(cache.Insert(v6, localhost));
}

TEST(SitePairCacheTest, Clear) {
  url::Origin origin_a = OriginFromHost(kHostA);
  url::Origin origin_b = OriginFromHost(kHostB);

  SitePairCache cache;
  ASSERT_TRUE(cache.Insert(origin_a, origin_b));
  ASSERT_FALSE(cache.Insert(origin_a, origin_b));

  cache.Clear();

  EXPECT_TRUE(cache.Insert(origin_a, origin_b));
}
