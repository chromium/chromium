// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_installer.h"

#include <algorithm>
#include <string>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_service_factory.h"
#include "chrome/browser/ash/crostini/crostini_disk.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_types.mojom.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/crostini_installer/crostini_installer_dialog.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "crostini_util.h"
#include "ui/display/types/display_constants.h"

using crostini::mojom::InstallerError;
using crostini::mojom::InstallerState;

namespace crostini {

namespace {
using SetupResult = CrostiniInstaller::SetupResult;
constexpr char kCrostiniSetupSourceHistogram[] = "Crostini.SetupSource";

constexpr int kUninitializedDiskSpace = -1;

constexpr char kCrostiniSetupResultHistogram[] = "Crostini.SetupResult";
constexpr char kCrostiniTimeFromDeviceSetupToInstall[] =
    "Crostini.TimeFromDeviceSetupToInstall";
constexpr char kCrostiniTimeToInstallSuccess[] =
    "Crostini.TimeToInstallSuccess";
constexpr char kCrostiniTimeToInstallCancel[] = "Crostini.TimeToInstallCancel";
constexpr char kCrostiniTimeToInstallError[] = "Crostini.TimeToInstallError";
constexpr char kCrostiniAvailableDiskSuccess[] =
    "Crostini.AvailableDiskSuccess";
constexpr char kCrostiniAvailableDiskCancel[] = "Crostini.AvailableDiskCancel";
constexpr char kCrostiniAvailableDiskError[] = "Crostini.AvailableDiskError";

void RecordTimeFromDeviceSetupToInstallMetric() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ash::StartupUtils::GetTimeSinceOobeFlagFileCreation),
      base::BindOnce([](base::TimeDelta time_from_device_setup) {
        if (time_from_device_setup.is_zero()) {
          return;
        }

        // The magic number 1471228928 is used for legacy reasons and changing
        // it would invalidate already logged data.
        base::UmaHistogramCustomTimes(kCrostiniTimeFromDeviceSetupToInstall,
                                      time_from_device_setup, base::Minutes(1),
                                      base::Milliseconds(1471228928), 50);
      }));
}

SetupResult ErrorToSetupResult(InstallerError error) {
  switch (error) {
    case InstallerError::kNone:
      return SetupResult::kSuccess;
    case InstallerError::kErrorLoadingTermina:
      return SetupResult::kErrorLoadingTermina;
    case InstallerError::kNeedUpdate:
      return SetupResult::kNeedUpdate;
    case InstallerError::kErrorCreatingDiskImage:
      return SetupResult::kErrorCreatingDiskImage;
    case InstallerError::kErrorStartingTermina:
      return SetupResult::kErrorStartingTermina;
    case InstallerError::kErrorStartingLxd:
      return SetupResult::kErrorStartingLxd;
    case InstallerError::kErrorStartingContainer:
      return SetupResult::kErrorStartingContainer;
    case InstallerError::kErrorConfiguringContainer:
      return SetupResult::kErrorConfiguringContainer;
    case InstallerError::kErrorOffline:
      return SetupResult::kErrorOffline;
    case InstallerError::kErrorSettingUpContainer:
      return SetupResult::kErrorSettingUpContainer;
    case InstallerError::kErrorInsufficientDiskSpace:
      return SetupResult::kErrorInsufficientDiskSpace;
    case InstallerError::kErrorCreateContainer:
      return SetupResult::kErrorCreateContainer;
    case InstallerError::kErrorUnknown:
      return SetupResult::kErrorUnknown;
  }

  NOTREACHED_IN_MIGRATION();
}

SetupResult InstallStateToCancelledSetupResult(
    InstallerState installing_state) {
  switch (installing_state) {
    case InstallerState::kStart:
      return SetupResult::kUserCancelledStart;
    case InstallerState::kInstallImageLoader:
      return SetupResult::kUserCancelledInstallImageLoader;
    case InstallerState::kCreateDiskImage:
      return SetupResult::kUserCancelledCreateDiskImage;
    case InstallerState::kStartTerminaVm:
      return SetupResult::kUserCancelledStartTerminaVm;
    case InstallerState::kStartLxd:
      return SetupResult::kUserCancelledStartLxd;
    case InstallerState::kCreateContainer:
      return SetupResult::kUserCancelledCreateContainer;
    case InstallerState::kSetupContainer:
      return SetupResult::kUserCancelledSetupContainer;
    case InstallerState::kStartContainer:
      return SetupResult::kUserCancelledStartContainer;
    case InstallerState::kConfigureContainer:
      return SetupResult::kUserCancelledConfiguringContainer;
  }

  NOTREACHED_IN_MIGRATION();
}

