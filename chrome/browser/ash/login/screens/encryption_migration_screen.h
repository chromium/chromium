// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_ENCRYPTION_MIGRATION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_ENCRYPTION_MIGRATION_SCREEN_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/encryption_migration_mode.h"
#include "chrome/browser/ash/login/screens/oobe_mojo_binder.h"
#include "chrome/browser/ui/webui/ash/login/encryption_migration_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_login.mojom.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/mount_performer.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

namespace ash {

class LoginFeedback;
class UserContext;

class EncryptionMigrationScreen
    : public BaseScreen,
      public chromeos::PowerManagerClient::Observer,
      public UserDataAuthClient::Observer,
      public screens_login::mojom::EncryptionMigrationPageHandler,
      public OobeMojoBinder<
          screens_login::mojom::EncryptionMigrationPageHandler,
          screens_login::mojom::EncryptionMigrationPage> {
 public:
  using TView = EncryptionMigrationScreenView;

  using SkipMigrationCallback =
      base::OnceCallback<void(std::unique_ptr<UserContext>)>;

  class EncryptionMigrationScreenTestDelegate {
   public:
    virtual ~EncryptionMigrationScreenTestDelegate() = default;

    // Returns free disk space for testing.
    virtual int64_t GetFreeSpace() const = 0;
  };

  explicit EncryptionMigrationScreen(
      base::WeakPtr<EncryptionMigrationScreenView> view);

  EncryptionMigrationScreen(const EncryptionMigrationScreen&) = delete;
  EncryptionMigrationScreen& operator=(const EncryptionMigrationScreen&) =
      delete;

  ~EncryptionMigrationScreen() override;

  // Sets the UserContext for a user whose cryptohome should be migrated.
  void SetUserContext(std::unique_ptr<UserContext> user_context);

  // Sets the migration mode.
  void SetMode(EncryptionMigrationMode mode);

  // Sets continue login callback and restart log in callback, which should be
  // called when the user want to log in to the session from the migration UI
  // and when the user should re-enter their password.
  void SetSkipMigrationCallback(SkipMigrationCallback skip_migration_callback);

  // Setup the initial view in the migration UI.
  // This should be called after other state like UserContext, etc... are set.
  void SetupInitialView();

  static void SetEncryptionMigrationScreenTestDelegate(
      EncryptionMigrationScreenTestDelegate* test_delegate);

 protected:
  virtual device::mojom::WakeLock* GetWakeLock();

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;

  // PowerManagerClient::Observer implementation:
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

  // UserDataAuthClient::Observer implementation:
  void DircryptoMigrationProgress(
      const ::user_data_auth::DircryptoMigrationProgress& progress) override;
  // screens_login::mojom::EncryptionMigrationPageHandler
  void OnStartMigration() override;
  void OnSkipMigration() override;
  void OnRequestRestartOnLowStorage() override;
  void OnRequestRestartOnFailure() override;
  void OnOpenFeedbackDialog() override;

  // Updates UI state.
  void UpdateUIState(
      screens_login::mojom::EncryptionMigrationPage::UIState state);

  void CheckAvailableStorage();
  void OnGetAvailableStorage(int64_t size);
  void WaitBatteryAndMigrate();
  void StartMigration();
  // Removes cryptohome and shows the error screen after the removal finishes.
  void RemoveCryptohome();

  // Handlers for cryptohome API callbacks.
  void OnMigrationRequested(std::unique_ptr<UserContext> context,
                            std::optional<AuthenticationError> error);
  void OnRemoveCryptohome(std::unique_ptr<UserContext> context,
                          std::optional<AuthenticationError> error);
  void OnMountExistingVault(std::unique_ptr<UserContext> context,
                            std::optional<AuthenticationError> error);

  // Records UMA about visible screen after delay.
  void OnDelayedRecordVisibleScreen(
      screens_login::mojom::EncryptionMigrationPage::UIState state);

  // True if |mode_| suggests that we are resuming an incomplete migration.
  bool IsResumingIncompleteMigration() const;

  // True if |mode_| suggests that migration should start immediately.
  bool IsStartImmediately() const;

  // Stop forcing migration if it was forced by policy.
  void MaybeStopForcingMigration();

  base::WeakPtr<EncryptionMigrationScreenView> view_;

  // The current UI state which should be refrected in the web UI.
  screens_login::mojom::EncryptionMigrationPage::UIState current_ui_state_ =
      screens_login::mojom::EncryptionMigrationPage::UIState::kInitial;

  // The current user's UserContext, which is used to request the migration to
  // cryptohome.
  std::unique_ptr<UserContext> user_context_;

  // The callback which is used to log in to the session from the migration UI.
  SkipMigrationCallback skip_migration_callback_;

  MountPerformer mount_performer_;

  // The migration mode (ask user / start migration automatically / resume
  // incomplete migratoin).
  EncryptionMigrationMode mode_ = EncryptionMigrationMode::ASK_USER;

  // The current battery level.
  std::optional<double> current_battery_percent_;

  // True if the migration should start immediately once the battery level gets
  // sufficient.
  bool should_migrate_on_enough_battery_ = false;

  // The battery level at the timing that the migration starts.
  double initial_battery_percent_ = 0.0;

  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  std::unique_ptr<LoginFeedback> login_feedback_;

  std::unique_ptr<
      base::ScopedObservation<UserDataAuthClient, UserDataAuthClient::Observer>>
      userdataauth_observer_;

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_observation_{this};

  base::WeakPtrFactory<EncryptionMigrationScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_ENCRYPTION_MIGRATION_SCREEN_H_
