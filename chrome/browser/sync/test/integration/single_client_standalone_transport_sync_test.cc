// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/chrome_paths.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "content/public/test/browser_test.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/sync/test/integration/os_sync_test.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/browser_sync/browser_sync_switches.h"
#endif

namespace {

syncer::ModelTypeSet AllowedTypesInStandaloneTransportMode() {
  static_assert(41 == syncer::ModelType::NUM_ENTRIES,
                "Add new types below if they run in transport mode");
  // Only some special whitelisted types (and control types) are allowed in
  // standalone transport mode.
  syncer::ModelTypeSet allowed_types(
      syncer::DEVICE_INFO, syncer::USER_CONSENTS, syncer::SECURITY_EVENTS,
      syncer::AUTOFILL_WALLET_DATA, syncer::SHARING_MESSAGE);
  allowed_types.PutAll(syncer::ControlTypes());
#if defined(OS_CHROMEOS)
  // OS sync types run in transport mode.
  if (chromeos::features::IsSplitSettingsSyncEnabled()) {
    allowed_types.PutAll({syncer::APPS, syncer::APP_SETTINGS, syncer::APP_LIST,
                          syncer::APP_SETTINGS, syncer::ARC_PACKAGE,
                          syncer::PRINTERS, syncer::OS_PREFERENCES,
                          syncer::OS_PRIORITY_PREFERENCES, syncer::WEB_APPS});
  }
  if (base::FeatureList::IsEnabled(switches::kSyncWifiConfigurations)) {
    allowed_types.Put(syncer::WIFI_CONFIGURATIONS);
  }
#endif  // defined(OS_CHROMEOS)
  return allowed_types;
}

base::FilePath GetTestFilePathForCacheGuid() {
  base::FilePath user_data_path;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_path);
  return user_data_path.AppendASCII("SyncTestTmpCacheGuid");
}

class SyncDisabledByUserChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncDisabledByUserChecker(syncer::ProfileSyncService* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for sync disabled by user";
    return service()->HasDisableReason(
        syncer::SyncService::DISABLE_REASON_USER_CHOICE);
  }
};

class SingleClientStandaloneTransportSyncTest : public SyncTest {
 public:
  SingleClientStandaloneTransportSyncTest() : SyncTest(SINGLE_CLIENT) {
    DisableVerifier();
  }

  ~SingleClientStandaloneTransportSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientStandaloneTransportSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportSyncTest,
                       StartsSyncTransportOnSignin) {
#if defined(OS_CHROMEOS)
  // On Chrome OS before SplitSettingSync, sync auto-starts on sign-in.
  if (!chromeos::features::IsSplitSettingsSyncEnabled())
    return;
#endif
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Signing in (without explicitly setting up Sync) should trigger starting the
  // Sync machinery in standalone transport mode.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  EXPECT_NE(syncer::SyncService::TransportState::DISABLED,
            GetSyncService(0)->GetTransportState());

  EXPECT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  // Both IsSyncRequested and IsFirstSetupComplete should remain false (i.e.
  // at their default values). They only get set during the Sync setup flow,
  // either by the Sync confirmation dialog or by the settings page if going
  // through the advanced settings flow.
  EXPECT_FALSE(GetSyncService(0)->GetUserSettings()->IsFirstSetupComplete());
  // TODO(crbug.com/906034,crbug.com/973770): Sort out the proper default value
  // for IsSyncRequested().
  // EXPECT_FALSE(GetSyncService(0)->GetUserSettings()->IsSyncRequested());

  EXPECT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  EXPECT_FALSE(GetSyncService(0)->IsSyncFeatureActive());

  // Make sure that only the allowed types got activated. Note that, depending
  // on some other feature flags, not all of the allowed types are necessarily
  // active, and that's okay.
  syncer::ModelTypeSet bad_types =
      syncer::Difference(GetSyncService(0)->GetActiveDataTypes(),
                         AllowedTypesInStandaloneTransportMode());
  EXPECT_TRUE(bad_types.Empty()) << syncer::ModelTypeSetToString(bad_types);
}

IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportSyncTest,
                       SwitchesBetweenTransportAndFeature) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Set up Sync-the-feature.
  ASSERT_TRUE(GetClient(0)->SetupSync());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());

  // Make sure that some model type which is not allowed in transport-only mode
  // got activated.
  ASSERT_FALSE(AllowedTypesInStandaloneTransportMode().Has(syncer::BOOKMARKS));
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kBookmarks));
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::BOOKMARKS));

  // Turn off Sync-the-feature by user choice. The machinery should start up
  // again in transport-only mode.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(false);
  EXPECT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  EXPECT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  EXPECT_FALSE(GetSyncService(0)->IsSyncFeatureActive());

  syncer::ModelTypeSet bad_types =
      syncer::Difference(GetSyncService(0)->GetActiveDataTypes(),
                         AllowedTypesInStandaloneTransportMode());
  EXPECT_TRUE(bad_types.Empty()) << syncer::ModelTypeSetToString(bad_types);

  // Finally, turn Sync-the-feature on again.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(true);
  EXPECT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  EXPECT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  EXPECT_TRUE(GetSyncService(0)->IsSyncFeatureActive());
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::BOOKMARKS));
}

// Tests the behavior of receiving a "Reset Sync" operation from the dashboard
// while Sync-the-feature is active: On non-ChromeOS, this signs the user out,
// so Sync will be fully disabled. On ChromeOS, there is no sign-out, so
// Sync-the-transport will start.
IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportSyncTest,
                       HandlesResetFromDashboardWhenSyncActive) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Set up Sync-the-feature.
  ASSERT_TRUE(GetClient(0)->SetupSync());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());

  // Trigger a "Reset Sync" from the dashboard and wait for it to apply. This
  // involves clearing the server data so that the birthday gets incremented,
  // and also sending an appropriate error.
  GetFakeServer()->ClearServerData();
  GetFakeServer()->TriggerActionableError(
      sync_pb::SyncEnums::NOT_MY_BIRTHDAY, "Reset Sync from Dashboard",
      "https://chrome.google.com/sync", sync_pb::SyncEnums::UNKNOWN_ACTION);
  EXPECT_TRUE(SyncDisabledByUserChecker(GetSyncService(0)).Wait());
  GetFakeServer()->ClearActionableError();

  // On all platforms, Sync-the-feature should now be disabled.
  EXPECT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  EXPECT_TRUE(GetSyncService(0)->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_USER_CHOICE));

#if defined(OS_CHROMEOS)
  // On ChromeOS, the primary account should remain, and Sync should start up
  // again in standalone transport mode.
  EXPECT_TRUE(GetSyncService(0)->IsAuthenticatedAccountPrimary());
  EXPECT_FALSE(GetSyncService(0)->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN));
  EXPECT_NE(syncer::SyncService::TransportState::DISABLED,
            GetSyncService(0)->GetTransportState());

  EXPECT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  EXPECT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
#else
  // On non-ChromeOS platforms, the "Reset Sync" operation should also remove
  // the primary account. Note that this behavior may change in the future, see
  // crbug.com/246839.
  EXPECT_FALSE(GetSyncService(0)->IsAuthenticatedAccountPrimary());
  // Note: In real life, the account would remain as an *unconsented* primary
  // account, and so Sync would start up again in standalone transport mode.
  // However, since we haven't set up cookies in this test, the account is *not*
  // considered primary anymore (not even "unconsented").
#endif  // defined(OS_CHROMEOS)
}

// Regression test for crbug.com/955989 that verifies the cache GUID is not
// reset upon restart of the browser, in standalone transport mode.
IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportSyncTest,
                       PRE_ReusesSameCacheGuid) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  // On platforms where Sync starts automatically (in practice, Android and
  // ChromeOS), IsFirstSetupComplete gets set automatically, and so the full
  // Sync feature will start upon sign-in to a primary account.
#if !defined(OS_CHROMEOS)
  ASSERT_FALSE(GetSyncService(0)->GetUserSettings()->IsFirstSetupComplete());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
#endif  // !defined(OS_CHROMEOS)

  syncer::SyncPrefs prefs(GetProfile(0)->GetPrefs());
  const std::string cache_guid = prefs.GetCacheGuid();
  ASSERT_FALSE(cache_guid.empty());

  // Save the cache GUID to file to remember after restart, for test
  // verification purposes only.
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_NE(-1, base::WriteFile(GetTestFilePathForCacheGuid(),
                                cache_guid.c_str(), cache_guid.size()));
}

IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportSyncTest,
                       ReusesSameCacheGuid) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  ASSERT_FALSE(GetSyncService(0)->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN));
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  // On platforms where Sync starts automatically (in practice, Android and
  // ChromeOS), IsFirstSetupComplete gets set automatically, and so the full
  // Sync feature will start upon sign-in to a primary account.
#if !defined(OS_CHROMEOS)
  ASSERT_FALSE(GetSyncService(0)->GetUserSettings()->IsFirstSetupComplete());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
#endif  // !defined(OS_CHROMEOS)

  syncer::SyncPrefs prefs(GetProfile(0)->GetPrefs());
  ASSERT_FALSE(prefs.GetCacheGuid().empty());

  std::string old_cache_guid;
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(
      base::ReadFileToString(GetTestFilePathForCacheGuid(), &old_cache_guid));
  ASSERT_FALSE(old_cache_guid.empty());

  EXPECT_EQ(old_cache_guid, prefs.GetCacheGuid());
}

#if defined(OS_CHROMEOS)
class SingleClientStandaloneTransportOsSyncTest : public OsSyncTest {
 public:
  SingleClientStandaloneTransportOsSyncTest() : OsSyncTest(SINGLE_CLIENT) {
    // Enable in-development types.
    scoped_features_.InitWithFeatures({features::kDesktopPWAsWithoutExtensions,
                                       switches::kSyncWifiConfigurations},
                                      {});
  }
  ~SingleClientStandaloneTransportOsSyncTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_features_;
};

IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportOsSyncTest,
                       OsTypesAreActiveWhenBrowserSyncIsOff) {
  ASSERT_TRUE(chromeos::features::IsSplitSettingsSyncEnabled());

  // Setup clients but don't start syncing yet.
  ASSERT_TRUE(SetupClients());
  syncer::SyncService* service = GetSyncService(0);
  syncer::SyncUserSettings* settings = service->GetUserSettings();

  // Simulate a signed-in user with browser sync off and OS sync on.
  settings->SetSyncRequested(false);
  settings->SetOsSyncFeatureEnabled(true);
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(service->IsSyncFeatureActive());

  // OS data types synced by the transport layer are active.
  syncer::ModelTypeSet active_types = service->GetActiveDataTypes();
  EXPECT_TRUE(active_types.Has(syncer::APP_LIST));
  EXPECT_TRUE(active_types.Has(syncer::APP_SETTINGS));
  EXPECT_TRUE(active_types.Has(syncer::APPS));
  EXPECT_TRUE(active_types.Has(syncer::ARC_PACKAGE));
  EXPECT_TRUE(active_types.Has(syncer::OS_PREFERENCES));
  EXPECT_TRUE(active_types.Has(syncer::OS_PRIORITY_PREFERENCES));
  EXPECT_TRUE(active_types.Has(syncer::PRINTERS));
  EXPECT_TRUE(active_types.Has(syncer::WEB_APPS));
  EXPECT_TRUE(active_types.Has(syncer::WIFI_CONFIGURATIONS));

  // Verify that a few browser non-transport-mode types are not active.
  EXPECT_FALSE(active_types.Has(syncer::BOOKMARKS));
  EXPECT_FALSE(active_types.Has(syncer::SESSIONS));
  EXPECT_FALSE(active_types.Has(syncer::TYPED_URLS));
}

IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportOsSyncTest,
                       OsTypesAreNotActiveWhenOsSyncIsOff) {
  ASSERT_TRUE(chromeos::features::IsSplitSettingsSyncEnabled());

  // Setup clients but don't start syncing yet.
  ASSERT_TRUE(SetupClients());
  syncer::SyncService* service = GetSyncService(0);
  syncer::SyncUserSettings* settings = service->GetUserSettings();

  // Simulate a user who leaves OS sync disabled but starts browser sync.
  settings->SetOsSyncFeatureEnabled(false);
  ASSERT_TRUE(GetClient(0)->SetupSync());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(service->IsSyncFeatureActive());
  ASSERT_FALSE(settings->IsOsSyncFeatureEnabled());

  // OS data types synced by the transport layer are not active.
  syncer::ModelTypeSet active_types = service->GetActiveDataTypes();
  EXPECT_FALSE(active_types.Has(syncer::APP_LIST));
  EXPECT_FALSE(active_types.Has(syncer::APP_SETTINGS));
  EXPECT_FALSE(active_types.Has(syncer::APPS));
  EXPECT_FALSE(active_types.Has(syncer::ARC_PACKAGE));
  EXPECT_FALSE(active_types.Has(syncer::OS_PREFERENCES));
  EXPECT_FALSE(active_types.Has(syncer::OS_PRIORITY_PREFERENCES));
  EXPECT_FALSE(active_types.Has(syncer::PRINTERS));
  EXPECT_FALSE(active_types.Has(syncer::WEB_APPS));
  EXPECT_FALSE(active_types.Has(syncer::WIFI_CONFIGURATIONS));

  // Browser non-transport-mode types are active.
  EXPECT_TRUE(active_types.Has(syncer::BOOKMARKS));
  EXPECT_TRUE(active_types.Has(syncer::SESSIONS));
  EXPECT_TRUE(active_types.Has(syncer::TYPED_URLS));
}
#endif  // defined(OS_CHROMEOS)

}  // namespace
