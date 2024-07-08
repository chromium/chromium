// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_sso/cookie_sync_test_util.h"

#include "components/sync/protocol/cookie_specifics.pb.h"

namespace ash::floating_sso {

sync_pb::CookieSpecifics DefaultCookieSpecificsForTest() {
  sync_pb::CookieSpecifics sync_specifics;
  sync_specifics.set_unique_key(kUniqueKeyForTests);
  sync_specifics.set_name(kNameForTests);
  sync_specifics.set_value(kValueForTests);
  sync_specifics.set_domain(kDomainForTests);
  sync_specifics.set_path(kPathForTests);
  sync_specifics.set_creation_time_windows_epoch_micros(
      kCreationTimeForTesting);
  sync_specifics.set_expiry_time_windows_epoch_micros(0);
  sync_specifics.set_last_access_time_windows_epoch_micros(
      kCreationTimeForTesting);
  sync_specifics.set_last_update_time_windows_epoch_micros(
      kLastUpdateTimeForTesting);
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

}  // namespace ash::floating_sso
