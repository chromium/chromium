// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/common/chrome_paths.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/data_sharing/public/features.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#endif

namespace {

using testing::ContainerEq;

syncer::DataTypeSet GetTypesGatedBehindHistoryOptIn() {
  syncer::DataTypeSet types = {syncer::COLLABORATION_GROUP,
                               syncer::HISTORY,
                               syncer::HISTORY_DELETE_DIRECTIVES,
                               syncer::SAVED_TAB_GROUP,
                               syncer::SHARED_TAB_GROUP_DATA,
                               syncer::SHARED_TAB_GROUP_ACCOUNT_DATA,
                               syncer::SESSIONS,
                               syncer::USER_EVENTS};
  if (base::FeatureList::IsEnabled(
          syncer::kSpellcheckSeparateLocalAndAccountDictionaries)) {
    types.Put(syncer::DICTIONARY);
  }
  return types;
}

base::FilePath GetTestFilePathForCacheGuid() {
  base::FilePath user_data_path;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_path);
  return user_data_path.AppendASCII("SyncTestTmpCacheGuid");
}

#if BUILDFLAG(IS_CHROMEOS)
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
#endif  // BUILDFLAG(IS_CHROMEOS)

class SingleClientStandaloneTransportSyncTest : public SyncTest {
 public:
  // TODO(crbug.com/464265742): Reconsider the scope of this file. It was
  // previously parameterized to test both transport-only and full-sync modes,
  // but now only runs in transport mode. It might be worth renaming or merging
  // with other tests.
  SingleClientStandaloneTransportSyncTest() : SyncTest(SINGLE_CLIENT) {
    override_features_.InitWithFeatures(
        /*enabled_features=*/
        {syncer::kSyncEnableContactInfoDataTypeForCustomPassphraseUsers,
         switches::kSyncEnableBookmarksInTransportMode,
#if !BUILDFLAG(IS_ANDROID)
         syncer::kReadingListEnableSyncTransportModeUponSignIn,
#endif  // !BUILDFLAG(IS_ANDROID)
         syncer::kReplaceSyncPromosWithSignInPromos},
        /*disabled_features=*/{});
  }

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return SyncTest::SetupSyncMode::kSyncTransportOnly;
  }

 private:
  base::test::ScopedFeatureList override_features_;
};

// On Chrome OS sync auto-starts on sign-in.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportSyncTest,
                       StartsSyncTransportOnSignin) {
  ASSERT_TRUE(SetupClients());

  // Signing in (without explicitly setting up Sync) should trigger starting the
  // Sync machinery in standalone transport mode.
  ASSERT_TRUE(SetupSyncWithMode(SyncTest::SetupSyncMode::kSyncTransportOnly));

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
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportSyncTest,
                       SwitchesBetweenTransportAndFeature) {
  const syncer::DataType kDataTypeExcludedInTransportMode = syncer::AUTOFILL;
  CHECK(!AllowedTypesInStandaloneTransportMode().Has(
      kDataTypeExcludedInTransportMode));

  ASSERT_TRUE(SetupClients());

  // Setup a primary account, but don't actually enable Sync-the-feature (so
  // that Sync will start in transport mode).
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive());

  syncer::DataTypeSet expected_types =
      Difference(AllowedTypesInStandaloneTransportMode(),
                 GetTypesGatedBehindHistoryOptIn());


  ASSERT_THAT(GetSyncService(0)->GetActiveDataTypes(),
              ContainerEq(expected_types));

  // Turn Sync-the-feature on.
  ASSERT_TRUE(GetClient(0)->SetupSync());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  EXPECT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  EXPECT_TRUE(GetSyncService(0)->IsSyncFeatureActive());
  // Make sure that some data type which is not allowed in transport-only mode
  // got activated.
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kAutofill));
  EXPECT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      kDataTypeExcludedInTransportMode));
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
// Tests the behavior of receiving a "Reset Sync" operation from the dashboard
// while Sync-the-feature is enabled.
IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportSyncTest,
                       HandlesResetFromDashboardWhenSyncActive) {
  ASSERT_TRUE(SetupClients());

  // Set up Sync-the-feature.
  ASSERT_TRUE(SetupSyncWithMode(SyncTest::SetupSyncMode::kSyncTheFeature));
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());

  // Trigger a "Reset Sync" from the dashboard and wait for it to apply. This
  // involves clearing the server data so that the birthday gets incremented.
  GetFakeServer()->ClearServerData();

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

  // There are no immediate plans to launch additional types on ChromeOS, so the
  // list is hardcoded here.
  syncer::DataTypeSet expected_types{
      syncer::DEVICE_INFO,     syncer::NIGORI,
      syncer::USER_CONSENTS,   syncer::SEND_TAB_TO_SELF,
      syncer::SECURITY_EVENTS, syncer::SHARING_MESSAGE};

  if (base::FeatureList::IsEnabled(
          syncer::kSyncSupportAlwaysSyncingPriorityPreferences)) {
    expected_types.Put(syncer::PRIORITY_PREFERENCES);
  }

  if (base::FeatureList::IsEnabled(syncer::kSyncAccountSettings)) {
    expected_types.Put(syncer::ACCOUNT_SETTING);
  }

  EXPECT_THAT(GetSyncService(0)->GetActiveDataTypes(),
              ContainerEq(expected_types));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Regression test for crbug.com/955989 that verifies the cache GUID is not
