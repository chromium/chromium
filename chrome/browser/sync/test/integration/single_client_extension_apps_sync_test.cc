// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/sync/test/integration/apps_helper.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/app_specifics.pb.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_server.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#include "base/test/scoped_path_override.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {
using apps_helper::AllProfilesHaveSameApps;
using apps_helper::InstallHostedApp;
using apps_helper::InstallPlatformApp;

class SingleClientExtensionAppsSyncTest : public SyncTest {
 public:
  SingleClientExtensionAppsSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientExtensionAppsSyncTest() override = default;

  bool UseVerifier() override {
    // TODO(crbug.com/1137717): rewrite tests to not use verifier profile.
    return true;
  }

 private:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  web_app::test::ScopedSkipMainProfileCheck skip_main_profile_check;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(IS_WIN)
  // This stops extension installation from creating a shortcut in the real
  // desktop startup dir. This prevents Chrome launching with the extension
  // on startup on trybots and developer machines.
  base::ScopedPathOverride override_start_menu_dir_{base::DIR_START_MENU};
#endif  // BUILDFLAG(IS_WIN)
};

IN_PROC_BROWSER_TEST_F(SingleClientExtensionAppsSyncTest, StartWithNoApps) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_F(SingleClientExtensionAppsSyncTest,
                       StartWithSomeLegacyApps) {
  ASSERT_TRUE(SetupClients());

  const int kNumApps = 2;
  for (int i = 0; i < kNumApps; ++i) {
    InstallHostedApp(GetProfile(0), i);
    InstallHostedApp(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_F(SingleClientExtensionAppsSyncTest,
                       StartWithSomePlatformApps) {
  ASSERT_TRUE(SetupClients());

  const int kNumApps = 2;
  for (int i = 0; i < kNumApps; ++i) {
    InstallPlatformApp(GetProfile(0), i);
    InstallPlatformApp(verifier(), i);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_F(SingleClientExtensionAppsSyncTest,
                       InstallSomeLegacyApps) {
  ASSERT_TRUE(SetupSync());

  const int kNumApps = 2;
  for (int i = 0; i < kNumApps; ++i) {
    InstallHostedApp(GetProfile(0), i);
    InstallHostedApp(verifier(), i);
  }

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_F(SingleClientExtensionAppsSyncTest,
                       InstallSomePlatformApps) {
  ASSERT_TRUE(SetupSync());

  const int kNumApps = 2;
  for (int i = 0; i < kNumApps; ++i) {
    InstallPlatformApp(GetProfile(0), i);
    InstallPlatformApp(verifier(), i);
  }

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

IN_PROC_BROWSER_TEST_F(SingleClientExtensionAppsSyncTest, InstallSomeApps) {
  ASSERT_TRUE(SetupSync());

  // TODO(crbug.com/1124986): Determine if these values
  // can be raised without introducing flakiness.
  const int kNumApps = 1;
  const int kNumPlatformApps = 1;

  int i = 0;

  for (int j = 0; j < kNumApps; ++i, ++j) {
    InstallHostedApp(GetProfile(0), i);
    InstallHostedApp(verifier(), i);
  }

  for (int j = 0; j < kNumPlatformApps; ++i, ++j) {
    InstallPlatformApp(GetProfile(0), i);
    InstallPlatformApp(verifier(), i);
  }

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(AllProfilesHaveSameApps());
}

std::vector<sync_pb::SyncEntity> FilterForBookmarkApps(
    const std::vector<sync_pb::SyncEntity>& entities) {
  std::vector<sync_pb::SyncEntity> bookmark_apps;
  for (const sync_pb::SyncEntity& entity : entities) {
    if (!entity.specifics().has_app() ||
        !entity.specifics().app().has_bookmark_app_url()) {
      continue;
    }
    bookmark_apps.push_back(entity);
  }
  return bookmark_apps;
}

class NoBookmarkAppServerChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  NoBookmarkAppServerChecker() = default;
  ~NoBookmarkAppServerChecker() override = default;
  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    std::vector<sync_pb::SyncEntity> bookmark_entities = FilterForBookmarkApps(
        fake_server()->GetSyncEntitiesByModelType(syncer::APPS));
    testing::StringMatchResultListener result_listener;
    const bool matches = testing::ExplainMatchResult(
        testing::IsEmpty(), bookmark_entities, &result_listener);
    *os << result_listener.str();
    return matches;
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientExtensionAppsSyncTest, NoBookmarkApps) {
  const int64_t kDefaultTime = 1234L;

  std::vector<sync_pb::SyncEntity> server_apps =
      GetFakeServer()->GetSyncEntitiesByModelType(syncer::APPS);
  ASSERT_EQ(0ul, server_apps.size());

  // This creates a "google photos" bookmark app specifics.
  sync_pb::EntitySpecifics specifics;
  sync_pb::AppSpecifics* app_specifics = specifics.mutable_app();
  sync_pb::ExtensionSpecifics* extension_specifics =
      app_specifics->mutable_extension();
  extension_specifics->set_id("ncmjhecbjeaamljdfahankockkkdmedg");
  extension_specifics->set_version("0");
  extension_specifics->set_enabled(true);
  extension_specifics->set_update_url("");
  extension_specifics->set_remote_install(false);
  extension_specifics->set_incognito_enabled(false);
  extension_specifics->set_disable_reasons(0);
  app_specifics->set_bookmark_app_url("https://photos.google.com/?lfhs=2");

  GetFakeServer()->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/"", "ncmjhecbjeaamljdfahankockkkdmedg", specifics,
          kDefaultTime, kDefaultTime));
  server_apps = FilterForBookmarkApps(
      GetFakeServer()->GetSyncEntitiesByModelType(syncer::APPS));
  ASSERT_EQ(1u, server_apps.size());

  ASSERT_TRUE(SetupSync());
  // Since bookmark apps are deprecated (https://crbug.com/877898), the client
  // should have deleted it from the server.
  EXPECT_TRUE(NoBookmarkAppServerChecker().Wait());
}

}  // namespace
