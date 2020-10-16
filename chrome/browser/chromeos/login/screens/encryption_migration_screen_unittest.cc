// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/arc/arc_migration_constants.h"
#include "chrome/browser/chromeos/login/screens/encryption_migration_mode.h"
#include "chrome/browser/chromeos/login/screens/encryption_migration_screen.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/encryption_migration_screen_handler.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/dbus/cryptohome/account_identifier_operators.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::WithArgs;

namespace chromeos {
namespace {

// Fake WakeLock implementation, required by EncryptionMigrationScreen.
class FakeWakeLock : public device::mojom::WakeLock {
 public:
  FakeWakeLock() {}
  ~FakeWakeLock() override {}

  // Implement device::mojom::WakeLock:
  void RequestWakeLock() override { has_wakelock_ = true; }
  void CancelWakeLock() override { has_wakelock_ = false; }
  void AddClient(
      mojo::PendingReceiver<device::mojom::WakeLock> receiver) override {}
  void ChangeType(device::mojom::WakeLockType type,
                  ChangeTypeCallback callback) override {
    NOTIMPLEMENTED();
  }
  void HasWakeLockForTests(HasWakeLockForTestsCallback callback) override {
    std::move(callback).Run(has_wakelock_);
  }

  bool HasWakeLock() { return has_wakelock_; }

 private:
  bool has_wakelock_ = false;
};

// Allows access to testing-only methods of EncryptionMigrationScreen.
class TestEncryptionMigrationScreen : public EncryptionMigrationScreen {
 public:
  explicit TestEncryptionMigrationScreen(EncryptionMigrationScreenView* view)
      : EncryptionMigrationScreen(view) {
    set_free_disk_space_fetcher_for_testing(base::BindRepeating(
        &TestEncryptionMigrationScreen::FreeDiskSpaceFetcher,
        base::Unretained(this)));
    set_tick_clock_for_testing(&testing_tick_clock_);
  }

  // Sets the free disk space seen by EncryptionMigrationScreen.
  void set_free_disk_space(int64_t free_disk_space) {
    free_disk_space_ = free_disk_space;
  }

  // Returns the SimpleTestTickClock used to simulate time elapsed during
  // migration.
  base::SimpleTestTickClock* testing_tick_clock() {
    return &testing_tick_clock_;
  }

  FakeWakeLock* fake_wake_lock() { return &fake_wake_lock_; }

 protected:
  // Override GetWakeLock -- returns our own FakeWakeLock.
  device::mojom::WakeLock* GetWakeLock() override { return &fake_wake_lock_; }

 private:
  // Used as free disk space fetcher callback.
  int64_t FreeDiskSpaceFetcher() { return free_disk_space_; }

  FakeWakeLock fake_wake_lock_;

  // Tick clock used to simulate time elapsed during migration.
  base::SimpleTestTickClock testing_tick_clock_;

  int64_t free_disk_space_;
};

class MockEncryptionMigrationScreenView : public EncryptionMigrationScreenView {
 public:
  MockEncryptionMigrationScreenView() = default;
  ~MockEncryptionMigrationScreenView() override = default;

  MOCK_METHOD(void, Show, ());
  MOCK_METHOD(void, Hide, ());
  MOCK_METHOD(void, SetDelegate, (EncryptionMigrationScreen * delegate));
  MOCK_METHOD(void,
              SetBatteryState,
              (double batteryPercent, bool isEnoughBattery, bool isCharging));
  MOCK_METHOD(void, SetIsResuming, (bool isResuming));
  MOCK_METHOD(void, SetUIState, (UIState state));
  MOCK_METHOD(void,
              SetSpaceInfoInString,
              (int64_t availableSpaceSize, int64_t necessarySpaceSize));
  MOCK_METHOD(void, SetNecessaryBatteryPercent, (double batteryPercent));
  MOCK_METHOD(void, SetMigrationProgress, (double progress));
};

class EncryptionMigrationScreenTest : public testing::Test {
 public:
  EncryptionMigrationScreenTest() = default;
  ~EncryptionMigrationScreenTest() override = default;

