// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_session_impl.h"

#include <fcntl.h>
#include <grp.h>
#include <poll.h>
#include <unistd.h>

#include <utility>
#include <vector>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_host_impl.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/mojo_init_data.h"
#include "ash/components/arc/session/mojo_invitation_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/process_metrics.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "chromeos/ash/components/system/scheduler_configuration_manager_base.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/channel.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"

namespace arc {

namespace {

constexpr char kOn[] = "on";
constexpr char kOff[] = "off";

// Used to classify device based on memory available, 4G, 8GB, 16GB.
constexpr int kClassify4GbDeviceInKb = 3500000;
constexpr int kClassify8GbDeviceInKb = 7500000;
constexpr int kClassify16GbDeviceInKb = 15500000;

// Waits until `raw_socket_fd` is readable.
// The operation may be cancelled originally triggered by user interaction to
// disable ARC, or ARC instance is unexpectedly stopped (e.g. crash).
// To notify such a situation, `raw_cancel_fd` is also passed to here, and the
// write side will be closed in such a case.
bool WaitForSocketReadable(int raw_socket_fd, int raw_cancel_fd) {
  struct pollfd fds[2] = {
      {raw_socket_fd, POLLIN, 0},
      {raw_cancel_fd, POLLIN, 0},
  };

  if (HANDLE_EINTR(poll(fds, std::size(fds), -1)) <= 0) {
    PLOG(ERROR) << "poll()";
    return false;
  }

  if (fds[1].revents) {
    // Notified that Stop() is invoked. Cancel the Mojo connecting.
    VLOG(1) << "Stop() was called during ConnectMojo()";
    return false;
  }

  DCHECK(fds[0].revents);
  return true;
}

// Applies dalvik memory profile to the ARC mini instance start params.
// Profile is determined based on enable feature and available memory on the
// device. Possible profiles 16G,8G and 4G. For low memory devices dalvik
// profile is not overridden. If `memory_stat_file_for_testing` is set,
// it specifies the file to read in tests instead of /proc/meminfo in
// production.
void ApplyDalvikMemoryProfile(
    ArcSessionImpl::SystemMemoryInfoCallback system_memory_info_callback,
    StartParams* params) {
  base::SystemMemoryInfoKB mem_info;
  if (!system_memory_info_callback.Run(&mem_info)) {
    LOG(ERROR) << "Failed to get system memory info";
    return;
  }

  std::string log_profile_name;
  if (mem_info.total >= kClassify16GbDeviceInKb) {
    params->dalvik_memory_profile = StartParams::DalvikMemoryProfile::M16G;
    log_profile_name = "high-memory 16G";
  } else if (mem_info.total >= kClassify8GbDeviceInKb) {
    params->dalvik_memory_profile = StartParams::DalvikMemoryProfile::M8G;
    log_profile_name = "high-memory 8G";
  } else if (mem_info.total >= kClassify4GbDeviceInKb) {
    params->dalvik_memory_profile = StartParams::DalvikMemoryProfile::M4G;
    log_profile_name = "high-memory 4G";
  } else {
    params->dalvik_memory_profile = StartParams::DalvikMemoryProfile::DEFAULT;
    log_profile_name = "default low-memory";
  }
  VLOG(1) << "Applied " << log_profile_name << " profile for the "
          << (mem_info.total / 1024) << "Mb device.";
}

void ApplyHostUreadaheadMode(StartParams* params) {
  // Check if deprecated flags are in use, override later if necessary
  const arc::ArcUreadaheadMode mode =
      arc::GetArcUreadaheadMode(ash::switches::kArcHostUreadaheadMode);
  switch (mode) {
    case arc::ArcUreadaheadMode::READAHEAD: {
      params->host_ureadahead_mode =
          StartParams::HostUreadaheadMode::MODE_READAHEAD;
      break;
    }
    case arc::ArcUreadaheadMode::GENERATE: {
      params->host_ureadahead_mode =
          StartParams::HostUreadaheadMode::MODE_GENERATE;
      break;
    }
    case arc::ArcUreadaheadMode::DISABLED: {
      params->host_ureadahead_mode =
          StartParams::HostUreadaheadMode::MODE_DISABLED;
      break;
    }
    default: {
      NOTREACHED();
    }
  }
}

void ApplyDisableDownloadProvider(StartParams* params) {
  params->disable_download_provider =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kArcDisableDownloadProvider);
}

void ApplyUseDevCaches(StartParams* params) {
  params->use_dev_caches = IsArcUseDevCaches();
}

// Real Delegate implementation to connect Mojo.
class ArcSessionDelegateImpl : public ArcSessionImpl::Delegate {
 public:
  ArcSessionDelegateImpl(ArcBridgeService* arc_bridge_service,
                         version_info::Channel channel);

