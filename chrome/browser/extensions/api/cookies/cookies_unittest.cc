// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests common functionality used by the Chrome Extensions Cookies API
// implementation.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/optional.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/cookies/cookies_api_constants.h"
#include "chrome/browser/extensions/api/cookies/cookies_helpers.h"
#include "chrome/common/extensions/api/cookies.h"
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

namespace keys = cookies_api_constants;

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
  EXPECT_TRUE(profile->GetOffTheRecordProfile());

  EXPECT_EQ(std::string("0"),
            cookies_helpers::GetStoreIdFromProfile(profile.get()));
  EXPECT_EQ(profile.get(),
            cookies_helpers::ChooseProfileFromStoreId(
                "0", profile.get(), true));
  EXPECT_EQ(profile.get(),
            cookies_helpers::ChooseProfileFromStoreId(
                "0", profile.get(), false));
  EXPECT_EQ(profile->GetOffTheRecordProfile(),
            cookies_helpers::ChooseProfileFromStoreId(
                "1", profile.get(), true));
  EXPECT_EQ(NULL,
            cookies_helpers::ChooseProfileFromStoreId(
                "1", profile.get(), false));

  EXPECT_EQ(std::string("1"),
            cookies_helpers::GetStoreIdFromProfile(
                profile->GetOffTheRecordProfile()));
  EXPECT_EQ(NULL,
            cookies_helpers::ChooseProfileFromStoreId(
                "0", profile->GetOffTheRecordProfile(), true));
  EXPECT_EQ(NULL,
            cookies_helpers::ChooseProfileFromStoreId(
                "0", profile->GetOffTheRecordProfile(), false));
  EXPECT_EQ(profile->GetOffTheRecordProfile(),
            cookies_helpers::ChooseProfileFromStoreId(
                "1", profile->GetOffTheRecordProfile(), true));
  EXPECT_EQ(profile->GetOffTheRecordProfile(),
            cookies_helpers::ChooseProfileFromStoreId(
                "1", profile->GetOffTheRecordProfile(), false));
}

TEST_F(ExtensionCookiesTest, ExtensionTypeCreation) {
  std::unique_ptr<net::CanonicalCookie> canonical_cookie1(
      std::make_unique<net::CanonicalCookie>(
          "ABC", "DEF", "www.example.com", "/", base::Time(), base::Time(),
          base::Time(), false, false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_DEFAULT));
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
  EXPECT_EQ(api::cookies::SAME_SITE_STATUS_NO_RESTRICTION, cookie1.same_site);
  EXPECT_TRUE(cookie1.session);
  EXPECT_FALSE(cookie1.expiration_date.get());
  EXPECT_EQ("some cookie store", cookie1.store_id);

  std::unique_ptr<net::CanonicalCookie> canonical_cookie2(
      std::make_unique<net::CanonicalCookie>(
          "ABC", "DEF", ".example.com", "/", base::Time(),
          base::Time::FromDoubleT(10000), base::Time(), false, false,
          net::CookieSameSite::STRICT_MODE, net::COOKIE_PRIORITY_DEFAULT));
  ASSERT_NE(nullptr, canonical_cookie2.get());
  Cookie cookie2 =
      cookies_helpers::CreateCookie(*canonical_cookie2, "some cookie store");
  EXPECT_FALSE(cookie2.host_only);
  EXPECT_FALSE(cookie2.session);
  EXPECT_EQ(api::cookies::SAME_SITE_STATUS_STRICT, cookie2.same_site);
  ASSERT_TRUE(cookie2.expiration_date.get());
  EXPECT_EQ(10000, *cookie2.expiration_date);

  TestingProfile profile;
  auto tab_ids_list = std::make_unique<base::ListValue>();
  std::vector<int> tab_ids;
  CookieStore cookie_store =
      cookies_helpers::CreateCookieStore(&profile, std::move(tab_ids_list));
  EXPECT_EQ("0", cookie_store.id);
  EXPECT_EQ(tab_ids, cookie_store.tab_ids);
}

TEST_F(ExtensionCookiesTest, GetURLFromCanonicalCookie) {
  std::unique_ptr<net::CanonicalCookie> cookie1(
      std::make_unique<net::CanonicalCookie>(
          "ABC", "DEF", ".example.com", "/", base::Time(), base::Time(),
          base::Time(), false, false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_DEFAULT));
  ASSERT_NE(nullptr, cookie1.get());
  EXPECT_EQ("http://example.com/",
            cookies_helpers::GetURLFromCanonicalCookie(*cookie1).spec());

  std::unique_ptr<net::CanonicalCookie> cookie2(
      std::make_unique<net::CanonicalCookie>(
          "ABC", "DEF", ".helloworld.com", "/", base::Time(), base::Time(),
          base::Time(), true, false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_DEFAULT));
  ASSERT_NE(nullptr, cookie2.get());
  EXPECT_EQ("https://helloworld.com/",
            cookies_helpers::GetURLFromCanonicalCookie(*cookie2).spec());
}

TEST_F(ExtensionCookiesTest, EmptyDictionary) {
  base::DictionaryValue dict;
  GetAll::Params::Details details;
  bool rv = GetAll::Params::Details::Populate(dict, &details);
  ASSERT_TRUE(rv);
  cookies_helpers::MatchFilter filter(&details);
  net::CanonicalCookie cookie;
  EXPECT_TRUE(filter.MatchesCookie(cookie));
}

TEST_F(ExtensionCookiesTest, DomainMatching) {
  const DomainMatchCase tests[] = {
      {"bar.com", "bar.com", true},       {".bar.com", "bar.com", true},
      {"bar.com", "food.bar.com", true},  {"bar.com", "bar.foo.com", false},
      {".bar.com", ".foo.bar.com", true}, {".bar.com", "baz.foo.bar.com", true},
      {"foo.bar.com", ".bar.com", false}};

  for (size_t i = 0; i < base::size(tests); ++i) {
    // Build up the Params struct.
    base::ListValue args;
    auto dict = std::make_unique<base::DictionaryValue>();
    dict->SetString(keys::kDomainKey, std::string(tests[i].filter));
    args.Set(0, std::move(dict));
    std::unique_ptr<GetAll::Params> params(GetAll::Params::Create(args));

    cookies_helpers::MatchFilter filter(&params->details);
    std::unique_ptr<net::CanonicalCookie> cookie(
        std::make_unique<net::CanonicalCookie>(
            "name", std::string(), tests[i].domain, "/", base::Time(),
            base::Time(), base::Time(), false, false,
            net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT));
    ASSERT_NE(nullptr, cookie.get());
    EXPECT_EQ(tests[i].matches, filter.MatchesCookie(*cookie)) << " test " << i;
  }
}

TEST_F(ExtensionCookiesTest, DecodeUTF8WithErrorHandling) {
  std::unique_ptr<net::CanonicalCookie> canonical_cookie(
      net::CanonicalCookie::Create(
          GURL("http://test.com"), "=011Q255bNX_1!yd\203e+;path=/path\203",
          base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_NE(nullptr, canonical_cookie.get());
  Cookie cookie =
      cookies_helpers::CreateCookie(*canonical_cookie, "some cookie store");
  EXPECT_EQ(std::string("011Q255bNX_1!yd\xEF\xBF\xBD"
                        "e+"),
            cookie.value);
  EXPECT_EQ(std::string(), cookie.path);
}

}  // namespace extensions
