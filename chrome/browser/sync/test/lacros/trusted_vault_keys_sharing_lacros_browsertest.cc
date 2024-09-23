// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/trusted_vault/trusted_vault_backend_service_ash.h"
#include "chrome/browser/ash/trusted_vault/trusted_vault_backend_service_factory_ash.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/webui/trusted_vault/trusted_vault_dialog_delegate.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/crosapi/mojom/trusted_vault.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_server_nigori_helper.h"
#include "components/sync/test/nigori_test_utils.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/test/fake_crosapi_trusted_vault_backend.h"
#include "components/trusted_vault/test/fake_crosapi_trusted_vault_backend_service.h"
#include "components/trusted_vault/test/fake_trusted_vault_client.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {

using testing::ElementsAre;
using testing::Eq;
using testing::SizeIs;

MATCHER_P4(StatusLabelsMatch,
           message_type,
           status_label_string_id,
           button_string_id,
           action_type,
           "") {
  if (arg.message_type != message_type) {
    *result_listener << "Wrong message type, expected: "
                     << static_cast<int>(message_type)
                     << ", got: " << static_cast<int>(arg.message_type);
    return false;
  }
  if (arg.status_label_string_id != status_label_string_id) {
    *result_listener << "Wrong status label, expected: "
                     << status_label_string_id
                     << ", got: " << arg.status_label_string_id;
    return false;
  }
  if (arg.button_string_id != button_string_id) {
    *result_listener << "Wrong button string, expected: " << button_string_id
                     << ", got: " << arg.button_string_id;
    return false;
  }
  if (arg.action_type != action_type) {
    *result_listener << "Wrong action type, expected: "
                     << static_cast<int>(action_type)
                     << ", got: " << static_cast<int>(arg.action_type);
    return false;
  }
  return true;
}

class TrustedVaultKeysSharingLacrosBrowserTest : public SyncTest {
 public:
  TrustedVaultKeysSharingLacrosBrowserTest() : SyncTest(SINGLE_CLIENT) {
    override_features_.InitAndEnableFeature(
        trusted_vault::kChromeOSTrustedVaultClientShared);
    fake_crosapi_backend_service_ =
        std::make_unique<trusted_vault::FakeCrosapiTrustedVaultBackendService>(
            &chrome_sync_trusted_vault_client_ash_,
            &passkeys_trusted_vault_client_ash_);
  }

  ~TrustedVaultKeysSharingLacrosBrowserTest() override = default;

  base::FilePath GetProfileBaseName(int index) override {
    // TrustedVault keys sharing is enabled only for the main profile, so
    // SyncTest should setup sync using it.
    CHECK_EQ(index, 0);
    return base::FilePath(chrome::kInitialProfile);
  }

  // This test replaces production TrustedVaultBackend Crosapi interface with
  // a fake one. It needs to be done before connection between Ash
  // TrustedVaultBackend and Lacros TrustedVaultClient is established (during
  // creation of Lacros profile), but after LacrosService is initialized. Thus
  // relying on CreatedBrowserMainParts().
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    SyncTest::CreatedBrowserMainParts(browser_main_parts);

    // Replace the production TrustedVaultBackendService Crosapi with a fake
    // for testing.
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        fake_crosapi_backend_service_->BindNewPipeAndPassRemote());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SyncTest::SetUpCommandLine(command_line);

    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    const GURL& base_url = embedded_test_server()->base_url();
    command_line->AppendSwitchASCII(switches::kGaiaUrl, base_url.spec());
  }

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();
    trusted_vault_widget_shown_waiter_ =
        std::make_unique<views::NamedWidgetShownWaiter>(
            views::test::AnyWidgetTestPasskey{},
            TrustedVaultDialogDelegate::kWidgetName);
  }

  bool SetupSyncAndTrustedVaultFakes() {
    if (!SetupSync()) {
      return false;
    }

    const CoreAccountInfo primary_account_info =
        GetSyncService(0)->GetAccountInfo();

    encryption_helper::SetupFakeTrustedVaultPages(
        primary_account_info.gaia, kTestTrustedVaultKey,
        /*trusted_vault_key_version=*/1, kTestRecoveryMethodPublicKey,
        embedded_test_server());

    embedded_test_server()->StartAcceptingConnections();

    fake_crosapi_backend_service_->chrome_sync_backend().SetPrimaryAccountInfo(
        GetSyncService(0)->GetAccountInfo());

    return true;
  }

  trusted_vault::FakeTrustedVaultClient& trusted_vault_client_ash() {
    return chrome_sync_trusted_vault_client_ash_;
  }

  bool WaitForTrustedVaultReauthCompletion() {
    CHECK(trusted_vault_widget_shown_waiter_);
    views::Widget* trusted_vault_widged =
        trusted_vault_widget_shown_waiter_->WaitIfNeededAndGet();
    views::test::WidgetDestroyedWaiter(trusted_vault_widged).Wait();
    return true;
  }

 protected:
  const std::vector<uint8_t> kTestTrustedVaultKey = {1, 2, 3};
  const std::vector<uint8_t> kTestRecoveryMethodPublicKey = {1, 2, 3, 4};

 private:
  base::test::ScopedFeatureList override_features_;

  std::unique_ptr<views::NamedWidgetShownWaiter>
      trusted_vault_widget_shown_waiter_;

  trusted_vault::FakeTrustedVaultClient chrome_sync_trusted_vault_client_ash_{
      /*auto_complete_requests=*/true};
  trusted_vault::FakeTrustedVaultClient passkeys_trusted_vault_client_ash_{
      /*auto_complete_requests=*/true};
  std::unique_ptr<trusted_vault::FakeCrosapiTrustedVaultBackendService>
      fake_crosapi_backend_service_;
};

IN_PROC_BROWSER_TEST_F(TrustedVaultKeysSharingLacrosBrowserTest,
                       ShouldFetchKeys) {
  ASSERT_TRUE(SetupSyncAndTrustedVaultFakes());

  // Mimic that trusted vault key is already available in Ash.
  trusted_vault_client_ash().StoreKeys(GetSyncService(0)->GetAccountInfo().gaia,
                                       {kTestTrustedVaultKey},
                                       /*last_key_version=*/1);

  // Inject trusted vault Nigori server-side.
  fake_server::SetNigoriInFakeServer(
      syncer::BuildTrustedVaultNigoriSpecifics({kTestTrustedVaultKey}),
      GetFakeServer());

  // Inject a password encrypted with trusted vault key server-side.
  const syncer::KeyParamsForTesting trusted_vault_key_params =
      syncer::TrustedVaultKeyParamsForTesting(kTestTrustedVaultKey);
  const password_manager::PasswordForm password_form =
      passwords_helper::CreateTestPasswordForm(1);
  passwords_helper::InjectEncryptedServerPassword(
      password_form, trusted_vault_key_params.password,
      trusted_vault_key_params.derivation_params, GetFakeServer());

  // Lacros should be able to fetch keys from Ash and decrypt the passwords.
  EXPECT_TRUE(PasswordFormsChecker(0, {password_form}).Wait());
}