crostini::mojom::InstallerError CrostiniResultToInstallerError(
    crostini::CrostiniResult result,
    InstallerState installer_state) {
  DCHECK_NE(result, CrostiniResult::SUCCESS);

  bool offline = content::GetNetworkConnectionTracker()->IsOffline();
  if (offline) {
    LOG(WARNING)
        << "Crostini installation may have failed due to being offline.";
  }

  switch (installer_state) {
    default:
    case InstallerState::kStart:
      NOTREACHED_IN_MIGRATION();
      return InstallerError::kErrorUnknown;
    case InstallerState::kInstallImageLoader:
      if (offline) {
        return InstallerError::kErrorOffline;
      } else if (result == CrostiniResult::NEED_UPDATE) {
        return InstallerError::kNeedUpdate;
      } else {
        return InstallerError::kErrorLoadingTermina;
      }
    case InstallerState::kCreateDiskImage:
      return InstallerError::kErrorCreatingDiskImage;
    case InstallerState::kStartTerminaVm:
      return InstallerError::kErrorStartingTermina;
    case InstallerState::kStartLxd:
      return InstallerError::kErrorStartingLxd;
    case InstallerState::kCreateContainer:
      if (offline) {
        return InstallerError::kErrorOffline;
      } else {
        return InstallerError::kErrorCreateContainer;
      }
    case InstallerState::kSetupContainer:
      if (offline) {
        return InstallerError::kErrorOffline;
      } else {
        return InstallerError::kErrorSettingUpContainer;
      }
    case InstallerState::kStartContainer:
      return InstallerError::kErrorStartingContainer;
    case InstallerState::kConfigureContainer:
      return InstallerError::kErrorConfiguringContainer;
  }
}

}  // namespace

CrostiniInstaller::CrostiniInstaller(Profile* profile) : profile_(profile) {}

CrostiniInstaller::~CrostiniInstaller() {
  // Guaranteed by |Shutdown()|.
  DCHECK_EQ(restart_id_, CrostiniManager::kUninitializedRestartId);
}

void CrostiniInstaller::Shutdown() {
  if (restart_id_ != CrostiniManager::kUninitializedRestartId) {
    CrostiniManager::GetForProfile(profile_)->CancelRestartCrostini(
        restart_id_);
    restart_id_ = CrostiniManager::kUninitializedRestartId;
  }
}

void CrostiniInstaller::ShowDialog(CrostiniUISurface ui_surface) {
  // Defensive check to prevent showing the installer when crostini is not
  // allowed.
  if (!CrostiniFeatures::Get()->IsAllowedNow(profile_)) {
    return;
  }
  base::UmaHistogramEnumeration(kCrostiniSetupSourceHistogram, ui_surface,
                                crostini::CrostiniUISurface::kCount);

  // TODO(lxj): We should pass the dialog |this| here instead of letting the
  // webui to call |GetForProfile()| later.
  ash::CrostiniInstallerDialog::Show(profile_);
}

