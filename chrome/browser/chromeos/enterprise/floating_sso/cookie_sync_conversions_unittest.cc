// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/enterprise/floating_sso/cookie_sync_conversions.h"

#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "base/strings/strcat.h"
#include "base/test/protobuf_matchers.h"
#include "base/time/time.h"
#include "components/sync/protocol/cookie_specifics.pb.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_partition_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos::floating_sso {

namespace {

constexpr char kNameForTests[] = "TestName";
constexpr char kValueForTests[] = "TestValue";
constexpr char kDomainForTests[] = "www.example.com";
constexpr char kPathForTests[] = "/baz";
constexpr char kTopLevelSiteForTesting[] = "https://toplevelsite.com";
constexpr char kUrlForTesting[] = "https://www.example.com/test/foo.html";
constexpr int kPortForTests = 19;

}  // namespace

// Verify that a cookie can be written to proto and then restored from it
// without loosing any data.
TEST(CookieSyncConversionsTest, CookieToProtoAndBack) {
  // Partition key is the hardest cookie field to serialize, so
  // make sure that we can handle both partitioned and non-partitioned
  // cookies.
  for (bool is_partitioned : {false, true}) {
    std::string cookie_line = base::StrCat(
        {kNameForTests, "=", kValueForTests, " Path=", kPathForTests,
         "; Secure", is_partitioned ? "; Partitioned;" : ""});
    base::Time creation_time = base::Time::Now();
    std::optional<base::Time> server_time = std::nullopt;
    auto partition_key = net::CookiePartitionKey::FromURLForTesting(
        GURL(kTopLevelSiteForTesting));
    std::unique_ptr<net::CanonicalCookie> cookie =
        net::CanonicalCookie::CreateForTesting(GURL(kUrlForTesting),
                                               cookie_line, creation_time,
                                               server_time, partition_key);

    ASSERT_TRUE(cookie);
    ASSERT_EQ(cookie->IsPartitioned(), is_partitioned);

    std::optional<sync_pb::CookieSpecifics> sync_specifics =
        ToSyncProto(*cookie);

    ASSERT_TRUE(sync_specifics.has_value());

    std::unique_ptr<net::CanonicalCookie> restored_cookie =
        FromSyncProto(*sync_specifics);

    ASSERT_TRUE(restored_cookie);

    EXPECT_TRUE(restored_cookie->HasEquivalentDataMembers(*cookie));
  }
}

// Verify that reading a cookie from Sync proto and then writing it back
// without changes results in the same proto.
TEST(CookieSyncConversionsTest, ProtoToCookieAndBack) {
  sync_pb::CookieSpecifics sync_specifics;
  sync_specifics.set_unique_key(
      "https://toplevelsite.comtrueTestNamewww.example.com/baz219");
  sync_specifics.set_name(kNameForTests);
  sync_specifics.set_value(kValueForTests);
  sync_specifics.set_domain(kDomainForTests);
  sync_specifics.set_path(kPathForTests);
  sync_specifics.set_creation_time_windows_epoch_micros(13357418862798591);
  sync_specifics.set_expiry_time_windows_epoch_micros(0);
  sync_specifics.set_last_access_time_windows_epoch_micros(13357418862798591);
  sync_specifics.set_last_update_time_windows_epoch_micros(13357418862799017);
  sync_specifics.set_secure(true);
  sync_specifics.set_httponly(false);
  sync_specifics.set_site_restrictions(
      sync_pb::CookieSpecifics_CookieSameSite_UNSPECIFIED);
  sync_specifics.set_priority(sync_pb::CookieSpecifics_CookiePriority_MEDIUM);
  sync_specifics.set_source_scheme(
      sync_pb::CookieSpecifics_CookieSourceScheme_SECURE);
  sync_specifics.mutable_partition_key()->set_top_level_site(
      kTopLevelSiteForTesting);
  sync_specifics.mutable_partition_key()->set_has_cross_site_ancestor(true);
  sync_specifics.set_source_port(kPortForTests);
  sync_specifics.set_source_type(
      sync_pb::CookieSpecifics_CookieSourceType_HTTP);

  std::unique_ptr<net::CanonicalCookie> cookie = FromSyncProto(sync_specifics);

  ASSERT_TRUE(cookie);

  std::optional<sync_pb::CookieSpecifics> restored_specifics =
      ToSyncProto(*cookie);

  ASSERT_TRUE(restored_specifics);

  EXPECT_THAT(sync_specifics, base::test::EqualsProto(*restored_specifics));
}

