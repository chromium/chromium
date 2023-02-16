// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_ARC_VM_DATA_MIGRATION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_ARC_VM_DATA_MIGRATION_SCREEN_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/login/arc_vm_data_migration_screen_handler.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class ScopedScreenLockBlocker;

class ArcVmDataMigrationScreen : public BaseScreen,
                                 public ConciergeClient::VmObserver,
                                 public chromeos::PowerManagerClient::Observer {
 public:
  explicit ArcVmDataMigrationScreen(
      base::WeakPtr<ArcVmDataMigrationScreenView> view);
  ~ArcVmDataMigrationScreen() override;
  ArcVmDataMigrationScreen(const ArcVmDataMigrationScreen&) = delete;
  ArcVmDataMigrationScreen& operator=(const ArcVmDataMigrationScreen&) = delete;

  // chromeos::PowerManagerClient::Observer override:
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

 private:
  // BaseScreen overrides:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  // Stops ARCVM instance and ARC-related Upstart jobs that have outlived the
  // previous session.
  void StopArcVmInstanceAndArcUpstartJobs();

  void OnGetVmInfoResponse(
      absl::optional<vm_tools::concierge::GetVmInfoResponse> response);
  void OnStopVmResponse(
      absl::optional<vm_tools::concierge::StopVmResponse> response);

  void StopArcUpstartJobs();
  void OnArcUpstartJobsStopped(bool result);

  void SetUpInitialView();

  void OnGetFreeDiskSpace(absl::optional<int64_t> reply);

  // Sets up the destination of the migration, and then triggers the migration.
  void SetUpDestinationAndTriggerMigration();
  void OnCreateDiskImageResponse(
      absl::optional<vm_tools::concierge::CreateDiskImageResponse> res);

  // Triggers the actual data migration.
  void TriggerMigration();

  // ConciergeClient::VmObserver overrides:
  void OnVmStarted(const vm_tools::concierge::VmStartedSignal& signal) override;
  void OnVmStopped(const vm_tools::concierge::VmStoppedSignal& signal) override;

  void UpdateUIState(ArcVmDataMigrationScreenView::UIState state);

  void HandleSkip();
  void HandleUpdate();

  virtual void HandleFatalError();

  virtual device::mojom::WakeLock* GetWakeLock();

  Profile* profile_;
  std::string user_id_hash_;

  ArcVmDataMigrationScreenView::UIState current_ui_state_ =
      ArcVmDataMigrationScreenView::UIState::kLoading;

  double battery_percent_ = 100.0;
  bool is_connected_to_charger_ = true;

  base::ScopedObservation<ConciergeClient, ConciergeClient::VmObserver>
      concierge_observation_{this};

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