  void SetUp() override {
    // Set up a MockUserManager.
    MockUserManager* mock_user_manager = new NiceMock<MockUserManager>();
    scoped_user_manager_enabler_ =
        std::make_unique<user_manager::ScopedUserManager>(
            base::WrapUnique(mock_user_manager));

    // This is used by EncryptionMigrationScreen to remove the existing
    // cryptohome. Ownership of mock_async_method_caller_ is transferred to
    // AsyncMethodCaller::InitializeForTesting.
    mock_async_method_caller_ = new cryptohome::MockAsyncMethodCaller;
    cryptohome::AsyncMethodCaller::InitializeForTesting(
        mock_async_method_caller_);

    // Set up fake dbus clients.
    CryptohomeClient::InitializeFake();
    fake_cryptohome_client_ = FakeCryptohomeClient::Get();
    PowerManagerClient::InitializeFake();

    PowerPolicyController::Initialize(PowerManagerClient::Get());

    // Build dummy user context.
    user_context_.SetAccountId(account_id_);
    user_context_.SetKey(
        Key(Key::KeyType::KEY_TYPE_SALTED_SHA256, "salt", "secret"));

    encryption_migration_screen_ =
        std::make_unique<TestEncryptionMigrationScreen>(&mock_view_);
    encryption_migration_screen_->SetOperationCallbacks(
        base::BindOnce(&EncryptionMigrationScreenTest::OnContinueLogin,
                       base::Unretained(this)),
        base::BindOnce(&EncryptionMigrationScreenTest::OnRestartLogin,
                       base::Unretained(this)));
    encryption_migration_screen_->set_free_disk_space(
        arc::kMigrationMinimumAvailableStorage);
    encryption_migration_screen_->SetUserContext(user_context_);
  }

  void TearDown() override {
    encryption_migration_screen_.reset();

    PowerPolicyController::Shutdown();
    PowerManagerClient::Shutdown();
    CryptohomeClient::Shutdown();
    cryptohome::AsyncMethodCaller::Shutdown();
  }

 protected:
  // A pointer to the EncryptionMigrationScreen used in this test.
  std::unique_ptr<TestEncryptionMigrationScreen> encryption_migration_screen_;

  // Accessory objects needed by EncryptionMigrationScreen.
  MockEncryptionMigrationScreenView mock_view_;

  // Must be the first member.
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_enabler_;
  FakeCryptohomeClient* fake_cryptohome_client_ = nullptr;  // unowned
  cryptohome::MockAsyncMethodCaller* mock_async_method_caller_ = nullptr;

  // Will be set to true in ContinueLogin.
  bool continue_login_callback_called_ = false;
  // Will be set to true in RestartLogin.
  bool restart_login_callback_called_ = false;

  const AccountId account_id_ =
      AccountId::FromUserEmail(user_manager::kStubUserEmail);
  UserContext user_context_;

 private:
  // This will be called by EncryptionMigrationScreen upon finished
  // minimal migration when sign-in should continue.
  void OnContinueLogin(const UserContext& user_context) {
    EXPECT_FALSE(continue_login_callback_called_)
        << "ContinueLogin/RestartLogin may only be called once.";
    EXPECT_FALSE(restart_login_callback_called_)
        << "ContinueLogin/RestartLogin may only be called once.";

    continue_login_callback_called_ = true;
  }

