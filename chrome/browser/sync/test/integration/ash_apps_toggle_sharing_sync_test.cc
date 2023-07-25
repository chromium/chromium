// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "chromeos/crosapi/mojom/sync.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/sync.mojom.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

// Used to wait until ash-side SyncUserSettingsClient crosapi notifies observers
// about expected apps toggle value.
class AppsSyncIsEnabledNotifiedToCrosapiObserverChecker
    : public StatusChangeChecker,
      public crosapi::mojom::SyncUserSettingsClientObserver {
 public:
  AppsSyncIsEnabledNotifiedToCrosapiObserverChecker(
      bool expected_apps_sync_is_enabled,
      mojo::Remote<crosapi::mojom::SyncUserSettingsClient>&
          remote_user_settings_client)
      : expected_apps_sync_is_enabled_(expected_apps_sync_is_enabled) {
    remote_user_settings_client.get()->AddObserver(
        receiver_.BindNewPipeAndPassRemote());
    // Need to flush mojo here, otherwise observers might be notified before
    // AddObserver() is actually completed.
    remote_user_settings_client.FlushForTesting();
  }

  ~AppsSyncIsEnabledNotifiedToCrosapiObserverChecker() override = default;

  // crosapi::mojom::SyncUserSettingsClientOBserver implementation.
  void OnAppsSyncEnabledChanged(bool enabled) override {
    last_notified_apps_sync_is_enabled_ = enabled;
    CheckExitCondition();
  }

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for OnAppsSyncEnabledChanged("
        << expected_apps_sync_is_enabled_ << ") call for crosapi observer";
    return last_notified_apps_sync_is_enabled_ &&
           last_notified_apps_sync_is_enabled_.value() ==
               expected_apps_sync_is_enabled_;
  }

 private:
  bool expected_apps_sync_is_enabled_;
  absl::optional<bool> last_notified_apps_sync_is_enabled_;
  mojo::Receiver<crosapi::mojom::SyncUserSettingsClientObserver> receiver_{
      this};
};

class AshAppsToggleSharingSyncTest : public SyncTest {
 public:
  AshAppsToggleSharingSyncTest() : SyncTest(SINGLE_CLIENT) {
    std::vector<base::test::FeatureRef> enabled_features =
        ash::standalone_browser::GetFeatureRefs();
    enabled_features.push_back(syncer::kSyncChromeOSAppsToggleSharing);
    feature_list_.InitWithFeatures(enabled_features, /*disabled_features=*/{});
  }

  ~AshAppsToggleSharingSyncTest() override = default;

  // SyncTest overrides.
  base::FilePath GetProfileBaseName(int index) override {
    // Need to reuse test user profile for this test - Crosapi explicitly
    // assumes there is only one regular profile.
    // TODO(crbug.com/1102768): eventually this should be the case for all Ash
    // tests.
    DCHECK_EQ(index, 0);
    return base::FilePath(
        ash::BrowserContextHelper::kTestUserBrowserContextDirName);
  }

  void SetupCrosapi() {
    crosapi::CrosapiAsh* crosapi_ash =
        crosapi::CrosapiManager::Get()->crosapi_ash();
    DCHECK(crosapi_ash);

    crosapi_ash->BindSyncService(
        sync_mojo_service_remote_.BindNewPipeAndPassReceiver());
    sync_mojo_service_remote_.get()->BindUserSettingsClient(
        user_settings_client_remote_.BindNewPipeAndPassReceiver());
  }

  mojo::Remote<crosapi::mojom::SyncUserSettingsClient>&
  user_settings_client_remote() {
    return user_settings_client_remote_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  mojo::Remote<crosapi::mojom::SyncService> sync_mojo_service_remote_;
  mojo::Remote<crosapi::mojom::SyncUserSettingsClient>
      user_settings_client_remote_;
};

IN_PROC_BROWSER_TEST_F(AshAppsToggleSharingSyncTest,
                       ShouldExposeAppsSyncIsEnabledAndNotifyObserver) {
  ASSERT_TRUE(SetupSync());
  SetupCrosapi();

  crosapi::mojom::SyncUserSettingsClientAsyncWaiter client_async_waiter(
      user_settings_client_remote().get());
  // By default apps sync is enabled after SetupSync() call.
  EXPECT_TRUE(client_async_waiter.IsAppsSyncEnabled());

  {
    // Disable apps sync and verify that crosapi notifies the observer and
    // exposes that apps sync is disabled.
    AppsSyncIsEnabledNotifiedToCrosapiObserverChecker checker(
        /*expected_apps_sync_is_enabled=*/false, user_settings_client_remote());

    GetSyncService(0)->GetUserSettings()->SetSelectedOsTypes(
        /*sync_all_os_types=*/false,
        /*types=*/base::Difference(syncer::UserSelectableOsTypeSet::All(),
                                   {syncer::UserSelectableOsType::kOsApps}));
    EXPECT_TRUE(checker.Wait());
    EXPECT_FALSE(client_async_waiter.IsAppsSyncEnabled());
  }

  {
    // Re-enable apps sync and verify that crosapi notifies the observer and
    // exposes that apps sync is disabled.
    AppsSyncIsEnabledNotifiedToCrosapiObserverChecker checker(
        /*expected_apps_sync_is_enabled=*/true, user_settings_client_remote());

    GetSyncService(0)->GetUserSettings()->SetSelectedOsTypes(
        /*sync_all_os_types=*/true,
        /*types=*/syncer::UserSelectableOsTypeSet::All());
    EXPECT_TRUE(checker.Wait());
    EXPECT_TRUE(client_async_waiter.IsAppsSyncEnabled());
  }
}

}  // namespace
