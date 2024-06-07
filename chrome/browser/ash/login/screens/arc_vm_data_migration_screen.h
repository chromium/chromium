// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_ARC_VM_DATA_MIGRATION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_ARC_VM_DATA_MIGRATION_SCREEN_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/oobe_mojo_binder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/login/arc_vm_data_migration_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_login.mojom.h"
#include "chromeos/ash/components/dbus/arc/arcvm_data_migrator_client.h"
#include "chromeos/ash/components/dbus/arcvm_data_migrator/arcvm_data_migrator.pb.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"

namespace base {
class TickClock;
}  // namespace base

namespace ash {

class ScopedScreenLockBlocker;
class ArcVmDataMigrationScreenView;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "ArcVmDataMigrationScreenSetupFailure" in tools/metrics/histograms/enums.xml.
enum class ArcVmDataMigrationScreenSetupFailure {
  kGetVmInfoFailure = 0,        // Deprecated.
  kStopVmFailure = 1,           // Deprecated.
  kStopUpstartJobsFailure = 2,  // Deprecated.
  kGetFreeDiskSpaceFailure = 3,
  kGetAndroidDataInfoFailure = 4,
  kCreateDiskImageDBusFailure = 5,
  kCreateDiskImageGeneralFailure = 6,
  kArcVmDataMigratorStartFailure = 7,
  kStartMigrationFailure = 8,
  kStopArcVmAndArcVmUpstartJobsFailure = 9,
  kMaxValue = kStopArcVmAndArcVmUpstartJobsFailure,
};

class ArcVmDataMigrationScreen
    : public BaseScreen,
      public ArcVmDataMigratorClient::Observer,
      public chromeos::PowerManagerClient::Observer,
      public screens_login::mojom::ArcVmDataMigrationPageHandler,
      public OobeMojoBinder<screens_login::mojom::ArcVmDataMigrationPageHandler,
                            screens_login::mojom::ArcVmDataMigrationPage> {
 public:
  using TView = ArcVmDataMigrationScreenView;
  explicit ArcVmDataMigrationScreen(
      base::WeakPtr<ArcVmDataMigrationScreenView> view);
  ~ArcVmDataMigrationScreen() override;
  ArcVmDataMigrationScreen(const ArcVmDataMigrationScreen&) = delete;
  ArcVmDataMigrationScreen& operator=(const ArcVmDataMigrationScreen&) = delete;

  void SetTickClockForTesting(const base::TickClock* tick_clock);
  void SetRemoteForTesting(
      mojo::PendingRemote<screens_login::mojom::ArcVmDataMigrationPage>
          pending_page);

 protected:
  // screens_login::mojom::ArcVmDataMigrationPageHandler
  void OnSkipClicked() override;
  void OnUpdateClicked() override;
  void OnResumeClicked() override;
  void OnFinishClicked() override;
  void OnReportClicked() override;

 private:
  // BaseScreen overrides:
  void ShowImpl() override;
  void HideImpl() override;

  void OnArcVmAndArcVmUpstartJobsStopped(bool result);

  void SetUpInitialView();

  void OnGetFreeDiskSpace(std::optional<int64_t> reply);

  void OnGetAndroidDataInfoResponse(
      uint64_t free_disk_space,
      const base::TimeTicks& time_before_get_android_data_info,
      std::optional<arc::data_migrator::GetAndroidDataInfoResponse> response);

  void CheckBatteryState();

  // chromeos::PowerManagerClient::Observer override:
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

  // Sets up the destination of the migration, and then triggers the migration.
  void SetUpDestinationAndTriggerMigration();
  void OnCreateDiskImageResponse(
      std::optional<vm_tools::concierge::CreateDiskImageResponse> res);

  // Triggers the migration by calling ArcVmDataMigrator's StartMigration().
  void TriggerMigration();

  void OnArcVmDataMigratorStarted(bool result);
  void OnStartMigrationResponse(bool result);

  // ArcVmDataMigratorClient::Observer override:
  void OnDataMigrationProgress(
      const arc::data_migrator::DataMigrationProgress& progress) override;

  void UpdateProgressBar(uint64_t current_bytes, uint64_t total_bytes);

  void RemoveArcDataAndShowFailureScreen();
  void OnArcDataRemoved(bool success);

  void UpdateUIState(
      screens_login::mojom::ArcVmDataMigrationPage::ArcVmUIState state);

  void HandleSetupFailure(ArcVmDataMigrationScreenSetupFailure failure);

  // Handle errors that are expected to be retriable after going back to the
  // desktop and re-entering the migration flow. Should not be called when
  // resuming, because it prevents the user from going back to the desktop.
  virtual void HandleRetriableFatalError();

  virtual device::mojom::WakeLock* GetWakeLock();

  raw_ptr<Profile> profile_;
  std::string user_id_hash_;

  screens_login::mojom::ArcVmDataMigrationPage::ArcVmUIState current_ui_state_ =
      screens_login::mojom::ArcVmDataMigrationPage::ArcVmUIState::kLoading;

  uint64_t disk_size_ = 0;

  double battery_percent_ = 100.0;
  bool is_connected_to_charger_ = true;

  // Holds the value of |battery_percent_| when |update_button_pressed_| is
  // flipped to true.
  double battery_percent_on_migration_start_;
  // Holds the lowest value of |battery_percent_| observed after
  // |update_button_pressed_| is flipped to true.
  double lowest_battery_percent_during_migration_;

  raw_ptr<const base::TickClock> tick_clock_ = nullptr;
  base::TimeTicks previous_ticks_ = {};
  uint64_t previous_bytes_ = 0;

  // Average speed of migration (bytes per millisecond) adjusted with a smooth
  // factor.
  double average_speed_ = 0.0;

  // Indicates whether the migration was previously stopped halfway and is being
  // resumed. When this is true, the free space check is skipped and the resume
  // screen (not the default welcome screen) is displayed as the initial screen.
  // Also, when resuming, setup failures are treated in the same way as
  // migration failures (i.e., wipe /data, mark the migration as finished, and
  // show the failure screen) to avoid unmanageable resumes.
  bool resuming_ = false;

  bool update_button_pressed_ = false;

  base::ScopedObservation<ArcVmDataMigratorClient,
                          ArcVmDataMigratorClient::Observer>
      migration_progress_observation_{this};

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_observation_{this};

  mojo::Remote<device::mojom::WakeLock> wake_lock_;
  std::unique_ptr<ScopedScreenLockBlocker> scoped_screen_lock_blocker_;

  base::WeakPtr<ArcVmDataMigrationScreenView> view_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ArcVmDataMigrationScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_ARC_VM_DATA_MIGRATION_SCREEN_H_
