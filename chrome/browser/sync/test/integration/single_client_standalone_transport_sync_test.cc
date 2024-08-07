// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/chrome_paths.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/service/glue/sync_transport_data_prefs.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_server_nigori_helper.h"
#include "components/sync/test/nigori_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

namespace {

#if !BUILDFLAG(IS_ANDROID)
base::FilePath GetTestFilePathForCacheGuid() {
  base::FilePath user_data_path;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_path);
  return user_data_path.AppendASCII("SyncTestTmpCacheGuid");
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
class SyncDisabledViaDashboardChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncDisabledViaDashboardChecker(syncer::SyncServiceImpl* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for sync disabled by dashboard";
    return service()->GetUserSettings()->IsSyncFeatureDisabledViaDashboard();
  }
};
#else
class SyncConsentDisabledChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncConsentDisabledChecker(syncer::SyncServiceImpl* service)
      : SingleClientStatusChangeChecker(service) {}

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for sync consent being disabled";
    return !service()->HasSyncConsent();
  }
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class SingleClientStandaloneTransportSyncTest : public SyncTest {
 public:
  SingleClientStandaloneTransportSyncTest() : SyncTest(SINGLE_CLIENT) {}
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

  // IsInitialSyncFeatureSetupComplete should remain false. It only gets set
  // during the Sync setup flow, either by the Sync confirmation dialog or by
  // the settings page if going through the advanced settings flow.
  EXPECT_FALSE(GetSyncService(0)
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportSyncTest,
                       SwitchesBetweenTransportAndFeature) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Setup a primary account, but don't actually enable Sync-the-feature (so
  // that Sync will start in transport mode).
  ASSERT_TRUE(
      GetClient(0)->SignInPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive());

  syncer::DataTypeSet bad_types =
      base::Difference(GetSyncService(0)->GetActiveDataTypes(),
                       AllowedTypesInStandaloneTransportMode());
  EXPECT_TRUE(bad_types.empty()) << syncer::DataTypeSetToDebugString(bad_types);

  // Turn Sync-the-feature on.
  ASSERT_TRUE(GetClient(0)->SetupSync());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
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
#endif  // BUILDFLAG(IS_ANDROID)

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Ash, the primary account should remain, and Sync should start up
  // again in standalone transport mode, but report this specific case via
  // IsSyncFeatureDisabledViaDashboard().
  EXPECT_TRUE(SyncDisabledViaDashboardChecker(GetSyncService(0)).Wait());
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
  EXPECT_TRUE(SyncConsentDisabledChecker(GetSyncService(0)).Wait());
  // Note: In real life, on platforms other than Ash and Mobile the account
  // would remain as an *unconsented* primary account, and so Sync would start
  // up again in standalone transport mode. However, since we haven't set up
  // cookies in this test, the account is *not* considered primary anymore
  // (not even "unconsented").
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// TODO(crbug.com/40200835): Android currently doesn't support PRE_ tests.
#if !BUILDFLAG(IS_ANDROID)
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
  // ChromeOS), IsInitialSyncFeatureSetupComplete gets set automatically, and so
  // the full Sync feature will start upon sign-in to a primary account.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsInitialSyncFeatureSetupComplete());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  syncer::SyncTransportDataPrefs transport_data_prefs(
      GetProfile(0)->GetPrefs(),
      GetClient(0)->GetGaiaIdHashForPrimaryAccount());
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
  // ChromeOS), IsInitialSyncFeatureSetupComplete gets set automatically, and so
  // the full Sync feature will start upon sign-in to a primary account.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  ASSERT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsInitialSyncFeatureSetupComplete());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  syncer::SyncTransportDataPrefs transport_data_prefs(
      GetProfile(0)->GetPrefs(),
      GetClient(0)->GetGaiaIdHashForPrimaryAccount());
  ASSERT_FALSE(transport_data_prefs.GetCacheGuid().empty());

  std::string old_cache_guid;
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(
      base::ReadFileToString(GetTestFilePathForCacheGuid(), &old_cache_guid));
  ASSERT_FALSE(old_cache_guid.empty());

  EXPECT_EQ(old_cache_guid, transport_data_prefs.GetCacheGuid());
}
#endif  // BUILDFLAG(IS_ANDROID)

class SingleClientStandaloneTransportWithReplaceSyncWithSigninSyncTest
    : public SingleClientStandaloneTransportSyncTest {
 public:
  SingleClientStandaloneTransportWithReplaceSyncWithSigninSyncTest() {
    override_features_.InitWithFeatures(
        /*enabled_features=*/
        {switches::kExplicitBrowserSigninUIOnDesktop,
         syncer::kEnablePreferencesAccountStorage,
         syncer::kSyncEnableContactInfoDataTypeInTransportMode,
         syncer::kSyncEnableContactInfoDataTypeForCustomPassphraseUsers,
         syncer::kReplaceSyncPromosWithSignInPromos,
         syncer::kSyncAutofillWalletCredentialData},
        /*disabled_features=*/{});
  }
  ~SingleClientStandaloneTransportWithReplaceSyncWithSigninSyncTest() override =
      default;

  bool WaitForPassphraseRequired() {
    return PassphraseRequiredChecker(GetSyncService(0)).Wait();
  }

  bool WaitForPassphraseAccepted() {
    return PassphraseAcceptedChecker(GetSyncService(0)).Wait();
  }

 private:
  base::test::ScopedFeatureList override_features_;
};

