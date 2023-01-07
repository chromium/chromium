// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ACTIVE_DIRECTORY_ACTIVE_DIRECTORY_MIGRATION_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_ACTIVE_DIRECTORY_ACTIVE_DIRECTORY_MIGRATION_MANAGER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/session_manager/core/session_manager_observer.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace policy {

// Manages the migration of AD managed devices into cloud management. The goal
// is to start the migration when (a) the device is on the login screen, (b) the
// enrollment ID has already been uploaded to DMServer and (c) the
// `ChromadToCloudMigrationEnabled` policy is enabled. After being constructed,
// this class listens to changes from the `SessionManager` and from the
// `kEnrollmentIdUploadedOnChromad` and `kChromadToCloudMigrationEnabled` local
// state prefs. Additionally, these checks are periodically executed while the
// device is on the login screen.
class ActiveDirectoryMigrationManager
    : public session_manager::SessionManagerObserver {
 public:
  explicit ActiveDirectoryMigrationManager(PrefService* local_state);

  ~ActiveDirectoryMigrationManager() override;

  // Disallow copy and assignment.
  ActiveDirectoryMigrationManager(const ActiveDirectoryMigrationManager&) =
      delete;
  ActiveDirectoryMigrationManager& operator=(
      const ActiveDirectoryMigrationManager&) = delete;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Registers to prefs changes and tries to start the migration.
  void Init();

  // Unregisters to prefs changes.
  void Shutdown();

  // Callback called when the `TryToStartMigration` method is executed. Returns
  // whether the migrations started or not, and whether a retry was scheduled or
  // not.
  using StatusCallback =
      base::OnceCallback<void(bool started, bool rescheduled)>;

  // Only used for testing.
  void SetStatusCallbackForTesting(StatusCallback callback);

 private:
  // Returns true if the enrollment ID has already been uploaded.
  bool HasUploadedEnrollmentId() const;

  // Returns true if the migration of Chromad devices to cloud management is
  // enabled.
  bool IsChromadMigrationEnabled() const;

  // Returns true if the last powerwash attempt happened more than
  // `kPowerwashBackoffTime` ago.
  bool HasBackoffTimePassed() const;

  // Pref change handlers.
  void OnEnrollmentIdUploadedPrefChanged();
  void OnChromadMigrationEnabledPrefChanged();

  // session_manager::SessionManagerObserver:
  void OnLoginOrLockScreenVisible() override;

  // Triggers a device powerwash, if the pre-requisites are satisfied. Called
  // every time one of the three events of interest happens. Also called
  // periodically while the device is on the login screen.
  void TryToStartMigration();

  // Executes the same steps as `TryToStartMigration`, but also updates the
  // value of `retry_already_scheduled_` accordingly.
  void RetryToStartMigration();

  // Sends a device powerwash request through D-Bus.
  void StartPowerwash();

  // Runs the `status_callback_for_testing_`, if it's not empty. Passes the
  // received boolean values to the callback. Only used for testing.
  void MaybeRunStatusCallback(bool started, bool rescheduled);

  // Local state prefs, not owned.
  raw_ptr<PrefService> local_state_;

  // Observer for Chromad migration related prefs.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  bool retry_already_scheduled_ = false;

  StatusCallback status_callback_for_testing_;

  // Must be the last member.
  base::WeakPtrFactory<ActiveDirectoryMigrationManager> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ACTIVE_DIRECTORY_ACTIVE_DIRECTORY_MIGRATION_MANAGER_H_
