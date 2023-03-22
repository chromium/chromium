// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/chrome_paths.h"
#include "components/send_tab_to_self/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/glue/sync_transport_data_prefs.h"
#include "components/sync/driver/sync_service_impl.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "components/browser_sync/browser_sync_switches.h"
#endif

namespace {

syncer::ModelTypeSet AllowedTypesInStandaloneTransportMode() {
  static_assert(46 == syncer::GetNumModelTypes(),
                "Add new types below if they run in transport mode");
  // Only some special allowlisted types (and control types) are allowed in
  // standalone transport mode.
  syncer::ModelTypeSet allowed_types(
      syncer::DEVICE_INFO, syncer::USER_CONSENTS, syncer::SECURITY_EVENTS,
      syncer::AUTOFILL_WALLET_DATA, syncer::CONTACT_INFO,
      syncer::SHARING_MESSAGE);
  allowed_types.PutAll(syncer::ControlTypes());
  allowed_types.Put(syncer::SEND_TAB_TO_SELF);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // OS sync types run in transport mode.
  allowed_types.PutAll(
      {syncer::APPS, syncer::APP_SETTINGS, syncer::APP_LIST,
       syncer::APP_SETTINGS, syncer::ARC_PACKAGE, syncer::PRINTERS,
       syncer::PRINTERS_AUTHORIZATION_SERVERS, syncer::OS_PREFERENCES,
       syncer::OS_PRIORITY_PREFERENCES, syncer::WEB_APPS,
       syncer::WORKSPACE_DESK, syncer::WIFI_CONFIGURATIONS});
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return allowed_types;
}

base::FilePath GetTestFilePathForCacheGuid() {
  base::FilePath user_data_path;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_path);
  return user_data_path.AppendASCII("SyncTestTmpCacheGuid");
}

class SyncDisabledByUserChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncDisabledByUserChecker(syncer::SyncServiceImpl* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for sync disabled by user";
    return service()->HasDisableReason(
        syncer::SyncService::DISABLE_REASON_USER_CHOICE);
  }
};

class SingleClientStandaloneTransportSyncTest : public SyncTest {
 public:
  SingleClientStandaloneTransportSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientStandaloneTransportSyncTest() override = default;
};

// On Chrome OS sync auto-starts on sign-in.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportSyncTest,
                       StartsSyncTransportOnSignin) {
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
      base::Difference(GetSyncService(0)->GetActiveDataTypes(),
                       AllowedTypesInStandaloneTransportMode());
  EXPECT_TRUE(bad_types.Empty())
      << syncer::ModelTypeSetToDebugString(bad_types);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
      base::Difference(GetSyncService(0)->GetActiveDataTypes(),
                       AllowedTypesInStandaloneTransportMode());
  EXPECT_TRUE(bad_types.Empty())
      << syncer::ModelTypeSetToDebugString(bad_types);

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
  // involves clearing the server data so that the birthday gets incremented.
  GetFakeServer()->ClearServerData();
  EXPECT_TRUE(SyncDisabledByUserChecker(GetSyncService(0)).Wait());

  // On all platforms, Sync-the-feature should now be disabled.
  EXPECT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  EXPECT_TRUE(GetSyncService(0)->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_USER_CHOICE));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Ash, the primary account should remain, and Sync should start up
  // again in standalone transport mode.
  EXPECT_TRUE(GetSyncService(0)->HasSyncConsent());
  EXPECT_FALSE(GetSyncService(0)->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN));
  EXPECT_NE(syncer::SyncService::TransportState::DISABLED,
            GetSyncService(0)->GetTransportState());

  EXPECT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  EXPECT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
#else
  // On platforms other than Ash, the "Reset Sync" operation should revoke
  // the Sync consent. On Mobile, "Reset Sync" also clears the primary account.
  EXPECT_FALSE(GetSyncService(0)->HasSyncConsent());
  // Note: In real life, on platforms other than Ash and Mobile the account
  // would remain as an *unconsented* primary account, and so Sync would start
  // up again in standalone transport mode. However, since we haven't set up
  // cookies in this test, the account is *not* considered primary anymore
  // (not even "unconsented").
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_FALSE(GetSyncService(0)->GetUserSettings()->IsFirstSetupComplete());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  syncer::SyncTransportDataPrefs transport_data_prefs(
      GetProfile(0)->GetPrefs());
  const std::string cache_guid = transport_data_prefs.GetCacheGuid();
  ASSERT_FALSE(cache_guid.empty());

  // Save the cache GUID to file to remember after restart, for test
  // verification purposes only.
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::WriteFile(GetTestFilePathForCacheGuid(), cache_guid));
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
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_FALSE(GetSyncService(0)->GetUserSettings()->IsFirstSetupComplete());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  syncer::SyncTransportDataPrefs transport_data_prefs(
      GetProfile(0)->GetPrefs());
  ASSERT_FALSE(transport_data_prefs.GetCacheGuid().empty());

  std::string old_cache_guid;
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(
      base::ReadFileToString(GetTestFilePathForCacheGuid(), &old_cache_guid));
  ASSERT_FALSE(old_cache_guid.empty());

  EXPECT_EQ(old_cache_guid, transport_data_prefs.GetCacheGuid());
}

}  // namespace
