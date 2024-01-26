// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <ostream>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/sync/sync_error_notifier.h"
#include "chrome/browser/ash/sync/sync_error_notifier_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
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
#include "components/trusted_vault/command_line_switches.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/standalone_trusted_vault_client.h"
#include "components/trusted_vault/test/fake_security_domains_server.h"
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

class WifiConfigurationsSyncActiveChecker
    : public SingleClientStatusChangeChecker {
 public:
  explicit WifiConfigurationsSyncActiveChecker(
      syncer::SyncServiceImpl* sync_service)
      : SingleClientStatusChangeChecker(sync_service) {}
  ~WifiConfigurationsSyncActiveChecker() override = default;

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for WIFI_CONFIGURATIONS sync to become active";
    return service()->GetActiveDataTypes().Has(syncer::WIFI_CONFIGURATIONS);
  }
};

class TrustedVaultDeviceRegisteredStateChecker
    : public StatusChangeChecker,
      public trusted_vault::StandaloneTrustedVaultClient::DebugObserver {
 public:
  TrustedVaultDeviceRegisteredStateChecker(const std::string& gaia_id,
                                           Profile* profile)
      : gaia_id_(gaia_id) {
    trusted_vault_client_ =
        static_cast<trusted_vault::StandaloneTrustedVaultClient*>(
            TrustedVaultServiceFactory::GetForProfile(profile)
                ->GetTrustedVaultClient(
                    trusted_vault::SecurityDomainId::kChromeSync));
    trusted_vault_client_->AddDebugObserverForTesting(this);
    OnBackendStateChanged();
  }

  ~TrustedVaultDeviceRegisteredStateChecker() override {
    trusted_vault_client_->RemoveDebugObserverForTesting(this);
  }

  // trusted_vault::StandaloneTrustedVaultClient::DebugObserver implementation.
  void OnBackendStateChanged() override {
    trusted_vault_client_->FetchIsDeviceRegisteredForTesting(
        gaia_id_, base::BindOnce(&TrustedVaultDeviceRegisteredStateChecker::
                                     OnIsDeviceRegisteredFetched,
                                 weak_ptr_factory_.GetWeakPtr()));
  }

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting until device is registered.";
    return is_device_registered_;
  }

 private:
  void OnIsDeviceRegisteredFetched(bool is_device_registered) {
    // `is_device_registered` should not regress.
    CHECK(is_device_registered || !is_device_registered_);
    is_device_registered_ = is_device_registered;
    CheckExitCondition();
  }

  const std::string gaia_id_;
  raw_ptr<trusted_vault::StandaloneTrustedVaultClient> trusted_vault_client_;
  bool is_device_registered_ = false;

  base::WeakPtrFactory<TrustedVaultDeviceRegisteredStateChecker>
      weak_ptr_factory_{this};
};

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
    command_line->AppendSwitchASCII(
        trusted_vault::kTrustedVaultServiceURLSwitch,
        trusted_vault::FakeSecurityDomainsServer::GetServerURL(
            embedded_https_test_server().base_url())
            .spec());
    security_domains_server_ =
        std::make_unique<trusted_vault::FakeSecurityDomainsServer>(
            embedded_https_test_server().base_url());
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
    if (!SetupClients()) {
      return false;
    }
    // SetupSync() may trigger some notifications, so need to create
    // `notification_display_service_tester_` before calling it, but after
    // profile is created by SetupClients().
    notification_display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(GetProfile(0));

    if (!SetupSync()) {
      return false;
    }

    embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
        &trusted_vault::FakeSecurityDomainsServer::HandleRequest,
        base::Unretained(security_domains_server_.get())));
    encryption_helper::SetupFakeTrustedVaultPages(
        GetSyncService(0)->GetAccountInfo().gaia, kTestTrustedVaultKey,
        /*trusted_vault_key_version=*/1,
        /*recovery_method_public_key=*/kTestRecoveryMethodPublicKey,
        &embedded_https_test_server());
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

  bool FetchDegradedRecoveribilityStateThroughCrosapi() {
    base::test::TestFuture<bool> fetched_state_future;
    trusted_vault_backend_remote_->GetIsRecoverabilityDegraded(
        GetSyncingUserAccountKey(), fetched_state_future.GetCallback());
    return fetched_state_future.Take();
  }

  void MarkLocalKeysAsStaleThroughCrosapi() {
    trusted_vault_backend_remote_->MarkLocalKeysAsStale(
        GetSyncingUserAccountKey(), base::DoNothing());
  }

  void AddTrustedRecoveryMethodThroughCrosapi() {
    trusted_vault_backend_remote_->AddTrustedRecoveryMethod(
        GetSyncingUserAccountKey(), kTestRecoveryMethodPublicKey,
        /*method_type_hint=*/0, base::DoNothing());
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

  std::optional<message_center::Notification> GetSyncNotification() {
    const std::string notification_id =
        ash::SyncErrorNotifierFactory::GetForProfile(GetProfile(0))
            ->GetNotificationIdForTesting();
    return notification_display_service_tester().GetNotification(
        notification_id);
  }

  bool WaitForTrustedVaultReauthCompletion() {
    CHECK(trusted_vault_widget_shown_waiter_);
    views::Widget* trusted_vault_widged =
        trusted_vault_widget_shown_waiter_->WaitIfNeededAndGet();
    views::test::WidgetDestroyedWaiter(trusted_vault_widged).Wait();
    return true;
  }

  trusted_vault::FakeSecurityDomainsServer& security_domains_server() {
    return *security_domains_server_;
  }

  NotificationDisplayServiceTester& notification_display_service_tester() {
    return *notification_display_service_tester_;
  }

  mojo::Remote<crosapi::mojom::TrustedVaultBackend>&
  trusted_vault_backend_remote() {
    return trusted_vault_backend_remote_;
  }

 protected:
  const std::vector<uint8_t> kTestTrustedVaultKey = {1, 2, 3};
  // Arbitrary (but valid) public key of a recovery method.
  const std::vector<uint8_t> kTestRecoveryMethodPublicKey =
      trusted_vault::SecureBoxKeyPair::GenerateRandom()
          ->public_key()
          .ExportToBytes();

 private:
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<views::NamedWidgetShownWaiter>
      trusted_vault_widget_shown_waiter_;

  std::unique_ptr<trusted_vault::FakeSecurityDomainsServer>
      security_domains_server_;

  // Should be created before any observed notification is shown. Must outlive
  // profile, i.e. TearDownOnMainThread().
  // TODO(crbug.com/1513038): would be better to avoid non-trivial lifetime
  // requirements. Perhaps, NotificationDisplayServiceTester simply should not
  // call SetTestingFactory(ctx, NullFactory) in destructor, since this
  // contradicts class-level comment ("Profile may outlive this") and unlikely
  // to be needed.
  std::unique_ptr<NotificationDisplayServiceTester>
      notification_display_service_tester_;

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

IN_PROC_BROWSER_TEST_F(AshTrustedVaultKeysSharingSyncTest,
                       ShouldStoreKeysThroughCrosapi) {
  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(
      syncer::BuildTrustedVaultNigoriSpecifics({kTestTrustedVaultKey}),
      GetFakeServer());

  ASSERT_TRUE(SetupSyncAndTrustedVaultFakes());
  SetupCrosapi();

  ASSERT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  ASSERT_FALSE(
      GetSyncService(0)->GetActiveDataTypes().Has(syncer::WIFI_CONFIGURATIONS));

  // Key missing notification should be displayed.
  auto notification = GetSyncNotification();
  ASSERT_TRUE(notification);
  ASSERT_THAT(notification->title(),
              Eq(l10n_util::GetStringUTF16(
                  IDS_SYNC_ERROR_PASSWORDS_BUBBLE_VIEW_TITLE)));
  ASSERT_THAT(
      notification->message(),
      Eq(l10n_util::GetStringUTF16(
          IDS_SYNC_NEEDS_KEYS_FOR_PASSWORDS_ERROR_BUBBLE_VIEW_MESSAGE)));

  // Mimic that Lacros provides trusted vault keys through Crosapi (i.e. user
  // went through key retrieval using Lacros).
  trusted_vault_backend_remote()->StoreKeys(GetSyncingUserAccountKey(),
                                            {kTestTrustedVaultKey},
                                            /*last_key_version=*/1);

  // Key should be (asynchronously) accepted and WIFI_CONFIGURATIONS should
  // become active.
  EXPECT_TRUE(TrustedVaultKeyRequiredStateChecker(GetSyncService(0),
                                                  /*desired_state=*/false)
                  .Wait());
  EXPECT_TRUE(WifiConfigurationsSyncActiveChecker(GetSyncService(0)).Wait());

  // Key missing notification should be closed automatically.
  EXPECT_THAT(GetSyncNotification(), Eq(std::nullopt));

  // Lacros should be able to fetch stored keys through Crosapi.
  EXPECT_THAT(FetchKeysThroughCrosapi(), ElementsAre(kTestTrustedVaultKey));
}

IN_PROC_BROWSER_TEST_F(AshTrustedVaultKeysSharingSyncTest,
                       ShouldAcceptKeysFromTheWebAndFetchThemThroughCrosapi) {
  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(
      syncer::BuildTrustedVaultNigoriSpecifics({kTestTrustedVaultKey}),
      GetFakeServer());

  ASSERT_TRUE(SetupSyncAndTrustedVaultFakes());
  SetupCrosapi();

  // No keys yet available in Ash, Lacros will fetch empty keys.
  EXPECT_THAT(FetchKeysThroughCrosapi(), IsEmpty());

  EXPECT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  // Key missing notification should be displayed.
  auto notification = GetSyncNotification();
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
  notification_display_service_tester().SimulateClick(
      NotificationHandler::Type::TRANSIENT, notification->id(),
      /*action_index=*/std::nullopt,
      /*reply=*/std::nullopt);
  EXPECT_TRUE(WaitForTrustedVaultReauthCompletion());

  // Now Lacros should be able to fetch keys.
  EXPECT_TRUE(keys_changed_notified_checker.Wait());
  EXPECT_THAT(FetchKeysThroughCrosapi(), ElementsAre(kTestTrustedVaultKey));
}

IN_PROC_BROWSER_TEST_F(
    AshTrustedVaultKeysSharingSyncTest,
    ShouldFollowInitialKeyRotationAndFetchKeysThroughCrosapi) {
  ASSERT_TRUE(SetupSyncAndTrustedVaultFakes());
  SetupCrosapi();
  // Wait until device is registered, otherwise it won't be able to follow key
  // rotation.
  ASSERT_TRUE(TrustedVaultDeviceRegisteredStateChecker(
                  GetSyncingUserAccountInfo().gaia, GetProfile(0))
                  .Wait());

  std::vector<uint8_t> new_trusted_vault_key =
      security_domains_server().RotateTrustedVaultKey(
          trusted_vault::GetConstantTrustedVaultKey());
  // FetchKeys() Crosapi should trigger following key rotation and already
  // receive real trusted vault key. Note, that since this is initial key
  // rotation, marking keys as stale is not necessary.
  EXPECT_THAT(FetchKeysThroughCrosapi(), ElementsAre(new_trusted_vault_key));
}

IN_PROC_BROWSER_TEST_F(AshTrustedVaultKeysSharingSyncTest,
                       ShouldFollowKeyRotationAndFetchKeysThroughCrosapi) {
  ASSERT_TRUE(SetupSyncAndTrustedVaultFakes());
  SetupCrosapi();

  // Wait until device is registered, otherwise it won't be able to follow key
  // rotations.
  ASSERT_TRUE(TrustedVaultDeviceRegisteredStateChecker(
                  GetSyncingUserAccountInfo().gaia, GetProfile(0))
                  .Wait());

  // Trigger initial key rotation and ensure Ash follows it (otherwise test
  // won't cover MarkKeysAsStale() Crosapi - since Ash won't even need its call
  // to fetch most recent keys).
  std::vector<uint8_t> trusted_vault_key_1 =
      security_domains_server().RotateTrustedVaultKey(
          trusted_vault::GetConstantTrustedVaultKey());
  ASSERT_THAT(FetchKeysThroughCrosapi(), ElementsAre(trusted_vault_key_1));

  // Trigger another key rotation, note that the keys are not being fetched from
  // the server immediately, e.g. FetchKeys() Crosapi returns stale keys.
  std::vector<uint8_t> trusted_vault_key_2 =
      security_domains_server().RotateTrustedVaultKey(trusted_vault_key_1);
  EXPECT_THAT(FetchKeysThroughCrosapi(), ElementsAre(trusted_vault_key_1));

  // Once keys are marked as stale, FetchKeys() Crosapi should trigger keys
  // downloading and return fresh keys.
  MarkLocalKeysAsStaleThroughCrosapi();
  EXPECT_THAT(FetchKeysThroughCrosapi(),
              ElementsAre(trusted_vault_key_1, trusted_vault_key_2));
}

IN_PROC_BROWSER_TEST_F(AshTrustedVaultKeysSharingSyncTest,
                       ShouldExposeDegradedRecoverabilityState) {
  ASSERT_TRUE(SetupSyncAndTrustedVaultFakes());
  SetupCrosapi();

  // Wait until device is registered, otherwise it won't be able to follow key
  // rotations.
  ASSERT_TRUE(TrustedVaultDeviceRegisteredStateChecker(
                  GetSyncingUserAccountInfo().gaia, GetProfile(0))
                  .Wait());

  // Mimic transition to kTrustedVaultPassphrase and entering degraded
  // recoverability state. Ash should notify Lacros through Crosapi.
  auto degraded_recoverability_notified_checker_1 =
      CreateTrustedVaultStateNotifiedToCrosapiObserverChecker(
          TrustedVaultStateNotifiedToCrosapiObserverChecker::
              ExpectedNotification::kRecoverabilityStateChanged);
  std::vector<uint8_t> trusted_vault_key =
      security_domains_server().RotateTrustedVaultKey(
          trusted_vault::GetConstantTrustedVaultKey());
  security_domains_server().RequirePublicKeyToAvoidRecoverabilityDegraded(
      kTestRecoveryMethodPublicKey);
  SetNigoriInFakeServer(
      syncer::BuildTrustedVaultNigoriSpecifics({trusted_vault_key}),
      GetFakeServer());

  EXPECT_TRUE(degraded_recoverability_notified_checker_1.Wait());
  EXPECT_TRUE(FetchDegradedRecoveribilityStateThroughCrosapi());

  // Mimic resolving the degraded recoverability state through system
  // notification. Ash should notify Lacros through Crosapi.
  auto notification = GetSyncNotification();
  ASSERT_TRUE(notification);
  ASSERT_THAT(notification->title(),
              Eq(l10n_util::GetStringUTF16(
                  IDS_SYNC_NEEDS_VERIFICATION_BUBBLE_VIEW_TITLE)));
  ASSERT_THAT(
      notification->message(),
      Eq(l10n_util::GetStringUTF16(
          IDS_SYNC_RECOVERABILITY_DEGRADED_FOR_PASSWORDS_ERROR_BUBBLE_VIEW_MESSAGE)));

  auto degraded_recoverability_notified_checker_2 =
      CreateTrustedVaultStateNotifiedToCrosapiObserverChecker(
          TrustedVaultStateNotifiedToCrosapiObserverChecker::
              ExpectedNotification::kRecoverabilityStateChanged);
  notification_display_service_tester().SimulateClick(
      NotificationHandler::Type::TRANSIENT, notification->id(),
      /*action_index=*/std::nullopt,
      /*reply=*/std::nullopt);
  ASSERT_TRUE(WaitForTrustedVaultReauthCompletion());

  EXPECT_TRUE(degraded_recoverability_notified_checker_2.Wait());
  EXPECT_FALSE(FetchDegradedRecoveribilityStateThroughCrosapi());
}

IN_PROC_BROWSER_TEST_F(AshTrustedVaultKeysSharingSyncTest,
                       ShouldAddRecoveryMethodThroughCrosapi) {
  ASSERT_TRUE(SetupSyncAndTrustedVaultFakes());
  SetupCrosapi();

  // Wait until device is registered, otherwise it won't be able to follow key
  // rotations.
  ASSERT_TRUE(TrustedVaultDeviceRegisteredStateChecker(
                  GetSyncingUserAccountInfo().gaia, GetProfile(0))
                  .Wait());

  // Mimic transition to kTrustedVaultPassphrase and entering degraded
  // recoverability state.
  std::vector<uint8_t> trusted_vault_key =
      security_domains_server().RotateTrustedVaultKey(
          trusted_vault::GetConstantTrustedVaultKey());
  security_domains_server().RequirePublicKeyToAvoidRecoverabilityDegraded(
      kTestRecoveryMethodPublicKey);
  SetNigoriInFakeServer(
      syncer::BuildTrustedVaultNigoriSpecifics({trusted_vault_key}),
      GetFakeServer());
  ASSERT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/true)
                  .Wait());

  // Degraded recoverability notification should be be shown.
  auto notification = GetSyncNotification();
  ASSERT_TRUE(notification);
  ASSERT_THAT(notification->title(),
              Eq(l10n_util::GetStringUTF16(
                  IDS_SYNC_NEEDS_VERIFICATION_BUBBLE_VIEW_TITLE)));
  ASSERT_THAT(
      notification->message(),
      Eq(l10n_util::GetStringUTF16(
          IDS_SYNC_RECOVERABILITY_DEGRADED_FOR_PASSWORDS_ERROR_BUBBLE_VIEW_MESSAGE)));

  // Resolve degraded recoverability state through Crosapi. Notification should
  // be removed and Ash should notify through Crosapi about recoverability state
  // change.
  auto degraded_recoverability_notified_checker =
      CreateTrustedVaultStateNotifiedToCrosapiObserverChecker(
          TrustedVaultStateNotifiedToCrosapiObserverChecker::
              ExpectedNotification::kRecoverabilityStateChanged);
  AddTrustedRecoveryMethodThroughCrosapi();

  EXPECT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/false)
                  .Wait());
  EXPECT_THAT(GetSyncNotification(), Eq(std::nullopt));
  EXPECT_TRUE(degraded_recoverability_notified_checker.Wait());
}

}  // namespace
