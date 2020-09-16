// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_installer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/numerics/ranges.h"
#include "base/strings/string16.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/crostini/ansible/ansible_management_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_disk.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_manager_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_terminal.h"
#include "chrome/browser/chromeos/crostini/crostini_types.mojom.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer_dialog.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "ui/display/types/display_constants.h"

using crostini::mojom::InstallerError;
using crostini::mojom::InstallerState;

namespace crostini {

namespace {
using SetupResult = CrostiniInstaller::SetupResult;
constexpr char kCrostiniSetupSourceHistogram[] = "Crostini.SetupSource";

class CrostiniInstallerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static crostini::CrostiniInstaller* GetForProfile(Profile* profile) {
    return static_cast<crostini::CrostiniInstaller*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static CrostiniInstallerFactory* GetInstance() {
    static base::NoDestructor<CrostiniInstallerFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<CrostiniInstallerFactory>;

  CrostiniInstallerFactory()
      : BrowserContextKeyedServiceFactory(
            "CrostiniInstallerService",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(crostini::CrostiniManagerFactory::GetInstance());
    DependsOn(crostini::AnsibleManagementServiceFactory::GetInstance());
  }

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    return new crostini::CrostiniInstaller(profile);
  }
};

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
      base::BindOnce(&chromeos::StartupUtils::GetTimeSinceOobeFlagFileCreation),
      base::BindOnce([](base::TimeDelta time_from_device_setup) {
        if (time_from_device_setup.is_zero())
          return;

        // The magic number 1471228928 is used for legacy reasons and changing
        // it would invalidate already logged data.
        base::UmaHistogramCustomTimes(
            kCrostiniTimeFromDeviceSetupToInstall, time_from_device_setup,
            base::TimeDelta::FromMinutes(1),
            base::TimeDelta::FromMilliseconds(1471228928), 50);
      }));
}

SetupResult ErrorToSetupResult(InstallerError error) {
  switch (error) {
    case InstallerError::kNone:
      return SetupResult::kSuccess;
    case InstallerError::kErrorLoadingTermina:
      return SetupResult::kErrorLoadingTermina;
    case InstallerError::kErrorCreatingDiskImage:
      return SetupResult::kErrorCreatingDiskImage;
    case InstallerError::kErrorStartingTermina:
      return SetupResult::kErrorStartingTermina;
    case InstallerError::kErrorStartingContainer:
      return SetupResult::kErrorStartingContainer;
    case InstallerError::kErrorConfiguringContainer:
      return SetupResult::kErrorConfiguringContainer;
    case InstallerError::kErrorOffline:
      return SetupResult::kErrorOffline;
    case InstallerError::kErrorFetchingSshKeys:
      return SetupResult::kErrorFetchingSshKeys;
    case InstallerError::kErrorMountingContainer:
      return SetupResult::kErrorMountingContainer;
    case InstallerError::kErrorSettingUpContainer:
      return SetupResult::kErrorSettingUpContainer;
    case InstallerError::kErrorInsufficientDiskSpace:
      return SetupResult::kErrorInsufficientDiskSpace;
    case InstallerError::kErrorCreateContainer:
      return SetupResult::kErrorCreateContainer;
    case InstallerError::kErrorUnknown:
      return SetupResult::kErrorUnknown;
  }

  NOTREACHED();
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
    case InstallerState::kCreateContainer:
      return SetupResult::kUserCancelledCreateContainer;
    case InstallerState::kSetupContainer:
      return SetupResult::kUserCancelledSetupContainer;
    case InstallerState::kStartContainer:
      return SetupResult::kUserCancelledStartContainer;
    case InstallerState::kConfigureContainer:
      return SetupResult::kUserCancelledConfiguringContainer;
    case InstallerState::kFetchSshKeys:
      return SetupResult::kUserCancelledFetchSshKeys;
    case InstallerState::kMountContainer:
      return SetupResult::kUserCancelledMountContainer;
  }

  NOTREACHED();
}

}  // namespace

