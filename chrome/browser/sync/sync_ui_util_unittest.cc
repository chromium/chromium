// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_ui_util.h"

#include <set>
#include <string>
#include <utility>

#include "base/test/task_environment.h"
#include "build/chromeos_buildflags.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#endif

namespace {

MATCHER_P4(SyncStatusLabelsMatch,
           message_type,
           status_label_string_id,
           button_string_id,
           action_type,
           "") {
  if (arg.message_type != message_type) {
    *result_listener << "Wrong message type";
    return false;
  }
  if (arg.status_label_string_id != status_label_string_id) {
    *result_listener << "Wrong status label";
    return false;
  }
  if (arg.button_string_id != button_string_id) {
    *result_listener << "Wrong button string";
    return false;
  }
  if (arg.action_type != action_type) {
    *result_listener << "Wrong action type";
    return false;
  }
  return true;
}

// A number of distinct states of the SyncService can be generated for tests.
enum DistinctState {
  STATUS_CASE_SETUP_IN_PROGRESS,
  STATUS_CASE_SETUP_ERROR,
  STATUS_CASE_AUTH_ERROR,
  STATUS_CASE_PROTOCOL_ERROR,
  STATUS_CASE_CONFIRM_SYNC_SETTINGS,
  STATUS_CASE_PASSPHRASE_ERROR,
  STATUS_CASE_TRUSTED_VAULT_KEYS_ERROR,
  STATUS_CASE_TRUSTED_VAULT_RECOVERABILITY_ERROR,
  STATUS_CASE_SYNCED,
  STATUS_CASE_SYNC_DISABLED_BY_POLICY,
  STATUS_CASE_SYNC_RESET_FROM_DASHBOARD,
  NUMBER_OF_STATUS_CASES
};

const char kTestUser[] = "test_user@test.com";

// Sets up a TestSyncService to emulate one of a number of distinct cases in
// order to perform tests on the generated messages. Returns the expected value
// GetSyncStatusLabels should return.
// TODO(mastiz): Split the cases below to separate tests.
SyncStatusLabels SetUpDistinctCase(
    syncer::TestSyncService* service,
    signin::IdentityTestEnvironment* test_environment,
    DistinctState case_number) {
  switch (case_number) {
    case STATUS_CASE_SETUP_IN_PROGRESS: {
      service->SetFirstSetupComplete(false);
      service->SetSetupInProgress(true);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      return {SyncStatusMessageType::kPreSynced, IDS_SYNC_SETUP_IN_PROGRESS,
              IDS_SETTINGS_EMPTY_STRING, SyncStatusActionType::kNoAction};
    }
    case STATUS_CASE_SETUP_ERROR: {
      service->SetFirstSetupComplete(false);
      service->SetSetupInProgress(false);
      service->SetDisableReasons(
          syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      return {
        SyncStatusMessageType::kSyncError,
#if !BUILDFLAG(IS_CHROMEOS_ASH)
            IDS_SYNC_STATUS_UNRECOVERABLE_ERROR,
#else
            IDS_SYNC_STATUS_UNRECOVERABLE_ERROR_NEEDS_SIGNOUT,
#endif
            IDS_SYNC_RELOGIN_BUTTON, SyncStatusActionType::kReauthenticate
      };
    }
    case STATUS_CASE_AUTH_ERROR: {
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetPassphraseRequired(false);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());

      // Make sure to fail authentication with an error in this case.
      CoreAccountId account_id =
          test_environment->identity_manager()->GetPrimaryAccountId(
              signin::ConsentLevel::kSync);
      test_environment->SetRefreshTokenForPrimaryAccount();
      service->SetAccountInfo(
          test_environment->identity_manager()->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSync));
      test_environment->UpdatePersistentErrorOfRefreshTokenForAccount(
          account_id,
          GoogleServiceAuthError(GoogleServiceAuthError::State::SERVICE_ERROR));
      service->SetDisableReasons(syncer::SyncService::DisableReasonSet());
      return {SyncStatusMessageType::kSyncError, IDS_SYNC_RELOGIN_ERROR,
              IDS_SYNC_RELOGIN_BUTTON, SyncStatusActionType::kReauthenticate};
    }
    case STATUS_CASE_PROTOCOL_ERROR: {
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetPassphraseRequired(false);
      syncer::SyncProtocolError protocol_error;
      protocol_error.action = syncer::UPGRADE_CLIENT;
      syncer::SyncStatus status;
      status.sync_protocol_error = protocol_error;
      service->SetDetailedSyncStatus(false, status);
      service->SetDisableReasons(syncer::SyncService::DisableReasonSet());
      return {SyncStatusMessageType::kSyncError, IDS_SYNC_UPGRADE_CLIENT,
              IDS_SYNC_UPGRADE_CLIENT_BUTTON,
              SyncStatusActionType::kUpgradeClient};
    }
    case STATUS_CASE_CONFIRM_SYNC_SETTINGS: {
      service->SetFirstSetupComplete(false);
      service->SetPassphraseRequired(false);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      return {SyncStatusMessageType::kSyncError,
              IDS_SYNC_SETTINGS_NOT_CONFIRMED,
              IDS_SYNC_ERROR_USER_MENU_CONFIRM_SYNC_SETTINGS_BUTTON,
              SyncStatusActionType::kConfirmSyncSettings};
    }
    case STATUS_CASE_PASSPHRASE_ERROR: {
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      service->SetDisableReasons(syncer::SyncService::DisableReasonSet());
      service->SetPassphraseRequired(true);
      service->SetPassphraseRequiredForPreferredDataTypes(true);
      return {SyncStatusMessageType::kSyncError, IDS_SYNC_STATUS_NEEDS_PASSWORD,
              IDS_SYNC_STATUS_NEEDS_PASSWORD_BUTTON,
              SyncStatusActionType::kEnterPassphrase};
    }
    case STATUS_CASE_TRUSTED_VAULT_KEYS_ERROR:
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      service->SetDisableReasons(syncer::SyncService::DisableReasonSet());
      service->SetPassphraseRequired(false);
      service->SetTrustedVaultKeyRequiredForPreferredDataTypes(true);
      return {SyncStatusMessageType::kPasswordsOnlySyncError,
              IDS_SETTINGS_EMPTY_STRING, IDS_SYNC_STATUS_NEEDS_KEYS_BUTTON,
              SyncStatusActionType::kRetrieveTrustedVaultKeys};
    case STATUS_CASE_TRUSTED_VAULT_RECOVERABILITY_ERROR:
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      service->SetDisableReasons(syncer::SyncService::DisableReasonSet());
      service->SetPassphraseRequired(false);
      service->SetTrustedVaultRecoverabilityDegraded(true);
      return {SyncStatusMessageType::kSynced, IDS_SYNC_ACCOUNT_SYNCING,
              IDS_SETTINGS_EMPTY_STRING, SyncStatusActionType::kNoAction};
    case STATUS_CASE_SYNCED: {
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      service->SetDisableReasons(syncer::SyncService::DisableReasonSet());
      service->SetPassphraseRequired(false);
      return {SyncStatusMessageType::kSynced, IDS_SYNC_ACCOUNT_SYNCING,
              IDS_SETTINGS_EMPTY_STRING, SyncStatusActionType::kNoAction};
    }
    case STATUS_CASE_SYNC_DISABLED_BY_POLICY: {
      service->SetDisableReasons(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
      service->SetFirstSetupComplete(false);
      service->SetTransportState(syncer::SyncService::TransportState::DISABLED);
      service->SetPassphraseRequired(false);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      return {SyncStatusMessageType::kSynced,
              IDS_SIGNED_IN_WITH_SYNC_DISABLED_BY_POLICY,
              IDS_SETTINGS_EMPTY_STRING, SyncStatusActionType::kNoAction};
    }
    case STATUS_CASE_SYNC_RESET_FROM_DASHBOARD: {
      // Note: On desktop, if there is a primary account, then
      // DISABLE_REASON_USER_CHOICE can only occur if Sync was reset from the
      // dashboard, and the UI treats it as such.
      service->SetDisableReasons(
          syncer::SyncService::DISABLE_REASON_USER_CHOICE);
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetPassphraseRequired(false);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      return {SyncStatusMessageType::kSyncError,
              IDS_SIGNED_IN_WITH_SYNC_STOPPED_VIA_DASHBOARD,
              IDS_SETTINGS_EMPTY_STRING, SyncStatusActionType::kNoAction};
    }
    case NUMBER_OF_STATUS_CASES:
      NOTREACHED();
  }
  return {SyncStatusMessageType::kPreSynced, IDS_SETTINGS_EMPTY_STRING,
          IDS_SETTINGS_EMPTY_STRING, SyncStatusActionType::kNoAction};
}

// This test ensures that each distinctive SyncService status will return a
// proper status and link messages from GetSyncStatusLabels().
TEST(SyncUIUtilTest, DistinctCasesReportProperMessages) {
  base::test::TaskEnvironment task_environment;

  for (int index = 0; index != NUMBER_OF_STATUS_CASES; index++) {
    syncer::TestSyncService service;
    signin::IdentityTestEnvironment environment;

    // Need a primary account signed in before calling SetUpDistinctCase().
    environment.MakePrimaryAccountAvailable(kTestUser,
                                            signin::ConsentLevel::kSync);

    SyncStatusLabels expected_labels = SetUpDistinctCase(
        &service, &environment, static_cast<DistinctState>(index));

    EXPECT_THAT(
        GetSyncStatusLabels(&service, environment.identity_manager(),
                            /*is_user_clear_primary_account_allowed=*/true),
        SyncStatusLabelsMatch(expected_labels.message_type,
                              expected_labels.status_label_string_id,
                              expected_labels.button_string_id,
                              expected_labels.action_type));
  }
}

TEST(SyncUIUtilTest, UnrecoverableErrorWithActionableProtocolError) {
  base::test::TaskEnvironment task_environment;
  syncer::TestSyncService service;
  signin::IdentityTestEnvironment environment;

  environment.SetPrimaryAccount(kTestUser, signin::ConsentLevel::kSync);
  service.SetFirstSetupComplete(true);
  service.SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);