// Verify that a cookie with non-serializable partition key can't be
// saved in a Sync proto.
TEST(CookieSyncConversionsTest, PartitionKeyShouldBeSerializable) {
  std::string cookie_line = std::string(kNameForTests) + "=" + kValueForTests +
                            "; Partitioned;" + " Path=" + kPathForTests +
                            "; Secure";
  base::Time creation_time = base::Time::Now();
  std::optional<base::Time> server_time = std::nullopt;
  // Partition key with a nonce can't be serialized.
  auto partition_key_with_nonce =
      std::make_optional(net::CookiePartitionKey::FromURLForTesting(
          GURL(kTopLevelSiteForTesting),
          net::CookiePartitionKey::AncestorChainBit::kCrossSite,
          base::UnguessableToken::Create()));
  net::CookieInclusionStatus status;
  std::unique_ptr<net::CanonicalCookie> cookie = net::CanonicalCookie::Create(
      GURL(kUrlForTesting), cookie_line, creation_time, server_time,
      partition_key_with_nonce, /*block_truncated=*/true,
      net::CookieSourceType::kHTTP, &status);

  ASSERT_TRUE(cookie);

  std::optional<sync_pb::CookieSpecifics> sync_specifics = ToSyncProto(*cookie);

  EXPECT_FALSE(sync_specifics.has_value());
}

// Verify that we don't build a CanonicalCookie object if the cookie
// saved in Sync proto is not canonical.
TEST(CookieSyncConversionsTest, NonCanonicalCookieInProto) {
  sync_pb::CookieSpecifics sync_specifics;
  sync_specifics.set_unique_key(
      "https://toplevelsite.comtrueTestNamewww.example.com/baz219");
  sync_specifics.set_name(kNameForTests);
  sync_specifics.set_value(kValueForTests);
  sync_specifics.set_domain(kDomainForTests);
  sync_specifics.set_path(kPathForTests);
  // Set creation time to null and last_access time to some valid value
  // - this should result in a failure when trying to build a
  // CanonicalCookie object.
  sync_specifics.set_creation_time_windows_epoch_micros(0);
  sync_specifics.set_expiry_time_windows_epoch_micros(0);
  sync_specifics.set_last_access_time_windows_epoch_micros(13357418862798591);
  sync_specifics.set_last_update_time_windows_epoch_micros(13357418862799017);
  sync_specifics.set_secure(true);
  sync_specifics.set_httponly(false);
  sync_specifics.set_site_restrictions(
      sync_pb::CookieSpecifics_CookieSameSite_UNSPECIFIED);
  sync_specifics.set_priority(sync_pb::CookieSpecifics_CookiePriority_MEDIUM);
  sync_specifics.set_source_scheme(
      sync_pb::CookieSpecifics_CookieSourceScheme_SECURE);
  sync_specifics.mutable_partition_key()->set_top_level_site(
      kTopLevelSiteForTesting);
  sync_specifics.mutable_partition_key()->set_has_cross_site_ancestor(true);
  sync_specifics.set_source_port(kPortForTests);
  sync_specifics.set_source_type(
      sync_pb::CookieSpecifics_CookieSourceType_HTTP);

  std::unique_ptr<net::CanonicalCookie> cookie = FromSyncProto(sync_specifics);

  EXPECT_FALSE(cookie);
}

}  // namespace chromeos::floating_sso
