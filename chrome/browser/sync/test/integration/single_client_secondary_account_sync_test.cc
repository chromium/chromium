// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/glue/sync_transport_data_prefs.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"

namespace {

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

}  // namespace
