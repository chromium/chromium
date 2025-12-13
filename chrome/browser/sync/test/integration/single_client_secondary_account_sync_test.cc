// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/chrome_paths.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/glue/sync_transport_data_prefs.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"

namespace {

#if !BUILDFLAG(IS_CHROMEOS)
base::FilePath GetTestFilePathForCacheGuid() {
  base::FilePath user_data_path;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_path);
  return user_data_path.AppendASCII("SyncTestTmpCacheGuid");
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

class SingleClientSecondaryAccountSyncTest : public SyncTest {
 public:
  SingleClientSecondaryAccountSyncTest() : SyncTest(SINGLE_CLIENT) {}

  SingleClientSecondaryAccountSyncTest(
      const SingleClientSecondaryAccountSyncTest&) = delete;
  SingleClientSecondaryAccountSyncTest& operator=(
      const SingleClientSecondaryAccountSyncTest&) = delete;

  ~SingleClientSecondaryAccountSyncTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    // The value doesn't matter, since the tests use SetupSyncWithMode(..) to
    // explicitly pick Sync-the-feature or Sync-the-transport.
    return SyncTest::SetupSyncMode::kSyncTransportOnly;
  }

  Profile* profile() { return GetProfile(0); }
};

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SingleClientSecondaryAccountSyncTest,
                       DoesNotStartSyncTransportOnSignin) {
  ASSERT_TRUE(SetupClients());

  // Signing in (without explicitly setting up Sync) should do nothing here,
  // since we're on a platform where the unconsented primary account is not
  // supported.
  secondary_account_helper::SignInUnconsentedAccount(
      profile(), &test_url_loader_factory_, "user@email.com");
  EXPECT_EQ(syncer::SyncService::TransportState::DISABLED,
            GetSyncService(0)->GetTransportState());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// ChromeOS doesn't support changes to the primary account after startup, so
// this test doesn't apply.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SingleClientSecondaryAccountSyncTest,
                       SwitchesFromTransportToFeature) {
  ASSERT_TRUE(SetupClients());

  // Set up Sync in transport mode.
  ASSERT_TRUE(SetupSyncWithMode(SetupSyncMode::kSyncTransportOnly));
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  // Simulate the user opting in to full Sync, and set first-time setup to
  // complete.
  secondary_account_helper::GrantSyncConsent(
      profile(),
      GetClient(0)->GetEmailForAccount(SyncTestAccount::kDefaultAccount));
  GetSyncService(0)->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);

  EXPECT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  EXPECT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  EXPECT_TRUE(GetSyncService(0)->IsSyncFeatureActive());

  // Make sure that some data type which is not allowed in transport-only mode
  // got activated.
  ASSERT_FALSE(AllowedTypesInStandaloneTransportMode().Has(syncer::AUTOFILL));
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kAutofill));
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::AUTOFILL));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Regression test for crbug.com/955989 that verifies the cache GUID is not
// reset upon restart of the browser, in standalone transport mode with
// unconsented accounts.
//
// The unconsented primary account isn't supported on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SingleClientSecondaryAccountSyncTest,
                       PRE_ReusesSameCacheGuid) {
  ASSERT_TRUE(SetupSyncWithMode(SetupSyncMode::kSyncTransportOnly));

  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  ASSERT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsInitialSyncFeatureSetupComplete());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  syncer::SyncTransportDataPrefs prefs(
      GetProfile(0)->GetPrefs(),
      GetClient(0)->GetGaiaIdHashForPrimaryAccount());
  const std::string cache_guid = prefs.GetCacheGuid();
  ASSERT_FALSE(cache_guid.empty());

  // Save the cache GUID to file to remember after restart, for test
  // verification purposes only.
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::WriteFile(GetTestFilePathForCacheGuid(), cache_guid));
}

IN_PROC_BROWSER_TEST_F(SingleClientSecondaryAccountSyncTest,
                       ReusesSameCacheGuid) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  ASSERT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsInitialSyncFeatureSetupComplete());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  syncer::SyncTransportDataPrefs prefs(
      GetProfile(0)->GetPrefs(),
      GetClient(0)->GetGaiaIdHashForPrimaryAccount());
  ASSERT_FALSE(prefs.GetCacheGuid().empty());

  std::string old_cache_guid;
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(
      base::ReadFileToString(GetTestFilePathForCacheGuid(), &old_cache_guid));
  ASSERT_FALSE(old_cache_guid.empty());

  EXPECT_EQ(old_cache_guid, prefs.GetCacheGuid());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace
