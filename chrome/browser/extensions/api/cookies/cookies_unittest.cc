// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// Tests common functionality used by the Chrome Extensions Cookies API
// implementation.

#include "chrome/common/extensions/api/cookies.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/test/gtest_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/cookies/cookies_helpers.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using extensions::api::cookies::Cookie;
using extensions::api::cookies::CookieStore;

namespace GetAll = extensions::api::cookies::GetAll;

namespace extensions {

constexpr char kDomainKey[] = "domain";

namespace {

struct DomainMatchCase {
  const char* filter;
  const char* domain;
  const bool matches;
};

}  // namespace

class ExtensionCookiesTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ExtensionCookiesTest, StoreIdProfileConversion) {
  TestingProfile::Builder profile_builder;
  std::unique_ptr<TestingProfile> profile = profile_builder.Build();
  // Trigger early creation of off-the-record profile.
  EXPECT_TRUE(profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  EXPECT_EQ(std::string("0"),
            cookies_helpers::GetStoreIdFromProfile(profile.get()));
  EXPECT_EQ(profile.get(),
            cookies_helpers::ChooseProfileFromStoreId(
                "0", profile.get(), true));
  EXPECT_EQ(profile.get(),
            cookies_helpers::ChooseProfileFromStoreId(
                "0", profile.get(), false));
  EXPECT_EQ(
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      cookies_helpers::ChooseProfileFromStoreId("1", profile.get(), true));
  EXPECT_EQ(nullptr, cookies_helpers::ChooseProfileFromStoreId(
                         "1", profile.get(), false));

  EXPECT_EQ(std::string("1"),
            cookies_helpers::GetStoreIdFromProfile(
                profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
  EXPECT_EQ(
      nullptr,
      cookies_helpers::ChooseProfileFromStoreId(
          "0", profile->GetPrimaryOTRProfile(/*create_if_needed=*/true), true));
  EXPECT_EQ(nullptr,
            cookies_helpers::ChooseProfileFromStoreId(
                "0", profile->GetPrimaryOTRProfile(/*create_if_needed=*/true),
                false));
  EXPECT_EQ(
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      cookies_helpers::ChooseProfileFromStoreId(
          "1", profile->GetPrimaryOTRProfile(/*create_if_needed=*/true), true));
  EXPECT_EQ(profile->GetPrimaryOTRProfile(/*create_if_needed=*/true),
            cookies_helpers::ChooseProfileFromStoreId(
                "1", profile->GetPrimaryOTRProfile(/*create_if_needed=*/true),
                false));
}

TEST_F(ExtensionCookiesTest, ExtensionTypeCreation) {
  std::unique_ptr<net::CanonicalCookie> canonical_cookie1 =
      net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "ABC", "DEF", "www.example.com", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), false, false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT);
  ASSERT_NE(nullptr, canonical_cookie1.get());
  Cookie cookie1 =
      cookies_helpers::CreateCookie(*canonical_cookie1, "some cookie store");
  EXPECT_EQ("ABC", cookie1.name);
  EXPECT_EQ("DEF", cookie1.value);
  EXPECT_EQ("www.example.com", cookie1.domain);
  EXPECT_TRUE(cookie1.host_only);
  EXPECT_EQ("/", cookie1.path);
  EXPECT_FALSE(cookie1.secure);
  EXPECT_FALSE(cookie1.http_only);
  EXPECT_EQ(api::cookies::SameSiteStatus::kNoRestriction, cookie1.same_site);
  EXPECT_TRUE(cookie1.session);
  EXPECT_FALSE(cookie1.expiration_date);
  EXPECT_EQ("some cookie store", cookie1.store_id);

  std::unique_ptr<net::CanonicalCookie> canonical_cookie2 =
      net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "ABC", "DEF", ".example.com", "/", base::Time(),
          base::Time::FromSecondsSinceUnixEpoch(10000), base::Time(),
          base::Time(), false, false, net::CookieSameSite::STRICT_MODE,
          net::COOKIE_PRIORITY_DEFAULT);
  ASSERT_NE(nullptr, canonical_cookie2.get());
  Cookie cookie2 =
      cookies_helpers::CreateCookie(*canonical_cookie2, "some cookie store");
  EXPECT_FALSE(cookie2.host_only);
  EXPECT_FALSE(cookie2.session);
  EXPECT_EQ(api::cookies::SameSiteStatus::kStrict, cookie2.same_site);
  ASSERT_TRUE(cookie2.expiration_date);
  EXPECT_EQ(10000, *cookie2.expiration_date);

