// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/arc_vm_data_migration_screen.h"

#include <deque>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_vm_client_adapter.h"
#include "ash/components/arc/session/arc_vm_data_migration_status.h"
#include "ash/public/cpp/session/scoped_screen_lock_blocker.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace ash {

namespace {

constexpr char kPathToCheckFreeDiskSpace[] = "/home/chronos/user";
// TODO(b/258278176): Set appropriate thresholds based on experiments.
constexpr int64_t kMinimumFreeDiskSpaceForMigration = 1LL << 30;  // 1 GB.
constexpr double kMinimumBatteryPercent = 30.0;

constexpr char kUserActionSkip[] = "skip";
constexpr char kUserActionUpdate[] = "update";

}  // namespace

ArcVmDataMigrationScreen::ArcVmDataMigrationScreen(
    base::WeakPtr<ArcVmDataMigrationScreenView> view)
    : BaseScreen(ArcVmDataMigrationScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)) {
  DCHECK(view_);
}

ArcVmDataMigrationScreen::~ArcVmDataMigrationScreen() = default;

void ArcVmDataMigrationScreen::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (proto.has_battery_percent()) {
    battery_percent_ = proto.battery_percent();
  }

  if (proto.has_external_power()) {
    is_connected_to_charger_ =
        proto.external_power() !=
        power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED;
  }

  if (!view_) {
    return;
  }
  view_->SetBatteryState(battery_percent_ >= kMinimumBatteryPercent,
                         is_connected_to_charger_);

  // TODO(b/258278176): Properly handle cases like the resume screen and the
  // progress screen.
  if (current_ui_state_ == ArcVmDataMigrationScreenView::UIState::kLoading) {
    UpdateUIState(ArcVmDataMigrationScreenView::UIState::kWelcome);
  }
}

void ArcVmDataMigrationScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  // The migration screen is shown after a session restart with an ARC-enabled
  // login user, and thus the primary profile is available at this point.
  profile_ = ProfileManager::GetPrimaryUserProfile();
  DCHECK(profile_);
  user_id_hash_ = ProfileHelper::GetUserIdHashFromProfile(profile_);
  DCHECK(!user_id_hash_.empty());

  GetWakeLock()->RequestWakeLock();

  DCHECK(Shell::Get());
  DCHECK(Shell::Get()->session_controller());
  scoped_screen_lock_blocker_ =
      Shell::Get()->session_controller()->GetScopedScreenLockBlocker();

  view_->Show();
  StopArcVmInstanceAndArcUpstartJobs();
}

void ArcVmDataMigrationScreen::HideImpl() {
  GetWakeLock()->CancelWakeLock();
  if (scoped_screen_lock_blocker_) {
    scoped_screen_lock_blocker_.reset();
  }
}

void ArcVmDataMigrationScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  VLOG(1) << "User action: action_id=" << action_id;
  if (action_id == kUserActionSkip) {
    HandleSkip();
  } else if (action_id == kUserActionUpdate) {
    HandleUpdate();
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void ArcVmDataMigrationScreen::StopArcVmInstanceAndArcUpstartJobs() {
  DCHECK(ConciergeClient::Get());

  // Check whether ARCVM is running. At this point ArcSessionManager is not
  // initialized yet, but a stale ARCVM instance can be running.
  vm_tools::concierge::GetVmInfoRequest request;
  request.set_name(arc::kArcVmName);
  request.set_owner_id(user_id_hash_);
  ConciergeClient::Get()->GetVmInfo(
      std::move(request),
      base::BindOnce(&ArcVmDataMigrationScreen::OnGetVmInfoResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcVmDataMigrationScreen::OnGetVmInfoResponse(
    absl::optional<vm_tools::concierge::GetVmInfoResponse> response) {
  if (!response.has_value()) {
    LOG(ERROR) << "GetVmInfo for ARCVM failed: No D-Bus response";
    HandleFatalError();
    return;
  }

  // Unsuccessful response means that ARCVM is not running, because concierge
  // looks at the list of running VMs. See concierge's Service::GetVmInfo().
  if (!response->success()) {
    VLOG(1) << "ARCVM is not running";
    StopArcUpstartJobs();
    return;
  }

  // ARCVM is running. Send the StopVmRequest signal and wait for OnVmStopped()
  // to be invoked.
  VLOG(1) << "ARCVM is running. Sending StopVmRequest to concierge";
  concierge_observation_.Observe(ConciergeClient::Get());
  vm_tools::concierge::StopVmRequest request;
  request.set_name(arc::kArcVmName);
  request.set_owner_id(user_id_hash_);
  ConciergeClient::Get()->StopVm(
      std::move(request),
      base::BindOnce(&ArcVmDataMigrationScreen::OnStopVmResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcVmDataMigrationScreen::OnStopVmResponse(
    absl::optional<vm_tools::concierge::StopVmResponse> response) {
  if (!response.has_value() || !response->success()) {
    LOG(ERROR) << "StopVm for ARCVM failed: "
               << (response.has_value() ? response->failure_reason()
                                        : "No D-Bus response");
    concierge_observation_.Reset();
    HandleFatalError();
  }
}

void ArcVmDataMigrationScreen::StopArcUpstartJobs() {
  std::deque<arc::JobDesc> jobs;
  for (const char* job : arc::kArcVmUpstartJobsToBeStoppedOnRestart) {
    jobs.emplace_back(job, arc::UpstartOperation::JOB_STOP,
                      std::vector<std::string>());
  }
  arc::ConfigureUpstartJobs(
      std::move(jobs),
      base::BindOnce(&ArcVmDataMigrationScreen::OnArcUpstartJobsStopped,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcVmDataMigrationScreen::OnArcUpstartJobsStopped(bool result) {
  // |result| is true when there are no stale Upstart jobs.
  if (!result) {
    LOG(ERROR) << "Failed to stop ARC Upstart jobs";
    HandleFatalError();
    return;
  }

  SetUpInitialView();
}

void ArcVmDataMigrationScreen::SetUpInitialView() {
  arc::ArcVmDataMigrationStatus data_migration_status =
      arc::GetArcVmDataMigrationStatus(profile_->GetPrefs());
  switch (data_migration_status) {
    case arc::ArcVmDataMigrationStatus::kConfirmed:
      // Set the status back to kNotified to prepare for cases where the
      // migration is skipped or the device is shut down before the migration is
      // started.
      arc::SetArcVmDataMigrationStatus(
          profile_->GetPrefs(), arc::ArcVmDataMigrationStatus::kNotified);
      DCHECK(ash::SpacedClient::Get());
      ash::SpacedClient::Get()->GetFreeDiskSpace(
          kPathToCheckFreeDiskSpace,
          base::BindOnce(&ArcVmDataMigrationScreen::OnGetFreeDiskSpace,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    case arc::ArcVmDataMigrationStatus::kStarted:
      // TODO(b/258278176): Show the resume screen.
      UpdateUIState(ArcVmDataMigrationScreenView::UIState::kWelcome);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ArcVmDataMigrationScreen::OnGetFreeDiskSpace(
    absl::optional<int64_t> reply) {
  if (!reply.has_value() || reply.value() < 0) {
    LOG(ERROR) << "Failed to get free disk space from spaced";
    HandleFatalError();
    return;
  }

  if (!view_) {
    return;
  }

  const int64_t free_disk_space = reply.value();
  VLOG(1) << "Free disk space is " << free_disk_space;
  if (free_disk_space < kMinimumFreeDiskSpaceForMigration) {
    view_->SetRequiredFreeDiskSpace(kMinimumFreeDiskSpaceForMigration);
    // Update the UI to show the low disk space warning and return, because the
    // user cannot free up the disk space while in the screen, and thus there is
    // no point in reporting the battery state in this case.
    DCHECK_EQ(current_ui_state_,
              ArcVmDataMigrationScreenView::UIState::kLoading);
    UpdateUIState(ArcVmDataMigrationScreenView::UIState::kWelcome);
    return;
  }

  view_->SetMinimumBatteryPercent(kMinimumBatteryPercent);

  // Request PowerManager to report the battery status updates. The UI will be
  // updated on PowerChanged().
  DCHECK(chromeos::PowerManagerClient::Get());
  power_manager_observation_.Observe(chromeos::PowerManagerClient::Get());
  chromeos::PowerManagerClient::Get()->RequestStatusUpdate();
}

void ArcVmDataMigrationScreen::SetUpDestinationAndTriggerMigration() {
  if (base::FeatureList::IsEnabled(arc::kLvmApplicationContainers)) {
    // TODO(b/258278176): Handle LVM backend cases.
    NOTIMPLEMENTED();
    return;
  }

  vm_tools::concierge::CreateDiskImageRequest request;
  request.set_cryptohome_id(user_id_hash_);
  request.set_vm_name(arc::kArcVmName);
  request.set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
  request.set_filesystem_type(vm_tools::concierge::FilesystemType::EXT4);
  // Keep the options in sync with the guest side ones set by arc-mkfs-blk-data.
  constexpr std::array<const char*, 4> kMkfsOpts{
      "-b4096",                                  // block-size
      "-i65536",                                 // bytes-per-inode
      "-Ocasefold,project,quota,verity",         // feature
      "-Equotatype=usrquota:grpquota:prjquota",  // extended-options
  };
  for (const char* mkfs_opt : kMkfsOpts) {
    request.add_mkfs_opts(mkfs_opt);
  }
  constexpr std::array<const char*, 2> kTune2fsOpts{
      "-g1065",   // Set the group which can use the reserved filesystem blocks.
      "-r32000",  // Set the number of reserved filesystem blocks.
  };
  for (const char* tune2fs_opt : kTune2fsOpts) {
    request.add_tune2fs_opts(tune2fs_opt);
  }
  // TODO(b/258278176): Show a different UI while setting up the disk image, and
  // prevent (or gracefully handle) cases where the update button is pressed
  // multiple times.
  ConciergeClient::Get()->CreateDiskImage(
      std::move(request),
      base::BindOnce(&ArcVmDataMigrationScreen::OnCreateDiskImageResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcVmDataMigrationScreen::OnCreateDiskImageResponse(
    absl::optional<vm_tools::concierge::CreateDiskImageResponse> response) {
  if (!response.has_value()) {
    LOG(ERROR) << "Failed to create a disk image for /data: No D-Bus response";
    HandleFatalError();
    return;
  }

  switch (response->status()) {
    case vm_tools::concierge::DISK_STATUS_CREATED:
      VLOG(1) << "Created a disk image for /data at " << response->disk_path();
      break;
    case vm_tools::concierge::DISK_STATUS_EXISTS:
      // This is actually unexpected. We should probably destroy the disk image
      // and then recreate it at least in non-resume cases.
      // TODO(b/258278176): Properly handle DISK_STATUS_EXISTS cases.
      LOG(WARNING) << "Disk image for /data already exists at "
                   << response->disk_path();
      break;
    default:
      LOG(ERROR) << "Failed to create a disk image for /data. Status: "
                 << response->status()
                 << ", reason: " << response->failure_reason();
      HandleFatalError();
      return;
  }

  TriggerMigration();
}

void ArcVmDataMigrationScreen::TriggerMigration() {
  arc::SetArcVmDataMigrationStatus(profile_->GetPrefs(),
                                   arc::ArcVmDataMigrationStatus::kStarted);
  // TODO(b/258278176): Trigger the migration.
  NOTIMPLEMENTED();
}

void ArcVmDataMigrationScreen::OnVmStarted(
    const vm_tools::concierge::VmStartedSignal& signal) {}

void ArcVmDataMigrationScreen::OnVmStopped(
    const vm_tools::concierge::VmStoppedSignal& signal) {
  if (signal.name() != arc::kArcVmName) {
    return;
  }

  VLOG(1) << "ARCVM is stopped";
  concierge_observation_.Reset();
  StopArcUpstartJobs();
}

void ArcVmDataMigrationScreen::UpdateUIState(
    ArcVmDataMigrationScreenView::UIState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_ui_state_ = state;
  if (view_) {
    view_->SetUIState(state);
  }
}

void ArcVmDataMigrationScreen::HandleSkip() {
  // TODO(b/258278176): Properly handle the skip action.
  chrome::AttemptRelaunch();
}

void ArcVmDataMigrationScreen::HandleUpdate() {
  SetUpDestinationAndTriggerMigration();
}

void ArcVmDataMigrationScreen::HandleFatalError() {
  // TODO(b/258278176): Show a fatal error screen and report the reason.
  chrome::AttemptRelaunch();
}

device::mojom::WakeLock* ArcVmDataMigrationScreen::GetWakeLock() {
  // |wake_lock_| is lazy bound and reused, even after a connection error.
  if (wake_lock_) {
    return wake_lock_.get();
  }

  mojo::PendingReceiver<device::mojom::WakeLock> receiver =
      wake_lock_.BindNewPipeAndPassReceiver();

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
  content::GetDeviceService().BindWakeLockProvider(
      wake_lock_provider.BindNewPipeAndPassReceiver());
  wake_lock_provider->GetWakeLockWithoutContext(
      device::mojom::WakeLockType::kPreventAppSuspension,
      device::mojom::WakeLockReason::kOther,
      "ARCVM /data migration is in progress...", std::move(receiver));
  return wake_lock_.get();
}

}  // namespace ash
