// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_WEBAPKS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_WEBAPKS_HELPER_H_

#include "chrome/browser/android/webapk/webapk_registrar.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace webapks_helper {

bool AreIconInfosMatching(const sync_pb::WebApkIconInfo& icon_info_1,
                          const sync_pb::WebApkIconInfo& icon_info_2);

// Matchers for sync_pb::WebApkSpecifics.

MATCHER_P(ServerManifestIdIs, manifest_id, "") {
  return arg.manifest_id() == manifest_id;
}

MATCHER_P(ServerStartUrlIs, start_url, "") {
  return arg.start_url() == start_url;
}

MATCHER_P(ServerNameIs, name, "") {
  return arg.name() == name;
}

MATCHER_P(ServerThemeColorIs, theme_color, "") {
  return arg.theme_color() == theme_color;
}

MATCHER_P(ServerScopeIs, scope, "") {
  return arg.scope() == scope;
}

MATCHER_P(ServerLastUsedTimeWindowsEpochMicrosIs,
          last_used_time_windows_epoch_micros,
          "") {
  return arg.last_used_time_windows_epoch_micros() ==
         last_used_time_windows_epoch_micros;
}

MATCHER_P(ServerIconInfoIs, icon_info, "") {
  return arg.icon_infos_size() == 1 &&
         AreIconInfosMatching(arg.icon_infos(0), *icon_info);
}

// Matchers for std::pair<const webapps::AppId,
// std::unique_ptr<webapk::WebApkProto>> (ie, a single item from a
// webapk::Registry).

MATCHER_P(LocalAppIdIs, app_id, "") {
  return arg.first == app_id;
}

MATCHER_P(LocalIsLocallyInstalledIs, is_locally_installed, "") {
  return arg.second->is_locally_installed() == is_locally_installed;
}

MATCHER_P(LocalManifestIdIs, manifest_id, "") {
  return arg.second->sync_data().manifest_id() == manifest_id;
}

MATCHER_P(LocalStartUrlIs, start_url, "") {
  return arg.second->sync_data().start_url() == start_url;
}

MATCHER_P(LocalNameIs, name, "") {
  return arg.second->sync_data().name() == name;
}

MATCHER_P(LocalThemeColorIs, theme_color, "") {
  return arg.second->sync_data().theme_color() == theme_color;
}

MATCHER_P(LocalScopeIs, scope, "") {
  return arg.second->sync_data().scope() == scope;
}

MATCHER_P(LocalLastUsedTimeWindowsEpochMicrosIs,
          last_used_time_windows_epoch_micros,
          "") {
  return arg.second->sync_data().last_used_time_windows_epoch_micros() ==
         last_used_time_windows_epoch_micros;
}

MATCHER_P(LocalIconInfoIs, icon_info, "") {
  return arg.second->sync_data().icon_infos_size() == 1 &&
         AreIconInfosMatching(arg.second->sync_data().icon_infos(0),
                              *icon_info);
}

// A helper class that waits for entries in the local webapk DB that match the
// given matchers.
class LocalWebApkMatchChecker : public SingleClientStatusChangeChecker {
 public:
  using Matcher = testing::Matcher<webapk::Registry>;

  explicit LocalWebApkMatchChecker(int profile_index,
                                   syncer::SyncServiceImpl* service,
                                   const Matcher& matcher);
  ~LocalWebApkMatchChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // syncer::SyncServiceObserver implementation.
  void OnSyncCycleCompleted(syncer::SyncService* sync) override;

 private:
  const int profile_index_;
  const Matcher matcher_;
};

// A helper class that waits for the WEB_APK entities on the FakeServer to match
// a given GMock matcher.
class ServerWebApkMatchChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  using Matcher = testing::Matcher<std::vector<sync_pb::WebApkSpecifics>>;

  explicit ServerWebApkMatchChecker(const Matcher& matcher);
  ~ServerWebApkMatchChecker() override;
  ServerWebApkMatchChecker(const ServerWebApkMatchChecker&) = delete;
  ServerWebApkMatchChecker& operator=(const ServerWebApkMatchChecker&) = delete;

  // FakeServer::Observer overrides.
  void OnCommit(syncer::DataTypeSet committed_data_types) override;

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const Matcher matcher_;
};

}  // namespace webapks_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_WEBAPKS_HELPER_H_
