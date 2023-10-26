// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/chrome_constants.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_sync_mojo_service.h"
#include "components/sync/test/fake_sync_user_settings_client_ash.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DatatypeSyncActiveStateChecker : public SingleClientStatusChangeChecker {
 public:
  DatatypeSyncActiveStateChecker(syncer::ModelType type,
                                 bool expected_state,
                                 syncer::SyncServiceImpl* sync_service)
      : SingleClientStatusChangeChecker(sync_service),
        type_(type),
        expected_state_(expected_state) {}
  ~DatatypeSyncActiveStateChecker() override = default;

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for " << syncer::ModelTypeToDebugString(type_)
        << " sync active state to become: " << expected_state_;
    return service()->GetActiveDataTypes().Has(type_) == expected_state_;
  }

 private:
  const syncer::ModelType type_;
  const bool expected_state_;
};

class SyncAppsToggleSharingLacrosBrowserTest : public SyncTest {
 public:
  SyncAppsToggleSharingLacrosBrowserTest() : SyncTest(SINGLE_CLIENT) {
    override_features_.InitAndEnableFeature(
        syncer::kSyncChromeOSAppsToggleSharing);
  }
  ~SyncAppsToggleSharingLacrosBrowserTest() override = default;

  void SetUp() override {
    // In the "initial" profile (see GetProfileBaseName() below), ChromeOS test
    // infra automatically signs in a stub user. Make sure Sync uses the same
    // account.
    // Note: This can't be done in SetUpCommandLine() because that happens
    // slightly too late (SyncTest::SetUp() already consumes this param).
    base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
    cl->AppendSwitchASCII(switches::kSyncUserForTest,
                          user_manager::kStubUserEmail);
    SyncTest::SetUp();
  }

  base::FilePath GetProfileBaseName(int index) override {
    // Apps toggle sharing is enabled only for the main profile, so SyncTest
    // should setup sync using it.
    DCHECK_EQ(index, 0);
    return base::FilePath(chrome::kInitialProfile);
  }

  // This test replaces production SyncService Crosapi interface with a fake.
  // It needs to be done before connection between Ash and Lacros user settings
  // clients is established (during creation of browser extra parts), but after
  // LacrosService is initialized. Thus CreatedBrowserMainParts() is the only
  // available option.
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    SyncTest::CreatedBrowserMainParts(browser_main_parts);

    // Replace the production SyncService Crosapi interface with a fake for
    // testing.
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        sync_mojo_service_.BindNewPipeAndPassRemote());
  }

  syncer::FakeSyncUserSettingsClientAsh& client_ash() {
    return sync_mojo_service_.GetFakeSyncUserSettingsClientAsh();
  }

 private:
  syncer::FakeSyncMojoService sync_mojo_service_;
  base::test::ScopedFeatureList override_features_;
};

class SyncAppsToggleSharingLacrosBrowserTestWithoutCrosapi : public SyncTest {
 public:
  SyncAppsToggleSharingLacrosBrowserTestWithoutCrosapi()
      : SyncTest(SINGLE_CLIENT) {
    override_features_.InitAndEnableFeature(
        syncer::kSyncChromeOSAppsToggleSharing);
  }
  ~SyncAppsToggleSharingLacrosBrowserTestWithoutCrosapi() override = default;

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    SyncTest::CreatedBrowserMainParts(browser_main_parts);
    // Mimic SyncUserSettingsClient Crosapi not available
    sync_mojo_service_.SetFakeSyncUserSettingsClientAshAvailable(false);
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        sync_mojo_service_.BindNewPipeAndPassRemote());
  }

  base::FilePath GetProfileBaseName(int index) override {
    // Apps toggle sharing is enabled only for the main profile, so SyncTest
    // should setup sync using it.
    DCHECK_EQ(index, 0);
    return base::FilePath(chrome::kInitialProfile);
  }

 private:
  syncer::FakeSyncMojoService sync_mojo_service_;
  base::test::ScopedFeatureList override_features_;
};

IN_PROC_BROWSER_TEST_F(SyncAppsToggleSharingLacrosBrowserTestWithoutCrosapi,
                       ShouldDisableAppsSyncByDefault) {
  ASSERT_TRUE(SetupSync());
  // Crosapi isn't setup in this test, so Lacros SyncService should assume apps
  // sync is disabled.
  EXPECT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::WEB_APPS));
}

IN_PROC_BROWSER_TEST_F(SyncAppsToggleSharingLacrosBrowserTest,
                       ShouldEnableAndDisableAppsSync) {
  ASSERT_TRUE(SetupSync());
  client_ash().SetAppsSyncIsEnabled(/*enabled=*/true);
  EXPECT_TRUE(DatatypeSyncActiveStateChecker(
                  syncer::WEB_APPS, /*expected_state=*/true, GetSyncService(0))
                  .Wait());

  client_ash().SetAppsSyncIsEnabled(/*enabled=*/false);
  EXPECT_TRUE(DatatypeSyncActiveStateChecker(
                  syncer::WEB_APPS, /*expected_state=*/false, GetSyncService(0))
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(SyncAppsToggleSharingLacrosBrowserTest,
                       ShouldEnableAppsTypeInTransportOnlyMode) {
  ASSERT_TRUE(SetupClients());

  // Setup a primary account, but don't actually enable Sync-the-feature (so
  // that Sync will start in transport-only mode).
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // By enabling apps sync in ash settings, apps types should become enabled
  // even in transport-only mode.
  client_ash().SetAppsSyncIsEnabled(/*enabled=*/true);
  EXPECT_TRUE(DatatypeSyncActiveStateChecker(
                  syncer::WEB_APPS, /*expected_state=*/true, GetSyncService(0))
                  .Wait());
  EXPECT_TRUE(DatatypeSyncActiveStateChecker(
                  syncer::APPS, /*expected_state=*/true, GetSyncService(0))
                  .Wait());
  EXPECT_TRUE(DatatypeSyncActiveStateChecker(syncer::APP_SETTINGS,
                                             /*expected_state=*/true,
                                             GetSyncService(0))
                  .Wait());
}

}  // namespace