IN_PROC_BROWSER_TEST_F(TrustedVaultKeysSharingLacrosBrowserTest,
                       ShouldAcceptAndStoreTrustedVaultKeysFromTheWeb) {
  ASSERT_TRUE(SetupSyncAndTrustedVaultFakes());

  // Inject trusted vault Nigori server-side.
  fake_server::SetNigoriInFakeServer(
      syncer::BuildTrustedVaultNigoriSpecifics({kTestTrustedVaultKey}),
      GetFakeServer());

  // No keys available in Ash, eventually Lacros should enter
  // TrustedVaultKeyRequired state.
  EXPECT_TRUE(TrustedVaultKeyRequiredStateChecker(GetSyncService(0),
                                                  /*desired_state=*/true)
                  .Wait());

  // Verify that error has been shown to the user.
  // 1. In profile menu:
  EXPECT_THAT(
      GetAvatarSyncErrorType(GetProfile(0)),
      Eq(AvatarSyncErrorType::kTrustedVaultKeyMissingForPasswordsError));
  // 2. In settings:
  EXPECT_THAT(GetSyncStatusLabels(GetProfile(0)),
              StatusLabelsMatch(
                  SyncStatusMessageType::kPasswordsOnlySyncError,
                  IDS_SETTINGS_EMPTY_STRING, IDS_SYNC_STATUS_NEEDS_KEYS_BUTTON,
                  SyncStatusActionType::kRetrieveTrustedVaultKeys));

  // Simulate that user clicks on the error. Normally that triggers a reauth,
  // but this test bypass it (opens page that mimics the reauth completion and
  // closes automatically).
  OpenDialogForSyncKeyRetrieval(
      GetProfile(0), syncer::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
  EXPECT_TRUE(WaitForTrustedVaultReauthCompletion());

  // Wait until trusted vault keys not required anymore, due to implementation
  // details this means that keys should be available in both Ash and Lacros.
  EXPECT_TRUE(TrustedVaultKeyRequiredStateChecker(GetSyncService(0),
                                                  /*desired_state=*/false)
                  .Wait());

  // Verify that errors disappeared from the UI.
  EXPECT_FALSE(GetAvatarSyncErrorType(GetProfile(0)).has_value());
  EXPECT_THAT(GetSyncStatusLabels(GetProfile(0)),
              StatusLabelsMatch(
                  SyncStatusMessageType::kSynced, IDS_SYNC_ACCOUNT_SYNCING,
                  IDS_SETTINGS_EMPTY_STRING, SyncStatusActionType::kNoAction));

  // Verify that keys are available in Ash.
  EXPECT_THAT(trusted_vault_client_ash().GetStoredKeys(
                  GetSyncService(0)->GetAccountInfo().gaia),
              ElementsAre(kTestTrustedVaultKey));
}

IN_PROC_BROWSER_TEST_F(TrustedVaultKeysSharingLacrosBrowserTest,
                       ShouldAddRecoveryMethodFromWeb) {
  ASSERT_TRUE(SetupSyncAndTrustedVaultFakes());

  // Inject trusted vault Nigori server-side and make it decryptable, degraded
  // recoverability is absent from the UI otherwise.
  fake_server::SetNigoriInFakeServer(
      syncer::BuildTrustedVaultNigoriSpecifics({kTestTrustedVaultKey}),
      GetFakeServer());
  trusted_vault_client_ash().StoreKeys(GetSyncService(0)->GetAccountInfo().gaia,
                                       {kTestTrustedVaultKey},
                                       /*last_key_version=*/1);

  // Enters degraded recoverability state.
  trusted_vault_client_ash().SetIsRecoveryMethodRequired(true);

  // Wait until Lacros is aware of degraded recoverability state.
  EXPECT_TRUE(
      TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0), true)
          .Wait());

  // Verify that error has been shown to the user in profile menu:
  EXPECT_THAT(GetAvatarSyncErrorType(GetProfile(0)),
              Eq(AvatarSyncErrorType::
                     kTrustedVaultRecoverabilityDegradedForPasswordsError));
  // No errors expected in settings.
  EXPECT_THAT(GetSyncStatusLabels(GetProfile(0)),
              StatusLabelsMatch(
                  SyncStatusMessageType::kSynced, IDS_SYNC_ACCOUNT_SYNCING,
                  IDS_SETTINGS_EMPTY_STRING, SyncStatusActionType::kNoAction));

  // Simulate that user clicks on the error. Normally that triggers a reauth,
  // but this test bypass it (opens page that mimics the reauth completion and
  // closes automatically).
  OpenDialogForSyncKeyRecoverabilityDegraded(
      GetProfile(0), syncer::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
  EXPECT_TRUE(WaitForTrustedVaultReauthCompletion());

  // Wait until degraded recoverability state is resolved.
  EXPECT_TRUE(
      TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0), false)
          .Wait());

  // Verify that errors disappeared from the UI.
  EXPECT_FALSE(GetAvatarSyncErrorType(GetProfile(0)).has_value());

  // Verify that recovery method was added.
  const auto recovery_methods =
      trusted_vault_client_ash().server()->GetRecoveryMethods(
          GetSyncService(0)->GetAccountInfo().gaia);
  ASSERT_THAT(recovery_methods, SizeIs(1));
  EXPECT_THAT(recovery_methods[0].public_key, Eq(kTestRecoveryMethodPublicKey));
}

}  // namespace