  ArcSessionDelegateImpl(const ArcSessionDelegateImpl&) = delete;
  ArcSessionDelegateImpl& operator=(const ArcSessionDelegateImpl&) = delete;

  ~ArcSessionDelegateImpl() override = default;

  // ArcSessionImpl::Delegate override.
  void CreateSocket(CreateSocketCallback callback) override;

  base::ScopedFD ConnectMojo(base::ScopedFD socket_fd,
                             ConnectMojoCallback callback) override;
  void GetFreeDiskSpace(GetFreeDiskSpaceCallback callback) override;
  version_info::Channel GetChannel() override;
  std::unique_ptr<ArcClientAdapter> CreateClient() override;

 private:
  // Synchronously create a UNIX domain socket. This is designed to run on a
  // blocking thread. Unlinks any existing files at socket address.
  static base::ScopedFD CreateSocketInternal();

  // Synchronously accepts a connection on `server_endpoint` and then processes
  // the connected socket's file descriptor. This is designed to run on a
  // blocking thread.
  static std::unique_ptr<MojoInvitationManager> ConnectMojoInternal(
      base::ScopedFD socket_fd,
      base::ScopedFD cancel_fd);

  // Called when Mojo connection is established or canceled.
  // In case of cancel or error, `server_pipe` is invalid.
  void OnMojoConnected(
      ConnectMojoCallback callback,
      std::unique_ptr<ArcBridgeHostImpl> host,
      std::unique_ptr<MojoInvitationManager> invitation_manager);

  // Owned by ArcServiceManager.
  const raw_ptr<ArcBridgeService> arc_bridge_service_;

  const version_info::Channel channel_;

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<ArcSessionDelegateImpl> weak_factory_{this};
};

ArcSessionDelegateImpl::ArcSessionDelegateImpl(
    ArcBridgeService* arc_bridge_service,
    version_info::Channel channel)
    : arc_bridge_service_(arc_bridge_service), channel_(channel) {}

void ArcSessionDelegateImpl::CreateSocket(CreateSocketCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ArcSessionDelegateImpl::CreateSocketInternal),
      std::move(callback));
}

base::ScopedFD ArcSessionDelegateImpl::ConnectMojo(
    base::ScopedFD socket_fd,
    ConnectMojoCallback callback) {
  // Prepare a pipe so that AcceptInstanceConnection can be interrupted on
  // Stop().
  base::ScopedFD cancel_fd;
  base::ScopedFD return_fd;
  if (!base::CreatePipe(&cancel_fd, &return_fd, true)) {
    PLOG(ERROR) << "Failed to create a pipe to cancel accept()";
    return base::ScopedFD();
  }

  // For production, `socket_fd` passed from session_manager is either a valid
  // socket or a valid file descriptor (/dev/null). For testing, `socket_fd`
  // might be invalid.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ArcSessionDelegateImpl::ConnectMojoInternal,
                     std::move(socket_fd), std::move(cancel_fd)),
      base::BindOnce(&ArcSessionDelegateImpl::OnMojoConnected,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::make_unique<ArcBridgeHostImpl>(arc_bridge_service_)));
  return return_fd;
}

void ArcSessionDelegateImpl::GetFreeDiskSpace(
    GetFreeDiskSpaceCallback callback) {
  ash::SpacedClient::Get()->GetFreeDiskSpace("/home/chronos/user",
                                             std::move(callback));
}

version_info::Channel ArcSessionDelegateImpl::GetChannel() {
  return channel_;
}

