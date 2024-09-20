// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_installer_impl.h"

#include <memory>
#include <sstream>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_types.mojom.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/infra/transition.h"
#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"

namespace borealis {

namespace {

// Time to wait for borealis' main app to appear. This is done almost
// immediately by garcon on launch so a short timeout is sufficient.
constexpr base::TimeDelta kWaitForMainAppTimeout = base::Seconds(5);

}  // namespace

using borealis::mojom::InstallResult;

class BorealisInstallerImpl::Installation
    : public Transition<BorealisInstallerImpl::InstallInfo,
                        BorealisInstallerImpl::InstallInfo,
                        Described<InstallResult>>,
      public guest_os::GuestOsRegistryService::Observer {
 public:
  Installation(
      Profile* profile,
      base::RepeatingCallback<void(double)> update_progress_callback,
      base::RepeatingCallback<void(InstallingState)> update_state_callback)
      : profile_(profile),
        installation_start_tick_(base::TimeTicks::Now()),
        update_progress_callback_(std::move(update_progress_callback)),
        update_state_callback_(std::move(update_state_callback)),
        apps_observation_(this),
        weak_factory_(this) {}

  base::TimeTicks start_time() { return installation_start_tick_; }

  void Cancel() {
    Fail({InstallResult::kCancelled, "Installation cancelled by user"});
  }

 private:
  void Start(std::unique_ptr<BorealisInstallerImpl::InstallInfo> start_instance)
      override {
    install_info_ = std::move(start_instance);
    SetState(InstallingState::kCheckingIfAllowed);
    BorealisServiceFactory::GetForProfile(profile_)->Features().IsAllowed(
        base::BindOnce(&Installation::OnAllowedCheckCompleted,
                       weak_factory_.GetWeakPtr()));
  }

  void SetState(InstallingState state) {
    update_state_callback_.Run(state);
    installing_state_ = state;
  }

  void OnAllowedCheckCompleted(BorealisFeatures::AllowStatus allow_status) {
    if (allow_status != BorealisFeatures::AllowStatus::kAllowed) {
      std::stringstream ss;
      ss << "Borealis is not allowed: " << allow_status;
      Fail({InstallResult::kBorealisNotAllowed, ss.str()});
      return;
    }
    SetState(InstallingState::kInstallingDlc);
    InstallDlc();
  }

  void InstallDlc() {
    dlc_installation_ = std::make_unique<guest_os::GuestOsDlcInstallation>(
        kBorealisDlcName,
        base::BindOnce(&Installation::OnDlcInstallationCompleted,
                       weak_factory_.GetWeakPtr()),
        base::BindRepeating(&Installation::OnDlcInstallationProgressUpdated,
                            weak_factory_.GetWeakPtr()));
  }

  void OnDlcInstallationProgressUpdated(double progress) {
    DCHECK_EQ(installing_state_, InstallingState::kInstallingDlc);
    update_progress_callback_.Run(progress);
  }

  void OnDlcInstallationCompleted(
      guest_os::GuestOsDlcInstallation::Result install_result) {
    DCHECK_EQ(installing_state_, InstallingState::kInstallingDlc);

    // If success, continue to the next state.
    if (install_result.has_value()) {
      // We are in the callback of DLC completion, and the first thing startup
      // will do is try to mount the DLC, so we need to use a PostTask to avoid
      // deadlocking ourselves.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&Installation::StartupBorealis,
                                    weak_factory_.GetWeakPtr()));
      return;
    }

    // At this point, the Borealis DLC installation has failed.
    Fail(DescribeDlcFailure(install_result.error()));
  }

  Described<InstallResult> DescribeDlcFailure(
      guest_os::GuestOsDlcInstallation::Error error) {
    switch (error) {
      case guest_os::GuestOsDlcInstallation::Error::Cancelled:
        return {InstallResult::kCancelled, "Installation cancelled by user."};
      case guest_os::GuestOsDlcInstallation::Error::Offline:
        return {InstallResult::kOffline,
                "Failed to download DLC while device is offline."};
      case guest_os::GuestOsDlcInstallation::Error::NeedUpdate:
        return {
            InstallResult::kDlcNeedUpdateError,
            "Omaha could not provide an image, device may need to be updated."};
      case guest_os::GuestOsDlcInstallation::Error::NeedReboot:
        return {InstallResult::kDlcNeedRebootError,
                "Device has pending update and needs a reboot to use Borealis "
                "DLC."};
      case guest_os::GuestOsDlcInstallation::Error::DiskFull:
        return {InstallResult::kDlcNeedSpaceError,
                "Device needs to free space to use Borealis DLC."};
      case guest_os::GuestOsDlcInstallation::Error::Busy:
        return {
            InstallResult::kDlcBusyError,
            "Borealis DLC is not able to be installed as dlcservice is busy."};
      case guest_os::GuestOsDlcInstallation::Error::Internal:
        return {InstallResult::kDlcInternalError,
                "Something went wrong internally with DlcService."};
      case guest_os::GuestOsDlcInstallation::Error::Invalid:
        return {InstallResult::kDlcUnsupportedError,
                "Borealis DLC is not supported, need to enable Borealis DLC."};
      case guest_os::GuestOsDlcInstallation::Error::UnknownFailure:
        return {InstallResult::kDlcUnknownError,
                "Unexpected DLC failure, please file feedback."};
    }

    NOTREACHED();
  }

  // As part of its installation we perform a dry run of borealis. This ensures
  // that the VM works somewhat and allows the container_guest daemon to update
  // Chrome. See go/borealis-mid-launch for details.
  void StartupBorealis() {
    SetState(InstallingState::kStartingUp);
    BorealisServiceFactory::GetForProfile(profile_)
        ->ContextManager()
        .StartBorealis(base::BindOnce(&Installation::OnBorealisStarted,
                                      weak_factory_.GetWeakPtr()));
  }

  void OnBorealisStarted(BorealisContextManager::ContextOrFailure result) {
    if (result.has_value()) {
      WaitForMainApp();
      return;
    }
    std::stringstream ss;
    ss << "Failed to start borealis (code "
       << static_cast<int>(result.error().error())
       << "): " << result.error().description();
    Fail({InstallResult::kStartupFailed, ss.str()});
  }

  void WaitForMainApp() {
    SetState(InstallingState::kAwaitingApplications);
    guest_os::GuestOsRegistryService* apps_registry =
        guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_);
    apps_observation_.Observe(apps_registry);
    std::optional<guest_os::GuestOsRegistryService::Registration> main_app =
        apps_registry->GetRegistration(kClientAppId);
    if (main_app.has_value() &&
        main_app->VmType() == guest_os::VmType::BOREALIS) {
      apps_observation_.Reset();
      MainAppFound(true);
      return;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&Installation::MainAppFound, weak_factory_.GetWeakPtr(),
                       false),
        kWaitForMainAppTimeout);
  }

  void OnRegistryUpdated(
      guest_os::GuestOsRegistryService* registry_service,
      guest_os::VmType vm_type,
      const std::vector<std::string>& updated_apps,
      const std::vector<std::string>& removed_apps,
      const std::vector<std::string>& inserted_apps) override {
    if (vm_type != guest_os::VmType::BOREALIS) {
      return;
    }

    for (const auto& app : inserted_apps) {
      if (app == kClientAppId) {
        MainAppFound(true);
        break;
      }
    }
  }

  void MainAppFound(bool found) {
    // We use the presence of the install_info_ object to prevent races here, so
    // return if it has already been removed.
    if (!install_info_) {
      return;
    }
    if (!found) {
      install_info_.reset();
      Fail({InstallResult::kMainAppNotPresent,
            "Failed to verify that the main app has been created"});
      return;
    }
    Succeed(std::move(install_info_));
  }

  const raw_ptr<Profile> profile_;
  base::TimeTicks installation_start_tick_;
  InstallingState installing_state_;
  base::RepeatingCallback<void(double)> update_progress_callback_;
  base::RepeatingCallback<void(InstallingState)> update_state_callback_;
  std::unique_ptr<BorealisInstallerImpl::InstallInfo> install_info_;
  std::unique_ptr<guest_os::GuestOsDlcInstallation> dlc_installation_;
  base::ScopedObservation<guest_os::GuestOsRegistryService,
                          guest_os::GuestOsRegistryService::Observer>
      apps_observation_;
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
    BorealisServiceFactory::GetForProfile(profile_)
        ->ContextManager()
        .ShutDownBorealis(base::BindOnce(&Uninstallation::OnShutdownCompleted,
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
        ->ClearApplicationList(vm_tools::apps::BOREALIS,
                               uninstall_info_->vm_name,
                               uninstall_info_->container_name);

    vm_tools::concierge::DestroyDiskImageRequest request;
    request.set_cryptohome_id(
        ash::ProfileHelper::GetUserIdHashFromProfile(profile_));
    request.set_vm_name(uninstall_info_->vm_name);
    ash::ConciergeClient::Get()->DestroyDiskImage(
        std::move(request), base::BindOnce(&Uninstallation::OnDiskRemoved,
                                           weak_factory_.GetWeakPtr()));
  }

  void OnDiskRemoved(
      std::optional<vm_tools::concierge::DestroyDiskImageResponse> response) {
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

    ash::DlcserviceClient::Get()->Uninstall(
        kBorealisDlcName, base::BindOnce(&Uninstallation::OnDlcUninstalled,
                                         weak_factory_.GetWeakPtr()));
  }

  void OnDlcUninstalled(std::string_view dlc_err) {
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

  const raw_ptr<Profile> profile_;
  std::unique_ptr<BorealisInstallerImpl::InstallInfo> uninstall_info_;
  base::WeakPtrFactory<Uninstallation> weak_factory_;
};

BorealisInstallerImpl::BorealisInstallerImpl(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {}

BorealisInstallerImpl::~BorealisInstallerImpl() = default;

bool BorealisInstallerImpl::IsProcessing() {
  return !!in_progress_installation_;
}

void BorealisInstallerImpl::Start() {
  RecordBorealisInstallNumAttemptsHistogram();
  if (IsProcessing()) {
    OnInstallComplete(base::unexpected(Installation::ErrorState{
        InstallResult::kBorealisInstallInProgress,
        "Installation of Borealis is already in progress"}));
    return;
  }

  if (content::GetNetworkConnectionTracker()->IsOffline()) {
    OnInstallComplete(base::unexpected(Installation::ErrorState{
        InstallResult::kOffline, "Can not install Borealis while offline"}));
    return;
  }

  // Reset mic permission, we don't want it to persist across
  // re-installation.
  profile_->GetPrefs()->SetBoolean(prefs::kBorealisMicAllowed, false);

  auto install_info = std::make_unique<InstallInfo>();
  install_info->vm_name = "borealis";
  install_info->container_name = "penguin";
  in_progress_installation_ = std::make_unique<Installation>(
      profile_,
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
  if (in_progress_installation_) {
    in_progress_installation_->Cancel();
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
  DCHECK(observers_.empty());
  observers_.AddObserver(observer);
}

void BorealisInstallerImpl::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
  DCHECK(observers_.empty());
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
    case InstallingState::kCheckingIfAllowed:
      start_range = 0;
      end_range = 0.1;
      break;
    case InstallingState::kInstallingDlc:
      start_range = 0.1;
      end_range = 0.5;
      break;
    case InstallingState::kStartingUp:
      start_range = 0.5;
      end_range = 0.8;
      break;
    case InstallingState::kAwaitingApplications:
      start_range = 0.8;
      end_range = 1.0;
      break;
    case InstallingState::kInactive:
      NOTREACHED_IN_MIGRATION();
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
  // The state just changed, so the progress towards that state is 0.
  UpdateProgress(0);
}

void BorealisInstallerImpl::OnInstallComplete(
    base::expected<std::unique_ptr<InstallInfo>, Described<InstallResult>>
        result_or_error) {
  InstallResult result = result_or_error.has_value()
                             ? InstallResult::kSuccess
                             : result_or_error.error().error();
  // If another installation is in progress, we don't want to reset any states
  // and interfere with the process. When that process completes, it will reset
  // these states.
  if (result != InstallResult::kBorealisInstallInProgress) {
    base::TimeDelta duration =
        in_progress_installation_
            ? base::TimeTicks::Now() - in_progress_installation_->start_time()
            : base::Seconds(0);
    in_progress_installation_.reset();
    installing_state_ = InstallingState::kInactive;
    if (result == InstallResult::kSuccess) {
      profile_->GetPrefs()->SetBoolean(prefs::kBorealisInstalledOnDevice, true);
      RecordBorealisInstallOverallTimeHistogram(duration);
    }
    // TODO(b/188713071): Clean up if installation fails.
    RecordBorealisInstallResultHistogram(result);
  }
  for (auto& observer : observers_) {
    observer.OnInstallationEnded(result,
                                 result_or_error.has_value()
                                     ? ""
                                     : result_or_error.error().description());
  }
}

void BorealisInstallerImpl::OnUninstallComplete(
    base::OnceCallback<void(BorealisUninstallResult)> on_uninstall_callback,
    base::expected<std::unique_ptr<InstallInfo>, BorealisUninstallResult>
        result) {
  in_progress_uninstallation_.reset();
  BorealisUninstallResult uninstall_result = BorealisUninstallResult::kSuccess;
  if (!result.has_value()) {
    uninstall_result = result.error();
  }
  RecordBorealisUninstallResultHistogram(uninstall_result);
  std::move(on_uninstall_callback).Run(uninstall_result);
}

}  // namespace borealis
