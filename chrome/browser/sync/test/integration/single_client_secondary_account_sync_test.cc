// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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

#if !BUILDFLAG(IS_CHROMEOS_ASH)
base::FilePath GetTestFilePathForCacheGuid() {
  base::FilePath user_data_path;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_path);
  return user_data_path.AppendASCII("SyncTestTmpCacheGuid");
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

class SingleClientSecondaryAccountSyncTest : public SyncTest {
 public:
  SingleClientSecondaryAccountSyncTest() : SyncTest(SINGLE_CLIENT) {}

  SingleClientSecondaryAccountSyncTest(
      const SingleClientSecondaryAccountSyncTest&) = delete;
  SingleClientSecondaryAccountSyncTest& operator=(
      const SingleClientSecondaryAccountSyncTest&) = delete;

  ~SingleClientSecondaryAccountSyncTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    SyncTest::SetUpInProcessBrowserTestFixture();

    test_signin_client_subscription_ =
        secondary_account_helper::SetUpSigninClient(&test_url_loader_factory_);
  }

  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    secondary_account_helper::InitNetwork();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    SyncTest::SetUpOnMainThread();
  }

  Profile* profile() { return GetProfile(0); }

 private:
  base::CallbackListSubscription test_signin_client_subscription_;
};

// The unconsented primary account isn't supported on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientSecondaryAccountSyncTest,
                       StartsSyncTransportOnSignin) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Signing in (without granting sync consent or explicitly setting up Sync)
  // should trigger starting the Sync machinery in standalone transport mode.
  secondary_account_helper::SignInUnconsentedAccount(
      profile(), &test_url_loader_factory_, "user@email.com");

  EXPECT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  ASSERT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsInitialSyncFeatureSetupComplete());

  EXPECT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  EXPECT_FALSE(GetSyncService(0)->IsSyncFeatureActive());

  // Make sure that only the allowed types got activated. Note that, depending
  // on some other feature flags, not all of the allowed types are necessarily
  // active, and that's okay.
  syncer::DataTypeSet bad_types =
      base::Difference(GetSyncService(0)->GetActiveDataTypes(),
                       AllowedTypesInStandaloneTransportMode());
  EXPECT_TRUE(bad_types.empty()) << syncer::DataTypeSetToDebugString(bad_types);
}
#else
IN_PROC_BROWSER_TEST_F(SingleClientSecondaryAccountSyncTest,
                       DoesNotStartSyncTransportOnSignin) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Signing in (without explicitly setting up Sync) should do nothing here,
  // since we're on a platform where the unconsented primary account is not
  // supported.
  secondary_account_helper::SignInUnconsentedAccount(
      profile(), &test_url_loader_factory_, "user@email.com");
  EXPECT_EQ(syncer::SyncService::TransportState::DISABLED,
            GetSyncService(0)->GetTransportState());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// ChromeOS doesn't support changes to the primary account after startup, so
// this test doesn't apply.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientSecondaryAccountSyncTest,
                       SwitchesFromTransportToFeature) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Set up Sync in transport mode for an unconsented account.
  secondary_account_helper::SignInUnconsentedAccount(
      profile(), &test_url_loader_factory_, "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive());

  // Simulate the user opting in to full Sync, and set first-time setup to
  // complete.
  secondary_account_helper::GrantSyncConsent(profile(), "user@email.com");
  GetSyncService(0)->SetSyncFeatureRequested();
  GetSyncService(0)->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);

  EXPECT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  EXPECT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  EXPECT_TRUE(GetSyncService(0)->IsSyncFeatureActive());

  // Make sure that some data type which is not allowed in transport-only mode
  // got activated.
  ASSERT_FALSE(AllowedTypesInStandaloneTransportMode().Has(syncer::BOOKMARKS));
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kBookmarks));
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::BOOKMARKS));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Regression test for crbug.com/955989 that verifies the cache GUID is not
// reset upon restart of the browser, in standalone transport mode with
// unconsented accounts.
//
// The unconsented primary account isn't supported on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientSecondaryAccountSyncTest,
                       PRE_ReusesSameCacheGuid) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  secondary_account_helper::SignInUnconsentedAccount(
      profile(), &test_url_loader_factory_, "user@email.com");
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
  const std::string cache_guid = prefs.GetCacheGuid();
  ASSERT_FALSE(cache_guid.empty());

  // Save the cache GUID to file to remember after restart, for test
  // verification purposes only.
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(base::WriteFile(GetTestFilePathForCacheGuid(), cache_guid));
}

IN_PROC_BROWSER_TEST_F(SingleClientSecondaryAccountSyncTest,
                       ReusesSameCacheGuid) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
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
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
