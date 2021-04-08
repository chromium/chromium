// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_installer_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/infra/transition.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
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

class BorealisInstallerImpl::Installation
    : public Transition<BorealisInstallerImpl::InstallInfo,
                        BorealisInstallerImpl::InstallInfo,
                        BorealisInstallResult> {
 public:
  explicit Installation(
      base::RepeatingCallback<void(double)> update_progress_callback,
      base::RepeatingCallback<void(InstallingState)> update_state_callback)
      : installation_start_tick_(base::TimeTicks::Now()),
        update_progress_callback_(std::move(update_progress_callback)),
        update_state_callback_(std::move(update_state_callback)),
        weak_factory_(this) {}

  base::TimeTicks start_time() { return installation_start_tick_; }

  void Cancel() { Fail(BorealisInstallResult::kCancelled); }

 private:
  void Start(std::unique_ptr<BorealisInstallerImpl::InstallInfo> start_instance)
      override {
    install_info_ = std::move(start_instance);

    SetState(InstallingState::kInstallingDlc);
    chromeos::DlcserviceClient::Get()->Install(
        kBorealisDlcName,
        base::BindOnce(&Installation::OnDlcInstallationCompleted,
                       weak_factory_.GetWeakPtr()),
        base::BindRepeating(&Installation::OnDlcInstallationProgressUpdated,
                            weak_factory_.GetWeakPtr()));
  }

  void SetState(InstallingState state) {
    update_state_callback_.Run(state);
    installing_state_ = state;
  }

  void OnDlcInstallationProgressUpdated(double progress) {
    DCHECK_EQ(installing_state_, InstallingState::kInstallingDlc);
    update_progress_callback_.Run(progress);
  }

  void OnDlcInstallationCompleted(
      const chromeos::DlcserviceClient::InstallResult& install_result) {
    DCHECK_EQ(installing_state_, InstallingState::kInstallingDlc);

    // If success, continue to the next state.
    if (install_result.error == dlcservice::kErrorNone) {
      Succeed(std::move(install_info_));
      return;
    }

    // At this point, the Borealis DLC installation has failed.
    BorealisInstallResult result = BorealisInstallResult::kDlcUnknownError;

    if (install_result.error == dlcservice::kErrorInternal) {
      LOG(ERROR) << "Something went wrong internally with DlcService.";
      result = BorealisInstallResult::kDlcInternalError;
    } else if (install_result.error == dlcservice::kErrorInvalidDlc) {
      LOG(ERROR)
          << "Borealis DLC is not supported, need to enable Borealis DLC.";
      result = BorealisInstallResult::kDlcUnsupportedError;
    } else if (install_result.error == dlcservice::kErrorBusy) {
      LOG(ERROR)
          << "Borealis DLC is not able to be installed as dlcservice is busy.";
      result = BorealisInstallResult::kDlcBusyError;
    } else if (install_result.error == dlcservice::kErrorNeedReboot) {
      LOG(ERROR) << "Device has pending update and needs a reboot to use "
                    "Borealis DLC.";
      result = BorealisInstallResult::kDlcNeedRebootError;
    } else if (install_result.error == dlcservice::kErrorAllocation) {
      LOG(ERROR) << "Device needs to free space to use Borealis DLC.";
      result = BorealisInstallResult::kDlcNeedSpaceError;
    } else if (install_result.error == dlcservice::kErrorNoImageFound) {
      LOG(ERROR)
          << "Omaha could not provide an image, device may need to be updated.";
      result = BorealisInstallResult::kDlcNeedUpdateError;
    } else {
      LOG(ERROR) << "Failed to install Borealis DLC: " << install_result.error;
    }

    Fail(result);
  }

  base::TimeTicks installation_start_tick_;
  InstallingState installing_state_;
  base::RepeatingCallback<void(double)> update_progress_callback_;
  base::RepeatingCallback<void(InstallingState)> update_state_callback_;
  std::unique_ptr<BorealisInstallerImpl::InstallInfo> install_info_;
  base::WeakPtrFactory<Installation> weak_factory_;
};

