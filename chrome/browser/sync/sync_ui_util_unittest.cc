// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "components/sync/driver/test_sync_service.h"
#include "components/sync/engine/sync_engine.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#endif

namespace sync_ui_util {

void PrintTo(const StatusLabels& labels, std::ostream* out) {
  *out << "{" << labels.message_type << ", " << labels.status_label_string_id
       << ", " << labels.button_string_id << ", " << labels.action_type << "}";
}

namespace {

MATCHER_P4(StatusLabelsMatch,
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
// GetStatusLabels should return.
// TODO(mastiz): Split the cases below to separate tests.
StatusLabels SetUpDistinctCase(
    syncer::TestSyncService* service,
    signin::IdentityTestEnvironment* test_environment,
    DistinctState case_number) {
  switch (case_number) {
    case STATUS_CASE_SETUP_IN_PROGRESS: {
      service->SetFirstSetupComplete(false);
      service->SetSetupInProgress(true);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      return {PRE_SYNCED, IDS_SYNC_SETUP_IN_PROGRESS, IDS_SETTINGS_EMPTY_STRING,
              NO_ACTION};
    }
    case STATUS_CASE_SETUP_ERROR: {
      service->SetFirstSetupComplete(false);
      service->SetSetupInProgress(false);
      service->SetDisableReasons(
          syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      return {
        SYNC_ERROR,
#if !BUILDFLAG(IS_CHROMEOS_ASH)
            IDS_SYNC_STATUS_UNRECOVERABLE_ERROR,
#else
            IDS_SYNC_STATUS_UNRECOVERABLE_ERROR_NEEDS_SIGNOUT,
#endif
            IDS_SYNC_RELOGIN_BUTTON, REAUTHENTICATE
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
      service->SetAuthenticatedAccountInfo(
          test_environment->identity_manager()->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSync));
      test_environment->UpdatePersistentErrorOfRefreshTokenForAccount(
          account_id,
          GoogleServiceAuthError(GoogleServiceAuthError::State::SERVICE_ERROR));
      service->SetDisableReasons(syncer::SyncService::DisableReasonSet());
      return {SYNC_ERROR, IDS_SYNC_RELOGIN_ERROR, IDS_SYNC_RELOGIN_BUTTON,
              REAUTHENTICATE};
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
      return {SYNC_ERROR, IDS_SYNC_UPGRADE_CLIENT,
              IDS_SYNC_UPGRADE_CLIENT_BUTTON, UPGRADE_CLIENT};
    }
    case STATUS_CASE_CONFIRM_SYNC_SETTINGS: {
      service->SetFirstSetupComplete(false);
      service->SetPassphraseRequired(false);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      return {SYNC_ERROR, IDS_SYNC_SETTINGS_NOT_CONFIRMED,
              IDS_SYNC_ERROR_USER_MENU_CONFIRM_SYNC_SETTINGS_BUTTON,
              CONFIRM_SYNC_SETTINGS};
    }
    case STATUS_CASE_PASSPHRASE_ERROR: {
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      service->SetDisableReasons(syncer::SyncService::DisableReasonSet());
      service->SetPassphraseRequired(true);
      service->SetPassphraseRequiredForPreferredDataTypes(true);
      return {SYNC_ERROR, IDS_SYNC_STATUS_NEEDS_PASSWORD,
              IDS_SYNC_STATUS_NEEDS_PASSWORD_BUTTON, ENTER_PASSPHRASE};
    }
    case STATUS_CASE_TRUSTED_VAULT_KEYS_ERROR:
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      service->SetDisableReasons(syncer::SyncService::DisableReasonSet());
      service->SetPassphraseRequired(false);
      service->SetTrustedVaultKeyRequiredForPreferredDataTypes(true);
      return {PASSWORDS_ONLY_SYNC_ERROR, IDS_SETTINGS_EMPTY_STRING,
              IDS_SYNC_STATUS_NEEDS_KEYS_BUTTON, RETRIEVE_TRUSTED_VAULT_KEYS};
    case STATUS_CASE_TRUSTED_VAULT_RECOVERABILITY_ERROR:
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      service->SetDisableReasons(syncer::SyncService::DisableReasonSet());
      service->SetPassphraseRequired(false);
      service->SetTrustedVaultRecoverabilityDegraded(true);
      return {SYNCED, IDS_SYNC_ACCOUNT_SYNCING, IDS_SETTINGS_EMPTY_STRING,
              NO_ACTION};
    case STATUS_CASE_SYNCED: {
      service->SetFirstSetupComplete(true);
      service->SetTransportState(syncer::SyncService::TransportState::ACTIVE);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      service->SetDisableReasons(syncer::SyncService::DisableReasonSet());
      service->SetPassphraseRequired(false);
      return {SYNCED, IDS_SYNC_ACCOUNT_SYNCING, IDS_SETTINGS_EMPTY_STRING,
              NO_ACTION};
    }
    case STATUS_CASE_SYNC_DISABLED_BY_POLICY: {
      service->SetDisableReasons(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
      service->SetFirstSetupComplete(false);
      service->SetTransportState(syncer::SyncService::TransportState::DISABLED);
      service->SetPassphraseRequired(false);
      service->SetDetailedSyncStatus(false, syncer::SyncStatus());
      return {SYNCED, IDS_SIGNED_IN_WITH_SYNC_DISABLED_BY_POLICY,
              IDS_SETTINGS_EMPTY_STRING, NO_ACTION};
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
      return {SYNC_ERROR, IDS_SIGNED_IN_WITH_SYNC_STOPPED_VIA_DASHBOARD,
              IDS_SETTINGS_EMPTY_STRING, NO_ACTION};
    }
    case NUMBER_OF_STATUS_CASES:
      NOTREACHED();
  }
  return {PRE_SYNCED, IDS_SETTINGS_EMPTY_STRING, IDS_SETTINGS_EMPTY_STRING,
          NO_ACTION};
}

// This test ensures that each distinctive SyncService status will return a
// proper status and link messages from GetStatusLabels().
TEST(SyncUIUtilTest, DistinctCasesReportProperMessages) {
  base::test::TaskEnvironment task_environment;

  for (int index = 0; index != NUMBER_OF_STATUS_CASES; index++) {
    syncer::TestSyncService service;
    signin::IdentityTestEnvironment environment;

    // Need a primary account signed in before calling SetUpDistinctCase().
    environment.MakePrimaryAccountAvailable(kTestUser);

    StatusLabels expected_labels = SetUpDistinctCase(
        &service, &environment, static_cast<DistinctState>(index));

    EXPECT_THAT(GetStatusLabels(&service, environment.identity_manager(),
                                /*is_user_signout_allowed=*/true),
                StatusLabelsMatch(expected_labels.message_type,
                                  expected_labels.status_label_string_id,
                                  expected_labels.button_string_id,
                                  expected_labels.action_type));
  }
}

TEST(SyncUIUtilTest, UnrecoverableErrorWithActionableError) {
  base::test::TaskEnvironment task_environment;
  syncer::TestSyncService service;
  signin::IdentityTestEnvironment environment;

  environment.SetPrimaryAccount(kTestUser);
  service.SetFirstSetupComplete(true);
  service.SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);

  // First time action is not set. We should get unrecoverable error.
  service.SetDetailedSyncStatus(true, syncer::SyncStatus());

  // Expect the generic unrecoverable error action which is to reauthenticate.
  EXPECT_THAT(GetStatusLabels(&service, environment.identity_manager(),
                              /*is_user_signout_allowed=*/true),
              StatusLabelsMatch(SYNC_ERROR,
#if !BUILDFLAG(IS_CHROMEOS_ASH)
                                IDS_SYNC_STATUS_UNRECOVERABLE_ERROR,
#else
                                IDS_SYNC_STATUS_UNRECOVERABLE_ERROR_NEEDS_SIGNOUT,
#endif
                                IDS_SYNC_RELOGIN_BUTTON, REAUTHENTICATE));

  // This time set action to UPGRADE_CLIENT.
  syncer::SyncStatus status;
  status.sync_protocol_error.action = syncer::UPGRADE_CLIENT;
  service.SetDetailedSyncStatus(true, status);

  EXPECT_THAT(
      GetStatusLabels(&service, environment.identity_manager(),
                      /*is_user_signout_allowed=*/true),
      StatusLabelsMatch(SYNC_ERROR, IDS_SYNC_UPGRADE_CLIENT,
                        IDS_SYNC_UPGRADE_CLIENT_BUTTON, UPGRADE_CLIENT));
}

TEST(SyncUIUtilTest, ActionableErrorWithPassiveMessage) {
  base::test::TaskEnvironment task_environment;
  syncer::TestSyncService service;
  signin::IdentityTestEnvironment environment;

  environment.SetPrimaryAccount(kTestUser);
  service.SetFirstSetupComplete(true);
  service.SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);

  // Set action to UPGRADE_CLIENT.
  syncer::SyncStatus status;
  status.sync_protocol_error.action = syncer::UPGRADE_CLIENT;
  service.SetDetailedSyncStatus(true, status);

  // Expect a 'client upgrade' call to action.
  EXPECT_THAT(
      GetStatusLabels(&service, environment.identity_manager(),
                      /*is_user_signout_allowed=*/true),
      StatusLabelsMatch(SYNC_ERROR, IDS_SYNC_UPGRADE_CLIENT,
                        IDS_SYNC_UPGRADE_CLIENT_BUTTON, UPGRADE_CLIENT));
}

TEST(SyncUIUtilTest, SyncSettingsConfirmationNeededTest) {
  base::test::TaskEnvironment task_environment;
  syncer::TestSyncService service;
  signin::IdentityTestEnvironment environment;

  environment.SetPrimaryAccount(kTestUser);
  service.SetFirstSetupComplete(false);
  ASSERT_TRUE(ShouldRequestSyncConfirmation(&service));

  EXPECT_THAT(
      GetStatusLabels(&service, environment.identity_manager(),
                      /*is_user_signout_allowed=*/true),
      StatusLabelsMatch(SYNC_ERROR, IDS_SYNC_SETTINGS_NOT_CONFIRMED,
                        IDS_SYNC_ERROR_USER_MENU_CONFIRM_SYNC_SETTINGS_BUTTON,
                        CONFIRM_SYNC_SETTINGS));
}

// Errors in non-sync accounts should be ignored.
TEST(SyncUIUtilTest, IgnoreSyncErrorForNonSyncAccount) {
  base::test::TaskEnvironment task_environment;
  syncer::TestSyncService service;
  signin::IdentityTestEnvironment environment;

  const AccountInfo primary_account_info =
      environment.MakePrimaryAccountAvailable(kTestUser);
  service.SetAuthenticatedAccountInfo(primary_account_info);
  service.SetFirstSetupComplete(true);

  // Setup a secondary account.
  const AccountInfo secondary_account_info =
      environment.MakeAccountAvailable("secondary-user@example.com");

  // Verify that we do not have any existing errors.
  ASSERT_THAT(GetStatusLabels(&service, environment.identity_manager(),
                              /*is_user_signout_allowed=*/true),
              StatusLabelsMatch(MessageType::SYNCED, IDS_SYNC_ACCOUNT_SYNCING,
                                IDS_SETTINGS_EMPTY_STRING, NO_ACTION));

  // Add an error to the secondary account.
  environment.UpdatePersistentErrorOfRefreshTokenForAccount(
      secondary_account_info.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  // Verify that we do not see any sign-in errors.
  EXPECT_THAT(GetStatusLabels(&service, environment.identity_manager(),
                              /*is_user_signout_allowed=*/true),
              StatusLabelsMatch(MessageType::SYNCED, IDS_SYNC_ACCOUNT_SYNCING,
                                IDS_SETTINGS_EMPTY_STRING, NO_ACTION));
}

TEST(SyncUIUtilTest, ShouldShowPassphraseError) {
  syncer::TestSyncService service;
  service.SetFirstSetupComplete(true);
  service.SetPassphraseRequiredForPreferredDataTypes(true);
  EXPECT_TRUE(ShouldShowPassphraseError(&service));
}

TEST(SyncUIUtilTest, ShouldShowPassphraseError_SyncDisabled) {
  syncer::TestSyncService service;
  service.SetFirstSetupComplete(false);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  service.GetUserSettings()->SetOsSyncFeatureEnabled(false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  service.SetPassphraseRequiredForPreferredDataTypes(true);
  EXPECT_FALSE(ShouldShowPassphraseError(&service));
}

TEST(SyncUIUtilTest, ShouldShowPassphraseError_NotUsingPassphrase) {
  syncer::TestSyncService service;
  service.SetFirstSetupComplete(true);
  service.SetPassphraseRequiredForPreferredDataTypes(false);
  EXPECT_FALSE(ShouldShowPassphraseError(&service));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST(SyncUIUtilTest, ShouldShowPassphraseError_OsSyncEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kSplitSettingsSync);
  syncer::TestSyncService service;
  service.SetPassphraseRequiredForPreferredDataTypes(true);
  service.SetFirstSetupComplete(false);
  service.GetUserSettings()->SetOsSyncFeatureEnabled(true);
  EXPECT_TRUE(ShouldShowPassphraseError(&service));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

}  // namespace sync_ui_util
