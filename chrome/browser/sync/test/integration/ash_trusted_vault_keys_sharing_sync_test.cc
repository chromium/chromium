// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/sync/sync_error_notifier.h"
#include "chrome/browser/ash/sync/sync_error_notifier_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "chrome/browser/ui/webui/trusted_vault/trusted_vault_dialog_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "chromeos/crosapi/mojom/trusted_vault.mojom.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_server_nigori_helper.h"
#include "components/sync/test/nigori_test_utils.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_service.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {

using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::NotNull;

class TrustedVaultStateNotifiedToCrosapiObserverChecker
    : public StatusChangeChecker,
      public crosapi::mojom::TrustedVaultBackendObserver {
 public:
  enum class ExpectedNotification { kKeysChanged, kRecoverabilityStateChanged };

  TrustedVaultStateNotifiedToCrosapiObserverChecker(
      mojo::Remote<crosapi::mojom::TrustedVaultBackend>* backend_remote,
      ExpectedNotification expected_notification)
      : expected_notification_(expected_notification) {
    CHECK(backend_remote);
    backend_remote->get()->AddObserver(receiver_.BindNewPipeAndPassRemote());
    backend_remote->FlushForTesting();
  }

  // crosapi::mojom::TrustedVaultBackendObserver overrides.
  void OnTrustedVaultKeysChanged() override {
    keys_changed_notified_ = true;
    CheckExitCondition();
  }

  void OnTrustedVaultRecoverabilityChanged() override {
    recoverability_state_changed_notified_ = true;
    CheckExitCondition();
  }

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    switch (expected_notification_) {
      case ExpectedNotification::kKeysChanged:
        *os << "Waiting for OnTrustedVaultKeysChanged() call for crosapi "
               "observer.";
        return keys_changed_notified_;
      case ExpectedNotification::kRecoverabilityStateChanged:
        *os << "Waiting for OnTrustedVaultRecoverabilityChanged() call for "
               "crosapi observer.";
        return recoverability_state_changed_notified_;
    }
    NOTREACHED_NORETURN();
  }

 private:
  bool keys_changed_notified_ = false;
  bool recoverability_state_changed_notified_ = false;

  ExpectedNotification expected_notification_;
  mojo::Receiver<crosapi::mojom::TrustedVaultBackendObserver> receiver_{this};
};

class AshTrustedVaultKeysSharingSyncTest : public SyncTest {
 public:
  AshTrustedVaultKeysSharingSyncTest() : SyncTest(SINGLE_CLIENT) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            ash::standalone_browser::features::kLacrosOnly,
            trusted_vault::kChromeOSTrustedVaultUseWebUIDialog,
            trusted_vault::kChromeOSTrustedVaultClientShared,
        },
        /*disabled_features=*/{});
  }

  ~AshTrustedVaultKeysSharingSyncTest() override = default;

  // SyncTest overrides.
  base::FilePath GetProfileBaseName(int index) override {
    // Need to reuse test user profile for this test - Crosapi explicitly
    // assumes there is only one regular profile.
    CHECK_EQ(index, 0);
    return base::FilePath(
        ash::BrowserContextHelper::kTestUserBrowserContextDirName);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SyncTest::SetUpCommandLine(command_line);

    ASSERT_TRUE(embedded_https_test_server().InitializeAndListen());
    const GURL& base_url = embedded_https_test_server().base_url();
    command_line->AppendSwitchASCII(switches::kGaiaUrl, base_url.spec());
  }

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();
    trusted_vault_widget_shown_waiter_ =
        std::make_unique<views::NamedWidgetShownWaiter>(
            views::test::AnyWidgetTestPasskey{},
            TrustedVaultDialogDelegate::kWidgetName);
  }

  void SetupCrosapi() {
    ASSERT_TRUE(crosapi::browser_util::IsLacrosEnabled());

    crosapi::CrosapiAsh* crosapi_ash =
        crosapi::CrosapiManager::Get()->crosapi_ash();
    ASSERT_THAT(crosapi_ash, NotNull());

    crosapi_ash->BindTrustedVaultBackend(
        trusted_vault_backend_remote_.BindNewPipeAndPassReceiver());
  }

  bool SetupSyncAndTrustedVaultFakes() {
    if (!SetupSync()) {
      return false;
    }

    encryption_helper::SetupFakeTrustedVaultPages(
        GetSyncService(0)->GetAccountInfo().gaia, kTestTrustedVaultKey,
        /*trusted_vault_key_version=*/1,
        /*recovery_method_public_key=*/{}, &embedded_https_test_server());
    embedded_https_test_server().StartAcceptingConnections();

    return true;
  }

  trusted_vault::TrustedVaultClient& GetAshSyncTrustedVaultClient() {
    trusted_vault::TrustedVaultService* trusted_vault_service =
        TrustedVaultServiceFactory::GetForProfile(GetProfile(0));
    CHECK(trusted_vault_service);

    trusted_vault::TrustedVaultClient* sync_trusted_vault_client =
        trusted_vault_service->GetTrustedVaultClient(
            trusted_vault::SecurityDomainId::kChromeSync);
    CHECK(sync_trusted_vault_client);
    return *sync_trusted_vault_client;
  }

  std::vector<std::vector<uint8_t>> FetchKeysThroughCrosapi() {
    base::test::TestFuture<std::vector<std::vector<uint8_t>>>
        fetched_keys_future;
    trusted_vault_backend_remote_->FetchKeys(
        GetSyncingUserAccountKey(),
        fetched_keys_future
            .GetCallback<const std::vector<std::vector<uint8_t>>&>());

    return fetched_keys_future.Take();
  }

  TrustedVaultStateNotifiedToCrosapiObserverChecker
  CreateTrustedVaultStateNotifiedToCrosapiObserverChecker(
      TrustedVaultStateNotifiedToCrosapiObserverChecker::ExpectedNotification
          expected_notification) {
    return TrustedVaultStateNotifiedToCrosapiObserverChecker(
        &trusted_vault_backend_remote_, expected_notification);
  }

  CoreAccountInfo GetSyncingUserAccountInfo() {
    return GetSyncService(0)->GetAccountInfo();
  }

  crosapi::mojom::AccountKeyPtr GetSyncingUserAccountKey() {
    auto account_key = crosapi::mojom::AccountKey::New();
    account_key->id = GetSyncService(0)->GetAccountInfo().gaia;
    account_key->account_type = crosapi::mojom::AccountType::kGaia;
    return account_key;
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

 private:
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<views::NamedWidgetShownWaiter>
      trusted_vault_widget_shown_waiter_;

  mojo::Remote<crosapi::mojom::TrustedVaultBackend>
      trusted_vault_backend_remote_;
};

