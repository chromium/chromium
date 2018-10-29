// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_driver_switches.h"

namespace {

syncer::ModelTypeSet AllowedTypesInStandaloneTransportMode() {
  // Only some special whitelisted types (and control types) are allowed in
  // standalone transport mode.
  syncer::ModelTypeSet allowed_types(syncer::USER_CONSENTS,
                                     syncer::AUTOFILL_WALLET_DATA);
  allowed_types.PutAll(syncer::ControlTypes());
  return allowed_types;
}

class SingleClientSecondaryAccountSyncTest : public SyncTest {
 public:
  SingleClientSecondaryAccountSyncTest() : SyncTest(SINGLE_CLIENT) {
    features_.InitWithFeatures(
        /*enabled_features=*/{switches::kSyncStandaloneTransport,
                              switches::kSyncSupportSecondaryAccount},
        /*disabled_features=*/{});
  }
  ~SingleClientSecondaryAccountSyncTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    fake_gaia_cookie_manager_factory_ =
        secondary_account_helper::SetUpFakeGaiaCookieManagerService();
  }

  void SetUpOnMainThread() override {
#if defined(OS_CHROMEOS)
    secondary_account_helper::InitNetwork();
#endif  // defined(OS_CHROMEOS)
  }

  Profile* profile() { return GetProfile(0); }

 private:
  base::test::ScopedFeatureList features_;

  secondary_account_helper::ScopedFakeGaiaCookieManagerServiceFactory
      fake_gaia_cookie_manager_factory_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientSecondaryAccountSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientSecondaryAccountSyncTest,
                       DoesNotStartSyncWithStandaloneTransportDisabled) {
  base::test::ScopedFeatureList disable_standalone_transport;
  disable_standalone_transport.InitAndDisableFeature(
      switches::kSyncStandaloneTransport);

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Since standalone transport is disabled, just signing in (without making the
  // account Chrome's primary one) should *not* start the Sync machinery.
  secondary_account_helper::SignInSecondaryAccount(profile(), "user@email.com");
  EXPECT_EQ(syncer::SyncService::TransportState::DISABLED,
            GetSyncService(0)->GetTransportState());
}

IN_PROC_BROWSER_TEST_F(SingleClientSecondaryAccountSyncTest,
                       DoesNotStartSyncWithSecondaryAccountSupportDisabled) {
  base::test::ScopedFeatureList disable_secondary_account_support;
  disable_secondary_account_support.InitAndDisableFeature(
      switches::kSyncSupportSecondaryAccount);

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Since secondary account support is disabled, just signing in (without
  // making the account Chrome's primary one) should *not* start the Sync
  // machinery.
  secondary_account_helper::SignInSecondaryAccount(profile(), "user@email.com");
  EXPECT_EQ(syncer::SyncService::TransportState::DISABLED,
            GetSyncService(0)->GetTransportState());
}

IN_PROC_BROWSER_TEST_F(SingleClientSecondaryAccountSyncTest,
                       StartsSyncTransportOnSignin) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Signing in (without making the account Chrome's primary one or explicitly
  // setting up Sync) should trigger starting the Sync machinery in standalone
  // transport mode.
  secondary_account_helper::SignInSecondaryAccount(profile(), "user@email.com");
  if (browser_defaults::kSyncAutoStarts) {
    EXPECT_EQ(syncer::SyncService::TransportState::INITIALIZING,
              GetSyncService(0)->GetTransportState());
  } else {
    EXPECT_EQ(syncer::SyncService::TransportState::START_DEFERRED,
              GetSyncService(0)->GetTransportState());
  }

  EXPECT_TRUE(GetClient(0)->AwaitSyncSetupCompletion(
      /*skip_passphrase_verification=*/false));

  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  ASSERT_EQ(browser_defaults::kSyncAutoStarts,
            GetSyncService(0)->IsFirstSetupComplete());

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

// ChromeOS doesn't support changes to the primary account after startup, so
// this test doesn't apply.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SingleClientSecondaryAccountSyncTest,
                       SwitchesFromTransportToFeature) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Set up Sync in transport mode for a non-primary account.
  secondary_account_helper::SignInSecondaryAccount(profile(), "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion(
      /*skip_passphrase_verification=*/false));
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive());

  // Simulate the user opting in to full Sync: Make the account primary, and
  // set first-time setup to complete.
  secondary_account_helper::MakeAccountPrimary(profile(), "user@email.com");
  GetSyncService(0)->SetFirstSetupComplete();

  EXPECT_TRUE(GetClient(0)->AwaitSyncSetupCompletion(
      /*skip_passphrase_verification=*/false));
  EXPECT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  EXPECT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  EXPECT_TRUE(GetSyncService(0)->IsSyncFeatureActive());

  // Make sure that some model type which is not allowed in transport-only mode
  // got activated.
  ASSERT_FALSE(AllowedTypesInStandaloneTransportMode().Has(syncer::BOOKMARKS));
  ASSERT_TRUE(
      GetSyncService(0)->GetPreferredDataTypes().Has(syncer::BOOKMARKS));
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::BOOKMARKS));
}
#endif  // !defined(OS_CHROMEOS)

}  // namespace
