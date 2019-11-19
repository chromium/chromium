// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cookies/cookies_helpers.h"

#include <limits>
#include <memory>

#include "net/cookies/canonical_cookie.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

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
  net::CanonicalCookie cookie("cookiename", "cookievalue", "example.com", "/",
                              base::Time::Now(), kExpirationDate, base::Time(),
                              false, false, net::CookieSameSite::NO_RESTRICTION,
                              net::COOKIE_PRIORITY_DEFAULT);

  // Serialize the cookie to JSON. We need to gracefully handle the infinite
  // expiration date, which should be converted to the maximum value.
  api::cookies::Cookie serialized_cookie =
      cookies_helpers::CreateCookie(cookie, "1");
  std::unique_ptr<base::Value> value_cookie = serialized_cookie.ToValue();
  ASSERT_TRUE(value_cookie);
  base::Value* expiration_time =
      value_cookie->FindKeyOfType("expirationDate", base::Value::Type::DOUBLE);
  ASSERT_TRUE(expiration_time);
  EXPECT_EQ(std::numeric_limits<double>::max(), expiration_time->GetDouble());
}

}  // namespace extensions