CrostiniInstaller* CrostiniInstaller::GetForProfile(Profile* profile) {
  return CrostiniInstallerFactory::GetForProfile(profile);
}

CrostiniInstaller::CrostiniInstaller(Profile* profile) : profile_(profile) {}

CrostiniInstaller::~CrostiniInstaller() {
  // Guaranteed by |Shutdown()|.
  DCHECK_EQ(restart_id_, CrostiniManager::kUninitializedRestartId);
}

void CrostiniInstaller::Shutdown() {
  if (restart_id_ != CrostiniManager::kUninitializedRestartId) {
    CrostiniManager::GetForProfile(profile_)->AbortRestartCrostini(
        restart_id_, base::DoNothing());
    restart_id_ = CrostiniManager::kUninitializedRestartId;
  }
}

void CrostiniInstaller::ShowDialog(CrostiniUISurface ui_surface) {
  // Defensive check to prevent showing the installer when crostini is not
  // allowed.
  if (!CrostiniFeatures::Get()->IsUIAllowed(profile_)) {
    return;
  }
  base::UmaHistogramEnumeration(kCrostiniSetupSourceHistogram, ui_surface,
                                crostini::CrostiniUISurface::kCount);

  // TODO(lxj): We should pass the dialog |this| here instead of letting the
  // webui to call |GetForProfile()| later.
  chromeos::CrostiniInstallerDialog::Show(profile_);
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
  progress_callback_ = std::move(progress_callback);
  result_callback_ = std::move(result_callback);

  install_start_time_ = base::TimeTicks::Now();
  require_cleanup_ = true;
  free_disk_space_ = kUninitializedDiskSpace;
  container_download_percent_ = 0;
  UpdateState(State::INSTALLING);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                     base::FilePath(crostini::kHomeDirectory)),
      base::BindOnce(&CrostiniInstaller::OnAvailableDiskSpace,
                     weak_ptr_factory_.GetWeakPtr()));

  // The default value of kCrostiniContainers is set to migrate existing
  // crostini users who don't have the pref set. If crostini is being installed,
  // then we know the user must not actually have any containers yet, so we set
  // this pref to the empty list.
  profile_->GetPrefs()->Set(crostini::prefs::kCrostiniContainers,
                            base::Value(base::Value::Type::LIST));
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
  crostini::CrostiniManager::GetForProfile(profile_)->AbortRestartCrostini(
      restart_id_, base::DoNothing());
  restart_id_ = CrostiniManager::kUninitializedRestartId;
  RecordSetupResult(InstallStateToCancelledSetupResult(installing_state_));

  if (require_cleanup_) {
    // Remove anything that got installed
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&crostini::CrostiniManager::RemoveCrostini,
                       base::Unretained(
                           crostini::CrostiniManager::GetForProfile(profile_)),
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

void CrostiniInstaller::OnStageStarted(InstallerState stage) {}

void CrostiniInstaller::OnComponentLoaded(CrostiniResult result) {
  DCHECK_EQ(installing_state_, InstallerState::kInstallImageLoader);

  if (result != CrostiniResult::SUCCESS) {
    if (content::GetNetworkConnectionTracker()->IsOffline()) {
      LOG(ERROR) << "Network connection dropped while downloading cros-termina";
      HandleError(InstallerError::kErrorOffline);
    } else {
      LOG(ERROR) << "Failed to install the cros-termina component";
      HandleError(InstallerError::kErrorLoadingTermina);
    }
    return;
  }
  UpdateInstallingState(InstallerState::kCreateDiskImage);
}

void CrostiniInstaller::OnDiskImageCreated(
    bool success,
    vm_tools::concierge::DiskImageStatus status,
    int64_t disk_size_available) {
  DCHECK_EQ(installing_state_, InstallerState::kCreateDiskImage);
  if (!success) {
    HandleError(InstallerError::kErrorCreatingDiskImage);
    return;
  }
  if (status == vm_tools::concierge::DiskImageStatus::DISK_STATUS_EXISTS) {
    require_cleanup_ = false;
  }
  UpdateInstallingState(InstallerState::kStartTerminaVm);
}

void CrostiniInstaller::OnVmStarted(bool success) {
  DCHECK_EQ(installing_state_, InstallerState::kStartTerminaVm);
  if (!success) {
    HandleError(InstallerError::kErrorStartingTermina);
    return;
  }
  UpdateInstallingState(InstallerState::kCreateContainer);
}

void CrostiniInstaller::OnContainerDownloading(int32_t download_percent) {
  DCHECK_EQ(installing_state_, InstallerState::kCreateContainer);
  container_download_percent_ = base::ClampToRange(download_percent, 0, 100);
  RunProgressCallback();
}

void CrostiniInstaller::OnContainerCreated(CrostiniResult result) {
  DCHECK_EQ(installing_state_, InstallerState::kCreateContainer);
  if (result != CrostiniResult::SUCCESS) {
    if (content::GetNetworkConnectionTracker()->IsOffline()) {
      LOG(ERROR) << "Network connection dropped while creating container";
      HandleError(InstallerError::kErrorOffline);
    } else {
      HandleError(InstallerError::kErrorCreateContainer);
    }
    return;
  }
  UpdateInstallingState(InstallerState::kSetupContainer);
}

void CrostiniInstaller::OnContainerSetup(bool success) {
  DCHECK_EQ(installing_state_, InstallerState::kSetupContainer);

  if (!success) {
    if (content::GetNetworkConnectionTracker()->IsOffline()) {
      LOG(ERROR) << "Network connection dropped while downloading container";
      HandleError(InstallerError::kErrorOffline);
    } else {
      HandleError(InstallerError::kErrorSettingUpContainer);
    }
    return;
  }
  UpdateInstallingState(InstallerState::kStartContainer);
  if (ShouldConfigureDefaultContainer(profile_)) {
    ansible_management_service_observer_.Add(
        AnsibleManagementService::GetForProfile(profile_));
  }
}

void CrostiniInstaller::OnAnsibleSoftwareConfigurationStarted() {
  DCHECK_EQ(installing_state_, InstallerState::kStartContainer);
  UpdateInstallingState(InstallerState::kConfigureContainer);
}

void CrostiniInstaller::OnAnsibleSoftwareConfigurationFinished(bool success) {
  DCHECK_EQ(installing_state_, InstallerState::kConfigureContainer);
  ansible_management_service_observer_.Remove(
      AnsibleManagementService::GetForProfile(profile_));

  if (!success) {
    LOG(ERROR) << "Failed to configure container";
    CrostiniManager::GetForProfile(profile_)->RemoveCrostini(
        kCrostiniDefaultVmName,
        base::BindOnce(
            &CrostiniInstaller::OnCrostiniRemovedAfterConfigurationFailed,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }
}

void CrostiniInstaller::OnCrostiniRemovedAfterConfigurationFailed(
    CrostiniResult result) {
  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Failed to remove Crostini after failed configuration";
  }

  if (content::GetNetworkConnectionTracker()->IsOffline()) {
    LOG(ERROR) << "Network connection dropped while configuring container";
    HandleError(InstallerError::kErrorOffline);
  } else {
    HandleError(InstallerError::kErrorConfiguringContainer);
  }
}

void CrostiniInstaller::OnContainerStarted(CrostiniResult result) {
  if (result == CrostiniResult::CONTAINER_CONFIGURATION_FAILED) {
    LOG(ERROR) << "Container start failed due to failed configuration";
    NOTREACHED();
    return;
  }

  DCHECK(installing_state_ == InstallerState::kStartContainer ||
         installing_state_ == InstallerState::kConfigureContainer);

  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Failed to start container with error code: "
               << static_cast<int>(result);
    HandleError(InstallerError::kErrorStartingContainer);
    return;
  }
  UpdateInstallingState(InstallerState::kFetchSshKeys);
}

void CrostiniInstaller::OnSshKeysFetched(bool success) {
  DCHECK_EQ(installing_state_, InstallerState::kFetchSshKeys);

  if (!success) {
    HandleError(InstallerError::kErrorFetchingSshKeys);
    return;
  }
  UpdateInstallingState(InstallerState::kMountContainer);
}

void CrostiniInstaller::OnContainerMounted(bool success) {
  DCHECK_EQ(installing_state_, InstallerState::kMountContainer);

  if (!success) {
    HandleError(InstallerError::kErrorMountingContainer);
  }
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
  auto state_max_time = base::TimeDelta::FromSeconds(1);

  switch (installing_state_) {
    case InstallerState::kStart:
      state_start_mark = 0;
      state_end_mark = 0;
      break;
    case InstallerState::kInstallImageLoader:
      state_start_mark = 0.0;
      state_end_mark = 0.20;
      state_max_time = base::TimeDelta::FromSeconds(30);
      break;
    case InstallerState::kCreateDiskImage:
      state_start_mark = 0.20;
      state_end_mark = 0.22;
      break;
    case InstallerState::kStartTerminaVm:
      state_start_mark = 0.22;
      state_end_mark = 0.28;
      state_max_time = base::TimeDelta::FromSeconds(8);
      break;
    case InstallerState::kCreateContainer:
      state_start_mark = 0.28;
      state_end_mark = 0.72;
      state_max_time = base::TimeDelta::FromSeconds(180);
      break;
    case InstallerState::kSetupContainer:
      state_start_mark = 0.72;
      state_end_mark = 0.76;
      state_max_time = base::TimeDelta::FromSeconds(8);
      break;
    case InstallerState::kStartContainer:
      state_start_mark = 0.76;
      state_end_mark = 0.79;
      state_max_time = base::TimeDelta::FromSeconds(8);
      break;
    case InstallerState::kConfigureContainer:
      state_start_mark = 0.79;
      state_end_mark = 0.99;
      // Ansible installation and playbook application.
      state_max_time = base::TimeDelta::FromSeconds(140 + 300);
      break;
    case InstallerState::kFetchSshKeys:
      state_start_mark = 0.99;
      state_end_mark = 1;
      break;
    case InstallerState::kMountContainer:
      state_start_mark = 1;
      state_end_mark = 1;
      break;
    default:
      NOTREACHED();
  }

  double state_fraction = time_in_state / state_max_time;

  if (installing_state_ == InstallerState::kCreateContainer) {
    // In CREATE_CONTAINER, consume half the progress bar with downloading,
    // the rest with time.
    state_fraction =
        0.5 * (state_fraction + 0.01 * container_download_percent_);
  }
  // TODO(https://crbug.com/1000173): Calculate configure container step
  // progress based on real progress.

  double progress =
      state_start_mark + base::ClampToRange(state_fraction, 0.0, 1.0) *
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
        FROM_HERE, base::TimeDelta::FromMilliseconds(500),
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

  if (result != CrostiniResult::SUCCESS) {
    if (state_ != State::ERROR && result != CrostiniResult::RESTART_ABORTED) {
      DCHECK_EQ(state_, State::INSTALLING);
      LOG(ERROR) << "Failed to restart Crostini with error code: "
                 << static_cast<int>(result);
      HandleError(InstallerError::kErrorUnknown);
    }
    return;
  }

  DCHECK_EQ(installing_state_, InstallerState::kMountContainer);
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
    crostini::LaunchTerminal(profile_, display::kInvalidDisplayId,
                             crostini::ContainerId::GetDefault());
  }
}

void CrostiniInstaller::OnAvailableDiskSpace(int64_t bytes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // |Cancel()| might be called immediately after |Install()|.
  if (state_ == State::CANCEL_ABORT_CHECK_DISK) {
    UpdateState(State::IDLE);
    RecordSetupResult(SetupResult::kNotStarted);
    std::move(cancel_callback_).Run();
    return;
  }

  DCHECK_EQ(installing_state_, InstallerState::kStart);

  free_disk_space_ = bytes;
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
              crostini::ContainerId::GetDefault(), std::move(restart_options_),
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
