// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_sso/cookie_sync_test_util.h"

#include <array>
#include <string>

#include "base/check.h"
#include "base/time/time.h"
#include "chrome/browser/ash/floating_sso/cookie_sync_conversions.h"
#include "components/sync/protocol/cookie_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"

namespace ash::floating_sso {

const std::array<std::string, 4> kUniqueKeysForTests{
    "https://toplevelsite.comtrueFirstNamewww.example.com/baz219",
    "https://toplevelsite.comtrueSecondNamewww.example.com/baz219",
    "https://toplevelsite.comtrueThirdNamewww.example.com/baz219",
    "https://toplevelsite.comtrueFourthNamewww.example.com/baz219"};

const std::array<std::string, 4> kNamesForTests{"FirstName", "SecondName",
                                                "ThirdName", "FourthName"};

// Assert that we have the same number of names and keys.
static_assert(std::tuple_size_v<decltype(kUniqueKeysForTests)> ==
              std::tuple_size_v<decltype(kNamesForTests)>);

sync_pb::CookieSpecifics CreatePredefinedCookieSpecificsForTest(
    size_t i,
    const base::Time& time,
    bool persistent) {
  CHECK(i < kNamesForTests.size());

  return CreateCookieSpecificsForTest(kUniqueKeysForTests[i], kNamesForTests[i],
                                      time, persistent);
}

sync_pb::CookieSpecifics CreateCookieSpecificsForTest(
    const std::string& unique_key,
    const std::string& name,
    const base::Time& creation_time,
    bool persistent,
    const std::string& domain) {
  sync_pb::CookieSpecifics sync_specifics;
  sync_specifics.set_unique_key(unique_key);
  sync_specifics.set_name(name);
  sync_specifics.set_value(kValueForTests);
  sync_specifics.set_domain(domain);
  sync_specifics.set_path(kPathForTests);
  int64_t creation_time_formatted = ToMicrosSinceWindowsEpoch(creation_time);
  sync_specifics.set_creation_time_windows_epoch_micros(
      creation_time_formatted);
  int64_t expiry_time =
      persistent ? ToMicrosSinceWindowsEpoch(creation_time + base::Days(10))
                 : 0;
  sync_specifics.set_expiry_time_windows_epoch_micros(expiry_time);
  sync_specifics.set_last_access_time_windows_epoch_micros(
      creation_time_formatted);
  sync_specifics.set_last_update_time_windows_epoch_micros(
      creation_time_formatted);
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

  return sync_specifics;
}

syncer::EntityData CreateEntityDataForTest(
    const sync_pb::CookieSpecifics& specifics) {
  syncer::EntityData entity_data;
  entity_data.specifics.mutable_cookie()->CopyFrom(specifics);
  entity_data.name = specifics.unique_key();
  return entity_data;
}

}  // namespace ash::floating_sso