void CrostiniInstaller::Install(CrostiniManager::RestartOptions options,
                                ProgressCallback progress_callback,
                                ResultCallback result_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!CanInstall()) {
    LOG(ERROR)
        << "Tried to start crostini installation in invalid state. state_="
        << static_cast<int>(state_);
    return;
  }

  restart_options_ = std::move(options);
  restart_options_.restart_source = RestartSource::kInstaller;
  progress_callback_ = std::move(progress_callback);
  result_callback_ = std::move(result_callback);

  // Check if there's additional setup required in the case of enterprise
  // specifying an Ansible playbook to be run for a pre-determined configuration
  // on the container.
  if (ShouldConfigureDefaultContainer(profile_)) {
    restart_options_.ansible_playbook = profile_->GetPrefs()->GetFilePath(
        prefs::kCrostiniAnsiblePlaybookFilePath);
  }

  install_start_time_ = base::TimeTicks::Now();
  require_cleanup_ = true;
  free_disk_space_ = kUninitializedDiskSpace;
  container_download_percent_ = 0;
  UpdateState(State::INSTALLING);

  // The spaced D-Bus client needs to be called from the ui thread
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ash::SpacedClient::GetFreeDiskSpace,
                     base::Unretained(ash::SpacedClient::Get()),
                     crostini::kHomeDirectory,
                     base::BindOnce(&CrostiniInstaller::OnAvailableDiskSpace,
                                    weak_ptr_factory_.GetWeakPtr())));

  // Reset mic permissions, we don't want it to persist across
  // re-installation.
  profile_->GetPrefs()->SetBoolean(prefs::kCrostiniMicAllowed, false);
}

void CrostiniInstaller::Cancel(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (state_ != State::INSTALLING) {
    LOG(ERROR) << "Tried to cancel in non-cancelable state. state_="
               << static_cast<int>(state_);
    return;
  }

  UMA_HISTOGRAM_LONG_TIMES(kCrostiniTimeToInstallCancel,
                           base::TimeTicks::Now() - install_start_time_);

  result_callback_.Reset();
  progress_callback_.Reset();
  cancel_callback_ = std::move(callback);

  if (installing_state_ == InstallerState::kStart) {
    // We have not called |RestartCrostini()| yet.
    DCHECK_EQ(restart_id_, CrostiniManager::kUninitializedRestartId);
    // OnAvailableDiskSpace() will take care of |cancel_callback_|.
    UpdateState(State::CANCEL_ABORT_CHECK_DISK);
    return;
  }

  DCHECK_NE(restart_id_, CrostiniManager::kUninitializedRestartId);

  if (free_disk_space_ != kUninitializedDiskSpace) {
    base::UmaHistogramCounts1M(kCrostiniAvailableDiskCancel,
                               free_disk_space_ >> 20);
  }

  // Abort the long-running flow, and RestartObserver methods will not be called
  // again until next installation.
  auto* crostini_manager = crostini::CrostiniManager::GetForProfile(profile_);
  crostini_manager->CancelRestartCrostini(restart_id_);
  restart_id_ = CrostiniManager::kUninitializedRestartId;
  RecordSetupResult(InstallStateToCancelledSetupResult(installing_state_));

  if (require_cleanup_) {
    // Remove anything that got installed
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&crostini::CrostiniManager::RemoveCrostini,
                       crostini_manager->GetWeakPtr(),
                       crostini::kCrostiniDefaultVmName,
                       base::BindOnce(&CrostiniInstaller::FinishCleanup,
                                      weak_ptr_factory_.GetWeakPtr())));
    UpdateState(State::CANCEL_CLEANUP);
  } else {
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                 std::move(cancel_callback_));
    UpdateState(State::IDLE);
  }
}

void CrostiniInstaller::CancelBeforeStart() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!CanInstall()) {
    LOG(ERROR) << "Not in pre-install state. state_="
               << static_cast<int>(state_);
    return;
  }
  RecordSetupResult(SetupResult::kNotStarted);
}

void CrostiniInstaller::OnStageStarted(InstallerState stage) {
  if (stage == InstallerState::kStart ||
      stage == InstallerState::kInstallImageLoader) {
    // Drop these as we manually set our internal state to kInstallImageLoader
    // upon starting the restart.
    return;
  }

  UpdateInstallingState(stage);
}

void CrostiniInstaller::OnDiskImageCreated(bool success,
                                           CrostiniResult result,
                                           int64_t disk_size_available) {
  if (result == CrostiniResult::CREATE_DISK_IMAGE_ALREADY_EXISTS) {
    require_cleanup_ = false;
  }
}

void CrostiniInstaller::OnContainerDownloading(int32_t download_percent) {
  container_download_percent_ = std::clamp(download_percent, 0, 100);
  RunProgressCallback();
}

bool CrostiniInstaller::CanInstall() {
  // Allow to start from State::ERROR. In that case, we're doing a Retry.
  return state_ == State::IDLE || state_ == State::ERROR;
}

void CrostiniInstaller::RunProgressCallback() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::INSTALLING);

  base::TimeDelta time_in_state =
      base::Time::Now() - installing_state_start_time_;

  double state_start_mark = 0;
  double state_end_mark = 0;
  auto state_max_time = base::Seconds(1);

  switch (installing_state_) {
    case InstallerState::kStart:
      state_start_mark = 0;
      state_end_mark = 0;
      break;
    case InstallerState::kInstallImageLoader:
      state_start_mark = 0.0;
      state_end_mark = 0.20;
      state_max_time = base::Seconds(30);
      break;
    case InstallerState::kCreateDiskImage:
      state_start_mark = 0.20;
      state_end_mark = 0.22;
      break;
    case InstallerState::kStartTerminaVm:
      state_start_mark = 0.22;
      state_end_mark = 0.28;
      state_max_time = base::Seconds(8);
      break;
    case InstallerState::kStartLxd:
      state_start_mark = 0.28;
      state_end_mark = 0.30;
      state_max_time = base::Seconds(2);
      break;
    case InstallerState::kCreateContainer:
      state_start_mark = 0.30;
      state_end_mark = 0.72;
      state_max_time = base::Seconds(180);
      break;
    case InstallerState::kSetupContainer:
      state_start_mark = 0.72;
      state_end_mark = 0.76;
      state_max_time = base::Seconds(8);
      break;
    case InstallerState::kStartContainer:
      state_start_mark = 0.76;
      state_end_mark = 0.79;
      state_max_time = base::Seconds(8);
      break;
    case InstallerState::kConfigureContainer:
      state_start_mark = 0.79;
      state_end_mark = 1;
      // Ansible installation and playbook application.
      state_max_time = base::Seconds(140 + 300);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  double state_fraction = time_in_state / state_max_time;

  if (installing_state_ == InstallerState::kCreateContainer) {
    // In CREATE_CONTAINER, consume half the progress bar with downloading,
    // the rest with time.
    state_fraction =
        0.5 * (state_fraction + 0.01 * container_download_percent_);
  }
  // TODO(crbug.com/40645509): Calculate configure container step
  // progress based on real progress.

  double progress = state_start_mark + std::clamp(state_fraction, 0.0, 1.0) *
                                           (state_end_mark - state_start_mark);
  progress_callback_.Run(installing_state_, progress);
}

