// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/chrome_constants.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service_impl.h"
#include "components/sync/test/fake_sync_mojo_service.h"
#include "components/sync/test/fake_sync_user_settings_client_ash.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Version of crosapi that is guaranteed to have SyncUserSettingsClient API (
// exposed by SyncService crosapi).
const uint32_t kMinCrosapiVersionWithSyncUserSettingsClient = 80;

class WebAppsSyncActiveStateChecker : public SingleClientStatusChangeChecker {
 public:
  WebAppsSyncActiveStateChecker(bool expected_state,
                                syncer::SyncServiceImpl* sync_service)
      : SingleClientStatusChangeChecker(sync_service),
        expected_state_(expected_state) {}
  ~WebAppsSyncActiveStateChecker() override = default;

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for WebApps sync active state to become: "
        << expected_state_;
    return service()->GetActiveDataTypes().Has(syncer::WEB_APPS) ==
           expected_state_;
  }

 private:
  bool expected_state_;
};

class SyncAppsToggleSharingLacrosBrowserTest : public SyncTest {
 public:
  SyncAppsToggleSharingLacrosBrowserTest() : SyncTest(SINGLE_CLIENT) {
    override_features_.InitAndEnableFeature(
        syncer::kSyncChromeOSAppsToggleSharing);
  }
  ~SyncAppsToggleSharingLacrosBrowserTest() override = default;

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

    // If SyncService Crosapi interface is not available on this version of
    // ash-chrome, this test suite will no-op.
    if (!IsServiceAvailable()) {
      return;
    }

    // Replace the production SyncService Crosapi interface with a fake for
    // testing.
    mojo::Remote<crosapi::mojom::SyncService>& remote =
        chromeos::LacrosService::Get()
            ->GetRemote<crosapi::mojom::SyncService>();
    remote.reset();
    sync_mojo_service_.BindReceiver(remote.BindNewPipeAndPassReceiver());
  }

  bool IsServiceAvailable() const {
    const chromeos::LacrosService* lacros_service =
        chromeos::LacrosService::Get();
    return lacros_service &&
           lacros_service->IsAvailable<crosapi::mojom::SyncService>() &&
           chromeos::BrowserParamsProxy::Get()->CrosapiVersion() >=
               kMinCrosapiVersionWithSyncUserSettingsClient;
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

  base::FilePath GetProfileBaseName(int index) override {
    // Apps toggle sharing is enabled only for the main profile, so SyncTest
    // should setup sync using it.
    DCHECK_EQ(index, 0);
    return base::FilePath(chrome::kInitialProfile);
  }

 private:
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
  if (!IsServiceAvailable()) {
    GTEST_SKIP() << "Unsupported Ash version.";
  }
  ASSERT_TRUE(SetupSync());
  client_ash().SetAppsSyncIsEnabled(/*enabled=*/true);
  EXPECT_TRUE(
      WebAppsSyncActiveStateChecker(/*expected_state=*/true, GetSyncService(0))
          .Wait());

  client_ash().SetAppsSyncIsEnabled(/*enabled=*/false);
  EXPECT_TRUE(
      WebAppsSyncActiveStateChecker(/*expected_state=*/false, GetSyncService(0))
          .Wait());
}

IN_PROC_BROWSER_TEST_F(SyncAppsToggleSharingLacrosBrowserTest,
                       ShouldEnableWebAppsInTransportOnlyMode) {
  if (!IsServiceAvailable()) {
    GTEST_SKIP() << "Unsupported Ash version.";
  }
  ASSERT_TRUE(SetupClients());

  // Setup a primary account, but don't actually enable Sync-the-feature (so
  // that Sync will start in transport-only mode).
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // By enabling apps sync in ash settings, WEB_APPS should become enabled even
  // in transport-only mode.
  client_ash().SetAppsSyncIsEnabled(/*enabled=*/true);
  EXPECT_TRUE(
      WebAppsSyncActiveStateChecker(/*expected_state=*/true, GetSyncService(0))
          .Wait());
}

}  // namespace