std::unique_ptr<ArcClientAdapter> ArcSessionDelegateImpl::CreateClient() {
  return ArcClientAdapter::Create();
}

// static
base::ScopedFD ArcSessionDelegateImpl::CreateSocketInternal() {
  constexpr char kArcBridgeSocketPath[] = "/run/chrome/arc_bridge.sock";
  constexpr char kArcVmBridgeSocketPath[] = "/run/chrome/arc/arc_bridge.sock";
  constexpr char kArcBridgeSocketGroup[] = "arc-bridge";

  const base::FilePath socket_path(IsArcVmEnabled() ? kArcVmBridgeSocketPath
                                                    : kArcBridgeSocketPath);
  if (IsArcVmEnabled()) {
    base::File::Error error;
    const base::FilePath socket_dir(socket_path.DirName());
    if (!base::CreateDirectoryAndGetError(socket_dir, &error)) {
      LOG(ERROR) << "Failed to create " << socket_dir << ": "
                 << base::File::ErrorToString(error);
      return base::ScopedFD();
    }
    if (!base::SetPosixFilePermissions(socket_dir, 0700)) {
      PLOG(ERROR) << "Could not set permissions: " << socket_dir;
      return base::ScopedFD();
    }
  }

  auto endpoint = mojo::NamedPlatformChannel({socket_path.value()});
  // TODO(cmtm): use NamedPlatformChannel to bootstrap mojo connection after
  // libchrome uprev in android.
  base::ScopedFD socket_fd =
      endpoint.TakeServerEndpoint().TakePlatformHandle().TakeFD();
  if (!socket_fd.is_valid()) {
    LOG(ERROR) << "Socket creation failed";
    return socket_fd;
  }

  // Change permissions on the socket. Note that since arcvm doesn't directly
  // share the socket with ARC, it can use 0600 and the default group. arcvm
  // build doesn't have `kArcBridgeSocketGroup` in the first place.
  if (!IsArcVmEnabled()) {
    struct group arc_bridge_group;
    struct group* arc_bridge_group_res = nullptr;
    int ret = 0;
    char buf[10000];
    do {
      ret = getgrnam_r(kArcBridgeSocketGroup, &arc_bridge_group, buf,
                       sizeof(buf), &arc_bridge_group_res);
    } while (ret == EINTR);
    if (ret != 0) {
      LOG(ERROR) << "getgrnam_r: " << strerror_r(ret, buf, sizeof(buf));
      return base::ScopedFD();
    }

    if (!arc_bridge_group_res) {
      LOG(ERROR) << "Group '" << kArcBridgeSocketGroup << "' not found";
      return base::ScopedFD();
    }

    if (chown(socket_path.value().c_str(), -1, arc_bridge_group.gr_gid) < 0) {
      PLOG(ERROR) << "chown failed";
      return base::ScopedFD();
    }
  }

  if (!base::SetPosixFilePermissions(socket_path,
                                     IsArcVmEnabled() ? 0600 : 0660)) {
    PLOG(ERROR) << "Could not set permissions: " << socket_path;
    return base::ScopedFD();
  }

  return socket_fd;
}

// static
std::unique_ptr<MojoInvitationManager>
ArcSessionDelegateImpl::ConnectMojoInternal(base::ScopedFD socket_fd,
                                            base::ScopedFD cancel_fd) {
  if (!WaitForSocketReadable(socket_fd.get(), cancel_fd.get())) {
    VLOG(1) << "Mojo connection was cancelled.";
    return nullptr;
  }

  base::ScopedFD connection_fd;
  if (!mojo::AcceptSocketConnection(socket_fd.get(), &connection_fd,
                                    /* check_peer_user = */ false) ||
      !connection_fd.is_valid()) {
    return nullptr;
  }

  // Send Mojo invitation to ARCVM.
  auto invitation_manager = std::make_unique<MojoInvitationManager>();
  mojo::PlatformChannel channel;
  MojoInitData mojo_init_data;
  invitation_manager->SendInvitation(channel, mojo_init_data.token());

  std::vector<base::ScopedFD> fds;
  fds.emplace_back(channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD());

  std::vector<iovec> data_arr = mojo_init_data.AsIOvecVector();
  if (mojo::SendmsgWithHandles(connection_fd.get(), data_arr.data(),
                               data_arr.size(), fds) == -1) {
    PLOG(ERROR) << "sendmsg";
    return nullptr;
  }

  return invitation_manager;
}