  // This will be called by EncryptionMigrationScreen upon finished
  // minimal migration when the user should re-enter their password.
  void OnRestartLogin(const UserContext& user_context) {
    EXPECT_FALSE(continue_login_callback_called_)
        << "ContinueLogin/RestartLogin may only be called once.";
    EXPECT_FALSE(restart_login_callback_called_)
        << "ContinueLogin/RestartLogin may only be called once.";

    restart_login_callback_called_ = true;
  }
};

}  // namespace

// Tests handling of a minimal migration run that finishes immediately.
TEST_F(EncryptionMigrationScreenTest, MinimalMigration) {
  encryption_migration_screen_->SetMode(
      EncryptionMigrationMode::START_MINIMAL_MIGRATION);
  encryption_migration_screen_->SetupInitialView();

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(encryption_migration_screen_->fake_wake_lock()->HasWakeLock());
  fake_cryptohome_client_->NotifyDircryptoMigrationProgress(
      cryptohome::DircryptoMigrationStatus::DIRCRYPTO_MIGRATION_SUCCESS,
      0 /* current */, 0 /* total */);

  EXPECT_TRUE(continue_login_callback_called_);
  EXPECT_FALSE(encryption_migration_screen_->fake_wake_lock()->HasWakeLock());
  EXPECT_TRUE(fake_cryptohome_client_->to_migrate_from_ecryptfs());
  EXPECT_TRUE(fake_cryptohome_client_->minimal_migration());
  EXPECT_EQ(cryptohome::CreateAccountIdentifierFromAccountId(
                user_context_.GetAccountId()),
            fake_cryptohome_client_->get_id_for_disk_migrated_to_dircrypto());
  EXPECT_EQ(
      user_context_.GetKey()->GetSecret(),
      fake_cryptohome_client_->get_secret_for_last_mount_authentication());
}

// Tests handling of a resumed minimal migration run. This should behave the
// same way that a freshly started minimal migration does (only UMA stats are
// different, but we don't test that at the moment).
TEST_F(EncryptionMigrationScreenTest, ResumeMinimalMigration) {
  encryption_migration_screen_->SetMode(
      EncryptionMigrationMode::RESUME_MINIMAL_MIGRATION);
  encryption_migration_screen_->SetupInitialView();

  task_environment_.RunUntilIdle();

  fake_cryptohome_client_->NotifyDircryptoMigrationProgress(
      cryptohome::DircryptoMigrationStatus::DIRCRYPTO_MIGRATION_SUCCESS,
      0 /* current */, 0 /* total */);

  EXPECT_TRUE(continue_login_callback_called_);
  EXPECT_TRUE(fake_cryptohome_client_->to_migrate_from_ecryptfs());
  EXPECT_TRUE(fake_cryptohome_client_->minimal_migration());
  EXPECT_EQ(cryptohome::CreateAccountIdentifierFromAccountId(
                user_context_.GetAccountId()),
            fake_cryptohome_client_->get_id_for_disk_migrated_to_dircrypto());
  EXPECT_EQ(
      user_context_.GetKey()->GetSecret(),
      fake_cryptohome_client_->get_secret_for_last_mount_authentication());
}

// Tests handling of a minimal migration run that takes a long time to finish.
// We expect that EncryptionMigrationScreen will require the user to re-enter
// their password.
TEST_F(EncryptionMigrationScreenTest, MinimalMigrationSlow) {
  encryption_migration_screen_->SetMode(
      EncryptionMigrationMode::START_MINIMAL_MIGRATION);
  encryption_migration_screen_->SetupInitialView();

  task_environment_.RunUntilIdle();

  encryption_migration_screen_->testing_tick_clock()->Advance(
      base::TimeDelta::FromMinutes(1));
  fake_cryptohome_client_->NotifyDircryptoMigrationProgress(
      cryptohome::DircryptoMigrationStatus::DIRCRYPTO_MIGRATION_SUCCESS,
      0 /* current */, 0 /* total */);

  EXPECT_TRUE(restart_login_callback_called_);
  EXPECT_TRUE(fake_cryptohome_client_->to_migrate_from_ecryptfs());
  EXPECT_TRUE(fake_cryptohome_client_->minimal_migration());
  EXPECT_EQ(cryptohome::CreateAccountIdentifierFromAccountId(
                user_context_.GetAccountId()),
            fake_cryptohome_client_->get_id_for_disk_migrated_to_dircrypto());
  EXPECT_EQ(
      user_context_.GetKey()->GetSecret(),
      fake_cryptohome_client_->get_secret_for_last_mount_authentication());
}

// Tests handling of a minimal migration run that fails.
TEST_F(EncryptionMigrationScreenTest, MinimalMigrationFails) {
  encryption_migration_screen_->SetMode(
      EncryptionMigrationMode::START_MINIMAL_MIGRATION);
  encryption_migration_screen_->SetupInitialView();

  task_environment_.RunUntilIdle();

  encryption_migration_screen_->testing_tick_clock()->Advance(
      base::TimeDelta::FromMinutes(1));
  fake_cryptohome_client_->NotifyDircryptoMigrationProgress(
      cryptohome::DircryptoMigrationStatus::DIRCRYPTO_MIGRATION_FAILED,
      0 /* current */, 0 /* total */);

  Mock::VerifyAndClearExpectations(mock_async_method_caller_);
  EXPECT_TRUE(fake_cryptohome_client_->to_migrate_from_ecryptfs());
  EXPECT_TRUE(fake_cryptohome_client_->minimal_migration());
  EXPECT_EQ(cryptohome::CreateAccountIdentifierFromAccountId(
                user_context_.GetAccountId()),
            fake_cryptohome_client_->get_id_for_disk_migrated_to_dircrypto());
  EXPECT_EQ(
      user_context_.GetKey()->GetSecret(),
      fake_cryptohome_client_->get_secret_for_last_mount_authentication());
}

}  // namespace chromeos