  // First time action is not set. We should get unrecoverable error.
  service.SetDetailedSyncStatus(true, syncer::SyncStatus());

  // Expect the generic unrecoverable error action which is to reauthenticate.
  int unrecoverable_error =
#if !BUILDFLAG(IS_CHROMEOS_ASH)
      IDS_SYNC_STATUS_UNRECOVERABLE_ERROR;
#else
      IDS_SYNC_STATUS_UNRECOVERABLE_ERROR_NEEDS_SIGNOUT;
#endif
  EXPECT_THAT(
      GetSyncStatusLabels(&service, environment.identity_manager(),
                          /*is_user_clear_primary_account_allowed=*/true),
      SyncStatusLabelsMatch(SyncStatusMessageType::kSyncError,
                            unrecoverable_error, IDS_SYNC_RELOGIN_BUTTON,
                            SyncStatusActionType::kReauthenticate));

  // This time set action to SyncStatusActionType::kUpgradeClient.
  syncer::SyncStatus status;
  status.sync_protocol_error.action = syncer::UPGRADE_CLIENT;
  service.SetDetailedSyncStatus(true, status);

  EXPECT_THAT(
      GetSyncStatusLabels(&service, environment.identity_manager(),
                          /*is_user_clear_primary_account_allowed=*/true),
      SyncStatusLabelsMatch(SyncStatusMessageType::kSyncError,
                            IDS_SYNC_UPGRADE_CLIENT,
                            IDS_SYNC_UPGRADE_CLIENT_BUTTON,
                            SyncStatusActionType::kUpgradeClient));
}

