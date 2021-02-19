// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_installer_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/borealis/borealis_context_manager.h"
#include "chrome/browser/chromeos/borealis/borealis_features.h"
#include "chrome/browser/chromeos/borealis/borealis_metrics.h"
#include "chrome/browser/chromeos/borealis/borealis_prefs.h"
#include "chrome/browser/chromeos/borealis/borealis_service.h"
#include "chrome/browser/chromeos/borealis/borealis_util.h"
#include "chrome/browser/chromeos/borealis/infra/transition.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "third_party/cros_system_api/dbus/dlcservice/dbus-constants.h"

namespace borealis {

class Uninstallation : public Transition<BorealisInstallerImpl::UninstallInfo,
                                         BorealisInstallerImpl::UninstallInfo,
                                         BorealisUninstallResult> {
 public:
  explicit Uninstallation(Profile* profile)
      : profile_(profile), weak_factory_(this) {}

  void Start(std::unique_ptr<BorealisInstallerImpl::UninstallInfo>
                 start_instance) override {
    uninstall_info_ = std::move(start_instance);
    BorealisService::GetForProfile(profile_)->ContextManager().ShutDownBorealis(
        base::BindOnce(&Uninstallation::OnShutdownCompleted,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void OnShutdownCompleted(BorealisShutdownResult result) {
    if (result != BorealisShutdownResult::kSuccess) {
      LOG(ERROR) << "Failed to shut down before uninstall (code="
                 << static_cast<int>(result) << ")";
      Fail(BorealisUninstallResult::kShutdownFailed);
      return;
    }
    // Clear the borealis apps.
    guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_)
        ->ClearApplicationList(vm_tools::apps::ApplicationList_VmType_BOREALIS,
                               uninstall_info_->vm_name,
                               uninstall_info_->container_name);

    vm_tools::concierge::DestroyDiskImageRequest request;
    request.set_cryptohome_id(
        chromeos::ProfileHelper::GetUserIdHashFromProfile(profile_));
    request.set_disk_path(uninstall_info_->vm_name);
    chromeos::DBusThreadManager::Get()->GetConciergeClient()->DestroyDiskImage(
        std::move(request), base::BindOnce(&Uninstallation::OnDiskRemoved,
                                           weak_factory_.GetWeakPtr()));
  }

  void OnDiskRemoved(
      base::Optional<vm_tools::concierge::DestroyDiskImageResponse> response) {
    if (!response) {
      LOG(ERROR) << "Failed to destroy disk image. Empty response.";
      Fail(BorealisUninstallResult::kRemoveDiskFailed);
      return;
    }
    if (response->status() != vm_tools::concierge::DISK_STATUS_DESTROYED &&
        response->status() != vm_tools::concierge::DISK_STATUS_DOES_NOT_EXIST) {
      LOG(ERROR) << "Failed to destroy disk image: "
                 << response->failure_reason();
      Fail(BorealisUninstallResult::kRemoveDiskFailed);
      return;
    }

    chromeos::DlcserviceClient::Get()->Uninstall(
        kBorealisDlcName, base::BindOnce(&Uninstallation::OnDlcUninstalled,
                                         weak_factory_.GetWeakPtr()));
  }

  void OnDlcUninstalled(const std::string& dlc_err) {
    if (dlc_err.empty()) {
      LOG(ERROR) << "Failed to remove DLC: no response.";
      Fail(BorealisUninstallResult::kRemoveDlcFailed);
      return;
    }
    if (dlc_err != dlcservice::kErrorNone) {
      LOG(ERROR) << "Failed to remove DLC: " << dlc_err;
      Fail(BorealisUninstallResult::kRemoveDlcFailed);
      return;
    }

    // Remove the pref last. This way we are still considered "installed" if we
    // fail to uninstall.  The practical effect is that every step is
    // recoverable from, so as far as the user is concerned things will go back
    // to the normal installed state.
    profile_->GetPrefs()->SetBoolean(prefs::kBorealisInstalledOnDevice, false);

    Succeed(std::move(uninstall_info_));
  }

  Profile* const profile_;
  std::unique_ptr<BorealisInstallerImpl::UninstallInfo> uninstall_info_;
  base::WeakPtrFactory<Uninstallation> weak_factory_;
};

BorealisInstallerImpl::BorealisInstallerImpl(Profile* profile)
    : state_(State::kIdle),
      installing_state_(InstallingState::kInactive),
      profile_(profile),
      weak_ptr_factory_(this) {}

BorealisInstallerImpl::~BorealisInstallerImpl() = default;

bool BorealisInstallerImpl::IsProcessing() {
  return state_ != State::kIdle;
}

void BorealisInstallerImpl::Start() {
  RecordBorealisInstallNumAttemptsHistogram();
  if (!BorealisService::GetForProfile(profile_)->Features().IsAllowed()) {
    LOG(ERROR) << "Installation of Borealis cannot be started because "
               << "Borealis is not allowed.";
    InstallationEnded(BorealisInstallResult::kBorealisNotAllowed);
    return;
  }

  if (IsProcessing()) {
    LOG(ERROR) << "Installation of Borealis is already in progress.";
    InstallationEnded(BorealisInstallResult::kBorealisInstallInProgress);
    return;
  }

  if (content::GetNetworkConnectionTracker()->IsOffline()) {
    InstallationEnded(BorealisInstallResult::kOffline);
    return;
  }

  installation_start_tick_ = base::TimeTicks::Now();

  progress_ = 0;
  StartDlcInstallation();
}

void BorealisInstallerImpl::Cancel() {
  if (state_ != State::kIdle) {
    state_ = State::kCancelling;
  }
  for (auto& observer : observers_) {
    observer.OnCancelInitiated();
  }
}

void BorealisInstallerImpl::Uninstall(
    base::OnceCallback<void(BorealisUninstallResult)> on_uninstall_callback) {
  if (in_progress_uninstallation_) {
    std::move(on_uninstall_callback)
        .Run(BorealisUninstallResult::kAlreadyInProgress);
    return;
  }
  RecordBorealisUninstallNumAttemptsHistogram();
  // TODO(b/179303903): The installer should own the relevant bits so the VM and
  // container names are kept by it (and not context_manager).
  auto uninstall_info = std::make_unique<UninstallInfo>();
  uninstall_info->vm_name = "borealis";
  uninstall_info->container_name = "penguin";
  uninstall_info->start_time = base::Time::Now();
  in_progress_uninstallation_ = std::make_unique<Uninstallation>(profile_);
  in_progress_uninstallation_->Begin(
      std::move(uninstall_info),
      base::BindOnce(&BorealisInstallerImpl::OnUninstallComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_uninstall_callback)));
}

void BorealisInstallerImpl::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void BorealisInstallerImpl::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void BorealisInstallerImpl::StartDlcInstallation() {
  state_ = State::kInstalling;
  UpdateInstallingState(InstallingState::kInstallingDlc);

  chromeos::DlcserviceClient::Get()->Install(
      kBorealisDlcName,
      base::BindOnce(&BorealisInstallerImpl::OnDlcInstallationCompleted,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          &BorealisInstallerImpl::OnDlcInstallationProgressUpdated,
          weak_ptr_factory_.GetWeakPtr()));
}

void BorealisInstallerImpl::InstallationEnded(BorealisInstallResult result) {
  // If another installation is in progress, we don't want to reset any states
  // and interfere with the process. When that process completes, it will reset
  // these states.
  if (result != BorealisInstallResult::kBorealisInstallInProgress) {
    state_ = State::kIdle;
    installing_state_ = InstallingState::kInactive;
  }
  if (result == BorealisInstallResult::kSuccess) {
    profile_->GetPrefs()->SetBoolean(prefs::kBorealisInstalledOnDevice, true);
    RecordBorealisInstallOverallTimeHistogram(base::TimeTicks::Now() -
                                              installation_start_tick_);
  }
  RecordBorealisInstallResultHistogram(result);
  for (auto& observer : observers_) {
    observer.OnInstallationEnded(result);
  }
}

void BorealisInstallerImpl::UpdateProgress(double state_progress) {
  DCHECK_EQ(state_, State::kInstalling);
  if (state_progress < 0 || state_progress > 1) {
    LOG(ERROR) << "Unexpected progress value " << state_progress
               << " in installing state "
               << GetInstallingStateName(installing_state_);
    return;
  }

  double start_range = 0;
  double end_range = 0;
  switch (installing_state_) {
    case InstallingState::kInstallingDlc:
      start_range = 0;
      end_range = 1;
      break;
    default:
      NOTREACHED();
  }

  double new_progress =
      start_range + (end_range - start_range) * state_progress;
  if (new_progress < progress_) {
    LOG(ERROR) << "Progress went backwards from " << progress_ << " to "
               << progress_;
    return;
  }

  progress_ = new_progress;
  for (auto& observer : observers_) {
    observer.OnProgressUpdated(new_progress);
  }
}

void BorealisInstallerImpl::UpdateInstallingState(
    InstallingState installing_state) {
  DCHECK_NE(installing_state, InstallingState::kInactive);
  installing_state_ = installing_state;
  for (auto& observer : observers_) {
    observer.OnStateUpdated(installing_state_);
  }
}

void BorealisInstallerImpl::OnDlcInstallationProgressUpdated(double progress) {
  DCHECK_EQ(installing_state_, InstallingState::kInstallingDlc);
  if (state_ == State::kCancelling)
    return;

  UpdateProgress(progress);
}

void BorealisInstallerImpl::OnDlcInstallationCompleted(
    const chromeos::DlcserviceClient::InstallResult& install_result) {
  DCHECK_EQ(installing_state_, InstallingState::kInstallingDlc);
  if (state_ == State::kCancelling) {
    InstallationEnded(BorealisInstallResult::kCancelled);
    return;
  }

  // If success, continue to the next state.
  if (install_result.error == dlcservice::kErrorNone) {
    InstallationEnded(BorealisInstallResult::kSuccess);
    return;
  }

  // At this point, the Borealis DLC installation has failed.
  BorealisInstallResult result = BorealisInstallResult::kDlcUnknownError;

  // TODO(b/172284265): Handle the case where a device update is required before
  // a DLC can be installed.
  if (install_result.error == dlcservice::kErrorInternal) {
    LOG(ERROR) << "Something went wrong internally with DlcService.";
    result = BorealisInstallResult::kDlcInternalError;
  } else if (install_result.error == dlcservice::kErrorInvalidDlc) {
    LOG(ERROR) << "Borealis DLC is not supported, need to enable Borealis DLC.";
    result = BorealisInstallResult::kDlcUnsupportedError;
  } else if (install_result.error == dlcservice::kErrorBusy) {
    LOG(ERROR)
        << "Borealis DLC is not able to be installed as dlcservice is busy.";
    result = BorealisInstallResult::kDlcBusyError;
  } else if (install_result.error == dlcservice::kErrorNeedReboot) {
    LOG(ERROR)
        << "Device has pending update and needs a reboot to use Borealis DLC.";
    result = BorealisInstallResult::kDlcNeedRebootError;
  } else if (install_result.error == dlcservice::kErrorAllocation) {
    LOG(ERROR) << "Device needs to free space to use Borealis DLC.";
    result = BorealisInstallResult::kDlcNeedSpaceError;
  } else {
    LOG(ERROR) << "Failed to install Borealis DLC: " << install_result.error;
  }

  InstallationEnded(result);
}

void BorealisInstallerImpl::OnUninstallComplete(
    base::OnceCallback<void(BorealisUninstallResult)> on_uninstall_callback,
    Expected<std::unique_ptr<UninstallInfo>, BorealisUninstallResult> result) {
  in_progress_uninstallation_.reset();
  BorealisUninstallResult uninstall_result = BorealisUninstallResult::kSuccess;
  if (!result) {
    uninstall_result = result.Error();
  }
  RecordBorealisUninstallResultHistogram(uninstall_result);
  std::move(on_uninstall_callback).Run(uninstall_result);
}

}  // namespace borealis
