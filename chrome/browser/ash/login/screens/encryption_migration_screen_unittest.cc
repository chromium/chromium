// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/arc/arc_migration_constants.h"
#include "chrome/browser/ash/login/screens/encryption_migration_mode.h"
#include "chrome/browser/ash/login/screens/encryption_migration_screen.h"
#include "chrome/browser/ash/login/users/mock_user_manager.h"
#include "chrome/browser/ui/webui/ash/login/base_webui_handler.h"
#include "chrome/browser/ui/webui/ash/login/encryption_migration_screen_handler.h"
#include "chromeos/ash/components/dbus/cryptohome/account_identifier_operators.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::WithArgs;

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

class MockEncryptionMigrationScreenView : public EncryptionMigrationScreenView {
 public:
  MockEncryptionMigrationScreenView() = default;
  ~MockEncryptionMigrationScreenView() override = default;

  MOCK_METHOD(void, Show, ());
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

// Allows access to testing-only methods of EncryptionMigrationScreen.
class TestEncryptionMigrationScreen : public EncryptionMigrationScreen {
 public:
  explicit TestEncryptionMigrationScreen(
      base::WeakPtr<MockEncryptionMigrationScreenView> view)
      : EncryptionMigrationScreen(std::move(view)) {}

  // Sets the free disk space seen by EncryptionMigrationScreen.
  void set_free_disk_space(int64_t free_disk_space) {
    free_disk_space_ = free_disk_space;
  }

  FakeWakeLock* fake_wake_lock() { return &fake_wake_lock_; }

 protected:
  // Override GetWakeLock -- returns our own FakeWakeLock.
  device::mojom::WakeLock* GetWakeLock() override { return &fake_wake_lock_; }

 private:
  // Used as free disk space fetcher callback.
  int64_t FreeDiskSpaceFetcher() { return free_disk_space_; }

  FakeWakeLock fake_wake_lock_;

  int64_t free_disk_space_;
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

    // Set up fake dbus clients.
    UserDataAuthClient::InitializeFake();
    fake_userdataauth_client_ = FakeUserDataAuthClient::Get();
    auto cryptohome_account_id =
        cryptohome::CreateAccountIdentifierFromAccountId(account_id_);
    FakeUserDataAuthClient::TestApi::Get()->AddExistingUser(
        std::move(cryptohome_account_id));
    chromeos::PowerManagerClient::InitializeFake();

    chromeos::PowerPolicyController::Initialize(
        chromeos::PowerManagerClient::Get());

    // Build dummy user context.
    auto user_context = std::make_unique<UserContext>();
    user_context->SetAccountId(account_id_);
    user_context->SetKey(
        Key(Key::KeyType::KEY_TYPE_SALTED_SHA256, "salt", "secret"));

    base::test::TestFuture<bool, std::unique_ptr<UserContext>,
                           absl::optional<AuthenticationError>>
        future;
    AuthPerformer auth_performer(fake_userdataauth_client_);
    auth_performer.StartAuthSession(
        std::move(user_context), /*ephemeral=*/false,
        AuthSessionIntent::kDecrypt, future.GetCallback());
    user_context = std::get<1>(future.Take());

    encryption_migration_screen_ =
        std::make_unique<TestEncryptionMigrationScreen>(std::move(mock_view_));
    encryption_migration_screen_->SetSkipMigrationCallback(
        base::BindOnce(&EncryptionMigrationScreenTest::OnContinueLogin,
                       base::Unretained(this)));
    encryption_migration_screen_->set_free_disk_space(
        arc::kMigrationMinimumAvailableStorage);
    encryption_migration_screen_->SetUserContext(std::move(user_context));
  }

  void TearDown() override {
    encryption_migration_screen_.reset();

    chromeos::PowerPolicyController::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
    UserDataAuthClient::Shutdown();
  }

 protected:
  // A pointer to the EncryptionMigrationScreen used in this test.
  std::unique_ptr<TestEncryptionMigrationScreen> encryption_migration_screen_;

  // Accessory objects needed by EncryptionMigrationScreen.
  base::WeakPtr<MockEncryptionMigrationScreenView> mock_view_;

  // Must be the first member.
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_enabler_;
  FakeUserDataAuthClient* fake_userdataauth_client_ = nullptr;  // unowned

  // Will be set to true in ContinueLogin.
  bool skip_migration_callback_called_ = false;

  const AccountId account_id_ =
      AccountId::FromUserEmail(user_manager::kStubUserEmail);

 private:
  // This will be called by EncryptionMigrationScreen upon finished
  // minimal migration when sign-in should continue.
  void OnContinueLogin(std::unique_ptr<UserContext> user_context) {
    EXPECT_FALSE(skip_migration_callback_called_)
        << "ContinueLogin/RestartLogin may only be called once.";

    skip_migration_callback_called_ = true;
  }
};

}  // namespace
}  // namespace ash
