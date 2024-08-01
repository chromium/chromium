// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_SSO_COOKIE_SYNC_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_FLOATING_SSO_COOKIE_SYNC_TEST_UTIL_H_

#include <array>
#include <cstdint>
#include <string>

namespace sync_pb {
class CookieSpecifics;
}

namespace ash::floating_sso {

extern const std::array<std::string, 4> kUniqueKeysForTests;
extern const std::array<std::string, 4> kNamesForTests;
inline constexpr char kValueForTests[] = "TestValue";
inline constexpr char kDomainForTests[] = "www.example.com";
inline constexpr char kPathForTests[] = "/baz";
inline constexpr char kTopLevelSiteForTesting[] = "https://toplevelsite.com";
inline constexpr char kUrlForTesting[] =
    "https://www.example.com/test/foo.html";
// 2024-04-12 18:07:42.798591 UTC, in microseconds from Windows epoch
inline constexpr int64_t kCreationTimeForTesting = 13357418862798591;
// 2024-04-12 18:07:42.799017 UTC, in microseconds from Windows epoch
inline constexpr int64_t kLastUpdateTimeForTesting = 13357418862799017;
inline constexpr int kPortForTests = 19;

// Returns a cookie proto with a name `kNamesForTests[i]` and a key
// `kUniqueKeysForTests[i]`, other fields will always be the same regardless of
// the value of `i`.
sync_pb::CookieSpecifics CreatePredefinedCookieSpecificsForTest(size_t i = 0);
// Creates a cookie proto with the provided unique key and name.
sync_pb::CookieSpecifics CreateCookieSpecificsForTest(
    const std::string& unique_key,
    const std::string& name);

}  // namespace ash::floating_sso

#endif  // CHROME_BROWSER_ASH_FLOATING_SSO_COOKIE_SYNC_TEST_UTIL_H_