IN_PROC_BROWSER_TEST_F(
    SingleClientStandaloneTransportWithReplaceSyncWithSigninSyncTest,
    DataTypesEnabledInTransportMode) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  // Sign in, without turning on Sync-the-feature.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Opt in to history and tabs.
  GetSyncService(0)->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  GetSyncService(0)->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, true);
  // Preferences are opted-into by default.
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPreferences));

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  // With `kReplaceSyncPromosWithSignInPromos`, all the history-related types
  // should be enabled in transport mode.
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::HISTORY));
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::HISTORY_DELETE_DIRECTIVES));
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::SESSIONS));
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::USER_EVENTS));

  // With `kReplaceSyncPromosWithSignInPromos`, both PREFERENCES and
  // PRIORITY_PREFERENCES should be enabled in transport mode.
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::PRIORITY_PREFERENCES));

  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));
  EXPECT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_CREDENTIAL));
}

// TODO(crbug.com/40200835): Android currently doesn't support PRE_ tests.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(
    SingleClientStandaloneTransportWithReplaceSyncWithSigninSyncTest,
    PRE_DataTypesEnabledInTransportModeWithCustomPassphrase) {
  // There's a custom passphrase on the server.
  const syncer::KeyParamsForTesting kKeyParams =
      syncer::Pbkdf2PassphraseKeyParamsForTesting("hunter2");
  SetNigoriInFakeServer(BuildCustomPassphraseNigoriSpecifics(kKeyParams),
                        GetFakeServer());

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  // Sign in, without turning on Sync-the-feature.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Opt in to history and tabs.
  GetSyncService(0)->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  GetSyncService(0)->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, true);
  // Preferences are opted-into by default.
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPreferences));

  ASSERT_TRUE(WaitForPassphraseRequired());
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->SetDecryptionPassphrase(
      kKeyParams.password));
  ASSERT_TRUE(WaitForPassphraseAccepted());

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  // With a custom passphrase, the actual HISTORY types are not supported.
  EXPECT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::HISTORY));
  EXPECT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::HISTORY_DELETE_DIRECTIVES));
  EXPECT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::USER_EVENTS));
  // But SESSIONS aka Open Tabs still works.
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::SESSIONS));

  // With `kReplaceSyncPromosWithSignInPromos`, both PREFERENCES and
  // PRIORITY_PREFERENCES should be enabled in transport mode.
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::PRIORITY_PREFERENCES));

  // CONTACT_INFO should be disabled by default for explicit-passphrase users.
  EXPECT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));

  // Enabling kAutofill to enable CONTACT_INFO.
  GetSyncService(0)->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kAutofill, true);

  ASSERT_NE(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // CONTACT_INFO should be enabled.
  EXPECT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
}

// Tests that a custom passphrase user's opt-in to kAutofill (which happened in
// the PRE_ test) survives a browser restart.
IN_PROC_BROWSER_TEST_F(
    SingleClientStandaloneTransportWithReplaceSyncWithSigninSyncTest,
    DataTypesEnabledInTransportModeWithCustomPassphrase) {
  ASSERT_TRUE(SetupClients());

  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  // CONTACT_INFO should be enabled after restarting.
  EXPECT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
}
#endif  // BUILDFLAG(IS_ANDROID)

class SingleClientStandaloneTransportWithoutReplaceSyncWithSigninSyncTest
    : public SingleClientStandaloneTransportSyncTest {
 public:
  SingleClientStandaloneTransportWithoutReplaceSyncWithSigninSyncTest() {
    override_features_.InitWithFeatures(
        /*enabled_features=*/{syncer::kEnablePreferencesAccountStorage},
        /*disabled_features=*/{syncer::kReplaceSyncPromosWithSignInPromos});
  }
  ~SingleClientStandaloneTransportWithoutReplaceSyncWithSigninSyncTest()
      override = default;

 private:
  base::test::ScopedFeatureList override_features_;
};

IN_PROC_BROWSER_TEST_F(
    SingleClientStandaloneTransportWithoutReplaceSyncWithSigninSyncTest,
    DataTypesNotEnabledInTransportMode) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  // Sign in, without turning on Sync-the-feature.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Without `kReplaceSyncPromosWithSignInPromos`, neither History/Tabs nor
  // Preferences are supported in transport mode, so they're reported as not
  // selected even if the user explicitly tries to turn them on.
  syncer::UserSelectableTypeSet types =
      GetSyncService(0)->GetUserSettings()->GetRegisteredSelectableTypes();
  ASSERT_TRUE(types.HasAll({syncer::UserSelectableType::kHistory,
                            syncer::UserSelectableType::kTabs,
                            syncer::UserSelectableType::kPreferences}));
  GetSyncService(0)->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/true, types);
  ASSERT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));
  ASSERT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kTabs));
  ASSERT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPreferences));

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  // Without `kReplaceSyncPromosWithSignInPromos`, none of the history-related
  // types should be active in transport mode (even if the user has opted in).
  EXPECT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::HISTORY));
  EXPECT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::HISTORY_DELETE_DIRECTIVES));
  EXPECT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::SESSIONS));
  EXPECT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::USER_EVENTS));

  // Without `kReplaceSyncPromosWithSignInPromos`, neither PREFERENCES nor
  // PRIORITY_PREFERENCES should be active in transport mode (even if the user
  // has opted in).
  EXPECT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::PREFERENCES));
  EXPECT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::PRIORITY_PREFERENCES));
}

// TODO(crbug.com/40145099): Android currently doesn't support PRE_ tests and
// all of these are.
#if !BUILDFLAG(IS_ANDROID)
// A test fixture to cover migration behavior: In PRE_ tests, the
// kReplaceSyncPromosWithSignInPromos is *dis*abled, in non-PRE_ tests it is
// *en*abled.
class SingleClientStandaloneTransportReplaceSyncWithSigninMigrationSyncTest
    : public SingleClientStandaloneTransportSyncTest {
 public:
  SingleClientStandaloneTransportReplaceSyncWithSigninMigrationSyncTest() {
    // Various features that are required for types to be supported in transport
    // mode are unconditionally enabled.
    default_features_.InitWithFeatures(
        /*enabled_features=*/
        {switches::kExplicitBrowserSigninUIOnDesktop,
         syncer::kReadingListEnableSyncTransportModeUponSignIn,
         syncer::kSyncEnableContactInfoDataTypeInTransportMode,
         syncer::kSyncEnableBookmarksInTransportMode,
         syncer::kEnablePreferencesAccountStorage},
        /*disabled_features=*/{});

    // The Sync-to-Signin feature is only enabled in non-PRE_ tests.
    sync_to_signin_feature_.InitWithFeatureState(
        syncer::kReplaceSyncPromosWithSignInPromos, !content::IsPreTest());
  }
  ~SingleClientStandaloneTransportReplaceSyncWithSigninMigrationSyncTest()
      override = default;

 private:
  base::test::ScopedFeatureList default_features_;
  base::test::ScopedFeatureList sync_to_signin_feature_;
};

IN_PROC_BROWSER_TEST_F(
    SingleClientStandaloneTransportReplaceSyncWithSigninMigrationSyncTest,
    PRE_MigratesSignedInUser) {
  ASSERT_TRUE(SetupClients());
  // Sign in, without turning on Sync-the-feature.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // E.g. Autofill and Payments are enabled by default (based on the
  // Features set by the fixture).
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kAutofill));
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPayments));
  // Preferences is not supported in transport mode (based on the Features
  // set by the fixture), so it should be reported as non-selected.
  ASSERT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPreferences));

  // The user disabled Payments, e.g. via a temporary toggle predating the
  // "unified settings panel" introduced by kReplaceSyncPromosWithSignInPromos.
  // Note that SyncUserSettings is already reading/writing from/to the
  // account-scoped prefs!
  GetSyncService(0)->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPayments, false);

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientStandaloneTransportReplaceSyncWithSigninMigrationSyncTest,
    MigratesSignedInUser) {
  ASSERT_TRUE(SetupClients());

  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  // Autofill and Payments should still be enabled and disabled, respectively.
  EXPECT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kAutofill));
  EXPECT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPayments));
  // Preferences is supported in transport mode now but should've been disabled
  // by the migration.
  EXPECT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPreferences));
  // But it's supported now, and the user can set it to true.
  GetSyncService(0)->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPreferences, true);
  EXPECT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPreferences));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientStandaloneTransportReplaceSyncWithSigninMigrationSyncTest,
    PRE_MigratesSignedInCustomPassphraseUser) {
  ASSERT_TRUE(SetupClients());
  // Sign in, without turning on Sync-the-feature.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  GetSyncService(0)->GetUserSettings()->SetEncryptionPassphrase("hunter2");
  ASSERT_TRUE(PassphraseTypeChecker(GetSyncService(0),
                                    syncer::PassphraseType::kCustomPassphrase)
                  .Wait());

  // E.g. Payments and Autofill are enabled by default.
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPayments));
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kAutofill));
  // Preferences is not supported without `kReplaceSyncPromosWithSignInPromos`.
  ASSERT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPreferences));
}

IN_PROC_BROWSER_TEST_F(
    SingleClientStandaloneTransportReplaceSyncWithSigninMigrationSyncTest,
    MigratesSignedInCustomPassphraseUser) {
  ASSERT_TRUE(SetupClients());

  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  ASSERT_EQ(GetSyncService(0)->GetUserSettings()->GetPassphraseType(),
            syncer::PassphraseType::kCustomPassphrase);

  // Preferences is supported now, but got disabled by the migration (same as
  // for non-custom-passphrase users).
  ASSERT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPreferences));
  // Autofill should've been disabled specifically for custom passphrase users.
  EXPECT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kAutofill));
  // Payments should continue to be enabled.
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPayments));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