void CrostiniInstaller::UpdateState(State new_state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(state_, new_state);

  state_ = new_state;
  if (state_ == State::INSTALLING) {
    // We are not calling the progress callback here because 1) there is nothing
    // interesting to report; 2) We reach here from |Install()|, so calling the
    // callback risk reentering |Install()|'s caller.
    UpdateInstallingState(InstallerState::kStart,
                          /*run_callback=*/false);

    state_progress_timer_.Start(
        FROM_HERE, base::Milliseconds(500),
        base::BindRepeating(&CrostiniInstaller::RunProgressCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  } else {
    state_progress_timer_.AbandonAndStop();
  }
}

void CrostiniInstaller::UpdateInstallingState(
    InstallerState new_installing_state,
    bool run_callback) {
  DCHECK_EQ(state_, State::INSTALLING);
  installing_state_start_time_ = base::Time::Now();
  installing_state_ = new_installing_state;

  if (run_callback) {
    RunProgressCallback();
  }
}

void CrostiniInstaller::HandleError(InstallerError error) {
  DCHECK_EQ(state_, State::INSTALLING);
  DCHECK_NE(error, InstallerError::kNone);

  UMA_HISTOGRAM_LONG_TIMES(kCrostiniTimeToInstallError,
                           base::TimeTicks::Now() - install_start_time_);
  if (free_disk_space_ != kUninitializedDiskSpace) {
    base::UmaHistogramCounts1M(kCrostiniAvailableDiskError,
                               free_disk_space_ >> 20);
  }

  RecordSetupResult(ErrorToSetupResult(error));

  // |restart_id_| is reset in |OnCrostiniRestartFinished()|.
  UpdateState(State::ERROR);

  std::move(result_callback_).Run(error);
  progress_callback_.Reset();
}

void CrostiniInstaller::FinishCleanup(crostini::CrostiniResult result) {
  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Failed to cleanup aborted crostini install";
  }
  UpdateState(State::IDLE);
  std::move(cancel_callback_).Run();
}

void CrostiniInstaller::RecordSetupResult(SetupResult result) {
  base::UmaHistogramEnumeration(kCrostiniSetupResultHistogram, result);
}

void CrostiniInstaller::OnCrostiniRestartFinished(CrostiniResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  restart_id_ = CrostiniManager::kUninitializedRestartId;

  if (result == CrostiniResult::RESTART_ABORTED ||
      result == CrostiniResult::RESTART_REQUEST_CANCELLED) {
    return;
  }

  if (result != CrostiniResult::SUCCESS) {
    DCHECK_EQ(state_, State::INSTALLING);
    HandleError(CrostiniResultToInstallerError(result, installing_state_));
    return;
  }

  // Reset state to allow |Install()| again in case the user remove and
  // re-install crostini.
  UpdateState(State::IDLE);

  RecordSetupResult(SetupResult::kSuccess);
  crostini::CrostiniManager::GetForProfile(profile_)
      ->UpdateLaunchMetricsForEnterpriseReporting();
  RecordTimeFromDeviceSetupToInstallMetric();
  UMA_HISTOGRAM_LONG_TIMES(kCrostiniTimeToInstallSuccess,
                           base::TimeTicks::Now() - install_start_time_);
  if (free_disk_space_ != kUninitializedDiskSpace) {
    base::UmaHistogramCounts1M(kCrostiniAvailableDiskSuccess,
                               free_disk_space_ >> 20);
  }

  std::move(result_callback_).Run(InstallerError::kNone);
  progress_callback_.Reset();

  if (!skip_launching_terminal_for_testing_) {
    // kInvalidDisplayId will launch terminal on the current active display.
    guest_os::LaunchTerminal(profile_, display::kInvalidDisplayId,
                             crostini::DefaultContainerId());
  }
}

void CrostiniInstaller::OnAvailableDiskSpace(std::optional<int64_t> bytes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // |Cancel()| might be called immediately after |Install()|.
  if (state_ == State::CANCEL_ABORT_CHECK_DISK) {
    UpdateState(State::IDLE);
    RecordSetupResult(SetupResult::kNotStarted);
    std::move(cancel_callback_).Run();
    return;
  }

  DCHECK_EQ(installing_state_, InstallerState::kStart);

  if (bytes.has_value()) {
    free_disk_space_ = bytes.value();
  }
  // Don't enforce minimum disk size on dev box or trybots because
  // base::SysInfo::AmountOfFreeDiskSpace returns zero in testing.
  if (base::SysInfo::IsRunningOnChromeOS() &&
      free_disk_space_ < restart_options_.disk_size_bytes.value_or(
                             crostini::disk::kDiskHeadroomBytes +
                             crostini::disk::kMinimumDiskSizeBytes)) {
    HandleError(InstallerError::kErrorInsufficientDiskSpace);
    return;
  }

  if (content::GetNetworkConnectionTracker()->IsOffline()) {
    HandleError(InstallerError::kErrorOffline);
    return;
  }

  UpdateInstallingState(InstallerState::kInstallImageLoader);

  // Kick off the Crostini Restart sequence. We will be added as an observer.
  restart_id_ =
      crostini::CrostiniManager::GetForProfile(profile_)
          ->RestartCrostiniWithOptions(
              crostini::DefaultContainerId(), std::move(restart_options_),
              base::BindOnce(&CrostiniInstaller::OnCrostiniRestartFinished,
                             weak_ptr_factory_.GetWeakPtr()),
              this);

  // |restart_id| will be invalid when |CrostiniManager::RestartCrostini()|
  // decides to fail immediately and calls |OnCrostiniRestartFinished()|, which
  // subsequently set |state_| to |ERROR|.
  DCHECK_EQ(restart_id_ == CrostiniManager::kUninitializedRestartId,
            state_ == State::ERROR);
}

}  // namespace crostini
