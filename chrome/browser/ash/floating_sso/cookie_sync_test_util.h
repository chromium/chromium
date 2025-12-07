// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FLOATING_SSO_COOKIE_SYNC_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_FLOATING_SSO_COOKIE_SYNC_TEST_UTIL_H_

#include <array>
#include <cstdint>
#include <string>

namespace base {
class Time;
}

namespace sync_pb {
class CookieSpecifics;
}

namespace syncer {
struct EntityData;
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
inline constexpr int kPortForTests = 19;

// Returns a cookie proto with a name `kNamesForTests[i]` and a key
// `kUniqueKeysForTests[i]`, other fields don't depend on the value of `i`. By
// default returns a session cookie, setting `persistent` to true will make it
// return a persistent cookie which expires in 10 days after `creation_time`.
sync_pb::CookieSpecifics CreatePredefinedCookieSpecificsForTest(
    size_t i,
    const base::Time& creation_time,
    bool persistent = false);

// Creates a cookie proto with the provided unique key and name. By default
// returns a session cookie, setting `persistent` to true will make it return a
// persistent cookie which expires in 10 days after `creation_time`.
sync_pb::CookieSpecifics CreateCookieSpecificsForTest(
    const std::string& unique_key,
    const std::string& name,
    const base::Time& creation_time,
    bool persistent = false,
    const std::string& domain = kDomainForTests);

syncer::EntityData CreateEntityDataForTest(
    const sync_pb::CookieSpecifics& specifics);

}  // namespace ash::floating_sso

#endif  // CHROME_BROWSER_ASH_FLOATING_SSO_COOKIE_SYNC_TEST_UTIL_H_
