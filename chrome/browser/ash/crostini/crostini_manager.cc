// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_manager.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_service.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_service_factory.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_manager_factory.h"
#include "chrome/browser/ash/crostini/crostini_metrics_service.h"
#include "chrome/browser/ash/crostini/crostini_mount_provider.h"
#include "chrome/browser/ash/crostini/crostini_port_forwarder.h"
#include "chrome/browser/ash/crostini/crostini_port_forwarder_factory.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_reporting_util.h"
#include "chrome/browser/ash/crostini/crostini_simple_types.h"
#include "chrome/browser/ash/crostini/crostini_sshfs.h"
#include "chrome/browser/ash/crostini/crostini_terminal_provider.h"
#include "chrome/browser/ash/crostini/crostini_types.mojom-shared.h"
#include "chrome/browser/ash/crostini/crostini_upgrade_available_notification.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/crostini/throttle/crostini_throttle_factory.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_remover.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_stability_monitor.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service_factory.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/scheduler_config/scheduler_configuration_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/crostini/crostini_expired_container_warning_view.h"
#include "chrome/browser/ui/views/crostini/crostini_update_filesystem_view.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/dbus/anomaly_detector/anomaly_detector_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/component_updater/component_updater_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace crostini {

namespace {
const auto kStartVmTimeout = base::Seconds(300);

ash::CiceroneClient* GetCiceroneClient() {
  return ash::CiceroneClient::Get();
}

ash::ConciergeClient* GetConciergeClient() {
  return ash::ConciergeClient::Get();
}

// Find any callbacks for the specified |vm_name|, invoke them with
// |arguments|... and erase them from the map.
template <typename... Parameters, typename... Arguments>
void InvokeAndErasePendingCallbacks(
    std::map<guest_os::GuestId, base::OnceCallback<void(Parameters...)>>*
        vm_keyed_map,
    const std::string& vm_name,
    Arguments... arguments) {
  for (auto it = vm_keyed_map->begin(); it != vm_keyed_map->end();) {
    if (it->first.vm_name == vm_name) {
      std::move(it->second).Run(arguments...);
      vm_keyed_map->erase(it++);
    } else {
      ++it;
    }
  }
}

// Find any callbacks for the specified |vm_name|, invoke them with
// |result| and erase them from the map.
void InvokeAndErasePendingCallbacks(
    std::multimap<std::string, CrostiniManager::CrostiniResultCallback>*
        vm_callbacks,
    const std::string& vm_name,
    CrostiniResult result) {
  auto range = vm_callbacks->equal_range(vm_name);
  for (auto it = range.first; it != range.second; ++it) {
    std::move(it->second).Run(result);
  }
  vm_callbacks->erase(range.first, range.second);
}

void EraseCommandUuid(std::map<std::string, guest_os::GuestId>* uuid_map,
                      const std::string& vm_name) {
  for (auto it = uuid_map->begin(); it != uuid_map->end();) {
    if (it->second.vm_name == vm_name) {
      uuid_map->erase(it++);
    } else {
      ++it;
    }
  }
}

// Find any container callbacks for the specified |container_id|, invoke them
// with |result| and erase them from the map.
void InvokeAndErasePendingContainerCallbacks(
    std::multimap<guest_os::GuestId, CrostiniManager::CrostiniResultCallback>*
        container_callbacks,
    const guest_os::GuestId& container_id,
    CrostiniResult result) {
  auto range = container_callbacks->equal_range(container_id);
  for (auto it = range.first; it != range.second; ++it) {
    VLOG(1) << "Invoking pending container callback for "
            << it->first.container_name;
    // We end up here when triggered by an observer method, which is
    // synchronous. Post the callback instead of continuing to run it in the
    // same task so other observers of e.g. ContainerStarted have a chance to
    // run and update first, so callers get a consistent view across GuestOS
    // services. See e.g. b/249219794 for an example of what can break without
    // this.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(it->second), result));
  }
  container_callbacks->erase(range.first, range.second);
}

void EmitCorruptionStateMetric(CorruptionStates state) {
  base::UmaHistogramEnumeration("Crostini.FilesystemCorruption", state);
}

void EmitTimeInStageHistogram(base::TimeDelta duration,
                              mojom::InstallerState state) {
  std::string name;
  switch (state) {
    case mojom::InstallerState::kStart:
      name = "Crostini.RestarterTimeInState2.Start";
      break;
    case mojom::InstallerState::kInstallImageLoader:
      name = "Crostini.RestarterTimeInState2.InstallImageLoader";
      break;
    case mojom::InstallerState::kCreateDiskImage:
      name = "Crostini.RestarterTimeInState2.CreateDiskImage";
      break;
    case mojom::InstallerState::kStartTerminaVm:
      name = "Crostini.RestarterTimeInState2.StartTerminaVm";
      break;
    case mojom::InstallerState::kStartLxd:
      name = "Crostini.RestarterTimeInState2.StartLxd";
      break;
    case mojom::InstallerState::kCreateContainer:
      name = "Crostini.RestarterTimeInState2.CreateContainer";
      break;
    case mojom::InstallerState::kSetupContainer:
      name = "Crostini.RestarterTimeInState2.SetupContainer";
      break;
    case mojom::InstallerState::kStartContainer:
      name = "Crostini.RestarterTimeInState2.StartContainer";
      break;
    case mojom::InstallerState::kConfigureContainer:
      name = "Crostini.RestarterTimeInState2.ConfigureContainer";
      break;
  }
  base::UmaHistogramCustomTimes(name, duration, base::Milliseconds(10),
                                base::Hours(6), 50);
}

}  // namespace

const char kCrostiniStabilityHistogram[] = "Crostini.Stability";

CrostiniManager::RestartId CrostiniManager::next_restart_id_ = 0;

CrostiniManager::RestartOptions::RestartOptions() = default;
CrostiniManager::RestartOptions::RestartOptions(RestartOptions&&) = default;
CrostiniManager::RestartOptions::~RestartOptions() = default;
CrostiniManager::RestartOptions& CrostiniManager::RestartOptions::operator=(
    RestartOptions&&) = default;

class CrostiniManager::CrostiniRestarter
    : public ash::VmShutdownObserver,
      public ash::SchedulerConfigurationManagerBase::Observer {
 public:
  struct RestartRequest {
    RestartId restart_id;
    RestartOptions options;
    CrostiniResultCallback callback;
    raw_ptr<RestartObserver> observer;  // optional
  };

  CrostiniRestarter(Profile* profile,
                    CrostiniManager* crostini_manager,
                    guest_os::GuestId container_id,
                    RestartRequest request);
  ~CrostiniRestarter() override;

  void AddRequest(RestartRequest request);

  // Start the restart flow. This should called immediately following
  // construction and only once. This cannot be called directly from the
  // constructor as in some cases it immediately (synchronously) fails and
  // causes |this| to be deleted.
  void Restart();

  // ash::VmShutdownObserver
  void OnVmShutdown(const std::string& vm_name) override;

  void Timeout(mojom::InstallerState state);

  // Cancel an individual request and fire its callback immediately. If there
  // are no other outstanding requests, stop the restarter once possible.
  void CancelRequest(RestartId restart_id);
  // Abort the entire restart. Pending requests are immediately completed, and
  // |callback| is called once the current operation has finished. Requests
  // should not be added to an aborted restarter.
  void Abort(base::OnceClosure callback);

  // These are called directly from CrostiniManager.
  void OnContainerDownloading(int download_percent);
  void OnLxdContainerStarting(
      vm_tools::cicerone::LxdContainerStartingSignal_Status status);

  const guest_os::GuestId& container_id() { return container_id_; }

  // This is public so CallRestarterStartLxdContainerFinishedForTesting can call
  // it.
  void StartLxdContainerFinished(CrostiniResult result);

 private:
  void StartStage(mojom::InstallerState stage);
  void EmitMetricIfInIncorrectState(mojom::InstallerState expected);

  using RequestFilter = base::RepeatingCallback<bool(const RestartRequest&)>;
  // Removes matched requests and returns a closure which will run the
  // corresponding completion callbacks.
  base::OnceClosure ExtractRequests(RequestFilter filter,
                                    CrostiniResult result);
  void FinishRequests(RequestFilter filter, CrostiniResult result) {
    return ExtractRequests(filter, result).Run();
  }

  // The restarter flow ends early if Abort() is called or all requests have
  // been cancelled or otherwise fulfilled (e.g. when start_vm_only is set).
  // If this method returns true, then FinishRestart() is called and |this|
  // gets deleted so it is unsafe to refer to any member variables.
  bool ReturnEarlyIfNeeded();

  // In a successful complete restart, every function in the below list in
  // called in order, from Restart() to FinishRestart(). If the restarter
  // finishes early (i.e. restarter aborted, all requests cancelled or
  // completed, operation fails or times out), it proceeds directly to
  // FinishRestart().

  // Public function - Restart();
  void ContinueRestart();
  void LoadComponentFinished(CrostiniResult result);
  void CreateDiskImageFinished(int64_t disk_size_bytes,
                               CrostiniResult result,
                               const base::FilePath& result_path);
  // ash::SchedulerConfigurationManagerBase::Observer:
  void OnConfigurationSet(bool success, size_t num_cores_disabled) override;
  void OnConfigureContainerFinished(bool success);
  void StartTerminaVmFinished(bool success);
  void SharePathsFinished(bool success, const std::string& failure_reason);
  void StartLxdFinished(CrostiniResult result);
  void CreateLxdContainerFinished(CrostiniResult result);
  void SetUpLxdContainerUserFinished(bool success);
  // Public function - StartLxdContainerFinished(CrostiniResult result);
  // FinishRestart() causes |this| to be deleted, so callers should return
  // immediately after calling this.
  void FinishRestart(CrostiniResult result);

  // If the current operation can be cancelled, cancel it. This is run at most
  // once, when all requests are cancelled or the restart is aborted.
  void MaybeCancelCurrentOperation();

  void LogRestarterResult(const RestartRequest& request, CrostiniResult result);

  void OnConciergeAvailable(bool service_available);

  base::OneShotTimer stage_timeout_timer_;
  base::TimeTicks stage_start_;

  // TODO(crbug/1153210): Better numbers for timeouts once we have data.
  const std::map<mojom::InstallerState, base::TimeDelta> stage_timeouts_ = {
      {mojom::InstallerState::kStart, base::Minutes(2)},
      {mojom::InstallerState::kInstallImageLoader,
       base::Hours(6)},  // May need to download DLC or component
      {mojom::InstallerState::kCreateDiskImage, base::Minutes(5)},
      {mojom::InstallerState::kStartTerminaVm, kStartVmTimeout},
      {mojom::InstallerState::kStartLxd, base::Minutes(5)},
      // While CreateContainer may need to download a file, we get progress
      // messages that reset the countdown.
      {mojom::InstallerState::kCreateContainer, base::Minutes(5)},
      {mojom::InstallerState::kSetupContainer, base::Minutes(5)},
      // StartContainer sends heartbeat messages on a 30-second interval, but
      // there's a bit of work that's not covered by heartbeat messages so to be
      // safe set a 8 minute timeout.
      {mojom::InstallerState::kStartContainer, base::Minutes(8)},
      // Configuration may be slow, making timeout 2 hours at first because some
      // playbooks are gigantic (e.g. Chromium playbook).
      {mojom::InstallerState::kConfigureContainer, base::Hours(2)},
  };

  // Use shorter timeouts for some states if Crostini is already installed.
  const std::map<mojom::InstallerState, base::TimeDelta>
      stage_timeouts_already_installed_ = {
          {mojom::InstallerState::kInstallImageLoader, base::Minutes(5)},
          // The configure step should only be reached during multi-container
          // installation.
          {mojom::InstallerState::kConfigureContainer, base::Seconds(5)},
      };

  raw_ptr<Profile> profile_;
  // This isn't accessed after the CrostiniManager is destroyed and we need a
  // reference to it during the CrostiniRestarter destructor.
  raw_ptr<CrostiniManager> crostini_manager_;

  const guest_os::GuestId container_id_;
  bool is_initial_install_ = false;
  std::vector<base::OnceClosure> abort_callbacks_;
  // Options which only affect new containers will be taken from the first
  // request.
  std::vector<RestartRequest> requests_;
  // Pulled out of requests_ for convenience.
  base::ObserverList<CrostiniManager::RestartObserver>::Unchecked
      observer_list_;
  // TODO(timloh): This should just be an extra state at the start of the flow.
  bool is_running_ = false;

  // Data passed between different steps of the restart flow.
  base::FilePath disk_path_;
  size_t num_cores_disabled_ = 0;

  mojom::InstallerState stage_ = mojom::InstallerState::kStart;

  base::ScopedObservation<ash::SchedulerConfigurationManagerBase,
                          ash::SchedulerConfigurationManagerBase::Observer>
      scheduler_configuration_manager_observation_{this};
  base::ScopedObservation<CrostiniManager, ash::VmShutdownObserver>
      vm_shutdown_observation_{this};

  base::WeakPtrFactory<CrostiniRestarter> weak_ptr_factory_{this};
};

CrostiniManager::CrostiniRestarter::CrostiniRestarter(
    Profile* profile,
    CrostiniManager* crostini_manager,
    guest_os::GuestId container_id,
    RestartRequest request)
    : profile_(profile),
      crostini_manager_(crostini_manager),
      container_id_(std::move(container_id)) {
  AddRequest(std::move(request));
}

CrostiniManager::CrostiniRestarter::~CrostiniRestarter() {
  if (!requests_.empty()) {
    // This is triggered by logging out when restarts are in progress.
    LOG(WARNING) << "Destroying with outstanding requests.";
    for (const auto& request : requests_) {
      LogRestarterResult(request, CrostiniResult::NEVER_FINISHED);
    }
  }
}

void CrostiniManager::CrostiniRestarter::Restart() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!CrostiniFeatures::Get()->IsAllowedNow(profile_)) {
    LOG(ERROR) << "Crostini UI not allowed for profile "
               << profile_->GetProfileUserName();
    FinishRestart(CrostiniResult::NOT_ALLOWED);
    return;
  }

  vm_shutdown_observation_.Observe(crostini_manager_.get());
  // TODO(b/205650706): It is possible to invoke a CrostiniRestarter to install
  // Crostini without using the actual installer. We should handle these better.
  RestartSource restart_source = requests_[0].options.restart_source;
  is_initial_install_ =
      restart_source == RestartSource::kInstaller ||
      restart_source == RestartSource::kMultiContainerCreation;

  StartStage(mojom::InstallerState::kStart);
  if (ReturnEarlyIfNeeded()) {
    return;
  }

  auto vm_info = crostini_manager_->GetVmInfo(container_id_.vm_name);
  // If vm is stopping, we wait until OnVmShutdown() to kick it off.
  if (vm_info && vm_info->state == VmState::STOPPING) {
    LOG(WARNING) << "Delay restart due to vm stopping";
  } else {
    ContinueRestart();
  }
}

void CrostiniManager::CrostiniRestarter::AddRequest(RestartRequest request) {
  // CrostiniManager doesn't add requests to aborted restarts.
  DCHECK(abort_callbacks_.empty());

  if (request.observer) {
    observer_list_.AddObserver(request.observer.get());
  }
  requests_.push_back(std::move(request));
}

void CrostiniManager::CrostiniRestarter::OnVmShutdown(
    const std::string& vm_name) {
  if (ReturnEarlyIfNeeded()) {
    return;
  }
  if (vm_name == container_id_.vm_name) {
    if (is_running_) {
      LOG(WARNING) << "Unexpected VM shutdown during restart for " << vm_name;
      FinishRestart(CrostiniResult::RESTART_FAILED_VM_STOPPED);
    } else {
      // We can only get here if Restart() was called to register the shutdown
      // observer, and since is_running_ is false, we are waiting for this
      // shutdown to actually kick off the process.
      VLOG(1) << "resume restart on vm shutdown";
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&CrostiniRestarter::ContinueRestart,
                                    weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void CrostiniManager::CrostiniRestarter::Timeout(mojom::InstallerState state) {
  CrostiniResult result = CrostiniResult::UNKNOWN_STATE_TIMED_OUT;
  LOG(ERROR) << "Timed out in state " << state;
  switch (state) {
    case mojom::InstallerState::kInstallImageLoader:
      result = CrostiniResult::INSTALL_IMAGE_LOADER_TIMED_OUT;
      break;
    case mojom::InstallerState::kCreateDiskImage:
      result = CrostiniResult::CREATE_DISK_IMAGE_TIMED_OUT;
      break;
    case mojom::InstallerState::kStartTerminaVm:
      result = CrostiniResult::START_TERMINA_VM_TIMED_OUT;
      break;
    case mojom::InstallerState::kStartLxd:
      result = CrostiniResult::START_LXD_TIMED_OUT;
      break;
    case mojom::InstallerState::kCreateContainer:
      result = CrostiniResult::CREATE_CONTAINER_TIMED_OUT;
      break;
    case mojom::InstallerState::kSetupContainer:
      result = CrostiniResult::SETUP_CONTAINER_TIMED_OUT;
      break;
    case mojom::InstallerState::kStartContainer:
      result = CrostiniResult::START_CONTAINER_TIMED_OUT;
      break;
    case mojom::InstallerState::kConfigureContainer:
      result = CrostiniResult::CONFIGURE_CONTAINER_TIMED_OUT;
      break;
    case mojom::InstallerState::kStart:
      result = CrostiniResult::START_TIMED_OUT;
  }
  // Note: FinishRestart deletes |this|.
  FinishRestart(result);
}

void CrostiniManager::CrostiniRestarter::CancelRequest(RestartId restart_id) {
  size_t num_requests = requests_.size();
  FinishRequests(
      base::BindRepeating(
          [](RestartId restart_id, const RestartRequest& request) -> bool {
            return request.restart_id == restart_id;
          },
          restart_id),
      CrostiniResult::RESTART_REQUEST_CANCELLED);
  DCHECK_LE(requests_.size(), num_requests);

  if (requests_.empty()) {
    MaybeCancelCurrentOperation();
  }
}

void CrostiniManager::CrostiniRestarter::Abort(base::OnceClosure callback) {
  abort_callbacks_.push_back(std::move(callback));
  if (requests_.empty()) {
    // New requests are not added to aborted restarters, so we've already been
    // aborted and/or all requests were explicitly cancelled.
    return;
  }

  // Run the result callbacks immediately, but wait for the current step to
  // finish before invoking the abort callback.
  FinishRequests(
      base::BindRepeating([](const RestartRequest& request) { return true; }),
      CrostiniResult::RESTART_ABORTED);
  MaybeCancelCurrentOperation();
}

void CrostiniManager::CrostiniRestarter::OnContainerDownloading(
    int download_percent) {
  if (!is_running_) {
    return;
  }
  if (stage_timeout_timer_.IsRunning()) {
    // We got a progress message, reset the timeout duration back to full.
    stage_timeout_timer_.Reset();
  }
  for (auto& observer : observer_list_) {
    observer.OnContainerDownloading(download_percent);
  }
}

void CrostiniManager::CrostiniRestarter::OnLxdContainerStarting(
    vm_tools::cicerone::LxdContainerStartingSignal_Status status) {
  if (!is_running_ || !stage_timeout_timer_.IsRunning() ||
      status != vm_tools::cicerone::LxdContainerStartingSignal::STARTING ||
      stage_ != mojom::InstallerState::kStartContainer) {
    VLOG(1) << "Got start container message but status is " << status
            << " and stage is " << stage_ << " so not extending timeout";
    return;
  }
  // We got a progress message, reset the timeout duration back to full.
  VLOG(1) << "Got start container heartbeat so extending timeout";
  stage_timeout_timer_.Reset();
}

void CrostiniManager::CrostiniRestarter::StartLxdContainerFinished(
    CrostiniResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  CloseCrostiniUpdateFilesystemView();
  if (ReturnEarlyIfNeeded()) {
    return;
  }
  EmitMetricIfInIncorrectState(mojom::InstallerState::kStartContainer);
  if (result != CrostiniResult::SUCCESS) {
    FinishRestart(result);
    return;
  }

  // If arc sideloading is enabled, configure the container for that.
  crostini_manager_->ConfigureForArcSideload();

  if (requests_[0].options.ansible_playbook.has_value()) {
    // Check to see if there's any additional configuration via Ansible
    // required.
    StartStage(mojom::InstallerState::kConfigureContainer);
    AnsibleManagementServiceFactory::GetForProfile(profile_)
        ->ConfigureContainer(
            container_id_, requests_[0].options.ansible_playbook.value(),
            base::BindOnce(&CrostiniRestarter::OnConfigureContainerFinished,
                           weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  // If default termina/penguin, then sshfs mount and reshare folders, else we
  // are finished. Because the session tracker update and this method are racing
  // on the same thread we do the update async once the session tracker is
  // ready.
  if (container_id_ == DefaultContainerId()) {
    crostini_manager_->primary_counter_mount_subscription_ =
        guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile_)
            ->RunOnceContainerStarted(
                container_id_,
                base::BindOnce(&CrostiniManager::MountCrostiniFilesBackground,
                               crostini_manager_->GetWeakPtr()));
  }
  FinishRestart(result);
}

void CrostiniManager::CrostiniRestarter::StartStage(
    mojom::InstallerState stage) {
  int finished_stage = static_cast<int>(stage) - 1;
  if (finished_stage >= 0) {
    EmitTimeInStageHistogram(
        base::TimeTicks::Now() - stage_start_,
        static_cast<mojom::InstallerState>(finished_stage));
  }
  this->stage_ = stage;
  stage_start_ = base::TimeTicks::Now();

  DCHECK(stage_timeouts_.find(stage) != stage_timeouts_.end());
  auto delay = stage_timeouts_.at(stage);

  if (requests_[0].options.restart_source != RestartSource::kInstaller) {
    auto already_installed_it = stage_timeouts_already_installed_.find(stage);
    if (already_installed_it != stage_timeouts_already_installed_.end()) {
      delay = already_installed_it->second;
    }
  }

  stage_timeout_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(&CrostiniRestarter::Timeout,
                     weak_ptr_factory_.GetWeakPtr(), stage));

  for (auto& observer : observer_list_) {
    observer.OnStageStarted(stage);
  }
}

void CrostiniManager::CrostiniRestarter::EmitMetricIfInIncorrectState(
    mojom::InstallerState expected) {
  if (expected != stage_) {
    base::UmaHistogramEnumeration("Crostini.InvalidStateTransition", expected);
  }
}

base::OnceClosure CrostiniManager::CrostiniRestarter::ExtractRequests(
    RequestFilter filter,
    CrostiniResult result) {
  std::vector<CrostiniResultCallback> callbacks;
  for (auto it = requests_.begin(); it != requests_.end();) {
    if (!filter.Run(*it)) {
      it++;
      continue;
    }

    LogRestarterResult(*it, result);

    crostini_manager_->RemoveRestartId(it->restart_id);
    if (it->observer) {
      observer_list_.RemoveObserver(it->observer.get());
    }
    callbacks.push_back(std::move(it->callback));
    it = requests_.erase(it);
  }

  return base::BindOnce(
      [](std::vector<CrostiniResultCallback> callbacks, CrostiniResult result) {
        for (auto& callback : callbacks) {
          std::move(callback).Run(result);
        }
      },
      std::move(callbacks), result);
}

bool CrostiniManager::CrostiniRestarter::ReturnEarlyIfNeeded() {
  if (!requests_.empty()) {
    return false;
  }
  // The result is ignored since there are no requests left.
  FinishRestart(CrostiniResult::UNKNOWN_ERROR);
  return true;
}

void CrostiniManager::CrostiniRestarter::ContinueRestart() {
  is_running_ = true;
  // Skip to the end immediately if testing.
  if (crostini_manager_->skip_restart_for_testing()) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&CrostiniRestarter::StartLxdContainerFinished,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  CrostiniResult::SUCCESS));
    return;
  }

  StartStage(mojom::InstallerState::kInstallImageLoader);
  crostini_manager_->InstallTermina(
      base::BindOnce(&CrostiniRestarter::LoadComponentFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniManager::CrostiniRestarter::LoadComponentFinished(
    CrostiniResult result) {
  if (ReturnEarlyIfNeeded()) {
    return;
  }
  EmitMetricIfInIncorrectState(mojom::InstallerState::kInstallImageLoader);
  if (result != CrostiniResult::SUCCESS) {
    FinishRestart(result);
    return;
  }
  // Set the pref here, after we first successfully install something
  profile_->GetPrefs()->SetBoolean(crostini::prefs::kCrostiniEnabled, true);

  // Ensure concierge is ready to serve requests
  GetConciergeClient()->WaitForServiceToBeAvailable(
      base::BindOnce(&CrostiniManager::CrostiniRestarter::OnConciergeAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniManager::CrostiniRestarter::OnConciergeAvailable(
    bool service_is_available) {
  if (!service_is_available) {
    LOG(ERROR) << "vm_concierge service is not available";
    FinishRestart(CrostiniResult::CONCIERGE_START_FAILED);
    return;
  }

  // Allow concierge to choose an appropriate disk image size.
  int64_t disk_size_bytes = requests_[0].options.disk_size_bytes.value_or(0);
  // If we have an already existing disk, CreateDiskImage will just return its
  // path so we can pass it to StartTerminaVm.
  StartStage(mojom::InstallerState::kCreateDiskImage);
  crostini_manager_->CreateDiskImage(
      container_id_.vm_name,
      vm_tools::concierge::StorageLocation::STORAGE_CRYPTOHOME_ROOT,
      disk_size_bytes,
      base::BindOnce(&CrostiniRestarter::CreateDiskImageFinished,
                     weak_ptr_factory_.GetWeakPtr(), disk_size_bytes));
}

void CrostiniManager::CrostiniRestarter::CreateDiskImageFinished(
    int64_t disk_size_bytes,
    CrostiniResult result,
    const base::FilePath& result_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result == CrostiniResult::CREATE_DISK_IMAGE_ALREADY_EXISTS &&
      is_initial_install_) {
    LOG(WARNING) << "Disk already existed for initial Crostini install. "
                    "Perhaps the VM was created via vmc?";
  } else if (result == CrostiniResult::SUCCESS && !is_initial_install_) {
    LOG(ERROR) << "Disk was created for a restart not tagged as an initial "
                  "Crostini installation.";
  }

  bool success = result == CrostiniResult::SUCCESS ||
                 result == CrostiniResult::CREATE_DISK_IMAGE_ALREADY_EXISTS;
  for (auto& observer : observer_list_) {
    observer.OnDiskImageCreated(success, result, disk_size_bytes);
  }
  if (ReturnEarlyIfNeeded()) {
    return;
  }
  EmitMetricIfInIncorrectState(mojom::InstallerState::kCreateDiskImage);
  if (!success) {
    FinishRestart(result);
    return;
  }
  crostini_manager_->EmitVmDiskTypeMetric(container_id_.vm_name);
  disk_path_ = result_path;

  auto* scheduler_configuration_manager =
      g_browser_process->platform_part()->scheduler_configuration_manager();
  std::optional<std::pair<bool, size_t>> scheduler_configuration =
      scheduler_configuration_manager->GetLastReply();
  if (!scheduler_configuration) {
    // Wait for the configuration to become available.
    LOG(WARNING) << "Scheduler configuration is not yet ready";
    scheduler_configuration_manager_observation_.Observe(
        scheduler_configuration_manager);
    return;
  }
  OnConfigurationSet(scheduler_configuration->first,
                     scheduler_configuration->second);
}

// ash::SchedulerConfigurationManagerBase::Observer:
void CrostiniManager::CrostiniRestarter::OnConfigurationSet(
    bool success,
    size_t num_cores_disabled) {
  if (ReturnEarlyIfNeeded()) {
    return;
  }

  // Note: On non-x86_64 devices, the configuration request to debugd always
  // fails. It is WAI, and to support that case, don't log anything even when
  // |success| is false. |num_cores_disabled| is always set regardless of
  // whether the call is successful.
  scheduler_configuration_manager_observation_.Reset();
  num_cores_disabled_ = num_cores_disabled;

  StartStage(mojom::InstallerState::kStartTerminaVm);
  crostini_manager_->StartTerminaVm(
      container_id_.vm_name, disk_path_, num_cores_disabled_,
      base::BindOnce(&CrostiniRestarter::StartTerminaVmFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniManager::CrostiniRestarter::OnConfigureContainerFinished(
    bool success) {
  if (ReturnEarlyIfNeeded()) {
    return;
  }
  if (!success) {
    // Failed to configure, time to abort.
    FinishRestart(CrostiniResult::CONTAINER_CONFIGURATION_FAILED);
    return;
  }
  FinishRestart(CrostiniResult::SUCCESS);
}

void CrostiniManager::CrostiniRestarter::StartTerminaVmFinished(bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  VLOG(2) << "StartTerminaVmFinished for " << container_id_;
  if (ReturnEarlyIfNeeded()) {
    return;
  }
  EmitMetricIfInIncorrectState(mojom::InstallerState::kStartTerminaVm);
  auto vm_info = crostini_manager_->GetVmInfo(container_id_.vm_name);
  if (!success || !vm_info.has_value()) {
    FinishRestart(CrostiniResult::VM_START_FAILED);
    return;
  }

  // Cache kernel version for enterprise reporting, if it is enabled
  // by policy, and we are in the default Termina case.
  if (profile_->GetPrefs()->GetBoolean(
          crostini::prefs::kReportCrostiniUsageEnabled) &&
      container_id_.vm_name == kCrostiniDefaultVmName) {
    crostini_manager_->UpdateTerminaVmKernelVersion();
  }

  // TODO(timloh): Requests with start_vm_only added too late will miss this and
  // thus fail if any later step fails. Perhaps they should be completed
  // immediately.
  FinishRequests(base::BindRepeating([](const RestartRequest& request) {
                   return request.options.start_vm_only;
                 }),
                 CrostiniResult::SUCCESS);
  if (ReturnEarlyIfNeeded()) {
    return;
  }

  // Share any non-persisted paths for the VM.
  // TODO(timloh): This should probably share paths from all requests. Requests
  // added too late will also miss this.
  guest_os::GuestOsSharePathFactory::GetForProfile(profile_)->SharePaths(
      container_id_.vm_name, vm_info->info.seneschal_server_handle(),
      requests_[0].options.share_paths,
      base::BindOnce(&CrostiniRestarter::SharePathsFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniManager::CrostiniRestarter::SharePathsFinished(
    bool success,
    const std::string& failure_reason) {
  VLOG(2) << "SharePathsFinished for " << container_id_;
  if (!success) {
    LOG(WARNING) << "Failed to share paths: " << failure_reason;
  }
  StartStage(mojom::InstallerState::kStartLxd);
  crostini_manager_->StartLxd(
      container_id_.vm_name,
      base::BindOnce(&CrostiniRestarter::StartLxdFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniManager::CrostiniRestarter::StartLxdFinished(
    CrostiniResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (ReturnEarlyIfNeeded()) {
    return;
  }
  EmitMetricIfInIncorrectState(mojom::InstallerState::kStartLxd);
  if (result != CrostiniResult::SUCCESS) {
    FinishRestart(result);
    return;
  }

  FinishRequests(base::BindRepeating([](const RestartRequest& request) {
                   return request.options.stop_after_lxd_available;
                 }),
                 CrostiniResult::SUCCESS);
  if (ReturnEarlyIfNeeded()) {
    return;
  }

  StartStage(mojom::InstallerState::kCreateContainer);
  crostini_manager_->CreateLxdContainer(
      container_id_, requests_[0].options.image_server_url,
      requests_[0].options.image_alias,
      base::BindOnce(&CrostiniRestarter::CreateLxdContainerFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniManager::CrostiniRestarter::CreateLxdContainerFinished(
    CrostiniResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (ReturnEarlyIfNeeded()) {
    return;
  }
  EmitMetricIfInIncorrectState(mojom::InstallerState::kCreateContainer);
  if (result != CrostiniResult::SUCCESS) {
    FinishRestart(result);
    return;
  }
  StartStage(mojom::InstallerState::kSetupContainer);
  crostini_manager_->SetUpLxdContainerUser(
      container_id_,
      requests_[0].options.container_username.value_or(
          DefaultContainerUserNameForProfile(profile_)),
      base::BindOnce(&CrostiniRestarter::SetUpLxdContainerUserFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniManager::CrostiniRestarter::SetUpLxdContainerUserFinished(
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (ReturnEarlyIfNeeded()) {
    return;
  }
  EmitMetricIfInIncorrectState(mojom::InstallerState::kSetupContainer);
  if (!success) {
    FinishRestart(CrostiniResult::CONTAINER_SETUP_FAILED);
    return;
  }

  StartStage(mojom::InstallerState::kStartContainer);
  crostini_manager_->StartLxdContainer(
      container_id_,
      base::BindOnce(&CrostiniRestarter::StartLxdContainerFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniManager::CrostiniRestarter::FinishRestart(CrostiniResult result) {
  VLOG(2) << "Finishing restart with result: " << CrostiniResultString(result)
          << " for " << container_id_;
  EmitTimeInStageHistogram(base::TimeTicks::Now() - stage_start_, stage_);

  base::OnceClosure closure;
  if (abort_callbacks_.empty()) {
    if (!requests_.empty() && result != CrostiniResult::SUCCESS) {
      LOG(ERROR) << "Failed to restart Crostini with error code: "
                 << static_cast<int>(result)
                 << ", container: " << container_id();
    }
    closure = ExtractRequests(
        base::BindRepeating([](const RestartRequest& request) { return true; }),
        result);
  } else {
    // Requests have already been completed, and new requests are not allowed.
    for (auto& abort_callback : abort_callbacks_) {
      std::move(abort_callback).Run();
    }
    abort_callbacks_.clear();
    closure = base::DoNothing();
  }

  DCHECK(requests_.empty());
  DCHECK(observer_list_.empty());

  // CrostiniManager::RestartCompleted deletes |this|
  crostini_manager_->RestartCompleted(this, std::move(closure));
}

void CrostiniManager::CrostiniRestarter::MaybeCancelCurrentOperation() {
  if (stage_ == mojom::InstallerState::kInstallImageLoader) {
    // Currently this is the only step that can be "cancelled". The relevant
    // completion callback, LoadComponentFinished(), is still called.
    crostini_manager_->CancelInstallTermina();
  }
}

void CrostiniManager::CrostiniRestarter::LogRestarterResult(
    const RestartRequest& request,
    CrostiniResult result) {
  // Log different histograms depending on the restart source. For an initial
  // install, only log for the first request. The Crostini installer also has
  // separate histograms in Crostini.SetupResult.
  switch (request.options.restart_source) {
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case RestartSource::kOther:
      if (is_initial_install_) {
        return;
      }
      base::UmaHistogramEnumeration("Crostini.RestarterResult", result);
      return;
    case RestartSource::kInstaller:
      if (!is_initial_install_) {
        LOG(WARNING)
            << "Restart request from Crostini installer was not first request.";
      }
      base::UmaHistogramEnumeration("Crostini.RestarterResult.Installer",
                                    result);
      return;
    case RestartSource::kMultiContainerCreation:
      if (!is_initial_install_) {
        LOG(WARNING) << "Restart request for multi-container creation was not "
                        "first request.";
      }
      base::UmaHistogramEnumeration(
          "Crostini.RestarterResult.MultiContainerCreation", result);
      return;
  }
}

// Unit tests need these to be initialized to sensible values. In Browser tests
// and real life, they are updated via MaybeUpdateCrostini.
bool CrostiniManager::is_dev_kvm_present_ = true;
bool CrostiniManager::is_vm_launch_allowed_ = true;

void CrostiniManager::UpdateVmState(std::string vm_name, VmState vm_state) {
  auto vm_info = running_vms_.find(vm_name);
  if (vm_info != running_vms_.end()) {
    vm_info->second.state = vm_state;
    return;
  }
  // This can happen normally when StopVm is called right after start up.
  LOG(WARNING) << "Attempted to set state for unknown vm: " << vm_name;
}

bool CrostiniManager::IsVmRunning(std::string vm_name) {
  auto vm_info = running_vms_.find(std::move(vm_name));
  if (vm_info != running_vms_.end()) {
    return vm_info->second.state == VmState::STARTED;
  }
  return false;
}

std::optional<VmInfo> CrostiniManager::GetVmInfo(std::string vm_name) {
  auto it = running_vms_.find(std::move(vm_name));
  if (it != running_vms_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void CrostiniManager::AddRunningVmForTesting(std::string vm_name,
                                             uint32_t cid) {
  guest_os::GuestId id(guest_os::VmType::TERMINA, vm_name, "");
  guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile_)
      ->AddGuestForTesting(  // IN-TEST
          id, guest_os::GuestInfo{id, cid, {}, {}, {}, {}});
  running_vms_[std::move(vm_name)] = VmInfo{VmState::STARTED};
}

void CrostiniManager::AddStoppingVmForTesting(std::string vm_name) {
  running_vms_[std::move(vm_name)] = VmInfo{VmState::STOPPING};
}

namespace {

ContainerOsVersion VersionFromOsRelease(
    const vm_tools::cicerone::OsRelease& os_release) {
  if (os_release.id() == "debian") {
    if (os_release.version_id() == "9") {
      return ContainerOsVersion::kDebianStretch;
    } else if (os_release.version_id() == "10") {
      return ContainerOsVersion::kDebianBuster;
    } else if (os_release.version_id() == "11") {
      return ContainerOsVersion::kDebianBullseye;
    } else if (os_release.version_id() == "12") {
      return ContainerOsVersion::kDebianBookworm;
    } else {
      return ContainerOsVersion::kDebianOther;
    }
  }
  return ContainerOsVersion::kOtherOs;
}

bool IsUpgradableContainerVersion(ContainerOsVersion version) {
  return version == ContainerOsVersion::kDebianStretch ||
         version == ContainerOsVersion::kDebianBuster ||
         version == ContainerOsVersion::kDebianBullseye;
}

}  // namespace

void CrostiniManager::SetContainerOsRelease(
    const guest_os::GuestId& container_id,
    const vm_tools::cicerone::OsRelease& os_release) {
  ContainerOsVersion version = VersionFromOsRelease(os_release);
  // Store the os release version in prefs. We can use this value to decide if
  // an upgrade can be offered.
  UpdateContainerPref(profile_, container_id,
                      guest_os::prefs::kContainerOsVersionKey,
                      base::Value(static_cast<int>(version)));
  UpdateContainerPref(profile_, container_id,
                      guest_os::prefs::kContainerOsPrettyNameKey,
                      base::Value(os_release.pretty_name()));

  std::optional<ContainerOsVersion> old_version;
  auto it = container_os_releases_.find(container_id);
  if (it != container_os_releases_.end()) {
    old_version = VersionFromOsRelease(it->second);
  }

  VLOG(1) << container_id;
  VLOG(1) << "os_release.pretty_name " << os_release.pretty_name();
  VLOG(1) << "os_release.name " << os_release.name();
  VLOG(1) << "os_release.version " << os_release.version();
  VLOG(1) << "os_release.version_id " << os_release.version_id();
  VLOG(1) << "os_release.id " << os_release.id();
  container_os_releases_[container_id] = os_release;
  if (!old_version || *old_version != version) {
    for (auto& observer : crostini_container_properties_observers_) {
      observer.OnContainerOsReleaseChanged(
          container_id, IsUpgradableContainerVersion(version));
    }
  }
  base::UmaHistogramEnumeration("Crostini.ContainerOsVersion", version);
}

void CrostiniManager::ConfigureForArcSideload() {
  ash::SessionManagerClient* session_manager_client =
      ash::SessionManagerClient::Get();
  if (!base::FeatureList::IsEnabled(features::kCrostiniArcSideload) ||
      !session_manager_client) {
    return;
  }
  session_manager_client->QueryAdbSideload(base::BindOnce(
      // We use a lambda to keep the arc sideloading implementation local, and
      // avoid header pollution. This means we have to manually check the weak
      // pointer is alive.
      [](base::WeakPtr<CrostiniManager> manager,
         ash::SessionManagerClient::AdbSideloadResponseCode response_code,
         bool is_allowed) {
        if (!manager || !is_allowed ||
            response_code !=
                ash::SessionManagerClient::AdbSideloadResponseCode::SUCCESS) {
          return;
        }
        vm_tools::cicerone::ConfigureForArcSideloadRequest request;
        request.set_owner_id(manager->owner_id_);
        request.set_vm_name(kCrostiniDefaultVmName);
        request.set_container_name(kCrostiniDefaultContainerName);
        GetCiceroneClient()->ConfigureForArcSideload(
            request,
            base::BindOnce(
                [](std::optional<
                    vm_tools::cicerone::ConfigureForArcSideloadResponse>
                       response) {
                  if (!response) {
                    LOG(ERROR) << "Failed to configure for arc sideloading: no "
                                  "response from vm";
                    return;
                  }
                  if (response->status() ==
                      vm_tools::cicerone::ConfigureForArcSideloadResponse::
                          SUCCEEDED) {
                    return;
                  }
                  LOG(ERROR) << "Failed to configure for arc sideloading: "
                             << response->failure_reason();
                }));
      },
      weak_ptr_factory_.GetWeakPtr()));
}

const vm_tools::cicerone::OsRelease* CrostiniManager::GetContainerOsRelease(
    const guest_os::GuestId& container_id) const {
  auto it = container_os_releases_.find(container_id);
  if (it != container_os_releases_.end()) {
    return &it->second;
  }
  return nullptr;
}

bool CrostiniManager::IsContainerUpgradeable(
    const guest_os::GuestId& container_id) const {
  ContainerOsVersion version = ContainerOsVersion::kUnknown;
  const auto* os_release = GetContainerOsRelease(container_id);
  if (os_release) {
    version = VersionFromOsRelease(*os_release);
  } else {
    // Check prefs instead.
    const base::Value* value = GetContainerPrefValue(
        profile_, container_id, guest_os::prefs::kContainerOsVersionKey);
    if (value) {
      version = static_cast<ContainerOsVersion>(value->GetInt());
    }
  }
  return IsUpgradableContainerVersion(version);
}

bool CrostiniManager::ShouldPromptContainerUpgrade(
    const guest_os::GuestId& container_id) const {
  if (!CrostiniFeatures::Get()->IsContainerUpgradeUIAllowed(profile_)) {
    return false;
  }
  if (container_upgrade_prompt_shown_.count(container_id) != 0) {
    // Already shown the upgrade dialog.
    return false;
  }
  if (container_id != DefaultContainerId()) {
    return false;
  }
  bool upgradable = IsContainerUpgradeable(container_id);
  return upgradable;
}

void CrostiniManager::UpgradePromptShown(
    const guest_os::GuestId& container_id) {
  container_upgrade_prompt_shown_.insert(container_id);
}

bool CrostiniManager::IsUncleanStartup() const {
  return is_unclean_startup_;
}

void CrostiniManager::SetUncleanStartupForTesting(bool is_unclean_startup) {
  is_unclean_startup_ = is_unclean_startup;
}

void CrostiniManager::AddRunningContainerForTesting(std::string vm_name,
                                                    ContainerInfo info,
                                                    bool notify) {
  auto* tracker =
      guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile_);
  std::optional<guest_os::GuestInfo> vm_info = tracker->GetInfo(
      guest_os::GuestId{guest_os::VmType::TERMINA, vm_name, ""});
  CHECK(vm_info);
  guest_os::GuestId id{guest_os::VmType::TERMINA, vm_name, info.name};
  guest_os::GuestInfo guest_info{
      id,           vm_info->cid,      info.username,
      info.homedir, info.ipv4_address, info.sftp_vsock_port};
  tracker->AddGuestForTesting(id, guest_info, notify);  // IN-TEST
}

void CrostiniManager::UpdateLaunchMetricsForEnterpriseReporting() {
  PrefService* const profile_prefs = profile_->GetPrefs();
  const component_updater::ComponentUpdateService* const update_service =
      g_browser_process->component_updater();
  const base::Clock* const clock = base::DefaultClock::GetInstance();
  WriteMetricsForReportingToPrefsIfEnabled(profile_prefs, update_service,
                                           clock);
}

CrostiniManager* CrostiniManager::GetForProfile(Profile* profile) {
  return CrostiniManagerFactory::GetForProfile(profile);
}

CrostiniManager::CrostiniManager(Profile* profile)
    : profile_(profile), owner_id_(CryptohomeIdForProfile(profile)) {
  DCHECK(!profile_->IsOffTheRecord());
  GetCiceroneClient()->AddObserver(this);
  GetConciergeClient()->AddVmObserver(this);
  GetConciergeClient()->AddDiskImageObserver(this);
  if (ash::AnomalyDetectorClient::Get()) {  // May be null in tests.
    ash::AnomalyDetectorClient::Get()->AddObserver(this);
  }
  if (ash::NetworkHandler::IsInitialized()) {
    network_state_handler_observer_.Observe(
        ash::NetworkHandler::Get()->network_state_handler());
  }
  if (chromeos::PowerManagerClient::Get()) {
    chromeos::PowerManagerClient::Get()->AddObserver(this);
  }
  CrostiniThrottleFactory::GetForBrowserContext(profile_);
  guest_os_stability_monitor_ =
      std::make_unique<guest_os::GuestOsStabilityMonitor>(
          kCrostiniStabilityHistogram);
  low_disk_notifier_ = std::make_unique<CrostiniLowDiskNotification>();
  crostini_sshfs_ = std::make_unique<CrostiniSshfs>(profile_);

  // It's possible for us to have containers in prefs while Crostini isn't
  // enabled, for example, maybe policy changed and now Crostini isn't allowed
  // any more. We still need to call RegisterContainer to update the terminal
  // prefs. Note: This means changes only take effect after a restart,
  // which is fine, since e.g. force-quitting a running VM because policy
  // changed isn't something we're going to do.
  for (const auto& container :
       guest_os::GetContainers(profile_, kCrostiniDefaultVmType)) {
    // For a short while in M106 Bruschetta was getting added to prefs without
    // a VM type, which meant it defaulted to Termina. If we've got it in
    // prefs remove it instead of registering it. This'll break anyone who
    // really has a vm named "bru" which is unfortunate, but we can't tell the
    // difference between a correct and incorrect pref, so hopefully no one's
    // done so.
    // TODO(b/241043433): This code can be removed after M118.
    if (container.vm_name == bruschetta::kBruschettaVmName) {
      guest_os::RemoveContainerFromPrefs(profile, container);
      continue;
    }

    if (crostini::CrostiniFeatures::Get()->IsEnabled(profile_)) {
      RegisterContainer(container);
    } else {
      RegisterContainerTerminal(container);
    }
  }
}

CrostiniManager::~CrostiniManager() {
  RemoveDBusObservers();
}

base::WeakPtr<CrostiniManager> CrostiniManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void CrostiniManager::RemoveDBusObservers() {
  if (dbus_observers_removed_) {
    return;
  }
  dbus_observers_removed_ = true;
  GetCiceroneClient()->RemoveObserver(this);
  if (ash::AnomalyDetectorClient::Get()) {  // May be null in tests.
    ash::AnomalyDetectorClient::Get()->RemoveObserver(this);
  }
  if (chromeos::PowerManagerClient::Get()) {
    chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  }
  // GuestOsStabilityMonitor and LowDiskNotifier need to be destructed here so
  // they can unregister from DBus clients that may no longer exist later.
  guest_os_stability_monitor_.reset();
  low_disk_notifier_.reset();
}

// static
bool CrostiniManager::IsDevKvmPresent() {
  return is_dev_kvm_present_;
}

// static
bool CrostiniManager::IsVmLaunchAllowed() {
  return is_vm_launch_allowed_;
}

void CrostiniManager::MaybeUpdateCrostini() {
  // This is a new user session, perhaps using an old CrostiniManager.
  container_upgrade_prompt_shown_.clear();
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CrostiniManager::CheckPaths),
      base::BindOnce(&CrostiniManager::CheckConciergeAvailable,
                     weak_ptr_factory_.GetWeakPtr()));

  // Probe Concierge - if it's still running after an unclean shutdown, a
  // success response will be received.
  vm_tools::concierge::GetVmInfoRequest concierge_request;
  concierge_request.set_owner_id(owner_id_);
  concierge_request.set_name(kCrostiniDefaultVmName);
  GetConciergeClient()->GetVmInfo(
      std::move(concierge_request),
      base::BindOnce(
          [](base::WeakPtr<CrostiniManager> weak_this,
             std::optional<vm_tools::concierge::GetVmInfoResponse> reply) {
            if (weak_this) {
              weak_this->is_unclean_startup_ =
                  reply.has_value() && reply->success();
              if (weak_this->is_unclean_startup_) {
                weak_this->RemoveUncleanSshfsMounts();
              }
            }
          },
          weak_ptr_factory_.GetWeakPtr()));
}

// static
void CrostiniManager::CheckPaths() {
  is_dev_kvm_present_ = base::PathExists(base::FilePath("/dev/kvm"));
}

void CrostiniManager::CheckConciergeAvailable() {
  GetConciergeClient()->WaitForServiceToBeAvailable(base::BindOnce(
      &CrostiniManager::CheckVmLaunchAllowed, weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniManager::CheckVmLaunchAllowed(bool service_is_available) {
  if (service_is_available) {
    vm_tools::concierge::GetVmLaunchAllowedRequest request;
    GetConciergeClient()->GetVmLaunchAllowed(
        std::move(request),
        base::BindOnce(&CrostiniManager::OnCheckVmLaunchAllowed,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  LOG(ERROR)
      << "Couldn't contact concierge to check if untrusted VMs are allowed";
  MaybeUpdateCrostiniAfterChecks();
}

void CrostiniManager::OnCheckVmLaunchAllowed(
    std::optional<vm_tools::concierge::GetVmLaunchAllowedResponse> response) {
  // is_vm_launch_allowed_ should be set before CrostiniFeatures is used,
  // otherwise a (possibly incorrect) default value is read.
  if (!response) {
    // Didn't get a reply - assume that VM launch is allowed.
    if (base::SysInfo::IsRunningOnChromeOS()) {
      LOG(ERROR) << "Failed to determine if VM launch is allowed";
    }
  } else {
    is_vm_launch_allowed_ = response->allowed();
    LOG_IF(WARNING, !is_vm_launch_allowed_)
        << "VM launch not allowed: " << response->reason();
  }

  MaybeUpdateCrostiniAfterChecks();
}

void CrostiniManager::MaybeUpdateCrostiniAfterChecks() {
  if (!CrostiniFeatures::Get()->IsEnabled(profile_)) {
    return;
  }
  if (!CrostiniFeatures::Get()->IsAllowedNow(profile_)) {
    return;
  }
  if (ShouldPromptContainerUpgrade(DefaultContainerId())) {
    upgrade_available_notification_ =
        CrostiniUpgradeAvailableNotification::Show(profile_, base::DoNothing());
  }
}

void CrostiniManager::InstallTermina(CrostiniResultCallback callback) {
  if (install_termina_never_completes_for_testing_) {
    LOG(ERROR)
        << "Dropping InstallTermina request. This is only used in tests.";
    return;
  }
  termina_installer_.Install(base::BindOnce(
      [](CrostiniResultCallback callback,
         TerminaInstaller::InstallResult result) {
        CrostiniResult res;
        if (result == TerminaInstaller::InstallResult::Success) {
          res = CrostiniResult::SUCCESS;
        } else if (result == TerminaInstaller::InstallResult::Offline) {
          LOG(ERROR) << "Installing Termina failed: offline";
          res = CrostiniResult::OFFLINE_WHEN_UPGRADE_REQUIRED;
        } else if (result == TerminaInstaller::InstallResult::Failure) {
          LOG(ERROR) << "Installing Termina failed";
          res = CrostiniResult::LOAD_COMPONENT_FAILED;
        } else if (result == TerminaInstaller::InstallResult::NeedUpdate) {
          LOG(ERROR) << "Installing Termina failed: need update";
          res = CrostiniResult::NEED_UPDATE;
        } else if (result == TerminaInstaller::InstallResult::Cancelled) {
          LOG(ERROR) << "Installing Termina failed: cancelled";
          res = CrostiniResult::INSTALL_TERMINA_CANCELLED;
        } else {
          CHECK(false)
              << "Got unexpected value of TerminaInstaller::InstallResult";
          res = CrostiniResult::LOAD_COMPONENT_FAILED;
        }
        std::move(callback).Run(res);
      },
      std::move(callback)));
}

void CrostiniManager::CancelInstallTermina() {
  termina_installer_.CancelInstall();
}

void CrostiniManager::UninstallTermina(BoolCallback callback) {
  termina_installer_.Uninstall(std::move(callback));
}

void CrostiniManager::CreateDiskImage(
    const std::string& vm_name,
    vm_tools::concierge::StorageLocation storage_location,
    int64_t disk_size_bytes,
    CreateDiskImageCallback callback) {
  if (vm_name.empty()) {
    LOG(ERROR) << "VM name must not be empty";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR, base::FilePath());
    return;
  }

  vm_tools::concierge::CreateDiskImageRequest request;
  request.set_cryptohome_id(CryptohomeIdForProfile(profile_));
  request.set_vm_name(std::move(vm_name));
  // The type of disk image to be created.
  request.set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);

  if (storage_location != vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT) {
    LOG(ERROR) << "'" << storage_location
               << "' is not a valid storage location";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR, base::FilePath());
    return;
  }
  request.set_storage_location(storage_location);
  // The logical size of the new disk image, in bytes.
  request.set_disk_size(std::move(disk_size_bytes));

  GetConciergeClient()->CreateDiskImage(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnCreateDiskImage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::StartTerminaVm(std::string name,
                                     const base::FilePath& disk_path,
                                     size_t num_cores_disabled,
                                     BoolCallback callback) {
  if (name.empty()) {
    LOG(ERROR) << "name is required";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  std::string disk_path_string = disk_path.AsUTF8Unsafe();
  if (disk_path_string.empty()) {
    LOG(ERROR) << "Disk path cannot be empty";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  auto* anomaly_detector_client = ash::AnomalyDetectorClient::Get();
  if (anomaly_detector_client &&
      !anomaly_detector_client->IsGuestFileCorruptionSignalConnected()) {
    LOG(ERROR) << "GuestFileCorruptionSignal not connected, will not be "
                  "able to detect file system corruption.";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  for (auto& observer : vm_starting_observers_) {
    observer.OnVmStarting();
  }

  vm_tools::concierge::StartVmRequest request;
  std::optional<std::string> dlc_id = termina_installer_.GetDlcId();
  if (dlc_id.has_value()) {
    request.mutable_vm()->set_dlc_id(*dlc_id);
  }
  request.set_name(std::move(name));
  request.set_start_termina(true);
  request.set_owner_id(owner_id_);
  request.set_timeout(static_cast<uint32_t>(kStartVmTimeout.InSeconds()));
  if (base::FeatureList::IsEnabled(ash::features::kCrostiniGpuSupport)) {
    request.set_enable_gpu(true);
  }
  if (profile_->GetPrefs()->GetBoolean(prefs::kCrostiniMicAllowed) &&
      profile_->GetPrefs()->GetBoolean(::prefs::kAudioCaptureAllowed)) {
    request.set_enable_audio_capture(true);
  }
  const int32_t cpus = base::SysInfo::NumberOfProcessors() - num_cores_disabled;
  DCHECK_LT(0, cpus);
  request.set_cpus(cpus);

  vm_tools::concierge::DiskImage* disk_image = request.add_disks();
  disk_image->set_path(std::move(disk_path_string));
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_writable(true);
  disk_image->set_do_mount(false);

  GetConciergeClient()->StartVm(
      request, base::BindOnce(&CrostiniManager::OnStartTerminaVm,
                              weak_ptr_factory_.GetWeakPtr(), request.name(),
                              std::move(callback)));
}

void CrostiniManager::StopVm(std::string name,
                             CrostiniResultCallback callback) {
  if (name.empty()) {
    LOG(ERROR) << "name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }

  UpdateVmState(name, VmState::STOPPING);

  vm_tools::concierge::StopVmRequest request;
  request.set_owner_id(owner_id_);
  request.set_name(name);

  GetConciergeClient()->StopVm(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnStopVm, weak_ptr_factory_.GetWeakPtr(),
                     std::move(name), std::move(callback)));
}

void CrostiniManager::StopRunningVms(CrostiniResultCallback callback) {
  std::vector<std::string> names;
  LOG(WARNING) << "StopRunningVms";
  for (const auto& it : running_vms_) {
    if (it.second.state != VmState::STOPPING) {
      names.push_back(it.first);
    }
  }
  auto barrier = base::BarrierCallback<CrostiniResult>(
      names.size(), base::BindOnce(
                        [](CrostiniResultCallback callback,
                           std::vector<CrostiniResult> results) {
                          auto result = CrostiniResult::SUCCESS;
                          for (auto res : results) {
                            if (res != CrostiniResult::SUCCESS) {
                              LOG(ERROR) << "StopVm failure code "
                                         << static_cast<int>(res);
                              result = res;
                              break;
                            }
                          }
                          std::move(callback).Run(result);
                        },
                        std::move(callback)));
  for (const auto& name : names) {
    VLOG(1) << "Stopping vm " << name;
    StopVm(name, barrier);
  }
}

void CrostiniManager::UpdateTerminaVmKernelVersion() {
  vm_tools::concierge::GetVmEnterpriseReportingInfoRequest request;
  request.set_vm_name(kCrostiniDefaultVmName);
  request.set_owner_id(owner_id_);
  GetConciergeClient()->GetVmEnterpriseReportingInfo(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnGetTerminaVmKernelVersion,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniManager::StartLxd(std::string vm_name,
                               CrostiniResultCallback callback) {
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (!GetCiceroneClient()->IsStartLxdProgressSignalConnected()) {
    LOG(ERROR) << "Async call to StartLxd can't complete when signals "
                  "are not connected.";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  vm_tools::cicerone::StartLxdRequest request;
  request.set_vm_name(std::move(vm_name));
  request.set_owner_id(owner_id_);
  request.set_reset_lxd_db(
      base::FeatureList::IsEnabled(ash::features::kCrostiniResetLxdDb));
  GetCiceroneClient()->StartLxd(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnStartLxd,
                     weak_ptr_factory_.GetWeakPtr(), request.vm_name(),
                     std::move(callback)));
}

namespace {

std::string GetImageServer() {
  std::string image_server_url;
  scoped_refptr<component_updater::ComponentManagerAsh> component_manager =
      g_browser_process->platform_part()->component_manager_ash();
  if (component_manager) {
    image_server_url =
        component_manager->GetCompatiblePath("cros-crostini-image-server-url")
            .value();
  }
  return image_server_url.empty() ? kCrostiniDefaultImageServerUrl
                                  : image_server_url;
}

std::string GetImageAlias() {
  std::string debian_version;
  auto* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(kCrostiniContainerFlag)) {
    debian_version = cmdline->GetSwitchValueASCII(kCrostiniContainerFlag);
  } else {
    debian_version = kCrostiniContainerDefaultVersion;
  }
  return base::StringPrintf(kCrostiniImageAliasPattern, debian_version.c_str());
}

}  // namespace

void CrostiniManager::CreateLxdContainer(
    guest_os::GuestId container_id,
    std::optional<std::string> opt_image_server_url,
    std::optional<std::string> opt_image_alias,
    CrostiniResultCallback callback) {
  if (container_id.vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (container_id.container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (!GetCiceroneClient()->IsLxdContainerCreatedSignalConnected() ||
      !GetCiceroneClient()->IsLxdContainerDownloadingSignalConnected()) {
    LOG(ERROR)
        << "Async call to CreateLxdContainer can't complete when signals "
           "are not connected.";
    std::move(callback).Run(CrostiniResult::SIGNAL_NOT_CONNECTED);
    return;
  }
  vm_tools::cicerone::CreateLxdContainerRequest request;
  request.set_vm_name(container_id.vm_name);
  request.set_container_name(container_id.container_name);
  request.set_owner_id(owner_id_);
  request.set_image_server(opt_image_server_url.value_or(GetImageServer()));
  request.set_image_alias(opt_image_alias.value_or(GetImageAlias()));

  VLOG(1) << "image_server_url = " << request.image_server()
          << ", image_alias = " << request.image_alias();

  GetCiceroneClient()->CreateLxdContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnCreateLxdContainer,
                     weak_ptr_factory_.GetWeakPtr(), std::move(container_id),
                     std::move(callback)));
}

void CrostiniManager::DeleteLxdContainer(guest_os::GuestId container_id,
                                         BoolCallback callback) {
  if (container_id.vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  if (container_id.container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  if (!GetCiceroneClient()->IsLxdContainerDeletedSignalConnected()) {
    LOG(ERROR)
        << "Async call to DeleteLxdContainer can't complete when signals "
           "are not connected.";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  vm_tools::cicerone::DeleteLxdContainerRequest request;
  request.set_vm_name(container_id.vm_name);
  request.set_container_name(container_id.container_name);
  request.set_owner_id(owner_id_);
  GetCiceroneClient()->DeleteLxdContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnDeleteLxdContainer,
                     weak_ptr_factory_.GetWeakPtr(), std::move(container_id),
                     std::move(callback)));
}

void CrostiniManager::OnDeleteLxdContainer(
    const guest_os::GuestId& container_id,
    BoolCallback callback,
    std::optional<vm_tools::cicerone::DeleteLxdContainerResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to delete lxd container in vm. Empty response.";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  if (response->status() ==
      vm_tools::cicerone::DeleteLxdContainerResponse::DELETING) {
    VLOG(1) << "Awaiting LxdContainerDeletedSignal for " << container_id;
    delete_lxd_container_callbacks_.emplace(container_id, std::move(callback));

  } else if (response->status() ==
             vm_tools::cicerone::DeleteLxdContainerResponse::DOES_NOT_EXIST) {
    RemoveLxdContainerFromPrefs(profile_, container_id);
    UnregisterContainer(container_id);
    std::move(callback).Run(/*success=*/true);

  } else {
    LOG(ERROR) << "Failed to delete container: " << response->failure_reason();
    std::move(callback).Run(/*success=*/false);
  }
}

void CrostiniManager::StartLxdContainer(guest_os::GuestId container_id,
                                        CrostiniResultCallback callback) {
  if (container_id.vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (container_id.container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (!GetCiceroneClient()->IsContainerStartedSignalConnected() ||
      !GetCiceroneClient()->IsContainerShutdownSignalConnected() ||
      !GetCiceroneClient()->IsLxdContainerStartingSignalConnected()) {
    LOG(ERROR) << "Async call to StartLxdContainer can't complete when signals "
                  "are not connected.";
    std::move(callback).Run(CrostiniResult::SIGNAL_NOT_CONNECTED);
    return;
  }
  vm_tools::cicerone::StartLxdContainerRequest request;
  request.set_vm_name(container_id.vm_name);
  request.set_container_name(container_id.container_name);
  request.set_owner_id(owner_id_);
  if (auto* integration_service =
          drive::DriveIntegrationServiceFactory::GetForProfile(profile_)) {
    request.set_drivefs_mount_path(
        integration_service->GetMountPointPath().value());
  }
  GetCiceroneClient()->StartLxdContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnStartLxdContainer,
                     weak_ptr_factory_.GetWeakPtr(), std::move(container_id),
                     std::move(callback)));
}

void CrostiniManager::StopLxdContainer(guest_os::GuestId container_id,
                                       CrostiniResultCallback callback) {
  if (container_id.vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (container_id.container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  vm_tools::cicerone::StopLxdContainerRequest request;
  request.set_vm_name(container_id.vm_name);
  request.set_container_name(container_id.container_name);
  request.set_owner_id(owner_id_);
  GetCiceroneClient()->StopLxdContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnStopLxdContainer,
                     weak_ptr_factory_.GetWeakPtr(), std::move(container_id),
                     std::move(callback)));
}

void CrostiniManager::SetUpLxdContainerUser(guest_os::GuestId container_id,
                                            std::string container_username,
                                            BoolCallback callback) {
  if (container_id.vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  if (container_id.container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  if (container_username.empty()) {
    LOG(ERROR) << "container_username is required";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  vm_tools::cicerone::SetUpLxdContainerUserRequest request;
  request.set_vm_name(container_id.vm_name);
  request.set_container_name(container_id.container_name);
  request.set_owner_id(owner_id_);
  request.set_container_username(std::move(container_username));
  GetCiceroneClient()->SetUpLxdContainerUser(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnSetUpLxdContainerUser,
                     weak_ptr_factory_.GetWeakPtr(), std::move(container_id),
                     std::move(callback)));
}

void CrostiniManager::ExportDiskImage(guest_os::GuestId vm_id,
                                      std::string user_id_hash,
                                      base::FilePath export_path,
                                      bool force,
                                      CrostiniResultCallback callback) {
  if (vm_id.vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (user_id_hash.empty()) {
    LOG(ERROR) << "user_id_hash is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }

  if (disk_image_callbacks_.find(vm_id) != disk_image_callbacks_.end()) {
    LOG(ERROR) << "Disk image operation currently running for " << vm_id;
    std::move(callback).Run(CrostiniResult::DISK_IMAGE_FAILED);
  }
  disk_image_callbacks_.emplace(vm_id, std::move(callback));

  vm_tools::concierge::ExportDiskImageRequest request;
  request.set_vm_name(vm_id.vm_name);
  request.set_cryptohome_id(user_id_hash);
  // Digest file is only used for pluginVM which will be deprecated soon.
  request.set_generate_sha256_digest(false);
  request.set_force(force);

  std::vector<base::ScopedFD> fds;
  base::File file(export_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open " << export_path;
    return;
  }

  fds.emplace_back(file.TakePlatformFile());

  GetConciergeClient()->ExportDiskImage(
      std::move(fds), std::move(request),
      base::BindOnce(&CrostiniManager::OnExportDiskImage,
                     weak_ptr_factory_.GetWeakPtr(), vm_id));
}

void CrostiniManager::OnExportDiskImage(
    guest_os::GuestId vm_id,
    std::optional<vm_tools::concierge::ExportDiskImageResponse> response) {
  auto it = disk_image_callbacks_.find(vm_id);
  if (it == disk_image_callbacks_.end()) {
    LOG(ERROR) << "No export callback for " << vm_id;
    return;
  }

  if (!response) {
    LOG(ERROR) << "Failed to export disk image. Empty response.";
    std::move(it->second).Run(CrostiniResult::DISK_IMAGE_FAILED);
    disk_image_callbacks_.erase(it);
    return;
  }

  // If export has started, the callback will be invoked when the
  // DiskImageProgressSignal signal indicates that export is
  // complete, otherwise this is an error.
  if (response->status() != vm_tools::concierge::DISK_STATUS_IN_PROGRESS) {
    LOG(ERROR) << "Failed to export disk image: status=" << response->status()
               << ", failure_reason=" << response->failure_reason();
    std::move(it->second).Run(CrostiniResult::DISK_IMAGE_FAILED);
    disk_image_callbacks_.erase(it);
  }

  disk_image_uuid_to_guest_id_.emplace(response->command_uuid(), vm_id);
}

void CrostiniManager::ImportDiskImage(guest_os::GuestId vm_id,
                                      std::string user_id_hash,
                                      base::FilePath import_path,
                                      CrostiniResultCallback callback) {
  if (vm_id.vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (user_id_hash.empty()) {
    LOG(ERROR) << "user_id_hash is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }

  if (disk_image_callbacks_.find(vm_id) != disk_image_callbacks_.end()) {
    LOG(ERROR) << "Disk image operation currently running for " << vm_id;
    std::move(callback).Run(CrostiniResult::DISK_IMAGE_FAILED);
  }
  disk_image_callbacks_.emplace(vm_id, std::move(callback));

  base::File file(import_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open " << import_path;
    return;
  }

  vm_tools::concierge::ImportDiskImageRequest request;
  request.set_vm_name(vm_id.vm_name);
  request.set_cryptohome_id(user_id_hash);
  // All vm's are stored in root except pluginvm, which is not supported in this
  // flow.
  request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
  request.set_source_size(file.GetLength());

  GetConciergeClient()->ImportDiskImage(
      base::ScopedFD(file.TakePlatformFile()), std::move(request),
      base::BindOnce(&CrostiniManager::OnImportDiskImage,
                     weak_ptr_factory_.GetWeakPtr(), vm_id));
}

void CrostiniManager::OnImportDiskImage(
    guest_os::GuestId vm_id,
    std::optional<vm_tools::concierge::ImportDiskImageResponse> response) {
  auto it = disk_image_callbacks_.find(vm_id);
  if (it == disk_image_callbacks_.end()) {
    LOG(ERROR) << "No import callback for " << vm_id;
    return;
  }

  if (!response) {
    LOG(ERROR) << "Failed to import disk image. Empty response.";
    std::move(it->second).Run(CrostiniResult::DISK_IMAGE_FAILED);
    disk_image_callbacks_.erase(it);
    return;
  }

  // If import has started, the callback will be invoked when the
  // DiskImageProgressSignal signal indicates that import is
  // complete, otherwise this is an error.
  if (response->status() != vm_tools::concierge::DISK_STATUS_IN_PROGRESS) {
    LOG(ERROR) << "Failed to import image: status=" << response->status()
               << ", failure_reason=" << response->failure_reason();
    std::move(it->second).Run(CrostiniResult::DISK_IMAGE_FAILED);
    disk_image_callbacks_.erase(it);
  }

  disk_image_uuid_to_guest_id_.emplace(response->command_uuid(), vm_id);
}

void CrostiniManager::OnDiskImageProgress(
    const vm_tools::concierge::DiskImageStatusResponse& signal) {
  bool call_observers = false;
  bool call_original_callback = false;
  CrostiniResult result;
  DiskImageProgressStatus status;
  switch (signal.status()) {
    case vm_tools::concierge::DISK_STATUS_IN_PROGRESS:
      status = DiskImageProgressStatus::IN_PROGRESS;
      call_observers = true;
      break;
    case vm_tools::concierge::DISK_STATUS_NOT_ENOUGH_SPACE:
      status = DiskImageProgressStatus::FAILURE_SPACE;
      result = CrostiniResult::DISK_IMAGE_FAILED_NO_SPACE;
      call_observers = true;
      call_original_callback = true;
      break;
    case vm_tools::concierge::DISK_STATUS_CREATED:
      call_original_callback = true;
      result = CrostiniResult::SUCCESS;
      break;
    default:
      call_original_callback = true;
      result = CrostiniResult::DISK_IMAGE_FAILED;
      LOG(ERROR) << "Failed during disk image export: " << signal.status()
                 << ", " << signal.failure_reason();
  }

  auto uuid_it = disk_image_uuid_to_guest_id_.find(signal.command_uuid());
  if (uuid_it == disk_image_uuid_to_guest_id_.end()) {
    LOG(ERROR) << "No GuestId mapping for command uuid: "
               << signal.command_uuid();
    return;
  }

  if (call_observers) {
    for (auto& observer : disk_image_progress_observers_) {
      observer.OnDiskImageProgress(uuid_it->second, status, signal.progress());
    }
  }

  if (call_original_callback) {
    auto it = disk_image_callbacks_.find(uuid_it->second);
    if (it == disk_image_callbacks_.end()) {
      LOG(ERROR) << "No export callbacks for " << uuid_it->second;
    }
    std::move(it->second).Run(result);
    // The callback and its uuid mapping are done now, remove
    disk_image_callbacks_.erase(it);
    disk_image_uuid_to_guest_id_.erase(uuid_it);
  }
}

void CrostiniManager::ExportLxdContainer(
    guest_os::GuestId container_id,
    base::FilePath export_path,
    ExportLxdContainerResultCallback callback) {
  if (container_id.vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR, 0, 0);
    return;
  }
  if (container_id.container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR, 0, 0);
    return;
  }
  if (export_path.empty()) {
    LOG(ERROR) << "export_path is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR, 0, 0);
    return;
  }

  if (export_lxd_container_callbacks_.find(container_id) !=
      export_lxd_container_callbacks_.end()) {
    LOG(ERROR) << "Export currently in progress for " << container_id;
    std::move(callback).Run(CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED, 0,
                            0);
    return;
  }
  export_lxd_container_callbacks_.emplace(container_id, std::move(callback));

  vm_tools::cicerone::ExportLxdContainerRequest request;
  request.set_vm_name(container_id.vm_name);
  request.set_container_name(container_id.container_name);
  request.set_owner_id(owner_id_);
  request.set_export_path(export_path.value());
  GetCiceroneClient()->ExportLxdContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnExportLxdContainer,
                     weak_ptr_factory_.GetWeakPtr(), std::move(container_id)));
}

void CrostiniManager::ImportLxdContainer(guest_os::GuestId container_id,
                                         base::FilePath import_path,
                                         CrostiniResultCallback callback) {
  if (container_id.vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (container_id.container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (import_path.empty()) {
    LOG(ERROR) << "import_path is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (import_lxd_container_callbacks_.find(container_id) !=
      import_lxd_container_callbacks_.end()) {
    LOG(ERROR) << "Import currently in progress for " << container_id;
    std::move(callback).Run(CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED);
    return;
  }
  import_lxd_container_callbacks_.emplace(container_id, std::move(callback));

  vm_tools::cicerone::ImportLxdContainerRequest request;
  request.set_vm_name(container_id.vm_name);
  request.set_container_name(container_id.container_name);
  request.set_owner_id(owner_id_);
  request.set_import_path(import_path.value());
  GetCiceroneClient()->ImportLxdContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnImportLxdContainer,
                     weak_ptr_factory_.GetWeakPtr(), std::move(container_id)));
}

void CrostiniManager::CancelDiskImageOp(guest_os::GuestId key) {
  auto it = disk_image_callbacks_.find(key);
  if (it == disk_image_callbacks_.end()) {
    LOG(ERROR) << "No disk image operation currently in progress for " << key;
    return;
  }

  auto uuid_it = std::find_if(
      disk_image_uuid_to_guest_id_.begin(), disk_image_uuid_to_guest_id_.end(),
      [&key](const auto& p) { return p.second == key; });
  if (uuid_it == disk_image_uuid_to_guest_id_.end()) {
    LOG(ERROR) << "No associated command UUID for " << key;
    return;
  }

  vm_tools::concierge::CancelDiskImageRequest request;
  request.set_command_uuid(uuid_it->first);
  GetConciergeClient()->CancelDiskImageOperation(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnCancelDiskImageOp,
                     weak_ptr_factory_.GetWeakPtr(), std::move(key)));
}

void CrostiniManager::CancelExportLxdContainer(guest_os::GuestId key) {
  const auto& vm_name = key.vm_name;
  const auto& container_name = key.container_name;
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    return;
  }
  if (container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    return;
  }

  auto it = export_lxd_container_callbacks_.find(key);
  if (it == export_lxd_container_callbacks_.end()) {
    LOG(ERROR) << "No export currently in progress for " << key;
    return;
  }

  vm_tools::cicerone::CancelExportLxdContainerRequest request;
  request.set_vm_name(vm_name);
  request.set_owner_id(owner_id_);
  request.set_in_progress_container_name(container_name);
  GetCiceroneClient()->CancelExportLxdContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnCancelExportLxdContainer,
                     weak_ptr_factory_.GetWeakPtr(), std::move(key)));
}

void CrostiniManager::CancelImportLxdContainer(guest_os::GuestId key) {
  const auto& vm_name = key.vm_name;
  const auto& container_name = key.container_name;
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    return;
  }
  if (container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    return;
  }

  auto it = import_lxd_container_callbacks_.find(key);
  if (it == import_lxd_container_callbacks_.end()) {
    LOG(ERROR) << "No import currently in progress for " << key;
    return;
  }

  vm_tools::cicerone::CancelImportLxdContainerRequest request;
  request.set_vm_name(vm_name);
  request.set_owner_id(owner_id_);
  request.set_in_progress_container_name(container_name);
  GetCiceroneClient()->CancelImportLxdContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnCancelImportLxdContainer,
                     weak_ptr_factory_.GetWeakPtr(), std::move(key)));
}

namespace {
vm_tools::cicerone::UpgradeContainerRequest::Version ConvertVersion(
    ContainerVersion from) {
  switch (from) {
    case ContainerVersion::STRETCH:
      return vm_tools::cicerone::UpgradeContainerRequest::DEBIAN_STRETCH;
    case ContainerVersion::BUSTER:
      return vm_tools::cicerone::UpgradeContainerRequest::DEBIAN_BUSTER;
    case ContainerVersion::BULLSEYE:
      return vm_tools::cicerone::UpgradeContainerRequest::DEBIAN_BULLSEYE;
    case ContainerVersion::BOOKWORM:
      return vm_tools::cicerone::UpgradeContainerRequest::DEBIAN_BOOKWORM;
    case ContainerVersion::UNKNOWN:
    default:
      return vm_tools::cicerone::UpgradeContainerRequest::UNKNOWN;
  }
}

}  // namespace

void CrostiniManager::UpgradeContainer(const guest_os::GuestId& key,
                                       ContainerVersion target_version,
                                       CrostiniResultCallback callback) {
  const auto& vm_name = key.vm_name;
  const auto& container_name = key.container_name;
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (!GetCiceroneClient()->IsUpgradeContainerProgressSignalConnected()) {
    // Technically we could still start the upgrade, but we wouldn't be able
    // to detect when the upgrade completes, successfully or otherwise.
    LOG(ERROR) << "Attempted to upgrade container when progress signal not "
                  "connected.";
    std::move(callback).Run(CrostiniResult::UPGRADE_CONTAINER_FAILED);
    return;
  }
  vm_tools::cicerone::UpgradeContainerRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name(vm_name);
  request.set_container_name(container_name);
  request.set_target_version(ConvertVersion(target_version));

  CrostiniResultCallback do_upgrade_container = base::BindOnce(
      [](base::WeakPtr<CrostiniManager> crostini_manager,
         vm_tools::cicerone::UpgradeContainerRequest request,
         CrostiniResultCallback final_callback, CrostiniResult result) {
        // When we fail to start the VM, we can't continue the upgrade.
        if (result != CrostiniResult::SUCCESS &&
            result != CrostiniResult::RESTART_ABORTED) {
          LOG(ERROR) << "Failed to restart the vm before attempting container "
                        "upgrade. Result code "
                     << static_cast<int>(result);
          std::move(final_callback)
              .Run(CrostiniResult::UPGRADE_CONTAINER_FAILED);
          return;
        }
        GetCiceroneClient()->UpgradeContainer(
            std::move(request),
            base::BindOnce(&CrostiniManager::OnUpgradeContainer,
                           crostini_manager, std::move(final_callback)));
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(request), std::move(callback));

  if (!IsVmRunning(vm_name)) {
    RestartCrostini(key, std::move(do_upgrade_container));
  } else {
    std::move(do_upgrade_container).Run(CrostiniResult::SUCCESS);
  }
}

void CrostiniManager::CancelUpgradeContainer(const guest_os::GuestId& key,
                                             CrostiniResultCallback callback) {
  const auto& vm_name = key.vm_name;
  const auto& container_name = key.container_name;
  if (vm_name.empty()) {
    LOG(ERROR) << "vm_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  if (container_name.empty()) {
    LOG(ERROR) << "container_name is required";
    std::move(callback).Run(CrostiniResult::CLIENT_ERROR);
    return;
  }
  vm_tools::cicerone::CancelUpgradeContainerRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name(vm_name);
  request.set_container_name(container_name);
  GetCiceroneClient()->CancelUpgradeContainer(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnCancelUpgradeContainer,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::GetContainerAppIcons(
    const guest_os::GuestId& container_id,
    std::vector<std::string> desktop_file_ids,
    int icon_size,
    int scale,
    GetContainerAppIconsCallback callback) {
  vm_tools::cicerone::ContainerAppIconRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name(container_id.vm_name);
  request.set_container_name(container_id.container_name);
  google::protobuf::RepeatedPtrField<std::string> ids(
      std::make_move_iterator(desktop_file_ids.begin()),
      std::make_move_iterator(desktop_file_ids.end()));
  request.mutable_desktop_file_ids()->Swap(&ids);
  request.set_size(icon_size);
  request.set_scale(scale);

  GetCiceroneClient()->GetContainerAppIcons(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnGetContainerAppIcons,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::GetLinuxPackageInfo(
    const guest_os::GuestId& container_id,
    std::string package_path,
    GetLinuxPackageInfoCallback callback) {
  vm_tools::cicerone::LinuxPackageInfoRequest request;
  request.set_owner_id(CryptohomeIdForProfile(profile_));
  request.set_vm_name(container_id.vm_name);
  request.set_container_name(container_id.container_name);
  request.set_file_path(std::move(package_path));

  GetCiceroneClient()->GetLinuxPackageInfo(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnGetLinuxPackageInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::InstallLinuxPackage(
    const guest_os::GuestId& container_id,
    std::string package_path,
    InstallLinuxPackageCallback callback) {
  if (!CrostiniFeatures::Get()->IsRootAccessAllowed(profile_)) {
    LOG(ERROR) << "Attempted to install package when root access to Crostini "
                  "VM not allowed.";
    std::move(callback).Run(CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED);
    return;
  }

  if (!GetCiceroneClient()->IsInstallLinuxPackageProgressSignalConnected()) {
    // Technically we could still start the install, but we wouldn't be able
    // to detect when the install completes, successfully or otherwise.
    LOG(ERROR)
        << "Attempted to install package when progress signal not connected.";
    std::move(callback).Run(CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED);
    return;
  }

  vm_tools::cicerone::InstallLinuxPackageRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name(container_id.vm_name);
  request.set_container_name(container_id.container_name);
  request.set_file_path(std::move(package_path));

  GetCiceroneClient()->InstallLinuxPackage(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnInstallLinuxPackage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::InstallLinuxPackageFromApt(
    const guest_os::GuestId& container_id,
    std::string package_id,
    InstallLinuxPackageCallback callback) {
  if (!GetCiceroneClient()->IsInstallLinuxPackageProgressSignalConnected()) {
    // Technically we could still start the install, but we wouldn't be able
    // to detect when the install completes, successfully or otherwise.
    LOG(ERROR)
        << "Attempted to install package when progress signal not connected.";
    std::move(callback).Run(CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED);
    return;
  }

  vm_tools::cicerone::InstallLinuxPackageRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name(container_id.vm_name);
  request.set_container_name(container_id.container_name);
  request.set_package_id(std::move(package_id));

  GetCiceroneClient()->InstallLinuxPackage(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnInstallLinuxPackage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::UninstallPackageOwningFile(
    const guest_os::GuestId& container_id,
    std::string desktop_file_id,
    CrostiniResultCallback callback) {
  if (!GetCiceroneClient()->IsUninstallPackageProgressSignalConnected()) {
    // Technically we could still start the uninstall, but we wouldn't be able
    // to detect when the uninstall completes, successfully or otherwise.
    LOG(ERROR) << "Attempted to uninstall package when progress signal not "
                  "connected.";
    std::move(callback).Run(CrostiniResult::UNINSTALL_PACKAGE_FAILED);
    return;
  }

  vm_tools::cicerone::UninstallPackageOwningFileRequest request;
  request.set_owner_id(owner_id_);
  request.set_vm_name(container_id.vm_name);
  request.set_container_name(container_id.container_name);
  request.set_desktop_file_id(std::move(desktop_file_id));

  GetCiceroneClient()->UninstallPackageOwningFile(
      std::move(request),
      base::BindOnce(&CrostiniManager::OnUninstallPackageOwningFile,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool CrostiniManager::GetCrostiniDialogStatus(DialogType dialog_type) const {
  return open_crostini_dialogs_.count(dialog_type) == 1;
}

void CrostiniManager::SetCrostiniDialogStatus(DialogType dialog_type,
                                              bool open) {
  if (open) {
    open_crostini_dialogs_.insert(dialog_type);
  } else {
    open_crostini_dialogs_.erase(dialog_type);
  }
  for (auto& observer : crostini_dialog_status_observers_) {
    observer.OnCrostiniDialogStatusChanged(dialog_type, open);
  }
}

void CrostiniManager::AddCrostiniDialogStatusObserver(
    CrostiniDialogStatusObserver* observer) {
  crostini_dialog_status_observers_.AddObserver(observer);
}

void CrostiniManager::RemoveCrostiniDialogStatusObserver(
    CrostiniDialogStatusObserver* observer) {
  crostini_dialog_status_observers_.RemoveObserver(observer);
}

void CrostiniManager::AddCrostiniContainerPropertiesObserver(
    CrostiniContainerPropertiesObserver* observer) {
  crostini_container_properties_observers_.AddObserver(observer);
}

void CrostiniManager::RemoveCrostiniContainerPropertiesObserver(
    CrostiniContainerPropertiesObserver* observer) {
  crostini_container_properties_observers_.RemoveObserver(observer);
}

void CrostiniManager::AddContainerShutdownObserver(
    ContainerShutdownObserver* observer) {
  container_shutdown_observers_.AddObserver(observer);
}

void CrostiniManager::RemoveContainerShutdownObserver(
    ContainerShutdownObserver* observer) {
  container_shutdown_observers_.RemoveObserver(observer);
}

CrostiniManager::RestartId CrostiniManager::RestartCrostini(
    guest_os::GuestId container_id,
    CrostiniResultCallback callback,
    RestartObserver* observer) {
  return RestartCrostiniWithOptions(std::move(container_id), RestartOptions(),
                                    std::move(callback), observer);
}

CrostiniManager::RestartId CrostiniManager::RestartCrostiniWithOptions(
    guest_os::GuestId container_id,
    RestartOptions options,
    CrostiniResultCallback callback,
    RestartObserver* observer) {
  if (GetCrostiniDialogStatus(DialogType::INSTALLER)) {
    base::UmaHistogramBoolean("Crostini.Setup.Started", true);
  } else {
    base::UmaHistogramBoolean("Crostini.Restarter.Started", true);
  }
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Currently, |remove_crostini_callbacks_| is only used just before running
  // guest_os::GuestOsRemover. If that changes, then we should check for a
  // currently running uninstaller in some other way.
  if (!remove_crostini_callbacks_.empty()) {
    LOG(ERROR)
        << "Tried to install crostini while crostini uninstaller is running";
    std::move(callback).Run(CrostiniResult::CROSTINI_UNINSTALLER_RUNNING);
    return kUninitializedRestartId;
  }

  // Initialize create_options which contains the stored CreateOptions.
  RestartOptions create_options;

  // Clone flags which we care about from the freshly given options.
  create_options.start_vm_only = options.start_vm_only;
  create_options.stop_after_lxd_available = options.stop_after_lxd_available;

  bool obsolete_create_options = true;
  AddNewLxdContainerToPrefs(profile_, container_id);
  RegisterContainer(container_id);
  if (!RegisterCreateOptions(container_id, options)) {
    // Do the path cloning only if we have to since this is more expensive than
    // setting a boolean flag.
    for (auto path : options.share_paths) {
      create_options.share_paths.emplace_back(path);
    }
    obsolete_create_options = FetchCreateOptions(container_id, &create_options);
  }

  RestartId restart_id = next_restart_id_++;
  restarters_by_id_.emplace(restart_id, container_id);

  CrostiniRestarter::RestartRequest request = {
      restart_id,
      obsolete_create_options ? std::move(options) : std::move(create_options),
      std::move(callback), observer};

  auto it = restarters_by_container_.find(container_id);
  if (it == restarters_by_container_.end()) {
    VLOG(1) << "Creating new restarter for " << container_id;
    restarters_by_container_[container_id] =
        std::make_unique<CrostiniRestarter>(profile_, this, container_id,
                                            std::move(request));
    // In some cases this will synchronously finish the restart and cause it to
    // be deleted and removed from the map.
    restarters_by_container_[container_id]->Restart();
  } else {
    VLOG(1) << "Already restarting " << container_id;
    if (request.options.container_username || request.options.disk_size_bytes ||
        request.options.image_server_url || request.options.image_alias) {
      LOG(ERROR)
          << "Crostini restart options for new containers will be ignored "
             "as a restart is already in progress.";
    }
    it->second->AddRequest(std::move(request));
  }

  return restart_id;
}

void CrostiniManager::CancelRestartCrostini(
    CrostiniManager::RestartId restart_id) {
  auto container_it = restarters_by_id_.find(restart_id);
  if (container_it == restarters_by_id_.end()) {
    // Only tests execute this path at the time of writing but be defensive
    // just in case.
    LOG(ERROR)
        << "Cancelling a restarter that does not exist (already finished?)"
        << ", id = " << restart_id;
    return;
  }
  auto restarter_it = restarters_by_container_.find(container_it->second);
  DCHECK(restarter_it != restarters_by_container_.end());
  restarter_it->second->CancelRequest(restart_id);
}

bool CrostiniManager::IsRestartPending(RestartId restart_id) {
  return restarters_by_id_.find(restart_id) != restarters_by_id_.end();
}

bool CrostiniManager::HasRestarterForTesting(
    const guest_os::GuestId& guest_id) {
  return restarters_by_container_.find(guest_id) !=
         restarters_by_container_.end();
}

void CrostiniManager::AddShutdownContainerCallback(
    guest_os::GuestId container_id,
    base::OnceClosure shutdown_callback) {
  shutdown_container_callbacks_.emplace(std::move(container_id),
                                        std::move(shutdown_callback));
}

void CrostiniManager::AddRemoveCrostiniCallback(
    RemoveCrostiniCallback remove_callback) {
  remove_crostini_callbacks_.emplace_back(std::move(remove_callback));
}

void CrostiniManager::AddLinuxPackageOperationProgressObserver(
    LinuxPackageOperationProgressObserver* observer) {
  linux_package_operation_progress_observers_.AddObserver(observer);
}

void CrostiniManager::RemoveLinuxPackageOperationProgressObserver(
    LinuxPackageOperationProgressObserver* observer) {
  linux_package_operation_progress_observers_.RemoveObserver(observer);
}

void CrostiniManager::AddPendingAppListUpdatesObserver(
    PendingAppListUpdatesObserver* observer) {
  pending_app_list_updates_observers_.AddObserver(observer);
}

void CrostiniManager::RemovePendingAppListUpdatesObserver(
    PendingAppListUpdatesObserver* observer) {
  pending_app_list_updates_observers_.RemoveObserver(observer);
}

void CrostiniManager::AddExportContainerProgressObserver(
    ExportContainerProgressObserver* observer) {
  export_container_progress_observers_.AddObserver(observer);
}

void CrostiniManager::RemoveExportContainerProgressObserver(
    ExportContainerProgressObserver* observer) {
  export_container_progress_observers_.RemoveObserver(observer);
}

void CrostiniManager::AddImportContainerProgressObserver(
    ImportContainerProgressObserver* observer) {
  import_container_progress_observers_.AddObserver(observer);
}

void CrostiniManager::RemoveImportContainerProgressObserver(
    ImportContainerProgressObserver* observer) {
  import_container_progress_observers_.RemoveObserver(observer);
}

void CrostiniManager::AddDiskImageProgressObserver(
    DiskImageProgressObserver* observer) {
  disk_image_progress_observers_.AddObserver(observer);
}

void CrostiniManager::RemoveDiskImageProgressObserver(
    DiskImageProgressObserver* observer) {
  disk_image_progress_observers_.RemoveObserver(observer);
}

void CrostiniManager::AddUpgradeContainerProgressObserver(
    UpgradeContainerProgressObserver* observer) {
  upgrade_container_progress_observers_.AddObserver(observer);
}

void CrostiniManager::RemoveUpgradeContainerProgressObserver(
    UpgradeContainerProgressObserver* observer) {
  upgrade_container_progress_observers_.RemoveObserver(observer);
}

void CrostiniManager::AddVmShutdownObserver(ash::VmShutdownObserver* observer) {
  vm_shutdown_observers_.AddObserver(observer);
}
void CrostiniManager::RemoveVmShutdownObserver(
    ash::VmShutdownObserver* observer) {
  vm_shutdown_observers_.RemoveObserver(observer);
}

void CrostiniManager::AddVmStartingObserver(ash::VmStartingObserver* observer) {
  vm_starting_observers_.AddObserver(observer);
}
void CrostiniManager::RemoveVmStartingObserver(
    ash::VmStartingObserver* observer) {
  vm_starting_observers_.RemoveObserver(observer);
}

void CrostiniManager::OnCreateDiskImage(
    CreateDiskImageCallback callback,
    std::optional<vm_tools::concierge::CreateDiskImageResponse> response) {
  CrostiniResult result;
  base::FilePath path;

  if (!response) {
    LOG(ERROR) << "Failed to create disk image. Empty response.";
    result = CrostiniResult::CREATE_DISK_IMAGE_NO_RESPONSE;
  } else if (response->status() == vm_tools::concierge::DISK_STATUS_CREATED) {
    result = CrostiniResult::SUCCESS;
    path = base::FilePath(response->disk_path());
  } else if (response->status() == vm_tools::concierge::DISK_STATUS_EXISTS) {
    result = CrostiniResult::CREATE_DISK_IMAGE_ALREADY_EXISTS;
    path = base::FilePath(response->disk_path());
  } else {
    LOG(ERROR) << "Failed to create disk image. Error: "
               << static_cast<int>(response->status())
               << ", reason: " << response->failure_reason();
    result = CrostiniResult::CREATE_DISK_IMAGE_FAILED;
  }

  std::move(callback).Run(result, path);
}

void CrostiniManager::OnStartTerminaVm(
    std::string vm_name,
    BoolCallback callback,
    std::optional<vm_tools::concierge::StartVmResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to start termina vm. Empty response.";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  switch (response->mount_result()) {
    case vm_tools::concierge::StartVmResponse::PARTIAL_DATA_LOSS:
      EmitCorruptionStateMetric(CorruptionStates::MOUNT_ROLLED_BACK);
      break;
    case vm_tools::concierge::StartVmResponse::FAILURE:
      EmitCorruptionStateMetric(CorruptionStates::MOUNT_FAILED);
      break;
    default:
      break;
  }

  // The UI can only resize the default VM, so only (maybe) show the
  // notification for the default VM, if we got a value, and if the value isn't
  // an error (the API we call for space returns -1 on error).
  if (vm_name == DefaultContainerId().vm_name &&
      response->free_bytes_has_value() && response->free_bytes() >= 0) {
    low_disk_notifier_->ShowNotificationIfAppropriate(response->free_bytes());
  }

  // If the vm is already marked "running" run the callback.
  if (response->status() == vm_tools::concierge::VM_STATUS_RUNNING) {
    running_vms_[vm_name] =
        VmInfo{VmState::STARTED, std::move(response->vm_info())};
    std::move(callback).Run(/*success=*/true);
    return;
  }

  // Any pending callbacks must exist from a previously running VM, and should
  // be marked as failed.
  InvokeAndErasePendingCallbacks(
      &export_lxd_container_callbacks_, vm_name,
      CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STARTED, 0, 0);
  InvokeAndErasePendingCallbacks(&disk_image_callbacks_, vm_name,
                                 CrostiniResult::DISK_IMAGE_FAILED);
  InvokeAndErasePendingCallbacks(
      &import_lxd_container_callbacks_, vm_name,
      CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STARTED);
  // Same for mappings, no longer valid.
  EraseCommandUuid(&disk_image_uuid_to_guest_id_, vm_name);

  if (response->status() == vm_tools::concierge::VM_STATUS_FAILURE ||
      response->status() == vm_tools::concierge::VM_STATUS_UNKNOWN) {
    LOG(ERROR) << "Failed to start VM: " << response->failure_reason();
    // If we thought vms and containers were running before, they aren't now.
    running_vms_.erase(vm_name);
    std::move(callback).Run(/*success=*/false);
    return;
  }

  // Otherwise, record the vm start and run the callback after the VM
  // starts.
  DCHECK_EQ(response->status(), vm_tools::concierge::VM_STATUS_STARTING);
  bool wait_for_tremplin = running_vms_.find(vm_name) == running_vms_.end();

  uint32_t seneschal_server_handle =
      response->vm_info().seneschal_server_handle();
  running_vms_[vm_name] =
      VmInfo{VmState::STARTING, std::move(response->vm_info())};
  // If we thought a container was running for this VM, we're wrong. This can
  // happen if the vm was formerly running, then stopped via crosh.

  if (wait_for_tremplin) {
    VLOG(1) << "Awaiting TremplinStartedSignal for " << owner_id_ << ", "
            << vm_name;
    tremplin_started_callbacks_.emplace(
        vm_name, base::BindOnce(&CrostiniManager::OnStartTremplin,
                                weak_ptr_factory_.GetWeakPtr(), vm_name,
                                seneschal_server_handle, std::move(callback)));
  } else {
    OnStartTremplin(vm_name, seneschal_server_handle, std::move(callback));
  }
}

void CrostiniManager::OnStartTremplin(std::string vm_name,
                                      uint32_t seneschal_server_handle,
                                      BoolCallback callback) {
  // Record the running vm.
  VLOG(1) << "Received TremplinStartedSignal, VM: " << owner_id_ << ", "
          << vm_name;
  UpdateVmState(vm_name, VmState::STARTED);

  // TODO(timloh): These should probably either be in CrostiniRestarter
  // alongside sharing non-persisted paths, or separated entirely from the
  // restart flow and instead run for all Guest OS types whenever they start
  // up. For fonts, this could be done directly in concierge (b/231252066).

  // Share fonts directory with the VM but don't persist as a shared path.
  guest_os::GuestOsSharePathFactory::GetForProfile(profile_)->SharePath(
      vm_name, seneschal_server_handle,
      base::FilePath(file_manager::util::kSystemFontsPath), base::DoNothing());

  // Run the original callback.
  std::move(callback).Run(/*success=*/true);
}

void CrostiniManager::OnStartLxdProgress(
    const vm_tools::cicerone::StartLxdProgressSignal& signal) {
  if (signal.owner_id() != owner_id_) {
    return;
  }
  CrostiniResult result = CrostiniResult::UNKNOWN_ERROR;

  switch (signal.status()) {
    case vm_tools::cicerone::StartLxdProgressSignal::STARTED:
      result = CrostiniResult::SUCCESS;
      break;
    case vm_tools::cicerone::StartLxdProgressSignal::STARTING:
    case vm_tools::cicerone::StartLxdProgressSignal::RECOVERING:
      // Still in-progress, keep waiting.
      return;
    case vm_tools::cicerone::StartLxdProgressSignal::FAILED:
      result = CrostiniResult::START_LXD_FAILED_SIGNAL;
      break;
    default:
      break;
  }

  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Failed to create container. VM: " << signal.vm_name()
               << " reason: " << signal.failure_reason();
  }

  InvokeAndErasePendingCallbacks(&start_lxd_callbacks_, signal.vm_name(),
                                 result);
}

void CrostiniManager::OnStopVm(
    std::string vm_name,
    CrostiniResultCallback callback,
    std::optional<vm_tools::concierge::StopVmResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to stop termina vm. Empty response.";
    std::move(callback).Run(CrostiniResult::STOP_VM_NO_RESPONSE);
    return;
  }

  if (!response->success()) {
    LOG(ERROR) << "Failed to stop VM: " << response->failure_reason();
    std::move(callback).Run(CrostiniResult::VM_STOP_FAILED);
    return;
  }

  std::move(callback).Run(CrostiniResult::SUCCESS);
}

void CrostiniManager::OnVmStoppedCleanup(const std::string& vm_name) {
  for (auto& observer : vm_shutdown_observers_) {
    observer.OnVmShutdown(vm_name);
  }

  // Check what state the VM was believed to be in before the stop signal was
  // received.
  //
  // If it was in the STARTED state, it's an unexpected shutdown and we should
  // log it here.
  //
  // If it was STARTING then the error is tracked as a restart failure, not
  // here. If it was STOPPING then the stop was expected and not an error. If
  // it wasn't tracked by CrostiniManager, then we don't care what happens to
  // it.
  //
  // Therefore this stop signal is unexpected if-and-only-if IsVmRunning().
  //
  // This check must run before removing the VM from |running_vms_|.
  if (IsVmRunning(vm_name)) {
    guest_os_stability_monitor_->LogUnexpectedVmShutdown();
  }

  // Remove from running_vms_, and other vm-keyed state.
  running_vms_.erase(vm_name);
  EraseCommandUuid(&disk_image_uuid_to_guest_id_, vm_name);
  InvokeAndErasePendingCallbacks(
      &export_lxd_container_callbacks_, vm_name,
      CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED, 0, 0);
  InvokeAndErasePendingCallbacks(&disk_image_callbacks_, vm_name,
                                 CrostiniResult::DISK_IMAGE_FAILED);
  InvokeAndErasePendingCallbacks(
      &import_lxd_container_callbacks_, vm_name,
      CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED);
  // After we shut down a VM, we are no longer in a state where we need to
  // prompt for user cleanup.
  is_unclean_startup_ = false;
}

void CrostiniManager::OnGetTerminaVmKernelVersion(
    std::optional<vm_tools::concierge::GetVmEnterpriseReportingInfoResponse>
        response) {
  // If there is an error, (re)set the kernel version pref to the empty string.
  std::string kernel_version;
  if (!response) {
    LOG(ERROR) << "No reply to GetVmEnterpriseReportingInfo";
  } else if (!response->success()) {
    LOG(ERROR) << "Error response for GetVmEnterpriseReportingInfo: "
               << response->failure_reason();
  } else {
    kernel_version = response->vm_kernel_version();
  }

  WriteTerminaVmKernelVersionToPrefsForReporting(profile_->GetPrefs(),
                                                 kernel_version);
}

void CrostiniManager::OnContainerStarted(
    const vm_tools::cicerone::ContainerStartedSignal& signal) {
  if (signal.owner_id() != owner_id_) {
    return;
  }
  guest_os::GuestId container_id(kCrostiniDefaultVmType, signal.vm_name(),
                                 signal.container_name());

  auto* metrics_service =
      CrostiniMetricsService::Factory::GetForProfile(profile_);
  // This is null in unit tests.
  if (metrics_service) {
    metrics_service->SetBackgroundActive(true);
  }

  VLOG(1) << "Container " << signal.container_name() << " started";
  InvokeAndErasePendingContainerCallbacks(
      &start_container_callbacks_, container_id, CrostiniResult::SUCCESS);

  if (signal.vm_name() == kCrostiniDefaultVmName) {
    AddShutdownContainerCallback(
        container_id,
        base::BindOnce(
            &CrostiniManager::DeallocateForwardedPortsCallback,
            weak_ptr_factory_.GetWeakPtr(),
            guest_os::GuestId(kCrostiniDefaultVmType, signal.vm_name(),
                              signal.container_name())));
  }
}

void CrostiniManager::OnGuestFileCorruption(
    const anomaly_detector::GuestFileCorruptionSignal& signal) {
  EmitCorruptionStateMetric(CorruptionStates::OTHER_CORRUPTION);
}

void CrostiniManager::OnVmStarted(
    const vm_tools::concierge::VmStartedSignal& signal) {}

void CrostiniManager::OnVmStopped(
    const vm_tools::concierge::VmStoppedSignal& signal) {
  if (signal.owner_id() != owner_id_) {
    return;
  }
  if (running_vms_.find(signal.name()) == running_vms_.end()) {
    LOG(WARNING) << "Ignoring VmStopped for " << signal.name();
    return;
  }
  OnVmStoppedCleanup(signal.name());
}

void CrostiniManager::OnVmStopping(
    const vm_tools::concierge::VmStoppingSignal& signal) {
  if (signal.owner_id() != owner_id_) {
    return;
  }
  auto iter = running_vms_.find(signal.name());
  if (iter == running_vms_.end()) {
    LOG(WARNING) << "Ignoring VmStopping for " << signal.name();
    return;
  }
  iter->second.state = VmState::STOPPING;
}

void CrostiniManager::OnContainerShutdown(
    const vm_tools::cicerone::ContainerShutdownSignal& signal) {
  if (signal.owner_id() != owner_id_) {
    return;
  }
  guest_os::GuestId container_id(kCrostiniDefaultVmType, signal.vm_name(),
                                 signal.container_name());
  // Find the callbacks to call, then erase them from the map.
  auto range_callbacks =
      shutdown_container_callbacks_.equal_range(container_id);
  for (auto it = range_callbacks.first; it != range_callbacks.second; ++it) {
    std::move(it->second).Run();
  }
  shutdown_container_callbacks_.erase(range_callbacks.first,
                                      range_callbacks.second);
  HandleContainerShutdown(container_id);
}

void CrostiniManager::OnInstallLinuxPackageProgress(
    const vm_tools::cicerone::InstallLinuxPackageProgressSignal& signal) {
  if (signal.owner_id() != owner_id_) {
    return;
  }
  if (signal.progress_percent() < 0 || signal.progress_percent() > 100) {
    LOG(ERROR) << "Received install progress with invalid progress of "
               << signal.progress_percent() << "%.";
    return;
  }

  InstallLinuxPackageProgressStatus status;
  std::string error_message;
  switch (signal.status()) {
    case vm_tools::cicerone::InstallLinuxPackageProgressSignal::SUCCEEDED:
      status = InstallLinuxPackageProgressStatus::SUCCEEDED;
      break;
    case vm_tools::cicerone::InstallLinuxPackageProgressSignal::FAILED:
      LOG(ERROR) << "Install failed: " << signal.failure_details();
      status = InstallLinuxPackageProgressStatus::FAILED;
      error_message = signal.failure_details();
      break;
    case vm_tools::cicerone::InstallLinuxPackageProgressSignal::DOWNLOADING:
      status = InstallLinuxPackageProgressStatus::DOWNLOADING;
      break;
    case vm_tools::cicerone::InstallLinuxPackageProgressSignal::INSTALLING:
      status = InstallLinuxPackageProgressStatus::INSTALLING;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  guest_os::GuestId container_id(kCrostiniDefaultVmType, signal.vm_name(),
                                 signal.container_name());
  for (auto& observer : linux_package_operation_progress_observers_) {
    observer.OnInstallLinuxPackageProgress(
        container_id, status, signal.progress_percent(), error_message);
  }
}

void CrostiniManager::OnUninstallPackageProgress(
    const vm_tools::cicerone::UninstallPackageProgressSignal& signal) {
  if (signal.owner_id() != owner_id_) {
    return;
  }

  if (signal.progress_percent() < 0 || signal.progress_percent() > 100) {
    LOG(ERROR) << "Received uninstall progress with invalid progress of "
               << signal.progress_percent() << "%.";
    return;
  }

  UninstallPackageProgressStatus status;
  switch (signal.status()) {
    case vm_tools::cicerone::UninstallPackageProgressSignal::SUCCEEDED:
      status = UninstallPackageProgressStatus::SUCCEEDED;
      break;
    case vm_tools::cicerone::UninstallPackageProgressSignal::FAILED:
      status = UninstallPackageProgressStatus::FAILED;
      LOG(ERROR) << "Uninstalled failed: " << signal.failure_details();
      break;
    case vm_tools::cicerone::UninstallPackageProgressSignal::UNINSTALLING:
      status = UninstallPackageProgressStatus::UNINSTALLING;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  guest_os::GuestId container_id(kCrostiniDefaultVmType, signal.vm_name(),
                                 signal.container_name());
  for (auto& observer : linux_package_operation_progress_observers_) {
    observer.OnUninstallPackageProgress(container_id, status,
                                        signal.progress_percent());
  }
}

void CrostiniManager::OnApplyAnsiblePlaybookProgress(
    const vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal& signal) {
  if (signal.owner_id() != owner_id_) {
    return;
  }

  // TODO(okalitova): Add an observer.
  AnsibleManagementServiceFactory::GetForProfile(profile_)
      ->OnApplyAnsiblePlaybookProgress(signal);
}

void CrostiniManager::OnUpgradeContainerProgress(
    const vm_tools::cicerone::UpgradeContainerProgressSignal& signal) {
  if (signal.owner_id() != owner_id_) {
    return;
  }

  UpgradeContainerProgressStatus status;
  switch (signal.status()) {
    case vm_tools::cicerone::UpgradeContainerProgressSignal::SUCCEEDED:
      status = UpgradeContainerProgressStatus::SUCCEEDED;
      break;
    case vm_tools::cicerone::UpgradeContainerProgressSignal::UNKNOWN:
    case vm_tools::cicerone::UpgradeContainerProgressSignal::FAILED:
      status = UpgradeContainerProgressStatus::FAILED;
      LOG(ERROR) << "Upgrade failed: " << signal.failure_reason();
      break;
    case vm_tools::cicerone::UpgradeContainerProgressSignal::IN_PROGRESS:
      status = UpgradeContainerProgressStatus::UPGRADING;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  std::vector<std::string> progress_messages;
  progress_messages.reserve(signal.progress_messages().size());
  for (const auto& msg : signal.progress_messages()) {
    if (!msg.empty()) {
      // Blank lines aren't sent to observers.
      progress_messages.push_back(msg);
    }
  }

  guest_os::GuestId container_id(kCrostiniDefaultVmType, signal.vm_name(),
                                 signal.container_name());
  for (auto& observer : upgrade_container_progress_observers_) {
    observer.OnUpgradeContainerProgress(container_id, status,
                                        progress_messages);
  }
}

void CrostiniManager::OnUninstallPackageOwningFile(
    CrostiniResultCallback callback,
    std::optional<vm_tools::cicerone::UninstallPackageOwningFileResponse>
        response) {
  if (!response) {
    LOG(ERROR) << "Failed to uninstall Linux package. Empty response.";
    std::move(callback).Run(CrostiniResult::UNINSTALL_PACKAGE_FAILED);
    return;
  }

  if (response->status() ==
      vm_tools::cicerone::UninstallPackageOwningFileResponse::FAILED) {
    LOG(ERROR) << "Failed to uninstall Linux package: "
               << response->failure_reason();
    std::move(callback).Run(CrostiniResult::UNINSTALL_PACKAGE_FAILED);
    return;
  }

  if (response->status() ==
      vm_tools::cicerone::UninstallPackageOwningFileResponse::
          BLOCKING_OPERATION_IN_PROGRESS) {
    LOG(WARNING) << "Failed to uninstall Linux package, another operation is "
                    "already active.";
    std::move(callback).Run(CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE);
    return;
  }

  std::move(callback).Run(CrostiniResult::SUCCESS);
}

void CrostiniManager::OnStartLxd(
    std::string vm_name,
    CrostiniResultCallback callback,
    std::optional<vm_tools::cicerone::StartLxdResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to start lxd in vm. Empty response.";
    std::move(callback).Run(CrostiniResult::START_LXD_FAILED);
    return;
  }

  switch (response->status()) {
    case vm_tools::cicerone::StartLxdResponse::STARTING:
      VLOG(1) << "Awaiting OnStartLxdProgressSignal for " << owner_id_ << ", "
              << vm_name;
      // The callback will be called when we receive the LxdContainerCreated
      // signal.
      start_lxd_callbacks_.emplace(std::move(vm_name), std::move(callback));
      break;
    case vm_tools::cicerone::StartLxdResponse::ALREADY_RUNNING:
      std::move(callback).Run(CrostiniResult::SUCCESS);
      break;
    default:
      LOG(ERROR) << "Failed to start LXD: " << response->failure_reason();
      std::move(callback).Run(CrostiniResult::START_LXD_FAILED);
  }
}

void CrostiniManager::OnCreateLxdContainer(
    const guest_os::GuestId& container_id,
    CrostiniResultCallback callback,
    std::optional<vm_tools::cicerone::CreateLxdContainerResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to create lxd container in vm. Empty response.";
    std::move(callback).Run(CrostiniResult::CONTAINER_CREATE_FAILED);
    return;
  }

  switch (response->status()) {
    case vm_tools::cicerone::CreateLxdContainerResponse::CREATING:
      VLOG(1) << "Awaiting LxdContainerCreatedSignal for " << owner_id_ << ", "
              << container_id;
      // The callback will be called when we receive the LxdContainerCreated
      // signal.
      create_lxd_container_callbacks_.emplace(container_id,
                                              std::move(callback));
      break;
    case vm_tools::cicerone::CreateLxdContainerResponse::EXISTS:
      // Containers are registered in OnContainerCreated() when created via the
      // UI. But for any created manually also register now (crbug.com/1330168).
      AddNewLxdContainerToPrefs(profile_, container_id);
      RegisterContainer(container_id);
      SetCreateOptionsUsed(container_id);
      std::move(callback).Run(CrostiniResult::SUCCESS);
      break;
    default:
      LOG(ERROR) << "Failed to create container: "
                 << response->failure_reason();
      // Remove all create options and the existence of this container.
      if (IsPendingCreation(container_id) &&
          container_id != DefaultContainerId()) {
        RemoveLxdContainerFromPrefs(profile_, container_id);
        UnregisterContainer(container_id);
      }
      std::move(callback).Run(CrostiniResult::CONTAINER_CREATE_FAILED);
  }
}

void CrostiniManager::OnStartLxdContainer(
    const guest_os::GuestId& container_id,
    CrostiniResultCallback callback,
    std::optional<vm_tools::cicerone::StartLxdContainerResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to start lxd container in vm. Empty response.";
    std::move(callback).Run(CrostiniResult::CONTAINER_START_FAILED);
    return;
  }

  VLOG(1) << "Got StartLxdContainer response status: " << response->status();
  switch (response->status()) {
    case vm_tools::cicerone::StartLxdContainerResponse::UNKNOWN:
    case vm_tools::cicerone::StartLxdContainerResponse::FAILED:
      LOG(ERROR) << "Failed to start container: " << response->failure_reason();
      std::move(callback).Run(CrostiniResult::CONTAINER_START_FAILED);
      break;

    case vm_tools::cicerone::StartLxdContainerResponse::STARTED:
    case vm_tools::cicerone::StartLxdContainerResponse::RUNNING:
      std::move(callback).Run(CrostiniResult::SUCCESS);
      break;

    case vm_tools::cicerone::StartLxdContainerResponse::REMAPPING:
      // Run the update container dialog to warn users of delays.
      // The callback will be called when we receive the LxdContainerStarting
      // signal.
      PrepareShowCrostiniUpdateFilesystemView(profile_,
                                              CrostiniUISurface::kAppList);
      // Then perform the same steps as for starting.
      [[fallthrough]];
    case vm_tools::cicerone::StartLxdContainerResponse::STARTING: {
      VLOG(1) << "Awaiting LxdContainerStartingSignal for " << owner_id_ << ", "
              << container_id;
      // The callback will be called when we receive the LxdContainerStarting
      // signal and (if successful) the ContainerStarted signal from Garcon.
      start_container_callbacks_.emplace(container_id, std::move(callback));
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  if (response->has_os_release()) {
    SetContainerOsRelease(container_id, response->os_release());
  }
}

void CrostiniManager::OnStopLxdContainer(
    const guest_os::GuestId& container_id,
    CrostiniResultCallback callback,
    std::optional<vm_tools::cicerone::StopLxdContainerResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to stop lxd container in vm. Empty response.";
    std::move(callback).Run(CrostiniResult::CONTAINER_STOP_FAILED);
    return;
  }

  switch (response->status()) {
    case vm_tools::cicerone::StopLxdContainerResponse::UNKNOWN:
    case vm_tools::cicerone::StopLxdContainerResponse::FAILED:
      LOG(ERROR) << "Failed to stop container: " << response->failure_reason();
      std::move(callback).Run(CrostiniResult::CONTAINER_STOP_FAILED);
      break;

    case vm_tools::cicerone::StopLxdContainerResponse::STOPPED:
      HandleContainerShutdown(container_id);
      std::move(callback).Run(CrostiniResult::SUCCESS);
      break;

    case vm_tools::cicerone::StopLxdContainerResponse::STOPPING:
      VLOG(1) << "Awaiting ContainerShutdownSignal for " << owner_id_ << ", "
              << container_id;
      shutdown_container_callbacks_.emplace(
          container_id,
          base::BindOnce(std::move(callback), CrostiniResult::SUCCESS));
      break;

    case vm_tools::cicerone::StopLxdContainerResponse::DOES_NOT_EXIST:
      LOG(ERROR) << "Container does not exist " << container_id;
      std::move(callback).Run(CrostiniResult::CONTAINER_STOP_FAILED);
      break;

    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void CrostiniManager::OnSetUpLxdContainerUser(
    const guest_os::GuestId& container_id,
    BoolCallback callback,
    std::optional<vm_tools::cicerone::SetUpLxdContainerUserResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to set up lxd container user. Empty response.";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  switch (response->status()) {
    case vm_tools::cicerone::SetUpLxdContainerUserResponse::UNKNOWN:
      // If we hit this then we don't know if users are set up or not; a
      // possible cause is we weren't able to read the /etc/passwd file.
      // We're in one of the following cases:
      // - Users are already set up but hit a transient error reading the file
      //   e.g. crbug/1216305. This would be a no-op so safe to continue.
      // - The container is in a bad state e.g. file is missing entirely.
      //   Once we start the container (next step) the system will try to repair
      //   this. It won't recover enough for restart to succeed, but it will
      //   give us a valid passwd file so that next launch we'll set up users
      //   and all will be good again. If we errored out here then we'd never
      //   repair the file and the container is borked for good.
      // - Lastly and least likely, it could be a transient issue but users
      //   aren't set up correctly. The container will either fail to start,
      //   or start but won't completely work (e.g. maybe adb sideloading will
      //   fail). Either way, restarting the container should get them back into
      //   a good state.
      // Note that if the user's account is missing then garcon won't start,
      // which combined with crbug/1197416 means launch will hang forever (well,
      // it's a 5 day timeout so not forever but may as well be). They would
      // have to be incredibly unlucky and restarting will fix things so that's
      // acceptable.
      base::UmaHistogramBoolean("Crostini.SetUpLxdContainerUser.UnknownResult",
                                true);
      LOG(ERROR) << "Failed to set up container user: "
                 << response->failure_reason();
      std::move(callback).Run(/*success=*/true);
      break;
    case vm_tools::cicerone::SetUpLxdContainerUserResponse::SUCCESS:
    case vm_tools::cicerone::SetUpLxdContainerUserResponse::EXISTS:
      base::UmaHistogramBoolean("Crostini.SetUpLxdContainerUser.UnknownResult",
                                false);
      std::move(callback).Run(/*success=*/true);
      break;
    case vm_tools::cicerone::SetUpLxdContainerUserResponse::FAILED:
      LOG(ERROR) << "Failed to set up container user: "
                 << response->failure_reason();
      base::UmaHistogramBoolean("Crostini.SetUpLxdContainerUser.UnknownResult",
                                false);
      std::move(callback).Run(/*success=*/false);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void CrostiniManager::OnLxdContainerCreated(
    const vm_tools::cicerone::LxdContainerCreatedSignal& signal) {
  if (signal.owner_id() != owner_id_) {
    return;
  }
  guest_os::GuestId container_id(kCrostiniDefaultVmType, signal.vm_name(),
                                 signal.container_name());
  CrostiniResult result;

  switch (signal.status()) {
    case vm_tools::cicerone::LxdContainerCreatedSignal::UNKNOWN:
      result = CrostiniResult::UNKNOWN_ERROR;
      break;
    case vm_tools::cicerone::LxdContainerCreatedSignal::CREATED:
      SetCreateOptionsUsed(container_id);
      result = CrostiniResult::SUCCESS;
      break;
    case vm_tools::cicerone::LxdContainerCreatedSignal::DOWNLOAD_TIMED_OUT:
      result = CrostiniResult::CONTAINER_DOWNLOAD_TIMED_OUT;
      break;
    case vm_tools::cicerone::LxdContainerCreatedSignal::CANCELLED:
      result = CrostiniResult::CONTAINER_CREATE_CANCELLED;
      break;
    case vm_tools::cicerone::LxdContainerCreatedSignal::FAILED:
      result = CrostiniResult::CONTAINER_CREATE_FAILED_SIGNAL;
      break;
    default:
      result = CrostiniResult::UNKNOWN_ERROR;
      break;
  }

  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Failed to create container. ID: " << container_id
               << " reason: " << signal.failure_reason();
    if (IsPendingCreation(container_id) &&
        container_id != DefaultContainerId()) {
      RemoveLxdContainerFromPrefs(profile_, container_id);
      UnregisterContainer(container_id);
    }
  }

  InvokeAndErasePendingContainerCallbacks(&create_lxd_container_callbacks_,
                                          container_id, result);
}

void CrostiniManager::OnLxdContainerDeleted(
    const vm_tools::cicerone::LxdContainerDeletedSignal& signal) {
  if (signal.owner_id() != owner_id_) {
    return;
  }

  guest_os::GuestId container_id(kCrostiniDefaultVmType, signal.vm_name(),
                                 signal.container_name());
  bool success =
      signal.status() == vm_tools::cicerone::LxdContainerDeletedSignal::DELETED;
  if (success) {
    RemoveLxdContainerFromPrefs(profile_, container_id);
    UnregisterContainer(container_id);
  } else {
    LOG(ERROR) << "Failed to delete container " << container_id << " : "
               << signal.failure_reason();
  }

  // Find the callbacks to call, then erase them from the map.
  auto range = delete_lxd_container_callbacks_.equal_range(container_id);
  for (auto it = range.first; it != range.second; ++it) {
    std::move(it->second).Run(success);
  }
  delete_lxd_container_callbacks_.erase(range.first, range.second);
}

void CrostiniManager::OnLxdContainerDownloading(
    const vm_tools::cicerone::LxdContainerDownloadingSignal& signal) {
  if (owner_id_ != signal.owner_id()) {
    return;
  }
  guest_os::GuestId container_id(kCrostiniDefaultVmType, signal.vm_name(),
                                 signal.container_name());
  auto iter = restarters_by_container_.find(container_id);
  if (iter != restarters_by_container_.end()) {
    iter->second->OnContainerDownloading(signal.download_progress());
  }
}

void CrostiniManager::OnTremplinStarted(
    const vm_tools::cicerone::TremplinStartedSignal& signal) {
  if (signal.owner_id() != owner_id_) {
    return;
  }

  // If this VM is not yet known in running_vms_, put it there in state
  // STARTING. This can happen if tremplin starts up faster than concierge can
  // finish its other startup work.
  if (running_vms_.find(signal.vm_name()) == running_vms_.end()) {
    running_vms_[signal.vm_name()] =
        VmInfo{VmState::STARTING, vm_tools::concierge::VmInfo{}};
  }
  // Find the callbacks to call, then erase them from the map.
  auto range = tremplin_started_callbacks_.equal_range(signal.vm_name());
  for (auto it = range.first; it != range.second; ++it) {
    std::move(it->second).Run();
  }
  tremplin_started_callbacks_.erase(range.first, range.second);
}

void CrostiniManager::OnLxdContainerStarting(
    const vm_tools::cicerone::LxdContainerStartingSignal& signal) {
  VLOG(1) << "Received OnLxdContainerStarting message with status: "
          << signal.status() << " for container " << signal.container_name();
  if (signal.owner_id() != owner_id_) {
    return;
  }
  guest_os::GuestId container_id(kCrostiniDefaultVmType, signal.vm_name(),
                                 signal.container_name());
  CrostiniResult result;

  switch (signal.status()) {
    case vm_tools::cicerone::LxdContainerStartingSignal::UNKNOWN:
      result = CrostiniResult::UNKNOWN_ERROR;
      break;
    case vm_tools::cicerone::LxdContainerStartingSignal::CANCELLED:
      result = CrostiniResult::CONTAINER_START_CANCELLED;
      break;
    case vm_tools::cicerone::LxdContainerStartingSignal::STARTED:
      result = CrostiniResult::SUCCESS;
      break;
    case vm_tools::cicerone::LxdContainerStartingSignal::FAILED:
      result = CrostiniResult::CONTAINER_START_FAILED;
      break;
    case vm_tools::cicerone::LxdContainerStartingSignal::STARTING: {
      auto iter = restarters_by_container_.find(container_id);
      if (iter != restarters_by_container_.end()) {
        iter->second->OnLxdContainerStarting(signal.status());
      }
      return;
    }
    default:
      result = CrostiniResult::UNKNOWN_ERROR;
      break;
  }

  if (result != CrostiniResult::SUCCESS) {
    LOG(ERROR) << "Failed to start container. ID: " << container_id
               << " reason: " << signal.failure_reason();
  }

  bool running = guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile_)
                     ->IsRunning(container_id);
  if (result == CrostiniResult::SUCCESS && !running) {
    VLOG(1) << "Awaiting ContainerStarted signal from Garcon, did not yet have "
               "information for container "
            << container_id.container_name;
    return;
  }

  if (signal.has_os_release()) {
    SetContainerOsRelease(container_id, signal.os_release());
  }

  InvokeAndErasePendingContainerCallbacks(&start_container_callbacks_,
                                          container_id, result);
}

void CrostiniManager::OnGetContainerAppIcons(
    GetContainerAppIconsCallback callback,
    std::optional<vm_tools::cicerone::ContainerAppIconResponse> response) {
  std::vector<Icon> icons;
  if (!response) {
    LOG(ERROR) << "Failed to get container application icons. Empty response.";
    std::move(callback).Run(/*success=*/false, icons);
    return;
  }

  for (auto& icon : *response->mutable_icons()) {
    icons.emplace_back(
        Icon{.desktop_file_id = std::move(*icon.mutable_desktop_file_id()),
             .content = std::move(*icon.mutable_icon()),
             .format = icon.format()});
  }
  std::move(callback).Run(/*success=*/true, icons);
}

void CrostiniManager::OnGetLinuxPackageInfo(
    GetLinuxPackageInfoCallback callback,
    std::optional<vm_tools::cicerone::LinuxPackageInfoResponse> response) {
  LinuxPackageInfo result;
  if (!response) {
    LOG(ERROR) << "Failed to get Linux package info. Empty response.";
    result.success = false;
    // The error message is currently only used in a console message. If we
    // want to display it to the user, we'd need to localize this.
    result.failure_reason = "D-Bus response was empty.";
    std::move(callback).Run(result);
    return;
  }

  if (!response->success()) {
    LOG(ERROR) << "Failed to get Linux package info: "
               << response->failure_reason();
    result.success = false;
    result.failure_reason = response->failure_reason();
    std::move(callback).Run(result);
    return;
  }

  // The |package_id| field is formatted like "name;version;arch;data". We're
  // currently only interested in name and version.
  std::vector<std::string> split = base::SplitString(
      response->package_id(), ";", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (split.size() < 2 || split[0].empty() || split[1].empty()) {
    LOG(ERROR) << "Linux package info contained invalid package id: \""
               << response->package_id() << '"';
    result.success = false;
    result.failure_reason = "Linux package info contained invalid package id.";
    std::move(callback).Run(result);
    return;
  }

  result.success = true;
  result.package_id = response->package_id();
  result.name = split[0];
  result.version = split[1];
  result.description = response->description();
  result.summary = response->summary();

  std::move(callback).Run(result);
}

void CrostiniManager::OnInstallLinuxPackage(
    InstallLinuxPackageCallback callback,
    std::optional<vm_tools::cicerone::InstallLinuxPackageResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to install Linux package. Empty response.";
    std::move(callback).Run(CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED);
    return;
  }

  if (response->status() ==
      vm_tools::cicerone::InstallLinuxPackageResponse::FAILED) {
    LOG(ERROR) << "Failed to install Linux package: "
               << response->failure_reason();
    std::move(callback).Run(CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED);
    return;
  }

  if (response->status() ==
      vm_tools::cicerone::InstallLinuxPackageResponse::INSTALL_ALREADY_ACTIVE) {
    LOG(WARNING) << "Failed to install Linux package, install already active.";
    std::move(callback).Run(CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE);
    return;
  }

  std::move(callback).Run(CrostiniResult::SUCCESS);
}

void CrostiniManager::RemoveCrostini(std::string vm_name,
                                     RemoveCrostiniCallback callback) {
  AddRemoveCrostiniCallback(std::move(callback));

  auto crostini_remover = base::MakeRefCounted<guest_os::GuestOsRemover>(
      profile_, guest_os::VmType::TERMINA, std::move(vm_name),
      base::BindOnce(&CrostiniManager::OnRemoveCrostini,
                     weak_ptr_factory_.GetWeakPtr()));

  auto abort_callback = base::BarrierClosure(
      restarters_by_container_.size(),
      base::BindOnce(
          [](scoped_refptr<guest_os::GuestOsRemover> remover) {
            content::GetUIThreadTaskRunner({})->PostTask(
                FROM_HERE,
                base::BindOnce(&guest_os::GuestOsRemover::RemoveVm, remover));
          },
          crostini_remover));

  for (const auto& iter : restarters_by_container_) {
    iter.second->Abort(abort_callback);
  }
}

void CrostiniManager::OnRemoveCrostini(
    guest_os::GuestOsRemover::Result result) {
  switch (result) {
    case guest_os::GuestOsRemover::Result::kStopVmNoResponse:
      FinishUninstall(CrostiniResult::STOP_VM_NO_RESPONSE);
      return;
    case guest_os::GuestOsRemover::Result::kStopVmFailed:
      FinishUninstall(CrostiniResult::VM_STOP_FAILED);
      return;
    case guest_os::GuestOsRemover::Result::kDestroyDiskImageFailed:
      FinishUninstall(CrostiniResult::DESTROY_DISK_IMAGE_FAILED);
      return;
    case guest_os::GuestOsRemover::Result::kSuccess:
      // Keep going instead of finishing now.
      break;
  }
  UninstallTermina(base::BindOnce(&CrostiniManager::OnRemoveTermina,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniManager::OnRemoveTermina(bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to uninstall Termina";
    FinishUninstall(CrostiniResult::UNINSTALL_TERMINA_FAILED);
    return;
  }

  profile_->GetPrefs()->SetBoolean(prefs::kCrostiniEnabled, false);
  profile_->GetPrefs()->ClearPref(prefs::kCrostiniLastDiskSize);
  guest_os::RemoveVmFromPrefs(profile_, kCrostiniDefaultVmType);
  profile_->GetPrefs()->ClearPref(prefs::kCrostiniDefaultContainerConfigured);
  UnregisterAllContainers();
  FinishUninstall(CrostiniResult::SUCCESS);
}

void CrostiniManager::FinishUninstall(CrostiniResult result) {
  base::UmaHistogramEnumeration("Crostini.UninstallResult.Reason", result);
  for (auto& callback : remove_crostini_callbacks_) {
    std::move(callback).Run(result);
  }
  remove_crostini_callbacks_.clear();
}

void CrostiniManager::RemoveRestartId(RestartId restart_id) {
  // restarters_by_container_ is handled in RestartCompleted()
  restarters_by_id_.erase(restart_id);
}

void CrostiniManager::RestartCompleted(CrostiniRestarter* restarter,
                                       base::OnceClosure closure) {
  guest_os::GuestId container_id = restarter->container_id();
  restarter = nullptr;
  // Destroy the restarter.
  restarters_by_container_.erase(container_id);

  if (ShouldWarnAboutExpiredVersion(container_id)) {
    CrostiniExpiredContainerWarningView::Show(profile_, std::move(closure));
  } else {
    std::move(closure).Run();
  }
}

void CrostiniManager::OnExportLxdContainer(
    const guest_os::GuestId& container_id,
    std::optional<vm_tools::cicerone::ExportLxdContainerResponse> response) {
  auto it = export_lxd_container_callbacks_.find(container_id);
  if (it == export_lxd_container_callbacks_.end()) {
    LOG(ERROR) << "No export callback for " << container_id;
    return;
  }

  if (!response) {
    LOG(ERROR) << "Failed to export lxd container. Empty response.";
    std::move(it->second)
        .Run(CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED, 0, 0);
    export_lxd_container_callbacks_.erase(it);
    return;
  }

  // If export has started, the callback will be invoked when the
  // ExportLxdContainerProgressSignal signal indicates that export is
  // complete, otherwise this is an error.
  if (response->status() !=
      vm_tools::cicerone::ExportLxdContainerResponse::EXPORTING) {
    LOG(ERROR) << "Failed to export container: status=" << response->status()
               << ", failure_reason=" << response->failure_reason();
    std::move(it->second)
        .Run(CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED, 0, 0);
    export_lxd_container_callbacks_.erase(it);
  }
}

void CrostiniManager::OnExportLxdContainerProgress(
    const vm_tools::cicerone::ExportLxdContainerProgressSignal& signal) {
  using ProgressSignal = vm_tools::cicerone::ExportLxdContainerProgressSignal;

  if (signal.owner_id() != owner_id_) {
    return;
  }

  const guest_os::GuestId container_id(kCrostiniDefaultVmType, signal.vm_name(),
                                       signal.container_name());

  CrostiniResult result;
  switch (signal.status()) {
    case ProgressSignal::EXPORTING_STREAMING: {
      const StreamingExportStatus status{
          .total_files = signal.total_input_files(),
          .total_bytes = signal.total_input_bytes(),
          .exported_files = signal.input_files_streamed(),
          .exported_bytes = signal.input_bytes_streamed()};
      for (auto& observer : export_container_progress_observers_) {
        observer.OnExportContainerProgress(container_id, status);
      }
      return;
    }
    case ProgressSignal::CANCELLED:
      result = CrostiniResult::CONTAINER_EXPORT_IMPORT_CANCELLED;
      break;
    case ProgressSignal::DONE:
      result = CrostiniResult::SUCCESS;
      break;
    default:
      result = CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED;
      LOG(ERROR) << "Failed during export container: " << signal.status()
                 << ", " << signal.failure_reason();
  }

  // Invoke original callback with either success or failure.
  auto it = export_lxd_container_callbacks_.find(container_id);
  if (it == export_lxd_container_callbacks_.end()) {
    LOG(ERROR) << "No export callback for " << container_id;
    return;
  }
  std::move(it->second)
      .Run(result, signal.input_bytes_streamed(), signal.bytes_exported());
  export_lxd_container_callbacks_.erase(it);
}

void CrostiniManager::OnImportLxdContainer(
    const guest_os::GuestId& container_id,
    std::optional<vm_tools::cicerone::ImportLxdContainerResponse> response) {
  auto it = import_lxd_container_callbacks_.find(container_id);
  if (it == import_lxd_container_callbacks_.end()) {
    LOG(ERROR) << "No import callback for " << container_id;
    return;
  }

  if (!response) {
    LOG(ERROR) << "Failed to import lxd container. Empty response.";
    std::move(it->second).Run(CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED);
    import_lxd_container_callbacks_.erase(it);
    return;
  }

  // If import has started, the callback will be invoked when the
  // ImportLxdContainerProgressSignal signal indicates that import is
  // complete, otherwise this is an error.
  if (response->status() !=
      vm_tools::cicerone::ImportLxdContainerResponse::IMPORTING) {
    LOG(ERROR) << "Failed to import container: " << response->failure_reason();
    std::move(it->second).Run(CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED);
    import_lxd_container_callbacks_.erase(it);
  }
}

void CrostiniManager::OnImportLxdContainerProgress(
    const vm_tools::cicerone::ImportLxdContainerProgressSignal& signal) {
  if (signal.owner_id() != owner_id_) {
    return;
  }

  bool call_observers = false;
  bool call_original_callback = false;
  ImportContainerProgressStatus status;
  CrostiniResult result;
  switch (signal.status()) {
    case vm_tools::cicerone::ImportLxdContainerProgressSignal::IMPORTING_UPLOAD:
      call_observers = true;
      status = ImportContainerProgressStatus::UPLOAD;
      break;
    case vm_tools::cicerone::ImportLxdContainerProgressSignal::IMPORTING_UNPACK:
      call_observers = true;
      status = ImportContainerProgressStatus::UNPACK;
      break;
    case vm_tools::cicerone::ImportLxdContainerProgressSignal::CANCELLED:
      call_original_callback = true;
      result = CrostiniResult::CONTAINER_EXPORT_IMPORT_CANCELLED;
      break;
    case vm_tools::cicerone::ImportLxdContainerProgressSignal::DONE:
      call_original_callback = true;
      result = CrostiniResult::SUCCESS;
      break;
    case vm_tools::cicerone::ImportLxdContainerProgressSignal::
        FAILED_ARCHITECTURE:
      call_observers = true;
      status = ImportContainerProgressStatus::FAILURE_ARCHITECTURE;
      call_original_callback = true;
      result = CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_ARCHITECTURE;
      break;
    case vm_tools::cicerone::ImportLxdContainerProgressSignal::FAILED_SPACE:
      call_observers = true;
      status = ImportContainerProgressStatus::FAILURE_SPACE;
      call_original_callback = true;
      result = CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_SPACE;
      break;
    default:
      call_original_callback = true;
      result = CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED;
      LOG(ERROR) << "Failed during import container: " << signal.status()
                 << ", " << signal.failure_reason();
  }

  const guest_os::GuestId container_id(kCrostiniDefaultVmType, signal.vm_name(),
                                       signal.container_name());

  if (call_observers) {
    for (auto& observer : import_container_progress_observers_) {
      observer.OnImportContainerProgress(
          container_id, status, signal.progress_percent(),
          signal.progress_speed(), signal.architecture_device(),
          signal.architecture_container(), signal.available_space(),
          signal.min_required_space());
    }
  }

  // Invoke original callback with either success or failure.
  if (call_original_callback) {
    auto it = import_lxd_container_callbacks_.find(container_id);
    if (it == import_lxd_container_callbacks_.end()) {
      LOG(ERROR) << "No import callback for " << container_id;
      return;
    }
    std::move(it->second).Run(result);
    import_lxd_container_callbacks_.erase(it);
  }
}

void CrostiniManager::OnCancelDiskImageOp(
    const guest_os::GuestId& key,
    std::optional<vm_tools::concierge::CancelDiskImageResponse> response) {
  auto it = disk_image_callbacks_.find(key);
  if (it == disk_image_callbacks_.end()) {
    LOG(ERROR) << "No export callback for " << key;
    return;
  }

  if (!response) {
    LOG(ERROR) << "Failed to cancel disk image operation. Empty response.";
    return;
  }

  if (!response->success()) {
    LOG(ERROR) << "Failed to cancel disk image operation, failure_reason="
               << response->failure_reason();
  }

  auto cb_it = disk_image_callbacks_.find(key);
  if (cb_it == disk_image_callbacks_.end()) {
    LOG(ERROR) << "No export callback for " << key;
    return;
  }

  std::move(cb_it->second).Run(CrostiniResult::DISK_IMAGE_CANCELLED);
  disk_image_callbacks_.erase(cb_it);
  EraseCommandUuid(&disk_image_uuid_to_guest_id_, key.vm_name);
}

void CrostiniManager::OnCancelExportLxdContainer(
    const guest_os::GuestId& key,
    std::optional<vm_tools::cicerone::CancelExportLxdContainerResponse>
        response) {
  auto it = export_lxd_container_callbacks_.find(key);
  if (it == export_lxd_container_callbacks_.end()) {
    LOG(ERROR) << "No export callback for " << key;
    return;
  }

  if (!response) {
    LOG(ERROR) << "Failed to cancel lxd container export. Empty response.";
    return;
  }

  if (response->status() !=
      vm_tools::cicerone::CancelExportLxdContainerResponse::CANCEL_QUEUED) {
    LOG(ERROR) << "Failed to cancel lxd container export:" << " status="
               << response->status()
               << ", failure_reason=" << response->failure_reason();
  }
}

void CrostiniManager::OnCancelImportLxdContainer(
    const guest_os::GuestId& key,
    std::optional<vm_tools::cicerone::CancelImportLxdContainerResponse>
        response) {
  auto it = import_lxd_container_callbacks_.find(key);
  if (it == import_lxd_container_callbacks_.end()) {
    LOG(ERROR) << "No import callback for " << key;
    return;
  }

  if (!response) {
    LOG(ERROR) << "Failed to cancel lxd container import. Empty response.";
    return;
  }

  if (response->status() !=
      vm_tools::cicerone::CancelImportLxdContainerResponse::CANCEL_QUEUED) {
    LOG(ERROR) << "Failed to cancel lxd container import:" << " status="
               << response->status()
               << ", failure_reason=" << response->failure_reason();
  }
}

void CrostiniManager::OnUpgradeContainer(
    CrostiniResultCallback callback,
    std::optional<vm_tools::cicerone::UpgradeContainerResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to start upgrading container. Empty response";
    std::move(callback).Run(CrostiniResult::UPGRADE_CONTAINER_FAILED);
    return;
  }
  CrostiniResult result = CrostiniResult::SUCCESS;
  switch (response->status()) {
    case vm_tools::cicerone::UpgradeContainerResponse::STARTED:
      break;
    case vm_tools::cicerone::UpgradeContainerResponse::ALREADY_RUNNING:
      result = CrostiniResult::UPGRADE_CONTAINER_ALREADY_RUNNING;
      LOG(ERROR) << "Upgrade already running. Nothing to do.";
      break;
    case vm_tools::cicerone::UpgradeContainerResponse::ALREADY_UPGRADED:
      LOG(ERROR) << "Container already upgraded. Nothing to do.";
      result = CrostiniResult::UPGRADE_CONTAINER_ALREADY_UPGRADED;
      break;
    case vm_tools::cicerone::UpgradeContainerResponse::NOT_SUPPORTED:
      result = CrostiniResult::UPGRADE_CONTAINER_NOT_SUPPORTED;
      break;
    case vm_tools::cicerone::UpgradeContainerResponse::UNKNOWN:
    case vm_tools::cicerone::UpgradeContainerResponse::FAILED:
    default:
      result = CrostiniResult::UPGRADE_CONTAINER_FAILED;
      break;
  }
  if (!response->failure_reason().empty()) {
    LOG(ERROR) << "Upgrade container failed. Failure reason: "
               << response->failure_reason();
  }
  std::move(callback).Run(result);
}

void CrostiniManager::OnCancelUpgradeContainer(
    CrostiniResultCallback callback,
    std::optional<vm_tools::cicerone::CancelUpgradeContainerResponse>
        response) {
  if (!response) {
    LOG(ERROR) << "Failed to cancel upgrading container. Empty response";
    std::move(callback).Run(CrostiniResult::CANCEL_UPGRADE_CONTAINER_FAILED);
    return;
  }
  CrostiniResult result = CrostiniResult::SUCCESS;
  switch (response->status()) {
    case vm_tools::cicerone::CancelUpgradeContainerResponse::CANCELLED:
    case vm_tools::cicerone::CancelUpgradeContainerResponse::NOT_RUNNING:
      break;

    case vm_tools::cicerone::CancelUpgradeContainerResponse::UNKNOWN:
    case vm_tools::cicerone::CancelUpgradeContainerResponse::FAILED:
    default:
      LOG(ERROR) << "Cancel upgrade container failed. Failure reason "
                 << response->failure_reason();
      result = CrostiniResult::CANCEL_UPGRADE_CONTAINER_FAILED;
      break;
  }
  std::move(callback).Run(result);
}

void CrostiniManager::OnPendingAppListUpdates(
    const vm_tools::cicerone::PendingAppListUpdatesSignal& signal) {
  guest_os::GuestId container_id(kCrostiniDefaultVmType, signal.vm_name(),
                                 signal.container_name());
  for (auto& observer : pending_app_list_updates_observers_) {
    observer.OnPendingAppListUpdates(container_id, signal.count());
  }
}

// TODO(danielng): Consider handling instant tethering.
void CrostiniManager::ActiveNetworksChanged(
    const std::vector<const ash::NetworkState*>& active_networks) {
  ash::NetworkStateHandler::NetworkStateList active_physical_networks;
  ash::NetworkHandler::Get()
      ->network_state_handler()
      ->GetActiveNetworkListByType(ash::NetworkTypePattern::Physical(),
                                   &active_physical_networks);
  if (active_physical_networks.empty()) {
    return;
  }
  const ash::NetworkState* network = active_physical_networks.at(0);
  if (!network) {
    return;
  }
  const ash::DeviceState* device =
      ash::NetworkHandler::Get()->network_state_handler()->GetDeviceState(
          network->device_path());
  if (!device) {
    return;
  }
  if (CrostiniFeatures::Get()->IsPortForwardingAllowed(profile_)) {
    crostini::CrostiniPortForwarderFactory::GetForProfile(profile_)
        ->ActiveNetworksChanged(device->interface(), network->GetIpAddress());
  }
}

void CrostiniManager::OnShuttingDown() {
  network_state_handler_observer_.Reset();
}

void CrostiniManager::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  if (!crostini_sshfs_->IsSshfsMounted(DefaultContainerId())) {
    return;
  }

  // Block suspend and try to unmount sshfs (https://crbug.com/968060).
  auto token = base::UnguessableToken::Create();
  chromeos::PowerManagerClient::Get()->BlockSuspend(token, "CrostiniManager");
  crostini_sshfs_->UnmountCrostiniFiles(
      DefaultContainerId(),
      base::BindOnce(&CrostiniManager::OnRemoveSshfsCrostiniVolume,
                     weak_ptr_factory_.GetWeakPtr(), token));
}

void CrostiniManager::SuspendDone(base::TimeDelta sleep_duration) {
  // https://crbug.com/968060.  Sshfs is unmounted before suspend,
  // call RestartCrostini to force remount if container is running.
  guest_os::GuestId container_id = DefaultContainerId();
  bool running = guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile_)
                     ->IsRunning(container_id);
  if (running) {
    // TODO(crbug/1142321): Double-check if anything breaks if we change this
    // to just remount the sshfs mounts, in particular check 9p mounts.
    RestartCrostini(container_id, base::DoNothing());
  }
}

void CrostiniManager::OnRemoveSshfsCrostiniVolume(
    base::UnguessableToken power_manager_suspend_token,
    bool result) {
  // Need to let the device suspend after cleaning up. Even if we failed we
  // still unblock suspend since there's nothing else we can do at this point.
  // TODO(crbug/1142321): Success metrics
  chromeos::PowerManagerClient::Get()->UnblockSuspend(
      power_manager_suspend_token);
}

void CrostiniManager::RemoveUncleanSshfsMounts() {
  // TODO(crbug/1142321): Success metrics
  crostini_sshfs_->UnmountCrostiniFiles(DefaultContainerId(),
                                        base::DoNothing());
}

void CrostiniManager::DeallocateForwardedPortsCallback(
    const guest_os::GuestId& container_id) {
  crostini::CrostiniPortForwarderFactory::GetForProfile(profile_)
      ->DeactivateAllActivePorts(container_id);
}

void CrostiniManager::EmitVmDiskTypeMetric(const std::string vm_name) {
  if ((time_of_last_disk_type_metric_ + base::Hours(12)) > base::Time::Now()) {
    // Only bother doing this once every 12 hours. We care about the number of
    // users in each histogram bucket, not the number of times restarted. We
    // do this 12-hourly instead of only at first launch since Crostini can
    // last for a while, and we want to ensure that e.g. looking at N-day
    // aggregation doesn't miss people who've got a long-running session.
    return;
  }
  time_of_last_disk_type_metric_ = base::Time::Now();

  vm_tools::concierge::ListVmDisksRequest request;
  request.set_cryptohome_id(CryptohomeIdForProfile(profile_));
  request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
  request.set_vm_name(vm_name);
  GetConciergeClient()->ListVmDisks(
      std::move(request),
      base::BindOnce(
          [](std::optional<vm_tools::concierge::ListVmDisksResponse> response) {
            if (response) {
              if (response.value().images().size() != 1) {
                LOG(ERROR) << "Got " << response.value().images().size()
                           << " disks for image, don't know how to proceed";
                base::UmaHistogramEnumeration(
                    "Crostini.DiskType", CrostiniDiskImageType::kMultiDisk);
                return;
              }
              auto image = response.value().images().Get(0);
              if (image.image_type() ==
                  vm_tools::concierge::DiskImageType::DISK_IMAGE_QCOW2) {
                base::UmaHistogramEnumeration(
                    "Crostini.DiskType", CrostiniDiskImageType::kQCow2Sparse);
              } else if (image.image_type() ==
                         vm_tools::concierge::DiskImageType::DISK_IMAGE_RAW) {
                if (image.user_chosen_size()) {
                  base::UmaHistogramEnumeration(
                      "Crostini.DiskType",
                      CrostiniDiskImageType::kRawPreallocated);
                } else {
                  base::UmaHistogramEnumeration(
                      "Crostini.DiskType", CrostiniDiskImageType::kRawSparse);
                }
              } else {
                // We shouldn't get back the other disk types for Crostini
                // disks.
                base::UmaHistogramEnumeration("Crostini.DiskType",
                                              CrostiniDiskImageType::kUnknown);
              }
            }
          }));
}

void CrostiniManager::MountCrostiniFiles(guest_os::GuestId container_id,
                                         CrostiniResultCallback callback,
                                         bool background) {
  crostini_sshfs_->MountCrostiniFiles(
      container_id,
      base::BindOnce(
          [](CrostiniResultCallback callback, bool success) {
            std::move(callback).Run(success
                                        ? CrostiniResult::SUCCESS
                                        : CrostiniResult::SSHFS_MOUNT_ERROR);
          },
          std::move(callback)),
      background);
}

void CrostiniManager::MountCrostiniFilesBackground(guest_os::GuestInfo info) {
  MountCrostiniFiles(info.guest_id, base::DoNothing(), true);
}

void CrostiniManager::GetInstallLocation(
    base::OnceCallback<void(base::FilePath)> callback) {
  if (!crostini::CrostiniFeatures::Get()->IsEnabled(profile_)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::FilePath()));
    return;
  }

  InstallTermina(base::BindOnce(
      [](base::WeakPtr<CrostiniManager> weak_this,
         base::OnceCallback<void(base::FilePath)> callback,
         CrostiniResult result) {
        if (result != CrostiniResult::SUCCESS || !weak_this) {
          std::move(callback).Run(base::FilePath());
        } else {
          std::move(callback).Run(
              weak_this->termina_installer_.GetInstallLocation());
        }
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniManager::CallRestarterStartLxdContainerFinishedForTesting(
    CrostiniManager::RestartId id,
    CrostiniResult result) {
  auto container_it = restarters_by_id_.find(id);
  DCHECK(container_it != restarters_by_id_.end());
  auto restarter_it = restarters_by_container_.find(container_it->second);
  DCHECK(restarter_it != restarters_by_container_.end());
  restarter_it->second->StartLxdContainerFinished(result);
}

void CrostiniManager::HandleContainerShutdown(
    const guest_os::GuestId& container_id) {
  // Run all ContainerShutdown observers
  for (auto& observer : container_shutdown_observers_) {
    observer.OnContainerShutdown(container_id);
  }
  if (!IsVmRunning(kCrostiniDefaultVmName)) {
    auto* metrics_service =
        CrostiniMetricsService::Factory::GetForProfile(profile_);
    // This is null in unit tests.
    if (metrics_service) {
      metrics_service->SetBackgroundActive(false);
    }
  }
}

void CrostiniManager::RegisterContainerTerminal(
    const guest_os::GuestId& container_id) {
  if (terminal_provider_ids_.find(container_id) ==
      terminal_provider_ids_.end()) {
    auto* registry = guest_os::GuestOsServiceFactory::GetForProfile(profile_)
                         ->TerminalProviderRegistry();
    terminal_provider_ids_[container_id] = registry->Register(
        std::make_unique<CrostiniTerminalProvider>(profile_, container_id));
  }
}

void CrostiniManager::RegisterContainer(const guest_os::GuestId& container_id) {
  RegisterContainerTerminal(container_id);

  if (CrostiniFeatures::Get()->IsMultiContainerAllowed(profile_) &&
      container_id != DefaultContainerId()) {
    // TODO(b/217469540): The default container is still using sshfs for now,
    // so start off using this approach only for non-default.
    if (mount_provider_ids_.find(container_id) == mount_provider_ids_.end()) {
      auto* registry = guest_os::GuestOsServiceFactory::GetForProfile(profile_)
                           ->MountProviderRegistry();
      mount_provider_ids_[container_id] = registry->Register(
          std::make_unique<CrostiniMountProvider>(profile_, container_id));
    }
  }

  guest_os::GuestOsSharePathFactory::GetForProfile(profile_)->RegisterGuest(
      container_id);
}

void CrostiniManager::UnregisterContainer(
    const guest_os::GuestId& container_id) {
  auto* terminal_registry =
      guest_os::GuestOsServiceFactory::GetForProfile(profile_)
          ->TerminalProviderRegistry();
  auto it = terminal_provider_ids_.find(container_id);
  if (it != terminal_provider_ids_.end()) {
    terminal_registry->Unregister(it->second);
    terminal_provider_ids_.erase(it);
  }

  auto* mount_registry =
      guest_os::GuestOsServiceFactory::GetForProfile(profile_)
          ->MountProviderRegistry();
  it = mount_provider_ids_.find(container_id);
  if (it != mount_provider_ids_.end()) {
    mount_registry->Unregister(it->second);
    mount_provider_ids_.erase(it);
  }

  guest_os::GuestOsSharePathFactory::GetForProfile(profile_)->UnregisterGuest(
      container_id);

  if (container_id == DefaultContainerId()) {
    // For now the upgrade notification only supports the default container. If
    // we're removing that container then destroy any notification we might have
    // for it.
    upgrade_available_notification_.reset();
  }
}

void CrostiniManager::UnregisterAllContainers() {
  auto* terminal_registry =
      guest_os::GuestOsServiceFactory::GetForProfile(profile_)
          ->TerminalProviderRegistry();
  for (const auto& pair : terminal_provider_ids_) {
    terminal_registry->Unregister(pair.second);
  }
  terminal_provider_ids_.clear();

  auto* mount_registry =
      guest_os::GuestOsServiceFactory::GetForProfile(profile_)
          ->MountProviderRegistry();
  for (const auto& pair : mount_provider_ids_) {
    mount_registry->Unregister(pair.second);
  }
  mount_provider_ids_.clear();

  auto* share_service =
      guest_os::GuestOsSharePathFactory::GetForProfile(profile_);
  // Copy the list since we're going to iterate+mutate.
  auto guests = base::flat_set<guest_os::GuestId>(share_service->ListGuests());
  for (const auto& guest : guests) {
    if (guest.vm_type == kCrostiniDefaultVmType) {
      share_service->UnregisterGuest(guest);
    }
  }

  upgrade_available_notification_.reset();
}

bool CrostiniManager::RegisterCreateOptions(
    const guest_os::GuestId& container_id,
    const RestartOptions& options) {
  if (guest_os::GetContainerPrefValue(
          profile_, container_id, guest_os::prefs::kContainerCreateOptions) !=
      nullptr) {
    return false;
  }

  base::Value::Dict new_create_options;

  base::Value::List share_paths;
  for (const base::FilePath& path : options.share_paths) {
    share_paths.Append(path.value());
  }
  new_create_options.Set(prefs::kCrostiniCreateOptionsSharePathsKey,
                         std::move(share_paths));

  if (options.container_username.has_value()) {
    new_create_options.Set(prefs::kCrostiniCreateOptionsContainerUsernameKey,
                           base::Value(options.container_username.value()));
  }
  if (options.disk_size_bytes.has_value()) {
    new_create_options.Set(
        prefs::kCrostiniCreateOptionsDiskSizeBytesKey,
        base::Value(base::NumberToString(options.disk_size_bytes.value())));
  }
  if (options.image_server_url.has_value()) {
    new_create_options.Set(prefs::kCrostiniCreateOptionsImageServerUrlKey,
                           base::Value(options.image_server_url.value()));
  }
  if (options.image_alias.has_value()) {
    new_create_options.Set(prefs::kCrostiniCreateOptionsImageAliasKey,
                           base::Value(options.image_alias.value()));
  }
  if (options.ansible_playbook.has_value()) {
    new_create_options.Set(
        prefs::kCrostiniCreateOptionsAnsiblePlaybookKey,
        base::Value(options.ansible_playbook.value().value()));
  }
  new_create_options.Set(prefs::kCrostiniCreateOptionsUsedKey,
                         base::Value(false));

  guest_os::UpdateContainerPref(profile_, container_id,
                                guest_os::prefs::kContainerCreateOptions,
                                base::Value(std::move(new_create_options)));
  return true;
}

bool CrostiniManager::IsPendingCreation(const guest_os::GuestId& container_id) {
  const base::Value* create_options = guest_os::GetContainerPrefValue(
      profile_, container_id, guest_os::prefs::kContainerCreateOptions);
  if (create_options == nullptr) {
    // Will only reach here if it's a vmc-started container. Treat it as if the
    // create options have already been used.
    return false;
  }

  return !(*create_options->GetDict().FindBool(
      prefs::kCrostiniCreateOptionsUsedKey));
}

void CrostiniManager::SetCreateOptionsUsed(
    const guest_os::GuestId& container_id) {
  const base::Value* create_options_val = guest_os::GetContainerPrefValue(
      profile_, container_id, guest_os::prefs::kContainerCreateOptions);
  if (create_options_val == nullptr) {
    // Should never reach here.
    LOG(ERROR)
        << "create_options_val in SetCreateOptionsUsed is pointing to null.";
    return;
  }

  base::Value::Dict mutable_create_options =
      create_options_val->GetDict().Clone();
  mutable_create_options.Set(prefs::kCrostiniCreateOptionsUsedKey,
                             base::Value(true));
  guest_os::UpdateContainerPref(profile_, container_id,
                                guest_os::prefs::kContainerCreateOptions,
                                base::Value(std::move(mutable_create_options)));
}

bool CrostiniManager::FetchCreateOptions(const guest_os::GuestId& container_id,
                                         RestartOptions* options) {
  DCHECK(options != nullptr);

  const base::Value* create_options_val = guest_os::GetContainerPrefValue(
      profile_, container_id, guest_os::prefs::kContainerCreateOptions);
  if (create_options_val == nullptr) {
    // Should never reach here. If we somehow do, just restart with the given
    // options.
    LOG(ERROR)
        << "create_options_val in FetchCreateOptions is pointing to null.";
    return true;
  }

  const base::Value::Dict& create_options = create_options_val->GetDict();
  for (const auto& path :
       *create_options.FindList(prefs::kCrostiniCreateOptionsSharePathsKey)) {
    options->share_paths.emplace_back(path.GetString());
  }

  if (create_options.Find(prefs::kCrostiniCreateOptionsContainerUsernameKey)) {
    options->container_username = *create_options.FindString(
        prefs::kCrostiniCreateOptionsContainerUsernameKey);
  }
  if (create_options.Find(prefs::kCrostiniCreateOptionsDiskSizeBytesKey)) {
    int64_t size;
    base::StringToInt64(*create_options.FindString(
                            prefs::kCrostiniCreateOptionsDiskSizeBytesKey),
                        &size);
    options->disk_size_bytes = size;
  }
  if (create_options.Find(prefs::kCrostiniCreateOptionsImageServerUrlKey)) {
    options->image_server_url = *create_options.FindString(
        prefs::kCrostiniCreateOptionsImageServerUrlKey);
  }
  if (create_options.Find(prefs::kCrostiniCreateOptionsImageAliasKey)) {
    options->image_alias =
        *create_options.FindString(prefs::kCrostiniCreateOptionsImageAliasKey);
  }
  if (create_options.Find(prefs::kCrostiniCreateOptionsAnsiblePlaybookKey)) {
    options->ansible_playbook = base::FilePath(*create_options.FindString(
        prefs::kCrostiniCreateOptionsAnsiblePlaybookKey));
  }

  return *create_options.FindBool(prefs::kCrostiniCreateOptionsUsedKey);
}

bool CrostiniManager::ShouldWarnAboutExpiredVersion(
    const guest_os::GuestId& container_id) {
  if (already_warned_expired_version_) {
    return false;
  }
  if (!CrostiniFeatures::Get()->IsContainerUpgradeUIAllowed(profile_)) {
    return false;
  }
  if (container_id != DefaultContainerId()) {
    return false;
  }
  // If the warning dialog is already open we can add more callbacks to it, but
  // if we've moved to the upgrade dialog proper we should run them now as they
  // may be part of the upgrade process.
  if (ash::SystemWebDialogDelegate::FindInstance(
          GURL{chrome::kChromeUICrostiniUpgraderUrl}.spec())) {
    return false;
  }

  if (!IsContainerVersionExpired(profile_, container_id)) {
    return false;
  }

  already_warned_expired_version_ = true;
  return true;
}

}  // namespace crostini