class BorealisInstallerImpl::Uninstallation
    : public Transition<BorealisInstallerImpl::InstallInfo,
                        BorealisInstallerImpl::InstallInfo,
                        BorealisUninstallResult> {
 public:
  explicit Uninstallation(Profile* profile)
      : profile_(profile), weak_factory_(this) {}

  void Start(std::unique_ptr<BorealisInstallerImpl::InstallInfo> start_instance)
      override {
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
  std::unique_ptr<BorealisInstallerImpl::InstallInfo> uninstall_info_;
  base::WeakPtrFactory<Uninstallation> weak_factory_;
};

BorealisInstallerImpl::BorealisInstallerImpl(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {}

BorealisInstallerImpl::~BorealisInstallerImpl() = default;

bool BorealisInstallerImpl::IsProcessing() {
  return in_progress_installation_ ? true : false;
}

void BorealisInstallerImpl::Start() {
  RecordBorealisInstallNumAttemptsHistogram();
  if (!BorealisService::GetForProfile(profile_)->Features().IsAllowed()) {
    LOG(ERROR) << "Installation of Borealis cannot be started because "
               << "Borealis is not allowed.";
    OnInstallComplete(
        Unexpected<std::unique_ptr<InstallInfo>, BorealisInstallResult>(
            BorealisInstallResult::kBorealisNotAllowed));
    return;
  }

  if (IsProcessing()) {
    LOG(ERROR) << "Installation of Borealis is already in progress.";
    OnInstallComplete(
        Unexpected<std::unique_ptr<InstallInfo>, BorealisInstallResult>(
            BorealisInstallResult::kBorealisInstallInProgress));
    return;
  }

  if (content::GetNetworkConnectionTracker()->IsOffline()) {
    OnInstallComplete(
        Unexpected<std::unique_ptr<InstallInfo>, BorealisInstallResult>(
            BorealisInstallResult::kOffline));
    return;
  }

  auto install_info = std::make_unique<InstallInfo>();
  install_info->vm_name = "borealis";
  install_info->container_name = "penguin";
  in_progress_installation_ = std::make_unique<Installation>(
      base::BindRepeating(&BorealisInstallerImpl::UpdateProgress,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&BorealisInstallerImpl::UpdateInstallingState,
                          weak_ptr_factory_.GetWeakPtr()));
  in_progress_installation_->Begin(
      std::move(install_info),
      base::BindOnce(&BorealisInstallerImpl::OnInstallComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BorealisInstallerImpl::Cancel() {
  if (in_progress_installation_)
    in_progress_installation_->Cancel();
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
  auto uninstall_info = std::make_unique<InstallInfo>();
  uninstall_info->vm_name = "borealis";
  uninstall_info->container_name = "penguin";
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

void BorealisInstallerImpl::UpdateProgress(double state_progress) {
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
    case InstallingState::kInactive:
      NOTREACHED();
  }

  double new_progress =
      start_range + (end_range - start_range) * state_progress;

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

void BorealisInstallerImpl::OnInstallComplete(
    Expected<std::unique_ptr<InstallInfo>, BorealisInstallResult>
        result_or_error) {
  BorealisInstallResult result = result_or_error
                                     ? BorealisInstallResult::kSuccess
                                     : result_or_error.Error();
  // If another installation is in progress, we don't want to reset any states
  // and interfere with the process. When that process completes, it will reset
  // these states.
  if (result != BorealisInstallResult::kBorealisInstallInProgress) {
    base::TimeDelta duration =
        in_progress_installation_
            ? base::TimeTicks::Now() - in_progress_installation_->start_time()
            : base::TimeDelta::FromSeconds(0);
    in_progress_installation_.reset();
    installing_state_ = InstallingState::kInactive;
    if (result == BorealisInstallResult::kSuccess) {
      profile_->GetPrefs()->SetBoolean(prefs::kBorealisInstalledOnDevice, true);
      RecordBorealisInstallOverallTimeHistogram(duration);
    }
    RecordBorealisInstallResultHistogram(result);
  }
  for (auto& observer : observers_) {
    observer.OnInstallationEnded(result);
  }
}

void BorealisInstallerImpl::OnUninstallComplete(
    base::OnceCallback<void(BorealisUninstallResult)> on_uninstall_callback,
    Expected<std::unique_ptr<InstallInfo>, BorealisUninstallResult> result) {
  in_progress_uninstallation_.reset();
  BorealisUninstallResult uninstall_result = BorealisUninstallResult::kSuccess;
  if (!result) {
    uninstall_result = result.Error();
  }
  RecordBorealisUninstallResultHistogram(uninstall_result);
  std::move(on_uninstall_callback).Run(uninstall_result);
}

}  // namespace borealis