void ArcSessionDelegateImpl::OnMojoConnected(
    ConnectMojoCallback callback,
    std::unique_ptr<ArcBridgeHostImpl> host,
    std::unique_ptr<MojoInvitationManager> invitation_manager) {
  if (!invitation_manager) {
    LOG(ERROR) << "Invalid pipe";
    std::move(callback).Run(nullptr, nullptr);
    return;
  }

  host->AddReceiver(mojo::PendingReceiver<mojom::ArcBridgeHost>(
      invitation_manager->TakePipe()));
  std::move(callback).Run(std::move(host), std::move(invitation_manager));
}

}  // namespace

// static
std::unique_ptr<ArcSessionImpl::Delegate> ArcSessionImpl::CreateDelegate(
    ArcBridgeService* arc_bridge_service,
    version_info::Channel channel) {
  return std::make_unique<ArcSessionDelegateImpl>(arc_bridge_service, channel);
}

ArcSessionImpl::ArcSessionImpl(
    std::unique_ptr<Delegate> delegate,
    ash::SchedulerConfigurationManagerBase* scheduler_configuration_manager,
    AdbSideloadingAvailabilityDelegate* adb_sideloading_availability_delegate)
    : delegate_(std::move(delegate)),
      client_(delegate_->CreateClient()),
      scheduler_configuration_manager_(scheduler_configuration_manager),
      adb_sideloading_availability_delegate_(
          adb_sideloading_availability_delegate),
      system_memory_info_callback_(
          base::BindRepeating(&base::GetSystemMemoryInfo)) {
  DCHECK(client_);
  client_->AddObserver(this);
}

ArcSessionImpl::~ArcSessionImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(state_ == State::NOT_STARTED || state_ == State::STOPPED);
  client_->RemoveObserver(this);
  if (scheduler_configuration_manager_)  // for testing
    scheduler_configuration_manager_->RemoveObserver(this);
}

void ArcSessionImpl::StartMiniInstance() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(state_, State::NOT_STARTED);

  const auto& last_reply = scheduler_configuration_manager_->GetLastReply();
  if (last_reply) {
    state_ = State::STARTING_MINI_INSTANCE;
    DoStartMiniInstance(last_reply->first ? last_reply->second : 0);
  } else {
    state_ = State::WAITING_FOR_NUM_CORES;
    scheduler_configuration_manager_->AddObserver(this);
  }
}

void ArcSessionImpl::DoStartMiniInstance(size_t num_cores_disabled) {
  DCHECK_GT(lcd_density_, 0);
  StartParams params;
  params.native_bridge_experiment =
      base::FeatureList::IsEnabled(arc::kNativeBridgeToggleFeature);
  params.arc_file_picker_experiment =
      base::FeatureList::IsEnabled(arc::kFilePickerExperimentFeature);
  // Enable Custom Tabs only on Dev and Canary.
  const bool is_custom_tab_enabled =
      base::FeatureList::IsEnabled(arc::kCustomTabsExperimentFeature) &&
      delegate_->GetChannel() != version_info::Channel::STABLE &&
      delegate_->GetChannel() != version_info::Channel::BETA;
  params.arc_custom_tabs_experiment = is_custom_tab_enabled;
  params.lcd_density = lcd_density_;
  params.num_cores_disabled = num_cores_disabled;
  params.enable_tts_caching = true;
  params.enable_consumer_auto_update_toggle = base::FeatureList::IsEnabled(
      ash::features::kConsumerAutoUpdateToggleAllowed);
  params.enable_privacy_hub_for_chrome =
      base::FeatureList::IsEnabled(ash::features::kCrosPrivacyHub);
  params.arc_switch_to_keymint = ShouldUseArcKeyMint();
  params.enable_arc_attestation = ShouldUseArcAttestation();
  params.use_virtio_blk_data = use_virtio_blk_data_;
  params.arc_signed_in = arc_signed_in_;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kArcPlayStoreAutoUpdate)) {
    const std::string value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            ash::switches::kArcPlayStoreAutoUpdate);
    if (value == kOn) {
      params.play_store_auto_update =
          StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_ON;
      VLOG(1) << "Play Store auto-update is forced on";
    } else if (value == kOff) {
      params.play_store_auto_update =
          StartParams::PlayStoreAutoUpdate::AUTO_UPDATE_OFF;
      VLOG(1) << "Play Store auto-update is forced off";
    } else {
      LOG(ERROR) << "Invalid parameter " << value << " for "
                 << ash::switches::kArcPlayStoreAutoUpdate;
    }
  }

  params.disable_media_store_maintenance =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kArcDisableMediaStoreMaintenance);
  if (params.disable_media_store_maintenance)
    VLOG(1) << "MediaStore maintenance task(s) are disabled";

  params.arc_generate_play_auto_install =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kArcGeneratePlayAutoInstall);

  VLOG(1) << "Starting ARC mini instance with lcd_density="
          << params.lcd_density
          << ", num_cores_disabled=" << params.num_cores_disabled
          << ", arc_signed_in=" << params.arc_signed_in;

  ApplyDalvikMemoryProfile(system_memory_info_callback_, &params);
  ApplyDisableDownloadProvider(&params);
  ApplyUseDevCaches(&params);
  ApplyHostUreadaheadMode(&params);

  client_->StartMiniArc(std::move(params),
                        base::BindOnce(&ArcSessionImpl::OnMiniInstanceStarted,
                                       weak_factory_.GetWeakPtr()));
}

void ArcSessionImpl::SetSystemMemoryInfoCallbackForTesting(
    SystemMemoryInfoCallback callback) {
  system_memory_info_callback_ = callback;
}

void ArcSessionImpl::RequestUpgrade(UpgradeParams params) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!params.locale.empty());

  upgrade_requested_ = true;
  upgrade_params_ = std::move(params);

  VLOG(1) << "Upgrade requested. state: " << state_;

  switch (state_) {
    case State::NOT_STARTED:
      NOTREACHED();
    case State::WAITING_FOR_NUM_CORES:
    case State::STARTING_MINI_INSTANCE:
      // OnMiniInstanceStarted() will restart a full instance.
      break;
    case State::RUNNING_MINI_INSTANCE:
      DoUpgrade();
      break;
    case State::STARTING_FULL_INSTANCE:
    case State::CONNECTING_MOJO:
    case State::RUNNING_FULL_INSTANCE:
    case State::STOPPED:
      // These mean RequestUpgrade() is called twice or called after
      // stopped, which are invalid operations.
      NOTREACHED();
  }
}

void ArcSessionImpl::OnMiniInstanceStarted(bool result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(state_, State::STARTING_MINI_INSTANCE);

  if (!result) {
    LOG(ERROR) << "Failed to start ARC mini container";
    OnStopped(ArcStopReason::GENERIC_BOOT_FAILURE);
    return;
  }

  VLOG(2) << "ARC mini container has been successfully started.";
  state_ = State::RUNNING_MINI_INSTANCE;

  if (stop_requested_) {
    // The ARC instance has started to run. Request to stop.
    StopArcInstance(/*on_shutdown=*/false, /*should_backup_log=*/false);
    return;
  }

  if (upgrade_requested_)
    // RequestUpgrade() has been called during the D-Bus call.
    DoUpgrade();
}

void ArcSessionImpl::DoUpgrade() {
  DCHECK_EQ(state_, State::RUNNING_MINI_INSTANCE);

  VLOG(2) << "Upgrading an existing ARC mini instance";
  state_ = State::STARTING_FULL_INSTANCE;

  // Getting the free disk space doesn't take long.
  delegate_->GetFreeDiskSpace(base::BindOnce(&ArcSessionImpl::OnFreeDiskSpace,
                                             weak_factory_.GetWeakPtr()));
}