TEST(SyncUIUtilTest, ActionableProtocolErrorWithPassiveMessage) {
  base::test::TaskEnvironment task_environment;
  syncer::TestSyncService service;
  signin::IdentityTestEnvironment environment;

  environment.SetPrimaryAccount(kTestUser, signin::ConsentLevel::kSync);
  service.SetFirstSetupComplete(true);
  service.SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);

  // Set action to SyncStatusActionType::kUpgradeClient.
  syncer::SyncStatus status;
  status.sync_protocol_error.action = syncer::UPGRADE_CLIENT;
  service.SetDetailedSyncStatus(true, status);

  // Expect a 'client upgrade' call to action.
  EXPECT_THAT(
      GetSyncStatusLabels(&service, environment.identity_manager(),
                          /*is_user_clear_primary_account_allowed=*/true),
      SyncStatusLabelsMatch(SyncStatusMessageType::kSyncError,
                            IDS_SYNC_UPGRADE_CLIENT,
                            IDS_SYNC_UPGRADE_CLIENT_BUTTON,
                            SyncStatusActionType::kUpgradeClient));
}

TEST(SyncUIUtilTest, SyncSettingsConfirmationNeededTest) {
  base::test::TaskEnvironment task_environment;
  syncer::TestSyncService service;
  signin::IdentityTestEnvironment environment;

  environment.SetPrimaryAccount(kTestUser, signin::ConsentLevel::kSync);
  service.SetFirstSetupComplete(false);
  ASSERT_TRUE(ShouldRequestSyncConfirmation(&service));

  EXPECT_THAT(
      GetSyncStatusLabels(&service, environment.identity_manager(),
                          /*is_user_clear_primary_account_allowed=*/true),
      SyncStatusLabelsMatch(
          SyncStatusMessageType::kSyncError, IDS_SYNC_SETTINGS_NOT_CONFIRMED,
          IDS_SYNC_ERROR_USER_MENU_CONFIRM_SYNC_SETTINGS_BUTTON,
          SyncStatusActionType::kConfirmSyncSettings));
}

// Errors in non-sync accounts should be ignored.
TEST(SyncUIUtilTest, IgnoreSyncErrorForNonSyncAccount) {
  base::test::TaskEnvironment task_environment;
  syncer::TestSyncService service;
  signin::IdentityTestEnvironment environment;

  const AccountInfo primary_account_info =
      environment.MakePrimaryAccountAvailable(kTestUser,
                                              signin::ConsentLevel::kSync);
  service.SetAccountInfo(primary_account_info);
  service.SetFirstSetupComplete(true);

  // Setup a secondary account.
  const AccountInfo secondary_account_info =
      environment.MakeAccountAvailable("secondary-user@example.com");

  // Verify that we do not have any existing errors.
  ASSERT_THAT(
      GetSyncStatusLabels(&service, environment.identity_manager(),
                          /*is_user_clear_primary_account_allowed=*/true),
      SyncStatusLabelsMatch(SyncStatusMessageType::kSynced,
                            IDS_SYNC_ACCOUNT_SYNCING, IDS_SETTINGS_EMPTY_STRING,
                            SyncStatusActionType::kNoAction));

  // Add an error to the secondary account.
  environment.UpdatePersistentErrorOfRefreshTokenForAccount(
      secondary_account_info.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  // Verify that we do not see any sign-in errors.
  EXPECT_THAT(
      GetSyncStatusLabels(&service, environment.identity_manager(),
                          /*is_user_clear_primary_account_allowed=*/true),
      SyncStatusLabelsMatch(SyncStatusMessageType::kSynced,
                            IDS_SYNC_ACCOUNT_SYNCING, IDS_SETTINGS_EMPTY_STRING,
                            SyncStatusActionType::kNoAction));
}

TEST(SyncUIUtilTest, ShouldShowSyncPassphraseError) {
  syncer::TestSyncService service;
  service.SetFirstSetupComplete(true);
  service.SetPassphraseRequiredForPreferredDataTypes(true);
  EXPECT_TRUE(ShouldShowSyncPassphraseError(&service));
}

TEST(SyncUIUtilTest, ShouldShowSyncPassphraseError_SyncDisabled) {
  syncer::TestSyncService service;
  service.SetFirstSetupComplete(false);
  service.SetPassphraseRequiredForPreferredDataTypes(true);
  EXPECT_FALSE(ShouldShowSyncPassphraseError(&service));
}

TEST(SyncUIUtilTest, ShouldShowSyncPassphraseError_NotUsingPassphrase) {
  syncer::TestSyncService service;
  service.SetFirstSetupComplete(true);
  service.SetPassphraseRequiredForPreferredDataTypes(false);
  EXPECT_FALSE(ShouldShowSyncPassphraseError(&service));
}

}  // namespace