IN_PROC_BROWSER_TEST_F(AshTrustedVaultKeysSharingSyncTest,
                       ShouldFetchStoredKeysThroughCrosapi) {
  ASSERT_TRUE(SetupSyncAndTrustedVaultFakes());

  // Mimic that Ash already has trusted vault key.
  GetAshSyncTrustedVaultClient().StoreKeys(GetSyncingUserAccountInfo().gaia,
                                           {kTestTrustedVaultKey},
                                           /*last_key_version*/ 1);

  // Mimic that Lacros starts and attempts to fetch keys, it should succeed.
  SetupCrosapi();
  EXPECT_THAT(FetchKeysThroughCrosapi(), ElementsAre(kTestTrustedVaultKey));
}

// TODO(https://crbug.com/1513038): Flaky on bots.
IN_PROC_BROWSER_TEST_F(
    AshTrustedVaultKeysSharingSyncTest,
    DISABLED_ShouldAcceptKeysFromTheWebAndFetchThemThroughCrosapi) {
  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(
      syncer::BuildTrustedVaultNigoriSpecifics({kTestTrustedVaultKey}),
      GetFakeServer());

  // Need to create `display_service` before key missing notification is shown
  // (it will be shown during SetupSyncAndTrustedVaultFakes()).
  ASSERT_TRUE(SetupClients());
  NotificationDisplayServiceTester display_service(GetProfile(0));

  ASSERT_TRUE(SetupSyncAndTrustedVaultFakes());
  SetupCrosapi();

  // No keys yet available in Ash, Lacros will fetch empty keys.
  EXPECT_THAT(FetchKeysThroughCrosapi(), IsEmpty());

  EXPECT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  // Key missing notification should be displayed.
  const std::string notification_id =
      ash::SyncErrorNotifierFactory::GetForProfile(GetProfile(0))
          ->GetNotificationIdForTesting();
  absl::optional<message_center::Notification> notification =
      display_service.GetNotification(notification_id);
  ASSERT_TRUE(notification);
  EXPECT_THAT(notification->title(),
              Eq(l10n_util::GetStringUTF16(
                  IDS_SYNC_ERROR_PASSWORDS_BUBBLE_VIEW_TITLE)));
  EXPECT_THAT(
      notification->message(),
      Eq(l10n_util::GetStringUTF16(
          IDS_SYNC_NEEDS_KEYS_FOR_PASSWORDS_ERROR_BUBBLE_VIEW_MESSAGE)));

  auto keys_changed_notified_checker =
      CreateTrustedVaultStateNotifiedToCrosapiObserverChecker(
          TrustedVaultStateNotifiedToCrosapiObserverChecker::
              ExpectedNotification::kKeysChanged);
  // Mimic the user going through key retrieval:
  // 1. User clicks on the notification.
  // 2. It opens reauth page (note that no actual reauth happens in this test,
  // page closes automatically as if user did the reauth).
  // 3. Reauth page supplies Ash with kTestTrustedVaultKey.
  display_service.SimulateClick(NotificationHandler::Type::TRANSIENT,
                                notification_id, /*action_index=*/absl::nullopt,
                                /*reply=*/absl::nullopt);
  EXPECT_TRUE(WaitForTrustedVaultReauthCompletion());

  // Now Lacros should be able to fetch keys.
  EXPECT_TRUE(keys_changed_notified_checker.Wait());
  EXPECT_THAT(FetchKeysThroughCrosapi(), ElementsAre(kTestTrustedVaultKey));
}

}  // namespace