void ArcSessionImpl::OnFreeDiskSpace(std::optional<int64_t> space) {
  // Ensure there's sufficient space on disk for the container.
  if (!space.has_value()) {
    LOG(ERROR) << "Could not determine free disk space";
    StopArcInstance(/*on_shutdown=*/false, /*should_backup_log=*/false);
    return;
  } else if (space.value() < kMinimumFreeDiskSpaceBytes) {
    VLOG(1) << "There is not enough disk space to start the ARC container";
    insufficient_disk_space_ = true;
    StopArcInstance(/*on_shutdown=*/false, /*should_backup_log=*/false);
    return;
  }

  adb_sideloading_availability_delegate_->CanChangeAdbSideloading(
      base::BindOnce(&ArcSessionImpl::OnCanChangeAdbSideloading,
                     weak_factory_.GetWeakPtr()));
}

void ArcSessionImpl::OnCanChangeAdbSideloading(
    bool can_change_adb_sideloading) {
  upgrade_params_.is_managed_adb_sideloading_allowed =
      can_change_adb_sideloading;

  delegate_->CreateSocket(base::BindOnce(&ArcSessionImpl::OnSocketCreated,
                                         weak_factory_.GetWeakPtr()));
}

void ArcSessionImpl::OnSocketCreated(base::ScopedFD socket_fd) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(state_, State::STARTING_FULL_INSTANCE);

  if (stop_requested_) {
    // The ARC instance has started to run. Request to stop.
    VLOG(1) << "Stop() called while creating socket";
    StopArcInstance(/*on_shutdown=*/false, /*should_backup_log=*/false);
    return;
  }

  if (!socket_fd.is_valid()) {
    LOG(ERROR) << "ARC: Error creating socket";
    StopArcInstance(/*on_shutdown=*/false, /*should_backup_log=*/false);
    return;
  }

  VLOG(2) << "Socket is created. Starting ARC container";
  client_->UpgradeArc(
      std::move(upgrade_params_),
      base::BindOnce(&ArcSessionImpl::OnUpgraded, weak_factory_.GetWeakPtr(),
                     std::move(socket_fd)));
}

void ArcSessionImpl::OnUpgraded(base::ScopedFD socket_fd, bool result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(state_, State::STARTING_FULL_INSTANCE);

  if (!result) {
    LOG(ERROR) << "Failed to upgrade ARC container";
    return;
  }

  VLOG(2) << "ARC instance is successfully upgraded.";

  if (stop_requested_) {
    // The ARC instance has started to run. Request to stop.
    StopArcInstance(/*on_shutdown=*/false, /*should_backup_log=*/false);
    return;
  }

  VLOG(2) << "Connecting mojo...";
  state_ = State::CONNECTING_MOJO;
  accept_cancel_pipe_ = delegate_->ConnectMojo(
      std::move(socket_fd), base::BindOnce(&ArcSessionImpl::OnMojoConnected,
                                           weak_factory_.GetWeakPtr()));
  if (!accept_cancel_pipe_.is_valid()) {
    // Failed to post a task to accept() the request.
    StopArcInstance(/*on_shutdown=*/false, /*should_backup_log=*/false);
    return;
  }
}

void ArcSessionImpl::OnMojoConnected(
    std::unique_ptr<mojom::ArcBridgeHost> arc_bridge_host,
    std::unique_ptr<MojoInvitationManager> invitation_manager) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(state_, State::CONNECTING_MOJO);
  accept_cancel_pipe_.reset();

  if (stop_requested_) {
    StopArcInstance(/*on_shutdown=*/false, /*should_backup_log=*/false);
    return;
  }

  if (!arc_bridge_host.get()) {
    LOG(ERROR) << "Invalid pipe.";
    // If we can't establish the connection with ARC bridge, it could
    // be a problem inside ARC thus setting `should_backup_log` to back up log
    // before container is shutdown.
    StopArcInstance(/*on_shutdown=*/false, /*should_backup_log*/ true);
    return;
  }
  arc_bridge_host_ = std::move(arc_bridge_host);
  mojo_invitation_manager_ = std::move(invitation_manager);

  VLOG(0) << "ARC ready.";
  state_ = State::RUNNING_FULL_INSTANCE;
}

void ArcSessionImpl::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(2) << "Stopping ARC session is requested.";

  // For second time or later, just do nothing.
  // It is already in the stopping phase.
  if (stop_requested_) {
    return;
  }

  stop_requested_ = true;
  arc_bridge_host_.reset();
  switch (state_) {
    case State::WAITING_FOR_NUM_CORES:
      if (scheduler_configuration_manager_)  // for testing
        scheduler_configuration_manager_->RemoveObserver(this);
      [[fallthrough]];
    case State::NOT_STARTED:
      // If `Stop()` is called while waiting for LCD density or CPU cores
      // information, it can directly move to stopped state.
      VLOG(1) << "ARC session is not started. state: " << state_;
      OnStopped(ArcStopReason::SHUTDOWN);
      return;
    case State::STARTING_MINI_INSTANCE:
    case State::STARTING_FULL_INSTANCE:
      // Before starting the ARC instance, we do nothing here.
      // At some point, a callback will be invoked on UI thread,
      // and stopping procedure will be run there.
      // On Chrome shutdown, it is not the case because the message loop is
      // already stopped here. Practically, it is not a problem because;
      // - On starting instance, the container instance can be leaked.
      // Practically it is not problematic because the session manager will
      // clean it up.
      return;

    case State::RUNNING_MINI_INSTANCE:
    case State::RUNNING_FULL_INSTANCE:
      // An ARC {mini,full} instance is running. Request to stop it.
      StopArcInstance(/*on_shutdown=*/false, /*should_backup_log=*/false);
      return;

    case State::CONNECTING_MOJO:
      // Mojo connection is being waited on ThreadPool's thread.
      // Request to cancel it. Following stopping procedure will run
      // in its callback.
      accept_cancel_pipe_.reset();
      return;

    case State::STOPPED:
      VLOG(1) << "ARC session is already stopped.";
      // The instance is already stopped. Do nothing.
      return;
  }
}

void ArcSessionImpl::StopArcInstance(bool on_shutdown, bool should_backup_log) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(state_ == State::WAITING_FOR_NUM_CORES ||
         state_ == State::STARTING_MINI_INSTANCE ||
         state_ == State::RUNNING_MINI_INSTANCE ||
         state_ == State::STARTING_FULL_INSTANCE ||
         state_ == State::CONNECTING_MOJO ||
         state_ == State::RUNNING_FULL_INSTANCE);

  VLOG(1) << "Requesting session_manager to stop ARC instance"
          << " on_shutdown: " << on_shutdown
          << " should_backup_log: " << should_backup_log;

  // When the instance is full instance, change the `state_` in
  // ArcInstanceStopped().
  client_->StopArcInstance(on_shutdown, should_backup_log);
}

void ArcSessionImpl::ArcInstanceStopped(bool is_system_shutdown) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(state_, State::STARTING_MINI_INSTANCE);
  VLOG(1) << "Notified that ARC instance is stopped";

  // In case that crash happens during before the Mojo channel is connected,
  // unlock the ThreadPool's thread.
  accept_cancel_pipe_.reset();

  ArcStopReason reason;
  if (stop_requested_ || is_system_shutdown) {
    // If the ARC instance is stopped after its explicit request or as part of
    // system shutdown, return SHUTDOWN.
    reason = ArcStopReason::SHUTDOWN;
  } else if (insufficient_disk_space_) {
    // ARC mini container is stopped because of upgarde failure due to low
    // disk space.
    reason = ArcStopReason::LOW_DISK_SPACE;
  } else if (state_ == State::STARTING_FULL_INSTANCE ||
             state_ == State::CONNECTING_MOJO) {
    // If the ARC instance is stopped during the upgrade, but it is not
    // explicitly requested, return GENERIC_BOOT_FAILURE for the case.
    reason = ArcStopReason::GENERIC_BOOT_FAILURE;
  } else {
    // Otherwise, this is caused by CRASH occured inside of the ARC instance.
    reason = ArcStopReason::CRASH;
  }
  OnStopped(reason);
}

bool ArcSessionImpl::IsStopRequested() {
  return stop_requested_;
}

void ArcSessionImpl::OnStopped(ArcStopReason reason) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // OnStopped() should be called once per instance.
  DCHECK_NE(state_, State::STOPPED);
  VLOG(1) << "ARC session is stopped. reason: " << reason
          << " state: " << state_;

  const bool was_running = (state_ == State::RUNNING_FULL_INSTANCE);
  arc_bridge_host_.reset();
  mojo_invitation_manager_.reset();
  state_ = State::STOPPED;
  for (auto& observer : observer_list_)
    observer.OnSessionStopped(reason, was_running, upgrade_requested_);
}

void ArcSessionImpl::OnShutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  stop_requested_ = true;
  if (state_ == State::STOPPED) {
    return;
  }

  // Here, the message loop is already stopped, and the Chrome will be soon
  // shutdown. Thus, it is not necessary to take care about restarting case.
  // If ArcSession is waiting for mojo connection, cancels it.
  accept_cancel_pipe_.reset();

  // Stops the ARC instance to let it graceful shutdown.
  // Note that this may fail if ARC container is not actually running, but
  // ignore an error as described below.
  if (state_ == State::STARTING_MINI_INSTANCE ||
      state_ == State::RUNNING_MINI_INSTANCE ||
      state_ == State::STARTING_FULL_INSTANCE ||
      state_ == State::CONNECTING_MOJO ||
      state_ == State::RUNNING_FULL_INSTANCE) {
    StopArcInstance(/*on_shutdown=*/true, /*should_backup_log*/ false);
  }

  // Directly set to the STOPPED state by OnStopped(). Note that calling
  // StopArcInstance() may not work well. At least, because the UI thread is
  // already stopped here, ArcInstanceStopped() callback cannot be invoked.
  OnStopped(ArcStopReason::SHUTDOWN);
}

void ArcSessionImpl::SetUserInfo(
    const cryptohome::Identification& cryptohome_id,
    const std::string& hash,
    const std::string& serial_number) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->SetUserInfo(cryptohome_id, hash, serial_number);
}

void ArcSessionImpl::SetDemoModeDelegate(
    ArcClientAdapter::DemoModeDelegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->SetDemoModeDelegate(delegate);
}

void ArcSessionImpl::TrimVmMemory(TrimVmMemoryCallback callback,
                                  int page_limit) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->TrimVmMemory(std::move(callback), page_limit);
}

void ArcSessionImpl::SetDefaultDeviceScaleFactor(float scale_factor) {
  lcd_density_ = GetLcdDensityForDeviceScaleFactor(scale_factor);
  DCHECK_GT(lcd_density_, 0);
}

void ArcSessionImpl::SetUseVirtioBlkData(bool use_virtio_blk_data) {
  use_virtio_blk_data_ = use_virtio_blk_data;
}

void ArcSessionImpl::SetArcSignedIn(bool arc_signed_in) {
  arc_signed_in_ = arc_signed_in;
}

void ArcSessionImpl::OnConfigurationSet(bool success,
                                        size_t num_cores_disabled) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(state_, State::WAITING_FOR_NUM_CORES);

  scheduler_configuration_manager_->RemoveObserver(this);
  state_ = State::STARTING_MINI_INSTANCE;

  // Note: On non-x86_64 devices, the configuration request to debugd always
  // fails. It is WAI, and to support that case, don't log anything even when
  // `success` is false. `num_cores_disabled` is always set regardless of
  // where the call is successful.
  DoStartMiniInstance(num_cores_disabled);
}

std::ostream& operator<<(std::ostream& os, ArcSessionImpl::State state) {
#define MAP_STATE(name)             \
  case ArcSessionImpl::State::name: \
    return os << #name

  switch (state) {
    MAP_STATE(NOT_STARTED);
    MAP_STATE(WAITING_FOR_NUM_CORES);
    MAP_STATE(STARTING_MINI_INSTANCE);
    MAP_STATE(RUNNING_MINI_INSTANCE);
    MAP_STATE(STARTING_FULL_INSTANCE);
    MAP_STATE(CONNECTING_MOJO);
    MAP_STATE(RUNNING_FULL_INSTANCE);
    MAP_STATE(STOPPED);
  }
#undef MAP_STATE

  // Some compilers report an error even if all values of an enum-class are
  // covered exhaustively in a switch statement.
  NOTREACHED() << "Invalid value " << static_cast<int>(state);
}

}  // namespace arc