// reset upon restart of the browser, in standalone transport mode.
IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportSyncTest,
                       PRE_ReusesSameCacheGuid) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(SetupSyncWithMode(SyncTest::SetupSyncMode::kSyncTransportOnly));

  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  // On platforms where Sync starts automatically (in practice, Android and
  // ChromeOS), IsInitialSyncFeatureSetupComplete gets set automatically, and so
  // the full Sync feature will start upon sign-in to a primary account.
#if !BUILDFLAG(IS_CHROMEOS)
  ASSERT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsInitialSyncFeatureSetupComplete());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
#endif  // !BUILDFLAG(IS_CHROMEOS)

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
  ASSERT_TRUE(SetupClients());
  ASSERT_FALSE(GetSyncService(0)->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_NOT_SIGNED_IN));
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  // On platforms where Sync starts automatically (in practice, Android and
  // ChromeOS), IsInitialSyncFeatureSetupComplete gets set automatically, and so
  // the full Sync feature will start upon sign-in to a primary account.
#if !BUILDFLAG(IS_CHROMEOS)
  ASSERT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsInitialSyncFeatureSetupComplete());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
#endif  // !BUILDFLAG(IS_CHROMEOS)

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

IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportSyncTest,
                       DataTypesEnabledInTransportModeWithoutAdditionalOptIns) {
  ASSERT_TRUE(SetupClients());
  // Sign in, without turning on Sync-the-feature.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));

  // Make sure that only the allowed types got activated.
  syncer::DataTypeSet expected_types =
      Difference(AllowedTypesInStandaloneTransportMode(),
                 GetTypesGatedBehindHistoryOptIn());


  EXPECT_THAT(GetSyncService(0)->GetActiveDataTypes(),
              ContainerEq(expected_types));
}

IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportSyncTest,
                       DataTypesEnabledInTransportModeWithHistorySync) {
  // Opting into history is only meaningful if
  // `kReplaceSyncPromosWithSignInPromos` is enabled.

  ASSERT_TRUE(SetupClients());
  // Sign in, without turning on Sync-the-feature.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));

  // Opt in to history and tabs.
  GetSyncService(0)->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  GetSyncService(0)->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, true);
#if !BUILDFLAG(IS_ANDROID)
  GetSyncService(0)->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kSavedTabGroups, true);
#endif  // !BUILDFLAG(IS_ANDROID)

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  // With the history opt in, all types that can run in transport mode should
  // be active.
  syncer::DataTypeSet expected_types = AllowedTypesInStandaloneTransportMode();


  EXPECT_THAT(GetSyncService(0)->GetActiveDataTypes(),
              ContainerEq(expected_types));
}
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportSyncTest,
                       DataTypesEnabledForImplicitSignIn) {
  ASSERT_TRUE(SetupClients());

  // Signing in (without granting sync consent or explicitly setting up Sync)
  // should trigger starting the Sync machinery in standalone transport mode.
  secondary_account_helper::ImplicitSignInUnconsentedAccount(
      GetProfile(0), &test_url_loader_factory_, "user@email.com");

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  // There are no immediate plans to launch additional types to implicitly
  // signed in users, so the list is hardcoded here.
  syncer::DataTypeSet expected_types{syncer::AUTOFILL_WALLET_CREDENTIAL,
                                     syncer::AUTOFILL_WALLET_DATA,
                                     syncer::AUTOFILL_WALLET_USAGE,
                                     syncer::DEVICE_INFO,
                                     syncer::NIGORI,
                                     syncer::PRIORITY_PREFERENCES,
                                     syncer::USER_CONSENTS,
                                     syncer::SEND_TAB_TO_SELF,
                                     syncer::SECURITY_EVENTS,
                                     syncer::SHARING_MESSAGE};

  if (base::FeatureList::IsEnabled(syncer::kSyncAccountSettings)) {
    expected_types.Put(syncer::ACCOUNT_SETTING);
  }

  EXPECT_THAT(GetSyncService(0)->GetActiveDataTypes(),
              ContainerEq(expected_types));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(
    SingleClientStandaloneTransportSyncTest,
    PRE_DataTypesEnabledInTransportModeWithCustomPassphrase) {
  // There's a custom passphrase on the server.
  const syncer::KeyParamsForTesting kKeyParams =
      syncer::Pbkdf2PassphraseKeyParamsForTesting("hunter2");
  SetNigoriInFakeServer(BuildCustomPassphraseNigoriSpecifics(kKeyParams),
                        GetFakeServer());

  ASSERT_TRUE(SetupClients());
  // Sign in, without turning on Sync-the-feature.
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(PassphraseRequiredChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->SetDecryptionPassphrase(
      kKeyParams.password));
  ASSERT_TRUE(PassphraseAcceptedChecker(GetSyncService(0)).Wait());

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  // Make sure that only the allowed types got activated.
  syncer::DataTypeSet expected_types =
      Difference(AllowedTypesInStandaloneTransportMode(),
                 GetTypesGatedBehindHistoryOptIn());

#if !(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
  // After SyncToSignin, CONTACT_INFO are enabled for Win/Mac/Linux, and
  // disabled for other platforms.
  // See `SyncServiceImpl::PassphraseTypeChanged`.
  expected_types.Remove(syncer::CONTACT_INFO);
#endif

  ASSERT_THAT(GetSyncService(0)->GetActiveDataTypes(),
              ContainerEq(expected_types));

  GetSyncService(0)->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  GetSyncService(0)->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kTabs, true);
  GetSyncService(0)->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kSavedTabGroups, true);

  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  syncer::DataTypeSet expected_types_after_history_opt_in =
      AllowedTypesInStandaloneTransportMode();