  TestingProfile profile;
  base::Value::List tab_ids_list;
  std::vector<int> tab_ids;
  CookieStore cookie_store =
      cookies_helpers::CreateCookieStore(&profile, std::move(tab_ids_list));
  EXPECT_EQ("0", cookie_store.id);
  EXPECT_EQ(tab_ids, cookie_store.tab_ids);
}

TEST_F(ExtensionCookiesTest, GetURLFromCanonicalCookie) {
  std::unique_ptr<net::CanonicalCookie> cookie1 =
      net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "ABC", "DEF", ".example.com", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), false, false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT);
  ASSERT_NE(nullptr, cookie1.get());
  EXPECT_EQ("http://example.com/",
            cookies_helpers::GetURLFromCanonicalCookie(*cookie1).spec());

  std::unique_ptr<net::CanonicalCookie> cookie2 =
      net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "ABC", "DEF", ".helloworld.com", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), true, false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT);
  ASSERT_NE(nullptr, cookie2.get());
  EXPECT_EQ("https://helloworld.com/",
            cookies_helpers::GetURLFromCanonicalCookie(*cookie2).spec());
}

TEST_F(ExtensionCookiesTest, EmptyDictionary) {
  base::Value::Dict dict;
  auto details = GetAll::Params::Details::FromValue(dict);
  ASSERT_TRUE(details);
  cookies_helpers::MatchFilter filter(&details.value());
  net::CanonicalCookie cookie;
  EXPECT_TRUE(filter.MatchesCookie(cookie));
}

TEST_F(ExtensionCookiesTest, DomainMatching) {
  static constexpr DomainMatchCase tests[] = {
      {"bar.com", "bar.com", true},       {".bar.com", "bar.com", true},
      {"bar.com", "food.bar.com", true},  {"bar.com", "bar.foo.com", false},
      {".bar.com", ".foo.bar.com", true}, {".bar.com", "baz.foo.bar.com", true},
      {"foo.bar.com", ".bar.com", false}};

  for (size_t i = 0; i < std::size(tests); ++i) {
    // Build up the Params struct.
    base::Value::List args;
    base::Value::Dict dict;
    dict.Set(kDomainKey, tests[i].filter);
    args.Append(std::move(dict));
    std::optional<GetAll::Params> params = GetAll::Params::Create(args);

    cookies_helpers::MatchFilter filter(&params->details);
    std::unique_ptr<net::CanonicalCookie> cookie =
        net::CanonicalCookie::CreateUnsafeCookieForTesting(
            "name", std::string(), tests[i].domain, "/", base::Time(),
            base::Time(), base::Time(), base::Time(), false, false,
            net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT);
    ASSERT_NE(nullptr, cookie.get());
    EXPECT_EQ(tests[i].matches, filter.MatchesCookie(*cookie)) << " test " << i;
  }
}

TEST_F(ExtensionCookiesTest, DecodeUTF8WithErrorHandling) {
  std::unique_ptr<net::CanonicalCookie> canonical_cookie(
      net::CanonicalCookie::CreateForTesting(
          GURL("http://test.com"), "=011Q255bNX_1!yd\203e+;path=/path\203",
          base::Time::Now()));
  ASSERT_NE(nullptr, canonical_cookie.get());
  Cookie cookie =
      cookies_helpers::CreateCookie(*canonical_cookie, "some cookie store");
  EXPECT_EQ(std::string("011Q255bNX_1!yd\xEF\xBF\xBD"
                        "e+"),
            cookie.value);
  EXPECT_EQ(std::string(), cookie.path);
}

TEST_F(ExtensionCookiesTest, PartitionKeySerialization) {
  std::string top_level_site = "https://toplevelsite.com";
  std::optional<extensions::api::cookies::CookiePartitionKey>
      partition_key_for_nonce_and_regular =
          extensions::api::cookies::CookiePartitionKey();
  std::optional<extensions::api::cookies::CookiePartitionKey>
      partition_key_for_opaque = extensions::api::cookies::CookiePartitionKey();
  partition_key_for_nonce_and_regular->top_level_site = top_level_site;
  partition_key_for_opaque->top_level_site = "";

  // Partition key to confirm crbug.com/1522601 is addressed.
  std::optional<extensions::api::cookies::CookiePartitionKey>
      partition_key_with_no_top_level_site_set =
          extensions::api::cookies::CookiePartitionKey();

  // Make a CanonicalCookie with a opaque top_level_site or nonce in partition
  // key.
  auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "__Host-A", "B", "x.y", "/", base::Time(), base::Time(), base::Time(),
      base::Time(), /*secure=*/true,
      /*httponly=*/false, net::CookieSameSite::UNSPECIFIED,
      net::COOKIE_PRIORITY_LOW,
      net::CookiePartitionKey::FromURLForTesting(GURL(top_level_site)));
  EXPECT_TRUE(cookie->IsPartitioned());
  EXPECT_FALSE(net::CookiePartitionKey::HasNonce(cookie->PartitionKey()));
  EXPECT_TRUE(cookie->PartitionKey()->IsSerializeable());

  // Make a CanonicalCookie with a opaque partition key.
  auto opaque_cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "__Host-A", "B", "x.y", "/", base::Time(), base::Time(), base::Time(),
      base::Time(), /*secure=*/true,
      /*httponly=*/false, net::CookieSameSite::UNSPECIFIED,
      net::COOKIE_PRIORITY_LOW,
      net::CookiePartitionKey::FromURLForTesting(GURL()));

  EXPECT_TRUE(opaque_cookie->IsPartitioned());
  EXPECT_FALSE(opaque_cookie->PartitionKey()->IsSerializeable());

  // Make a CanonicalCookie with an nonce partition key.
  auto nonce_cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "__Host-A", "B", "x.y", "/", base::Time(), base::Time(), base::Time(),
      base::Time(), /*secure=*/true,
      /*httponly=*/false, net::CookieSameSite::UNSPECIFIED,
      net::COOKIE_PRIORITY_LOW,
      net::CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite.com"),
          net::CookiePartitionKey::AncestorChainBit::kCrossSite,
          base::UnguessableToken::Create()));

  EXPECT_TRUE(nonce_cookie->IsPartitioned());
  EXPECT_TRUE(net::CookiePartitionKey::HasNonce(nonce_cookie->PartitionKey()));
  EXPECT_FALSE(nonce_cookie->PartitionKey()->IsSerializeable());

  // Confirm that to be matchable, the partition key
  // must be serializable.
  EXPECT_TRUE(
      cookies_helpers::CanonicalCookiePartitionKeyMatchesApiCookiePartitionKey(
          partition_key_for_nonce_and_regular, *cookie->PartitionKey()));
  EXPECT_FALSE(
      cookies_helpers::CanonicalCookiePartitionKeyMatchesApiCookiePartitionKey(
          partition_key_for_nonce_and_regular, *nonce_cookie->PartitionKey()));
  EXPECT_FALSE(
      cookies_helpers::CanonicalCookiePartitionKeyMatchesApiCookiePartitionKey(
          partition_key_for_opaque, *opaque_cookie->PartitionKey()));
  EXPECT_FALSE(
      cookies_helpers::CanonicalCookiePartitionKeyMatchesApiCookiePartitionKey(
          partition_key_with_no_top_level_site_set, *cookie->PartitionKey()));
  EXPECT_FALSE(
      cookies_helpers::CanonicalCookiePartitionKeyMatchesApiCookiePartitionKey(
          partition_key_with_no_top_level_site_set,
          *nonce_cookie->PartitionKey()));
  EXPECT_FALSE(
      cookies_helpers::CanonicalCookiePartitionKeyMatchesApiCookiePartitionKey(
          partition_key_with_no_top_level_site_set,
          *opaque_cookie->PartitionKey()));

  // Confirm that a CanonicalCookie with serializable partition key
  // can be used to create a cookie.
  auto api_cookie = cookies_helpers::CreateCookie(*cookie, "0");
  EXPECT_TRUE(api_cookie.partition_key);

  // Confirm that a CanonicalCookie with a non-serializable partition key
  // dies when a cookie is attempted to be created.
  EXPECT_CHECK_DEATH(cookies_helpers::CreateCookie(*nonce_cookie, "0"));
  EXPECT_CHECK_DEATH(cookies_helpers::CreateCookie(*opaque_cookie, "0"));
}

}  // namespace extensions
