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
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/ui/login_feedback.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace ash {

namespace {

constexpr char kArcRemoveDataJobName[] = "arc_2dremove_2ddata";

constexpr char kPathToCheckFreeDiskSpace[] = "/home/chronos/user";
// TODO(b/258278176): Set appropriate thresholds based on experiments.
constexpr int64_t kMinimumFreeDiskSpaceForMigration = 1LL << 30;  // 1 GB.
constexpr double kMinimumBatteryPercent = 30.0;

// |average_speed_| is calculated using the smooth factor k as:
// k * (average speed of the latest interval) + (1 - k) * |average_speed_|.
constexpr double kAverageSpeedSmoothFactor = 0.1;

// |average_speed_| := max(|average_speed_|, kAverageSpeedDropBound).
constexpr double kAverageSpeedDropBound = 1e-8;

// Minimum length of interval to update the migration's progress and calculate
// its average speed.
constexpr base::TimeDelta kMinimumIntervalLengthForProgressUpdate =
    base::Milliseconds(100);
constexpr base::TimeDelta kMaximumEstimatedRemainingTime = base::Days(1);

constexpr char kUserActionSkip[] = "skip";
constexpr char kUserActionUpdate[] = "update";
constexpr char kUserActionResume[] = "resume";
constexpr char kUserActionFinish[] = "finish";
constexpr char kUserActionReport[] = "report";

std::string GetChromeOsUsername(Profile* profile) {
  const AccountId account(multi_user_util::GetAccountIdFromProfile(profile));
  return cryptohome::CreateAccountIdentifierFromAccountId(account).account_id();
}

}  // namespace

ArcVmDataMigrationScreen::ArcVmDataMigrationScreen(
    base::WeakPtr<ArcVmDataMigrationScreenView> view)
    : BaseScreen(ArcVmDataMigrationScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      view_(std::move(view)) {
  DCHECK(view_);
}

ArcVmDataMigrationScreen::~ArcVmDataMigrationScreen() = default;

void ArcVmDataMigrationScreen::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
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
  } else if (action_id == kUserActionResume) {
    HandleResume();
  } else if (action_id == kUserActionFinish) {
    HandleFinish();
  } else if (action_id == kUserActionReport) {
    HandleReport();
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
  DCHECK(!concierge_observation_.IsObserving());
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(profile_);
  switch (arc::GetArcVmDataMigrationStatus(profile_->GetPrefs())) {
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
      resuming_ = true;
      CheckBatteryState();
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

  CheckBatteryState();
}

void ArcVmDataMigrationScreen::CheckBatteryState() {
  if (!view_) {
    return;
  }

  view_->SetMinimumBatteryPercent(kMinimumBatteryPercent);

  // Request PowerManager to report the battery status updates. The UI will be
  // updated on PowerChanged().
  DCHECK(chromeos::PowerManagerClient::Get());
  DCHECK(!power_manager_observation_.IsObserving());
  power_manager_observation_.Observe(chromeos::PowerManagerClient::Get());
  chromeos::PowerManagerClient::Get()->RequestStatusUpdate();
}

void ArcVmDataMigrationScreen::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (proto.has_battery_percent()) {
    battery_percent_ = proto.battery_percent();
  } else {
    LOG(WARNING) << "No battery percent is reported. Reusing the old value: "
                 << battery_percent_;
  }

  if (proto.has_external_power()) {
    is_connected_to_charger_ =
        proto.external_power() !=
        power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED;
  } else {
    LOG(WARNING) << "No external power info is reported. Reusing the old info: "
                 << "is_connected_to_charger_ = " << is_connected_to_charger_;
  }

  if (!view_) {
    return;
  }
  view_->SetBatteryState(battery_percent_ >= kMinimumBatteryPercent,
                         is_connected_to_charger_);

  if (update_button_pressed_ ||
      current_ui_state_ != ArcVmDataMigrationScreenView::UIState::kLoading) {
    // No need to update the UI state if this is not the initial loading screen.
    return;
  }

  if (resuming_) {
    UpdateUIState(ArcVmDataMigrationScreenView::UIState::kResume);
  } else {
    UpdateUIState(ArcVmDataMigrationScreenView::UIState::kWelcome);
  }
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
  std::vector<std::string> environment = {"CHROMEOS_USER=" +
                                          GetChromeOsUsername(profile_)};
  std::deque<arc::JobDesc> jobs{arc::JobDesc{
      arc::kArcVmDataMigratorJobName, arc::UpstartOperation::JOB_STOP_AND_START,
      std::move(environment)}};
  arc::ConfigureUpstartJobs(
      std::move(jobs),
      base::BindOnce(&ArcVmDataMigrationScreen::OnArcVmDataMigratorStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcVmDataMigrationScreen::OnArcVmDataMigratorStarted(bool result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result) {
    LOG(ERROR) << "Failed to start arcvm-data-migrator";
    HandleFatalError();
    return;
  }

  DCHECK(tick_clock_);
  previous_ticks_ = tick_clock_->NowTicks();
  DCHECK(ArcVmDataMigratorClient::Get());
  DCHECK(!migration_progress_observation_.IsObserving());
  migration_progress_observation_.Observe(ArcVmDataMigratorClient::Get());
  UpdateUIState(ArcVmDataMigrationScreenView::UIState::kProgress);
  SetArcVmDataMigrationStatus(profile_->GetPrefs(),
                              arc::ArcVmDataMigrationStatus::kStarted);

  arc::data_migrator::StartMigrationRequest request;
  request.set_username(GetChromeOsUsername(profile_));
  request.set_destination_type(
      base::FeatureList::IsEnabled(arc::kLvmApplicationContainers)
          ? arc::data_migrator::LVM_DEVICE
          : arc::data_migrator::CROSVM_DISK);
  ArcVmDataMigratorClient::Get()->StartMigration(
      request,
      base::BindOnce(&ArcVmDataMigrationScreen::OnStartMigrationResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcVmDataMigrationScreen::OnStartMigrationResponse(bool result) {
  if (!result) {
    LOG(ERROR) << "Failed to start migration";
    migration_progress_observation_.Reset();
    HandleFatalError();
    return;
  }
}

void ArcVmDataMigrationScreen::OnDataMigrationProgress(
    const arc::data_migrator::DataMigrationProgress& progress) {
  switch (progress.status()) {
    case arc::data_migrator::DATA_MIGRATION_IN_PROGRESS:
      VLOG(1) << "ARCVM /data migration in progress: current_bytes="
              << progress.current_bytes()
              << ", total_bytes=" << progress.total_bytes();
      UpdateProgressBar(progress.current_bytes(), progress.total_bytes());
      return;
    case arc::data_migrator::DATA_MIGRATION_SUCCESS:
      VLOG(1) << "ARCVM /data migration finished successfully";
      migration_progress_observation_.Reset();
      SetArcVmDataMigrationStatus(profile_->GetPrefs(),
                                  arc::ArcVmDataMigrationStatus::kFinished);
      UpdateUIState(ArcVmDataMigrationScreenView::UIState::kSuccess);
      return;
    case arc::data_migrator::DATA_MIGRATION_FAILED:
      LOG(ERROR) << "ARCVM /data migration failed";
      migration_progress_observation_.Reset();
      RemoveArcDataAndShowFailureScreen();
      return;
    default:
      NOTREACHED();
      return;
  }
}

void ArcVmDataMigrationScreen::UpdateProgressBar(uint64_t current_bytes,
                                                 uint64_t total_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(tick_clock_);
  if (!current_bytes || !total_bytes) {
    // Initializing. Just update the clock.
    previous_ticks_ = tick_clock_->NowTicks();
    return;
  }
  base::TimeTicks current_ticks = tick_clock_->NowTicks();
  base::TimeDelta delta = current_ticks - previous_ticks_;
  if (delta < kMinimumIntervalLengthForProgressUpdate) {
    return;
  }
  previous_ticks_ = current_ticks;

  double current_speed =
      (current_bytes - previous_bytes_) / delta.InMillisecondsF();
  previous_bytes_ = current_bytes;
  if (average_speed_ == 0.0) {
    average_speed_ = current_speed;
  }
  average_speed_ = kAverageSpeedSmoothFactor * current_speed +
                   (1.0 - kAverageSpeedSmoothFactor) * average_speed_;
  if (average_speed_ < kAverageSpeedDropBound) {
    average_speed_ = kAverageSpeedDropBound;
  }

  double estimated_remaining_time_in_millis =
      (total_bytes - current_bytes) / average_speed_;

  if (!view_) {
    return;
  }
  view_->SetMigrationProgress(100.0 * current_bytes / total_bytes);
  view_->SetEstimatedRemainingTime(base::Milliseconds(static_cast<int64_t>(
      std::round(std::min(estimated_remaining_time_in_millis,
                          kMaximumEstimatedRemainingTime.InMillisecondsF())))));
}

void ArcVmDataMigrationScreen::RemoveArcDataAndShowFailureScreen() {
  std::vector<std::string> environment = {"CHROMEOS_USER=" +
                                          GetChromeOsUsername(profile_)};
  std::deque<arc::JobDesc> jobs{
      arc::JobDesc{
          arc::kArcVmDataMigratorJobName, arc::UpstartOperation::JOB_STOP, {}},
      arc::JobDesc{kArcRemoveDataJobName,
                   arc::UpstartOperation::JOB_STOP_AND_START,
                   std::move(environment)},
  };
  arc::ConfigureUpstartJobs(
      std::move(jobs),
      base::BindOnce(&ArcVmDataMigrationScreen::OnArcDataRemoved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcVmDataMigrationScreen::OnArcDataRemoved(bool success) {
  DCHECK(profile_);
  if (!success) {
    LOG(ERROR) << "Failed to remove /data. Requesting removal in next session";
    // Set |kArcDataRemoveRequested| so that ArcSessionManager tries to remove
    // /data before starting the next session. The reason why we do not just
    // rely on this pref is to increase the chance of successfully removing
    // /data; the pref is effective only once and invalidated even when /data
    // could not be removed.
    profile_->GetPrefs()->SetBoolean(arc::prefs::kArcDataRemoveRequested, true);
  }
  SetArcVmDataMigrationStatus(profile_->GetPrefs(),
                              arc::ArcVmDataMigrationStatus::kFinished);
  UpdateUIState(ArcVmDataMigrationScreenView::UIState::kFailure);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (update_button_pressed_) {
    return;
  }
  update_button_pressed_ = true;
  UpdateUIState(ArcVmDataMigrationScreenView::UIState::kLoading);
  if (resuming_) {
    TriggerMigration();
  } else {
    SetUpDestinationAndTriggerMigration();
  }
}

void ArcVmDataMigrationScreen::HandleResume() {
  DCHECK(resuming_);
  HandleUpdate();
}

void ArcVmDataMigrationScreen::HandleFinish() {
  chrome::AttemptRelaunch();
}

void ArcVmDataMigrationScreen::HandleReport() {
  const int64_t unique_identifier =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
  const std::string description_template =
      base::StringPrintf("Report tag: #arcvm-data-migration (%s)",
                         base::NumberToString(unique_identifier).c_str());
  LoginFeedback login_feedback(profile_);
  login_feedback.Request(description_template);
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