#if !(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
  // CONTACT_INFO should remain disabled since it's gated by kAutofill.
  expected_types_after_history_opt_in.Remove(syncer::CONTACT_INFO);
#endif

  // With a custom passphrase, the actual HISTORY types are not supported.
  expected_types_after_history_opt_in.Remove(syncer::HISTORY);
  expected_types_after_history_opt_in.Remove(syncer::HISTORY_DELETE_DIRECTIVES);
  expected_types_after_history_opt_in.Remove(syncer::USER_EVENTS);

  // But SESSIONS aka Open Tabs still works.
  CHECK(expected_types_after_history_opt_in.Has(syncer::SESSIONS));

  EXPECT_THAT(GetSyncService(0)->GetActiveDataTypes(),
              ContainerEq(expected_types_after_history_opt_in));

  // Enabling kAutofill to enable CONTACT_INFO.
  GetSyncService(0)->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kAutofill, true);
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  // CONTACT_INFO should be enabled.
  EXPECT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
}

// Tests that a custom passphrase user's opt-in to kAutofill (which happened in
// the PRE_ test) survives a browser restart.
IN_PROC_BROWSER_TEST_F(SingleClientStandaloneTransportSyncTest,
                       DataTypesEnabledInTransportModeWithCustomPassphrase) {
  ASSERT_TRUE(SetupClients());

  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  // CONTACT_INFO is controlled by the kAutofill datatype. With a custom
  // passphrase, it's additionally guarded by
  // kSyncEnableContactInfoDataTypeForCustomPassphraseUsers. The feature is
  // enabled by default on mobile, but disabled on desktop. This test enables
  // it on all platforms.
  EXPECT_TRUE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::CONTACT_INFO));
}

// ReplaceSyncWithSigninMigrationSyncTest is
// disabled on CrOS as the signed in, non-syncing state does not exist.
#if !BUILDFLAG(IS_CHROMEOS)
// A test fixture to cover migration behavior: In PRE_ tests, the
// kReplaceSyncPromosWithSignInPromos is *dis*abled, in non-PRE_ tests it is
// *en*abled.
// This test intends to test the mobile migration behavior, but runs on desktop.
// Desktop and mobile have different behaviors, and as a consequence is test is
// only an approximation.
class ReplaceSyncWithSigninMigrationSyncTest : public SyncTest {
 public:
  ReplaceSyncWithSigninMigrationSyncTest() : SyncTest(SINGLE_CLIENT) {
    // Various features that are required for types to be supported in transport
    // mode are unconditionally enabled.
    std::vector<base::test::FeatureRef> enabled_features = {
        // This feature would not be needed on mobile, but on desktop it is a
        // prerequisite to account storage for preferences.
        syncer::kSeparateLocalAndAccountSearchEngines,
        switches::kSyncEnableBookmarksInTransportMode};
#if !BUILDFLAG(IS_ANDROID)
    enabled_features.push_back(
        syncer::kReadingListEnableSyncTransportModeUponSignIn);
#endif
    default_features_.InitWithFeatures(enabled_features,
                                       /*disabled_features=*/{});

    // The Sync-to-Signin feature is only enabled in non-PRE_ tests.
    sync_to_signin_feature_.InitWithFeatureStates(
        {{syncer::kReplaceSyncPromosWithSignInPromos, !content::IsPreTest()},
         {switches::kEnablePreferencesAccountStorage, !content::IsPreTest()}});
  }
  ~ReplaceSyncWithSigninMigrationSyncTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return SyncTest::SetupSyncMode::kSyncTheFeature;
  }

 private:
  base::test::ScopedFeatureList default_features_;
  base::test::ScopedFeatureList sync_to_signin_feature_;
};

IN_PROC_BROWSER_TEST_F(ReplaceSyncWithSigninMigrationSyncTest,
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

IN_PROC_BROWSER_TEST_F(ReplaceSyncWithSigninMigrationSyncTest,
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
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Preferences is supported in transport mode and is enabled by the migration.
  EXPECT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPreferences));
#else
  // Preferences is supported in transport mode now but should've been disabled
  // by the migration.
  EXPECT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPreferences));
  // But it's supported now, and the user can set it to true.
  GetSyncService(0)->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPreferences, true);
  EXPECT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPreferences));
#endif
}

IN_PROC_BROWSER_TEST_F(ReplaceSyncWithSigninMigrationSyncTest,
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

IN_PROC_BROWSER_TEST_F(ReplaceSyncWithSigninMigrationSyncTest,
                       MigratesSignedInCustomPassphraseUser) {
  ASSERT_TRUE(SetupClients());

  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  ASSERT_EQ(GetSyncService(0)->GetUserSettings()->GetPassphraseType(),
            syncer::PassphraseType::kCustomPassphrase);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Preferences is supported now and is enabled by the migration (same as
  // for non-custom-passphrase users).
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPreferences));
#else
  // Preferences is supported now, but got disabled by the migration (same as
  // for non-custom-passphrase users).
  ASSERT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPreferences));
#endif
  // Autofill should've been disabled specifically for custom passphrase users.
  EXPECT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kAutofill));
  // Payments should continue to be enabled.
  ASSERT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kPayments));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// On ChromeOS, there exists no sync setup incomplete state.
#if !BUILDFLAG(IS_CHROMEOS)
class SyncSetupIncompleteMigrationSyncTest : public SyncTest {
 public:
  SyncSetupIncompleteMigrationSyncTest() : SyncTest(SINGLE_CLIENT) {
    std::vector<base::test::FeatureRef> features = {
        switches::kMigrateOutOfSyncSetupIncompleteState,
        syncer::kUnoPhase2FollowUp};
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features = {
        // This is disabled because if enabled, the sync to sign-in migration is
        // triggered which also migrates the sync setup incomplete users.
        switches::kMigrateSyncingUserToSignedIn};
    if (content::IsPreTest()) {
      disabled_features.insert(disabled_features.end(), features.begin(),
                               features.end());
    } else {
      enabled_features.insert(enabled_features.end(), features.begin(),
                              features.end());
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SyncSetupIncompleteMigrationSyncTest,
                       PRE_MigratesSyncSetupIncompleteUser) {
  ASSERT_TRUE(SetupClients());

  // Using `SetupSyncWithCustomSettings` with `base::DoNothing()` doesn't set
  // the sync-setup-complete bit.
  ASSERT_TRUE(GetClient(0)->SetupSyncWithCustomSettings(base::DoNothing()));
  ASSERT_TRUE(GetSyncService(0)->HasSyncConsent());
  ASSERT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsInitialSyncFeatureSetupComplete());
}

IN_PROC_BROWSER_TEST_F(SyncSetupIncompleteMigrationSyncTest,
                       MigratesSyncSetupIncompleteUser) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  // The user is not syncing anymore.
  EXPECT_FALSE(GetSyncService(0)->HasSyncConsent());
  // History is toggled off.
  EXPECT_FALSE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));
}

IN_PROC_BROWSER_TEST_F(SyncSetupIncompleteMigrationSyncTest,
                       PRE_DoesNotMigrateSyncSetupCompleteUser) {
  ASSERT_TRUE(SetupClients());

  ASSERT_TRUE(GetClient(0)->SetupSyncWithCustomSettings(base::DoNothing()));
  ASSERT_TRUE(GetSyncService(0)->HasSyncConsent());

  GetSyncService(0)->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
  ASSERT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsInitialSyncFeatureSetupComplete());
}

IN_PROC_BROWSER_TEST_F(SyncSetupIncompleteMigrationSyncTest,
                       DoesNotMigrateSyncSetupCompleteUser) {
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  EXPECT_TRUE(GetSyncService(0)->HasSyncConsent());
  EXPECT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsInitialSyncFeatureSetupComplete());
  EXPECT_TRUE(GetSyncService(0)->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory));
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace
