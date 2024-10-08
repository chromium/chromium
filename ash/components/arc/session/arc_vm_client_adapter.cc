// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_vm_client_adapter.h"

#include <inttypes.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <deque>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_client_adapter.h"
#include "ash/components/arc/session/arc_dlc_installer.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/arc_session.h"
#include "ash/components/arc/session/connection_holder.h"
#include "ash/components/arc/session/file_system_status.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/patchpanel/patchpanel_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "chromeos/dbus/common/dbus_callback.h"
#include "chromeos/system/core_scheduling.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {
namespace {

constexpr const char kArcVmBootNotificationServerSocketPath[] =
    "/run/arcvm_boot_notification_server/host.socket";

constexpr int64_t kInvalidCid = -1;

constexpr base::TimeDelta kConnectTimeoutLimit = base::Seconds(20);
constexpr base::TimeDelta kConnectSleepDurationInitial =
    base::Milliseconds(100);

constexpr const char kEmptyDiskPath[] = "/dev/null";

// Value of vm_tools::GetEncodedName("arcvm").
constexpr const char kArcvmEncodedName[] = "YXJjdm0=";

constexpr const char kVmConciergeServiceName[] = "vm_5fconcierge";

std::optional<base::TimeDelta> g_connect_timeout_limit_for_testing;
std::optional<base::TimeDelta> g_connect_sleep_duration_initial_for_testing;
std::optional<int> g_boot_notification_server_fd;

ash::ConciergeClient* GetConciergeClient() {
  return ash::ConciergeClient::Get();
}

ash::DebugDaemonClient* GetDebugDaemonClient() {
  return ash::DebugDaemonClient::Get();
}

ArcBinaryTranslationType IdentifyBinaryTranslationType(
    const StartParams& start_params) {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  const bool is_houdini_available =
      command_line->HasSwitch(ash::switches::kEnableHoudini) ||
      command_line->HasSwitch(ash::switches::kEnableHoudini64);
  const bool is_ndk_translation_available =
      command_line->HasSwitch(ash::switches::kEnableNdkTranslation) ||
      command_line->HasSwitch(ash::switches::kEnableNdkTranslation64);

  if (!is_houdini_available && !is_ndk_translation_available)
    return ArcBinaryTranslationType::NONE;

  const bool prefer_ndk_translation =
      !is_houdini_available || start_params.native_bridge_experiment;

  if (is_ndk_translation_available && prefer_ndk_translation)
    return ArcBinaryTranslationType::NDK_TRANSLATION;

  return ArcBinaryTranslationType::HOUDINI;
}

std::vector<std::string> GenerateUpgradeProps(
    const UpgradeParams& upgrade_params,
    const std::string& serial_number,
    const std::string& prefix) {
  std::vector<std::string> result = {
      base::StringPrintf("%s.disable_boot_completed=%d", prefix.c_str(),
                         upgrade_params.skip_boot_completed_broadcast),
      base::StringPrintf("%s.enable_adb_sideloading=%d", prefix.c_str(),
                         upgrade_params.is_adb_sideloading_enabled),
      base::StringPrintf("%s.copy_packages_cache=%d", prefix.c_str(),
                         static_cast<int>(upgrade_params.packages_cache_mode)),
      base::StringPrintf("%s.skip_gms_core_cache=%d", prefix.c_str(),
                         upgrade_params.skip_gms_core_cache),
      base::StringPrintf("%s.arc_demo_mode=%d", prefix.c_str(),
                         upgrade_params.is_demo_session),
      base::StringPrintf("%s.enable_arc_nearby_share=%d", prefix.c_str(),
                         upgrade_params.enable_arc_nearby_share),
      base::StringPrintf(
          "%s.supervision.transition=%d", prefix.c_str(),
          static_cast<int>(upgrade_params.management_transition)),
      base::StringPrintf("%s.serialno=%s", prefix.c_str(),
                         serial_number.c_str()),
      base::StringPrintf("%s.skip_tts_cache=%d", prefix.c_str(),
                         upgrade_params.skip_tts_cache),
  };
  // Conditionally sets more properties based on |upgrade_params|.
  if (!upgrade_params.locale.empty()) {
    result.push_back(base::StringPrintf("%s.locale=%s", prefix.c_str(),
                                        upgrade_params.locale.c_str()));
    if (!upgrade_params.preferred_languages.empty()) {
      result.push_back(base::StringPrintf(
          "%s.preferred_languages=%s", prefix.c_str(),
          base::JoinString(upgrade_params.preferred_languages, ",").c_str()));
    }
  }

  if (upgrade_params.enable_priority_app_lmk_delay &&
      !upgrade_params.priority_app_lmk_delay_list.empty()) {
    result.push_back(base::StringPrintf(
        "%s.arc.lmk.enable_priority_app_delay=%d", prefix.c_str(),
        upgrade_params.enable_priority_app_lmk_delay));
    result.push_back(
        base::StringPrintf("%s.arc.lmk.priority_apps=%s", prefix.c_str(),
                           upgrade_params.priority_app_lmk_delay_list.c_str()));
    result.push_back(base::StringPrintf(
        "%s.arc.lmk.priority_app_delay_duration_sec=%d", prefix.c_str(),
        upgrade_params.priority_app_lmk_delay_second));
  }

  if (upgrade_params.enable_lmk_perceptible_min_state_update) {
    result.push_back(base::StringPrintf(
        "%s.arc.lmk.perceptible_min_state_update=1", prefix.c_str()));
  }

  if (GetArcAndroidSdkVersionAsInt() == kArcVersionT &&
      upgrade_params.skip_dexopt_cache) {
    result.push_back(
        base::StringPrintf("%s.skip_dexopt_cache=1", prefix.c_str()));
  }

  return result;
}

void AppendParamsFromStartParams(
    vm_tools::concierge::StartArcVmRequest& request,
    const StartParams& start_params) {

  switch (IdentifyBinaryTranslationType(start_params)) {
    case ArcBinaryTranslationType::NONE:
      request.set_native_bridge_experiment(
          vm_tools::concierge::StartArcVmRequest::BINARY_TRANSLATION_TYPE_NONE);
      break;
    case ArcBinaryTranslationType::HOUDINI:
      request.set_native_bridge_experiment(
          vm_tools::concierge::StartArcVmRequest::
              BINARY_TRANSLATION_TYPE_HOUDINI);
      break;
    case ArcBinaryTranslationType::NDK_TRANSLATION:
      request.set_native_bridge_experiment(
          vm_tools::concierge::StartArcVmRequest::
              BINARY_TRANSLATION_TYPE_NDK_TRANSLATION);
      break;
  }
  *request.mutable_mini_instance_request() =
      ArcClientAdapter::ConvertStartParamsToStartArcMiniInstanceRequest(
          start_params);
}

int GetDefaultVmMemoryMiB(ArcVmClientAdapterDelegate* delegate) {
  base::SystemMemoryInfoKB info;
  if (!delegate->GetSystemMemoryInfo(&info)) {
    return 0;
  }
  const int sys_memory_mb = info.total / 1024;
  int vm_memory_mb;
  if (sys_memory_mb >= 4096) {
    // On devices with >=4GB RAM, reserve 1GB for other processes.
    vm_memory_mb = sys_memory_mb - 1024;
  } else {
    vm_memory_mb = sys_memory_mb / 4 * 3;
  }

  if (delegate->IsCrosvm32bit()) {
    vm_memory_mb = std::min(vm_memory_mb, static_cast<int>(k32bitVmRamMaxMib));
  }

  return vm_memory_mb;
}

// Returns whether an LVM-provided disk should be used for virtio-blk /data.
bool ShouldUseLvmApplicationContainerForVirtioBlkData() {
  // Allow tests to override use_lvm param.
  if (base::FeatureList::IsEnabled(kVirtioBlkDataConfigOverride)) {
    return kVirtioBlkDataConfigUseLvm.Get();
  }

  // Use LVM backend if LVM application containers feature is supported and
  // user cryptohome data is not ephemeral (b/278305150).
  return base::FeatureList::IsEnabled(kLvmApplicationContainers) &&
         !user_manager::UserManager::Get()->IsUserCryptohomeDataEphemeral(
             arc::ArcServiceManager::Get()->account_id());
}

vm_tools::concierge::StartArcVmRequest CreateStartArcVmRequest(
    const std::string& user_id_hash,
    uint32_t cpus,
    const base::FilePath& demo_session_apps_path,
    const std::optional<base::FilePath>& data_disk_path,
    const FileSystemStatus& file_system_status,
    bool use_per_vm_core_scheduling,
    const StartParams& start_params,
    ArcVmClientAdapterDelegate* delegate) {
  vm_tools::concierge::StartArcVmRequest request;

  request.set_name(kArcVmName);
  request.set_owner_id(user_id_hash);
  request.set_use_per_vm_core_scheduling(use_per_vm_core_scheduling);

  const bool should_set_blocksize =
      !base::FeatureList::IsEnabled(arc::kUseDefaultBlockSize);
  constexpr uint32_t kBlockSize = 4096;

  // Add rootfs as /dev/vda.
  request.set_rootfs_writable(file_system_status.is_host_rootfs_writable() &&
                              file_system_status.is_system_image_ext_format());
  if (should_set_blocksize)
    request.set_rootfs_block_size(kBlockSize);

  // Enable O_DIRECT for ARC system image (rootfs) and vendor image if the
  // images are formatted with EROFS. (b/287383456)
  const bool is_arc_erofs_enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(ash::switches::kArcErofs);
  request.set_rootfs_o_direct(is_arc_erofs_enabled);

  request.set_rootfs_multiple_workers(
      base::FeatureList::IsEnabled(kEnableVirtioBlkMultipleWorkers));

  // Add /vendor as /dev/block/vdb. The device name has to be consistent with
  // the one in GenerateFirstStageFstab() in platform2/arc/setup/.
  vm_tools::concierge::DiskImage* disk_image = request.add_disks();
  disk_image->set_path(file_system_status.vendor_image_path().value());
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_writable(false);
  disk_image->set_do_mount(true);
  disk_image->set_o_direct(is_arc_erofs_enabled);
  if (should_set_blocksize)
    disk_image->set_block_size(kBlockSize);

  // Add /run/imageloader/.../android_demo_apps.squash as /dev/block/vdc if
  // needed. If it's not needed we pass /dev/null so that /dev/block/vdc
  // always corresponds to the demo image.
  disk_image = request.add_disks();
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_writable(false);
  disk_image->set_do_mount(true);
  if (!demo_session_apps_path.empty()) {
    disk_image->set_path(demo_session_apps_path.value());
    if (should_set_blocksize)
      disk_image->set_block_size(kBlockSize);
  } else {
    // This should never be mounted as it's only mounted if
    // ro.boot.arc_demo_mode is set.
    disk_image->set_path(kEmptyDiskPath);
  }

  // Add /opt/google/vms/android/apex/payload.img as /dev/block/vdd if
  // needed. If it's not needed we pass /dev/null so that /dev/block/vdd
  // always corresponds to the block apex composite disk.
  disk_image = request.add_disks();
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_writable(false);
  disk_image->set_do_mount(true);
  if (!file_system_status.block_apex_path().empty()) {
    disk_image->set_path(file_system_status.block_apex_path().value());
  } else {
    // Android will not mount this is the system property
    // apexd.payload_metadata.path is not set, and it should
    // always be set if the block apex payload exists.
    disk_image->set_path(kEmptyDiskPath);
  }

  // Add |data_disk_path| path as /dev/block/vde for mounting Android /data.
  disk_image = request.add_disks();
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_do_mount(true);
  if (data_disk_path) {
    disk_image->set_path(data_disk_path->value());
    disk_image->set_multiple_workers(
        base::FeatureList::IsEnabled(kEnableVirtioBlkMultipleWorkers));
    disk_image->set_writable(true);
    if (should_set_blocksize) {
      disk_image->set_block_size(kBlockSize);
    }
    // Set the O_DIRECT option only when the disk image is backed by LVM
    // application container, because the option is invalidated on disk images
    // in ext4 crypto.
    disk_image->set_o_direct(
        ShouldUseLvmApplicationContainerForVirtioBlkData());
  } else {
    // This should never be mounted as it's only mounted if
    // ro.boot.arcvm_virtio_blk_data=1 is set.
    disk_image->set_path(kEmptyDiskPath);
    // Ensure to set writable to false, otherwise crosvm will exit with
    // "failed to lock disk image" error.
    disk_image->set_writable(false);
  }

  // For U+, add the metadata disk path as /dev/block/vdf for mounting Android
  // /metadata. If the disk doesn't exist, concierge::StartArcVm will create
  // an empty one at this path. (go/arcvm-metadata).
  const bool add_metadata_disk = GetArcAndroidSdkVersionAsInt() > kArcVersionT;
  const std::string metadata_disk_path =
      base::StringPrintf("/run/daemon-store/crosvm/%s/%s.metadata.img",
                         user_id_hash.c_str(), kArcvmEncodedName);
  disk_image = request.add_disks();
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
  disk_image->set_do_mount(true);
  if (add_metadata_disk) {
    disk_image->set_path(metadata_disk_path);
    disk_image->set_writable(true);
  } else {
    disk_image->set_path(kEmptyDiskPath);
    disk_image->set_writable(false);
  }

  // Always add the ARCVM runtime system properties file as a disk at path
  // /dev/block/vdg. We share this file as a disk image instead of using a
  // shared directory because it needs to be accessed during early init before
  // virtio-fs is available in Android.
  const std::string sysprop_disk_path =
      base::StringPrintf("/run/daemon-store/crosvm/%s/%s.runtime.prop",
                         user_id_hash.c_str(), kArcvmEncodedName);
  disk_image = request.add_disks();
  disk_image->set_image_type(vm_tools::concierge::DISK_IMAGE_RAW);
  disk_image->set_do_mount(true);
  disk_image->set_path(sysprop_disk_path);
  disk_image->set_writable(false);

  // Add cpus.
  request.set_cpus(cpus);

  // Add ignore_dev_conf setting for dev mode.
  request.set_ignore_dev_conf(IsArcVmDevConfIgnored());

  // Add enable_rt_vcpu.
  request.set_enable_rt_vcpu(IsArcVmRtVcpuEnabled(cpus));

  // Add hugepages.
  request.set_use_hugepages(IsArcVmUseHugePages());

  // Request guest memory locking, if configured.
  request.set_lock_guest_memory(base::FeatureList::IsEnabled(kLockGuestMemory));

  // Controls whether WebView Zygote is lazily initialized in ARC.
  request.set_enable_web_view_zygote_lazy_init(
      base::FeatureList::IsEnabled(arc::kEnableLazyWebViewInit));

  // Specify VM Memory.
  if (base::FeatureList::IsEnabled(kVmMemorySize)) {
    base::SystemMemoryInfoKB info;
    if (delegate->GetSystemMemoryInfo(&info)) {
      const int ram_mib = info.total / 1024;
      const int shift_mib = kVmMemorySizeShiftMiB.Get();
      const int max_mib = kVmMemorySizeMaxMiB.Get();
      const int ram_percentage = kVmMemorySizePercentage.Get();
      int vm_ram_mib =
          std::min(max_mib, ram_percentage * ram_mib / 100 + shift_mib);
      if (delegate->IsCrosvm32bit()) {
        // This is a workaround for ARM Chromebooks where userland including
        // crosvm is compiled in 32 bit.
        // TODO(khmel): Remove this once crosvm becomes 64 bit binary on ARM.
        if (vm_ram_mib > static_cast<int>(k32bitVmRamMaxMib)) {
          VLOG(2) << "VmMemorySize is enabled, but we are on a 32-bit device, "
                  << "so limit the size to " << k32bitVmRamMaxMib << " MiB.";
          vm_ram_mib = k32bitVmRamMaxMib;
        }
      }

      request.set_memory_mib(vm_ram_mib);
      VLOG(2) << "VmMemorySize is enabled. memory_mib=" << vm_ram_mib;
    } else {
      VLOG(2) << "VmMemorySize is enabled, but GetSystemMemoryInfo failed.";
    }
  } else {
    VLOG(2) << "VmMemorySize is disabled.";
  }

  AppendParamsFromStartParams(request, start_params);

  auto* mini_instance_request = request.mutable_mini_instance_request();
  mini_instance_request->set_enable_consumer_auto_update_toggle(
      base::FeatureList::IsEnabled(
          ash::features::kConsumerAutoUpdateToggleAllowed));

  mini_instance_request->set_enable_privacy_hub_for_chrome(
      base::FeatureList::IsEnabled(ash::features::kCrosPrivacyHub));
  if (GetArcAndroidSdkVersionAsInt() == kArcVersionT) {
    mini_instance_request->set_arc_switch_to_keymint(ShouldUseArcKeyMint());
    mini_instance_request->set_enable_arc_attestation(
        ShouldUseArcAttestation());
  }

  request.set_enable_broadcast_anr_prenotify(
      base::FeatureList::IsEnabled(arc::kVmBroadcastPreNotifyANR));

  request.set_enable_virtio_blk_data(start_params.use_virtio_blk_data);

  // Enable block IO scheduler for virtio-blk /data devices.
  // LVM-backed devices are excluded to mitigate boot time regression.
  // (See go/arcvm-data-block-io-scheduler)
  request.set_enable_data_block_io_scheduler(
      start_params.use_virtio_blk_data &&
      !ShouldUseLvmApplicationContainerForVirtioBlkData() &&
      base::FeatureList::IsEnabled(kBlockIoScheduler) &&
      kEnableDataBlockIoScheduler.Get());

  if (base::FeatureList::IsEnabled(kGuestSwap)) {
    request.set_guest_swappiness(kGuestZramSwappiness.Get());
    int guestSwapSizeMiB = kGuestSwapSize.Get() / (1024 * 1024);
    if (kGuestZramSizePercentage.Get() != 0) {
      // If there's no custom memory_mib set, try to get the default value to
      // determine the ZRAM size.
      if (request.memory_mib() == 0) {
        request.set_memory_mib(GetDefaultVmMemoryMiB(delegate));
      }
      guestSwapSizeMiB =
          request.memory_mib() * kGuestZramSizePercentage.Get() / 100;
    }

    if (kVirtualSwapEnabled.Get()) {
      auto* virtual_swap_config = request.mutable_virtual_swap_config();
      virtual_swap_config->set_swap_interval_ms(kVirtualSwapIntervalMs.Get());
      virtual_swap_config->set_size_mib(guestSwapSizeMiB);
    } else {
      request.set_guest_zram_mib(guestSwapSizeMiB);
    }
  }

  if (base::FeatureList::IsEnabled(kMglruReclaim)) {
    request.set_mglru_reclaim_interval(kMglruReclaimInterval.Get());
    request.set_mglru_reclaim_swappiness(kMglruReclaimSwappiness.Get());
  } else {
    request.set_mglru_reclaim_interval(0);
    request.set_mglru_reclaim_swappiness(0);
  }

  if (base::FeatureList::IsEnabled(kVmMemoryPSIReports))
    request.set_vm_memory_psi_period(kVmMemoryPSIReportsPeriod.Get());
  else
    request.set_vm_memory_psi_period(-1);

  request.set_enable_vmm_swap(
      base::FeatureList::IsEnabled(kVmmSwapPolicy) ||
      base::FeatureList::IsEnabled(kVmmSwapKeyboardShortcut));

#if defined(ARCH_CPU_X86_64)
  if (base::FeatureList::IsEnabled(kEnableArcS2Idle)) {
    request.set_enable_s2idle(true);
  }
#endif  // defined(ARCH_CPU_X86_64)

  if (base::FeatureList::IsEnabled(kArcVmPvclock)) {
    request.set_enable_pvclock(true);
  }

  auto orientation = display::PanelOrientation::kNormal;
  if (auto* screen = display::Screen::GetScreen()) {
    const auto display_id = screen->GetPrimaryDisplay().id();
    if (auto* shell = ash::Shell::Get()) {
      const auto& info = shell->display_manager()->GetDisplayInfo(display_id);
      orientation = info.panel_orientation();
    }
  }
  switch (orientation) {
    using StartArcVmRequest = vm_tools::concierge::StartArcVmRequest;
    case display::PanelOrientation::kNormal:
      request.set_panel_orientation(StartArcVmRequest::ORIENTATION_0);
      break;
    case display::PanelOrientation::kRightUp:
      request.set_panel_orientation(StartArcVmRequest::ORIENTATION_90);
      break;
    case display::PanelOrientation::kBottomUp:
      request.set_panel_orientation(StartArcVmRequest::ORIENTATION_180);
      break;
    case display::PanelOrientation::kLeftUp:
      request.set_panel_orientation(StartArcVmRequest::ORIENTATION_270);
      break;
  }

  const ArcUreadaheadMode mode =
      GetArcUreadaheadMode(ash::switches::kArcVmUreadaheadMode);
  switch (mode) {
    using StartArcVmRequest = vm_tools::concierge::StartArcVmRequest;
    case ArcUreadaheadMode::READAHEAD:
      request.set_ureadahead_mode(StartArcVmRequest::UREADAHEAD_MODE_READAHEAD);
      break;
    case ArcUreadaheadMode::GENERATE:
      request.set_ureadahead_mode(StartArcVmRequest::UREADAHEAD_MODE_GENERATE);
      break;
    case ArcUreadaheadMode::DISABLED:
      request.set_ureadahead_mode(StartArcVmRequest::UREADAHEAD_MODE_DISABLED);
      break;
    default:
      NOTREACHED();
  }

  if (request.memory_mib() < kMinVmMemorySizeMiB) {
    request.set_memory_mib(kMinVmMemorySizeMiB);
    VLOG(2) << "Overriding VM memory size to 3243 MiB for app compatibility";
  }

  request.set_use_gki(base::FeatureList::IsEnabled(kArcVmGki));
  return request;
}

const sockaddr_un* GetArcVmBootNotificationServerAddress() {
  static struct sockaddr_un address {
    .sun_family = AF_UNIX,
    .sun_path = "/run/arcvm_boot_notification_server/host.socket"
  };
  return &address;
}

// Connects to UDS socket at |kArcVmBootNotificationServerSocketPath|.
// Returns the connected socket fd if successful, or else an invalid fd. This
// function can only be called with base::MayBlock().
base::ScopedFD ConnectToArcVmBootNotificationServer() {
  if (g_boot_notification_server_fd)
    return base::ScopedFD(HANDLE_EINTR(dup(*g_boot_notification_server_fd)));

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  base::ScopedFD fd(socket(AF_UNIX, SOCK_STREAM, 0));
  DCHECK(fd.is_valid());

  if (HANDLE_EINTR(connect(fd.get(),
                           reinterpret_cast<const sockaddr*>(
                               GetArcVmBootNotificationServerAddress()),
                           sizeof(sockaddr_un)))) {
    PLOG(WARNING) << "Unable to connect to "
                  << kArcVmBootNotificationServerSocketPath;
    return {};
  }

  return fd;
}

// Connects to arcvm-boot-notification-server to verify that it is listening.
// When this function is called, the server has just started and may not be
// listening on the socket yet, so this function will retry connecting for up
// to 20s, with exponential backoff. This function can only be called with
// base::MayBlock().
bool IsArcVmBootNotificationServerListening() {
  const base::ElapsedTimer timer;
  const base::TimeDelta limit = g_connect_timeout_limit_for_testing
                                    ? *g_connect_timeout_limit_for_testing
                                    : kConnectTimeoutLimit;
  base::TimeDelta sleep_duration =
      g_connect_sleep_duration_initial_for_testing
          ? *g_connect_sleep_duration_initial_for_testing
          : kConnectSleepDurationInitial;

  do {
    if (ConnectToArcVmBootNotificationServer().is_valid())
      return true;

    LOG(WARNING) << "Retrying to connect to boot notification server in "
                 << sleep_duration;
    base::PlatformThread::Sleep(sleep_duration);
    sleep_duration *= 2;
  } while (timer.Elapsed() < limit);
  return false;
}

// Sends upgrade props to arcvm-boot-notification-server over socket at
// |kArcVmBootNotificationServerSocketPath|. This function can only be called
// with base::MayBlock().
bool SendUpgradePropsToArcVmBootNotificationServer(
    int64_t cid,
    const UpgradeParams& params,
    const std::string& serial_number) {
  std::string props = base::StringPrintf(
      "CID=%" PRId64 "\n%s", cid,
      base::JoinString(GenerateUpgradeProps(params, serial_number, "ro.boot"),
                       "\n")
          .c_str());

  base::ScopedFD fd = ConnectToArcVmBootNotificationServer();
  if (!fd.is_valid())
    return false;

  if (!base::WriteFileDescriptor(fd.get(), props)) {
    PLOG(ERROR) << "Unable to write props to "
                << kArcVmBootNotificationServerSocketPath;
    return false;
  }
  return true;
}

}  // namespace

bool ArcVmClientAdapterDelegate::GetSystemMemoryInfo(
    base::SystemMemoryInfoKB* info) {
  // Call the base function by default.
  return base::GetSystemMemoryInfo(info);
}

bool ArcVmClientAdapterDelegate::IsCrosvm32bit() {
  // Assume that crosvm is 32-bit if chrome is 32-bit.
  return sizeof(uintptr_t) == 4;
}

class ArcVmClientAdapter : public ArcClientAdapter,
                           public ash::ConciergeClient::VmObserver,
                           public ash::ConciergeClient::Observer,
                           public ConnectionObserver<arc::mojom::AppInstance> {
 public:
  ArcVmClientAdapter() : ArcVmClientAdapter(FileSystemStatusRewriter{}) {}

  // For testing purposes and the internal use (by the other ctor) only.
  explicit ArcVmClientAdapter(const FileSystemStatusRewriter& rewriter)
      : delegate_(std::make_unique<ArcVmClientAdapterDelegate>()),
        file_system_status_rewriter_for_testing_(rewriter) {
    auto* client = GetConciergeClient();
    client->AddVmObserver(this);
    client->AddObserver(this);

    auto* arc_service_manager = arc::ArcServiceManager::Get();
    DCHECK(arc_service_manager);
    arc_service_manager->arc_bridge_service()->app()->AddObserver(this);
  }

  ArcVmClientAdapter(const ArcVmClientAdapter&) = delete;
  ArcVmClientAdapter& operator=(const ArcVmClientAdapter&) = delete;

  ~ArcVmClientAdapter() override {
    auto* arc_service_manager = arc::ArcServiceManager::Get();
    if (arc_service_manager)
      arc_service_manager->arc_bridge_service()->app()->RemoveObserver(this);

    auto* client = GetConciergeClient();
    client->RemoveObserver(this);
    client->RemoveVmObserver(this);
  }

  // ash::ConciergeClient::VmObserver overrides:
  void OnVmStarted(
      const vm_tools::concierge::VmStartedSignal& signal) override {
    if (signal.name() == kArcVmName)
      VLOG(2) << "OnVmStarted: ARCVM cid=" << signal.vm_info().cid();
  }

  void OnVmStopped(
      const vm_tools::concierge::VmStoppedSignal& signal) override {
    if (signal.name() != kArcVmName)
      return;
    const int64_t cid = signal.cid();
    if (cid != current_cid_) {
      VLOG(2) << "Ignoring VmStopped signal: current CID=" << current_cid_
              << ", stopped CID=" << cid;
      return;
    }
    VLOG(2) << "OnVmStopped: ARCVM cid=" << cid;
    current_cid_ = kInvalidCid;
    const bool is_system_shutdown =
        signal.reason() == vm_tools::concierge::SERVICE_SHUTDOWN;
    OnArcInstanceStopped(is_system_shutdown);
  }

  // ArcClientAdapter overrides:
  void StartMiniArc(StartParams params,
                    chromeos::VoidDBusMethodCallback callback) override {
    // This step is mandatory regardless of StartMiniArc is called or not
    // from |ArcSessionManager|. It is also called after login for ARCVM.
    if (user_id_hash_.empty()) {
      LOG(ERROR) << "User ID hash is not set";
      StopArcInstanceInternal();
      std::move(callback).Run(false);
      return;
    }

    start_params_ = std::move(params);

    // Stop existing VM and Upstart jobs if any (e.g. in case of a chrome
    // crash).
    VLOG(2) << "Stopping existing VM if any";
    EnsureStaleArcVmAndArcVmUpstartJobsStopped(
        user_id_hash_,
        base::BindOnce(&ArcVmClientAdapter::OnExistingVmStopped,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void UpgradeArc(UpgradeParams params,
                  chromeos::VoidDBusMethodCallback callback) override {
    DCHECK(!user_id_hash_.empty());
    if (serial_number_.empty()) {
      LOG(ERROR) << "Serial number is not set";
      StopArcInstanceInternal();
      std::move(callback).Run(false);
      return;
    }

    VLOG(2) << "Checking adb sideload status";
    ash::SessionManagerClient::Get()->QueryAdbSideload(base::BindOnce(
        &ArcVmClientAdapter::OnQueryAdbSideload, weak_factory_.GetWeakPtr(),
        std::move(params), std::move(callback)));
  }

  void StopArcInstance(bool on_shutdown, bool should_backup_log) override {
    if (on_shutdown) {
      // Do nothing when |on_shutdown| is true because either vm_concierge.conf
      // job (in case of user session termination) or session_manager (in case
      // of browser-initiated exit on e.g. chrome://flags or UI language change)
      // will stop all VMs including ARCVM right after the browser exits.
      VLOG(2)
          << "StopArcInstance is called during browser shutdown. Do nothing.";
      return;
    }
    DCHECK_NE(current_cid_, kInvalidCid) << "ARCVM is not running.";

    if (should_backup_log) {
      GetDebugDaemonClient()->BackupArcBugReport(
          cryptohome::CreateAccountIdentifierFromIdentification(cryptohome_id_),
          base::BindOnce(&ArcVmClientAdapter::OnArcBugReportBackedUp,
                         weak_factory_.GetWeakPtr()));
    } else {
      StopArcInstanceInternal();
    }
  }

  void SetUserInfo(const cryptohome::Identification& cryptohome_id,
                   const std::string& hash,
                   const std::string& serial_number) override {
    DCHECK(cryptohome_id_.id().empty());
    DCHECK(user_id_hash_.empty());
    DCHECK(serial_number_.empty());
    if (cryptohome_id.id().empty())
      LOG(WARNING) << "cryptohome_id is empty";
    if (hash.empty())
      LOG(WARNING) << "hash is empty";
    if (serial_number.empty())
      LOG(WARNING) << "serial_number is empty";
    cryptohome_id_ = cryptohome_id;
    user_id_hash_ = hash;
    serial_number_ = serial_number;
  }

  void SetDemoModeDelegate(DemoModeDelegate* delegate) override {
    demo_mode_delegate_ = delegate;
  }

  void TrimVmMemory(TrimVmMemoryCallback callback, int page_limit) override {
    VLOG(2) << "Start trimming VM memory";
    if (user_id_hash_.empty()) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), /*success=*/false,
                         /*failure_reason=*/"user_id_hash_ is not set"));
      return;
    }

    vm_tools::concierge::ReclaimVmMemoryRequest request;
    request.set_name(kArcVmName);
    request.set_owner_id(user_id_hash_);
    request.set_page_limit(page_limit);
    GetConciergeClient()->ReclaimVmMemory(
        request,
        base::BindOnce(&ArcVmClientAdapter::OnTrimVmMemory,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  // ash::ConciergeClient::Observer overrides:
  void ConciergeServiceStopped() override {
    VLOG(2) << "vm_concierge stopped";
    // At this point, all crosvm processes are gone. Notify the observer of the
    // event.
    // NOTE: In a normal system shutdown OnVmStopped() is called before this.
    // When vm_concierge crashes, this is called without OnVmStopped().
    OnArcInstanceStopped(false /* is_system_shutdown */);
  }

  void ConciergeServiceStarted() override {}

  // ConnectionObserver<arc::mojom::AppInstance> overrides:
  void OnConnectionReady() override {
    VLOG(2) << "Sending ArcVmCompleteBoot Request";

    auto* arc_service_manager = arc::ArcServiceManager::Get();
    DCHECK(arc_service_manager);
    arc_service_manager->arc_bridge_service()->app()->RemoveObserver(this);

    vm_tools::concierge::ArcVmCompleteBootRequest request;
    DCHECK(!user_id_hash_.empty());
    request.set_owner_id(user_id_hash_);
    GetConciergeClient()->ArcVmCompleteBoot(
        request,
        base::BindOnce(&ArcVmClientAdapter::OnArcVmCompleteBootResponse));
  }

  void set_delegate_for_testing(  // IN-TEST
      std::unique_ptr<ArcVmClientAdapterDelegate> delegate) {
    delegate_ = std::move(delegate);
  }

 private:
  void OnArcBugReportBackedUp(bool result) {
    if (!result) {
      LOG(ERROR) << "Error contacting debugd to back up ARC bug report.";
    }

    StopArcInstanceInternal();
  }

  void StopArcInstanceInternal() {
    VLOG(1) << "Sending request to stop ARCVM";
    // This may be called before ARCVM has been upgraded and the proper VM id
    // has been set. Since ConciergeClient::StopVm() returns successfully
    // regardless of whether the VM exists, check to see which VM is actually
    // running.

    vm_tools::concierge::StopVmRequest request;
    request.set_name(kArcVmName);
    request.set_owner_id(user_id_hash_);

    GetConciergeClient()->StopVm(
        request, base::BindOnce(&ArcVmClientAdapter::OnStopVmReply,
                                weak_factory_.GetWeakPtr()));
  }

  void OnExistingVmStopped(chromeos::VoidDBusMethodCallback callback,
                           bool stopped) {
    if (!stopped) {
      LOG(ERROR) << "Failed to stop existing ARCVM";
      std::move(callback).Run(false);
      return;
    }

    std::vector<std::string> environment;
    std::deque<JobDesc> jobs{
        // Note: the first Upstart job is a task, and the callback for the start
        // request won't be called until the task finishes. When the callback is
        // called with true, it is ensured that the per-board features files
        // exist.
        JobDesc{kArcVmPerBoardFeaturesJobName, UpstartOperation::JOB_START, {}},
        JobDesc{kArcVmPreLoginServicesJobName, UpstartOperation::JOB_START,
                std::move(environment)},
    };

    ConfigureUpstartJobs(
        std::move(jobs),
        base::BindOnce(
            &ArcVmClientAdapter::OnConfigureUpstartJobsOnStartMiniArc,
            weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnConfigureUpstartJobsOnStartMiniArc(
      chromeos::VoidDBusMethodCallback callback,
      bool result) {
    if (!result) {
      LOG(ERROR) << "ConfigureUpstartJobs (on starting mini ARCVM) failed";
      std::move(callback).Run(false);
      return;
    }

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&IsArcVmBootNotificationServerListening),
        base::BindOnce(
            &ArcVmClientAdapter::OnArcVmBootNotificationServerIsListening,
            weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnArcVmBootNotificationServerIsListening(
      chromeos::VoidDBusMethodCallback callback,
      bool result) {
    if (!result) {
      LOG(ERROR) << "Failed to connect to arcvm-boot-notification-server";
      std::move(callback).Run(false);
      return;
    }

    VLOG(2) << "Checking file system status";
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&FileSystemStatus::GetFileSystemStatusBlocking),
        base::BindOnce(&ArcVmClientAdapter::OnFileSystemStatus,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnFileSystemStatus(chromeos::VoidDBusMethodCallback callback,
                          FileSystemStatus file_system_status) {
    VLOG(2) << "Got file system status";
    if (file_system_status_rewriter_for_testing_)
      file_system_status_rewriter_for_testing_.Run(&file_system_status);

    VLOG(2) << "Wait for DLC installation if necessary";
    // Waits for a stable state (kInstalled/kUninstalled) and proceeds
    // regardless of installation result because even if the installation
    // has failed, it will only affect limited functionality (e.g. without
    // Houdini library for ARM apps). ARCVM should still continue to start.
    ArcDlcInstaller::Get()->WaitForStableState(base::BindOnce(
        &ArcVmClientAdapter::LoadDemoResources, weak_factory_.GetWeakPtr(),
        std::move(callback), std::move(file_system_status)));
  }

  void LoadDemoResources(chromeos::VoidDBusMethodCallback callback,
                         FileSystemStatus file_system_status) {
    VLOG(2) << "Retrieving demo session apps path";
    DCHECK(demo_mode_delegate_);
    demo_mode_delegate_->EnsureResourcesLoaded(base::BindOnce(
        &ArcVmClientAdapter::OnDemoResourcesLoaded, weak_factory_.GetWeakPtr(),
        std::move(callback), std::move(file_system_status)));
  }

  void OnDemoResourcesLoaded(chromeos::VoidDBusMethodCallback callback,
                             FileSystemStatus file_system_status) {
    if (!start_params_.use_virtio_blk_data) {
      VLOG(2) << "Using virtio-fs for /data";
      StartArcVm(std::move(callback), std::move(file_system_status),
                 /*data_disk_path=*/std::nullopt);
      return;
    }

    if (ShouldUseLvmApplicationContainerForVirtioBlkData()) {
      VLOG(2) << "Using virtio-blk with the LVM-provided disk for /data";

      // LVM disk name is generated by cryptohome::DmcryptVolumePrefix in
      // src/platform2/cryptohome.
      const std::string lvm_disk_path =
          base::StringPrintf("/dev/mapper/vm/dmcrypt-%s-arcvm",
                             user_id_hash_.substr(0, 8).c_str());
      StartArcVm(std::move(callback), std::move(file_system_status),
                 base::FilePath(lvm_disk_path));
      return;
    }

    VLOG(2) << "Using virtio-blk with the concierge-provided disk for /data";

    // If request.disk_size is not set, concierge calculates the desired size
    // (95% of the total space) and creates a sparse disk image.
    vm_tools::concierge::CreateDiskImageRequest request;
    request.set_cryptohome_id(user_id_hash_);
    request.set_vm_name(kArcVmName);
    request.set_image_type(vm_tools::concierge::DISK_IMAGE_AUTO);
    request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
    request.set_storage_ballooning(true);

    GetConciergeClient()->CreateDiskImage(
        std::move(request),
        base::BindOnce(&ArcVmClientAdapter::OnDataDiskImageCreated,
                       weak_factory_.GetWeakPtr(), std::move(callback),
                       std::move(file_system_status)));
  }

  void OnDataDiskImageCreated(
      chromeos::VoidDBusMethodCallback callback,
      FileSystemStatus file_system_status,
      std::optional<vm_tools::concierge::CreateDiskImageResponse> res) {
    if (!res) {
      LOG(ERROR) << "Failed to create a disk image for /data. Empty response.";
      std::move(callback).Run(false);
      return;
    }

    switch (res->status()) {
      case vm_tools::concierge::DISK_STATUS_CREATED:
        VLOG(2) << "Created a disk image for /data at " << res->disk_path();
        StartArcVm(std::move(callback), std::move(file_system_status),
                   base::FilePath(res->disk_path()));
        return;
      case vm_tools::concierge::DISK_STATUS_EXISTS:
        VLOG(2) << "Disk image for /data already exists: " << res->disk_path();
        StartArcVm(std::move(callback), std::move(file_system_status),
                   base::FilePath(res->disk_path()));
        return;
      // TODO(niwa): Also handle DISK_STATUS_NOT_ENOUGH_SPACE.
      default:
        LOG(ERROR) << "Failed to create a disk image for /data. Status:"
                   << res->status() << " Reason:" << res->failure_reason();
        std::move(callback).Run(false);
        return;
    }
  }

  void StartArcVm(chromeos::VoidDBusMethodCallback callback,
                  FileSystemStatus file_system_status,
                  std::optional<base::FilePath> data_disk_path) {
    const base::FilePath demo_session_apps_path =
        demo_mode_delegate_->GetDemoAppsPath();
    const bool use_per_vm_core_scheduling =
        base::FeatureList::IsEnabled(kEnablePerVmCoreScheduling);

    // When the CPU has MDS or L1TF vulnerabilities, and per-VM core scheduling
    // is not enabled via |kEnablePerVmCoreScheduling|, crosvm won't be allowed
    // to run two vCPUs on the same physical core at the same time. This mode is
    // called per-vCPU core scheduling, and it effectively disables SMT on
    // crosvm. Because of this restriction, when per-vCPU core scheduling is in
    // use, set |cpus| to the number of physical cores. Otherwise, set the
    // variable to the number of logical cores minus the ones disabled by
    // chrome://flags/#scheduler-configuration.
    const int32_t cpus = (chromeos::system::IsCoreSchedulingAvailable() &&
                          !use_per_vm_core_scheduling)
                             ? chromeos::system::NumberOfPhysicalCores()
                             : base::SysInfo::NumberOfProcessors() -
                                   start_params_.num_cores_disabled;
    DCHECK_LT(0, cpus);

    auto start_request = CreateStartArcVmRequest(
        user_id_hash_, cpus, demo_session_apps_path, data_disk_path,
        file_system_status, use_per_vm_core_scheduling, start_params_,
        delegate_.get());

    // vm_concierge service needs to be running to start ARCVM but it may not be
    // started. Explicitly start the service to make sure it's running. We don't
    // have to stop it since it stops on "stopping ui".
    std::vector<std::string> env;
    ash::UpstartClient::Get()->StartJobWithErrorDetails(
        kVmConciergeServiceName, env,
        base::BindOnce(&ArcVmClientAdapter::OnVmConciergeServiceStarted,
                       weak_factory_.GetWeakPtr(), std::move(start_request),
                       std::move(callback)));
  }

  void OnVmConciergeServiceStarted(
      vm_tools::concierge::StartArcVmRequest start_request,
      chromeos::VoidDBusMethodCallback callback,
      bool result,
      std::optional<std::string> error_name,
      std::optional<std::string> error_message) {
    if (!result) {
      // vm_concierge may be already started by "started-user-session" upstart
      // signal.
      if (error_name.has_value() &&
          error_name.value() == ash::UpstartClient::kAlreadyStartedError) {
        DVLOG(1) << "vm_concierge is already running";
      } else {
        LOG(ERROR)
            << "Failed to start arcvm. vm_concierge service cannot be started: "
            << (error_name.has_value() ? error_name.value() : "unknown error")
            << ": " << (error_message.has_value() ? error_message.value() : "");
        std::move(callback).Run(false);
        return;
      }
    }

    // Wait until vm_concierge is ready to serve D-Bus requests.
    ash::ConciergeClient::Get()->WaitForServiceToBeAvailable(
        base::BindOnce(&ArcVmClientAdapter::OnVmConciergeServiceAvailable,
                       weak_factory_.GetWeakPtr(), std::move(start_request),
                       std::move(callback)));
  }

  void OnVmConciergeServiceAvailable(
      vm_tools::concierge::StartArcVmRequest start_request,
      chromeos::VoidDBusMethodCallback callback,
      bool service_is_available) {
    if (!service_is_available) {
      LOG(ERROR)
          << "Failed to start arcvm. vm_concierge service is not available.";
      std::move(callback).Run(false);
      return;
    }
    // ARCVM startup will fail if patchpanel DBus service is not available. The
    // startup of patchpanel is slow on some low-end devices. According to
    // b/325850116 the interval between ARCVM startup and patchpanel getting
    // ready was 2s in that case. So let's wait 5s here before suspecting the
    // startup to be blocked by patchpanel and printing the log.
    patchpanel_timer_ = std::make_unique<base::OneShotTimer>();
    patchpanel_timer_->Start(
        FROM_HERE, base::Seconds(5), base::BindOnce([]() {
          LOG(WARNING) << "Still waiting for patchpanel before starting ARCVM.";
        }));
    ash::PatchPanelClient::Get()->WaitForServiceToBeAvailable(
        base::BindOnce(&ArcVmClientAdapter::OnPatchPanelServiceAvailable,
                       weak_factory_.GetWeakPtr(), std::move(start_request),
                       std::move(callback)));
  }

  void OnPatchPanelServiceAvailable(
      vm_tools::concierge::StartArcVmRequest start_request,
      chromeos::VoidDBusMethodCallback callback,
      bool service_is_available) {
    patchpanel_timer_->Stop();
    patchpanel_timer_.reset();

    if (!service_is_available) {
      LOG(ERROR)
          << "Failed to start arcvm. Patchpanel service is not available.";
      std::move(callback).Run(false);
      return;
    }

    VLOG(1) << "Sending request to start ARCVM";
    GetConciergeClient()->StartArcVm(
        start_request,
        base::BindOnce(&ArcVmClientAdapter::OnStartArcVmReply,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnStartArcVmReply(
      chromeos::VoidDBusMethodCallback callback,
      std::optional<vm_tools::concierge::StartVmResponse> reply) {
    if (!reply.has_value()) {
      LOG(ERROR) << "Failed to start arcvm. Empty response.";
      std::move(callback).Run(false);
      return;
    }

    const vm_tools::concierge::StartVmResponse& response = reply.value();
    if (response.status() != vm_tools::concierge::VM_STATUS_RUNNING) {
      LOG(ERROR) << "Failed to start arcvm: status=" << response.status()
                 << ", reason=" << response.failure_reason();
      std::move(callback).Run(false);
      return;
    }
    current_cid_ = response.vm_info().cid();
    should_notify_observers_ = true;
    VLOG(1) << "ARCVM started cid=" << current_cid_;
    std::move(callback).Run(true);
  }

  void OnQueryAdbSideload(
      UpgradeParams params,
      chromeos::VoidDBusMethodCallback callback,
      ash::SessionManagerClient::AdbSideloadResponseCode response_code,
      bool enabled) {
    VLOG(2) << "IsAdbSideloadAllowed, response_code="
            << static_cast<int>(response_code) << ", enabled=" << enabled;

    switch (response_code) {
      case ash::SessionManagerClient::AdbSideloadResponseCode::FAILED:
        LOG(ERROR) << "Failed response from QueryAdbSideload";
        StopArcInstanceInternal();
        std::move(callback).Run(false);
        return;
      case ash::SessionManagerClient::AdbSideloadResponseCode::NEED_POWERWASH:
        params.is_adb_sideloading_enabled = false;
        break;
      case ash::SessionManagerClient::AdbSideloadResponseCode::SUCCESS:
        params.is_adb_sideloading_enabled = enabled;
        break;
    }

    std::string arcvm_data_type;
    if (start_params_.use_virtio_blk_data) {
      arcvm_data_type = ShouldUseLvmApplicationContainerForVirtioBlkData()
                            ? "lvm_volume"
                            : "concierge_disk";
    } else {
      arcvm_data_type = "virtiofs";
    }

    VLOG(2) << "Starting upstart jobs for UpgradeArc()";
    std::vector<std::string> environment{
        "CHROMEOS_USER=" +
            cryptohome::CreateAccountIdentifierFromIdentification(
                cryptohome_id_)
                .account_id(),
        "ARCVM_DATA_TYPE=" + arcvm_data_type};
    std::deque<JobDesc> jobs{
        JobDesc{kArcVmPostLoginServicesJobName, UpstartOperation::JOB_START,
                std::move(environment)},
    };

    ConfigureUpstartJobs(
        std::move(jobs),
        base::BindOnce(&ArcVmClientAdapter::OnConfigureUpstartJobsOnUpgradeArc,
                       weak_factory_.GetWeakPtr(), std::move(params),
                       std::move(callback)));
  }

  void OnConfigureUpstartJobsOnUpgradeArc(
      UpgradeParams params,
      chromeos::VoidDBusMethodCallback callback,
      bool result) {
    if (!result) {
      LOG(ERROR) << "ConfigureUpstartJobs (on upgrading ARCVM) failed. ";
      StopArcInstanceInternal();
      std::move(callback).Run(false);
      return;
    }

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&SendUpgradePropsToArcVmBootNotificationServer,
                       current_cid_, std::move(params), serial_number_),
        base::BindOnce(&ArcVmClientAdapter::OnUpgradePropsSent,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnUpgradePropsSent(chromeos::VoidDBusMethodCallback callback,
                          bool result) {
    if (!result) {
      LOG(ERROR)
          << "Failed to send upgrade props to arcvm-boot-notification-server";
      StopArcInstanceInternal();
      std::move(callback).Run(false);
      return;
    }

    VLOG(2) << "Starting arcvm-post-vm-start-services.";
    std::vector<std::string> environment;
    std::deque<JobDesc> jobs{JobDesc{kArcVmPostVmStartServicesJobName,
                                     UpstartOperation::JOB_START,
                                     std::move(environment)}};
    ConfigureUpstartJobs(
        std::move(jobs),
        base::BindOnce(&ArcVmClientAdapter::OnConfigureUpstartJobsAfterVmStart,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnConfigureUpstartJobsAfterVmStart(
      chromeos::VoidDBusMethodCallback callback,
      bool result) {
    if (!result) {
      LOG(ERROR) << "ConfigureUpstartJobs (after starting ARCVM) failed.";
      StopArcInstanceInternal();
      std::move(callback).Run(false);
      return;
    }

    VLOG(1) << "ARCVM upgrade completed";
    std::move(callback).Run(true);
  }

  void OnArcInstanceStopped(bool is_system_shutdown) {
    VLOG(1) << "ARCVM stopped.";

    // If this method is called before even mini VM is started (e.g. very early
    // vm_concierge crash), or this method is called twice (e.g. crosvm crash
    // followed by vm_concierge crash), do nothing.
    if (!should_notify_observers_)
      return;
    should_notify_observers_ = false;

    for (auto& observer : observer_list_)
      observer.ArcInstanceStopped(is_system_shutdown);
  }

  void OnStopVmReply(std::optional<vm_tools::concierge::StopVmResponse> reply) {
    // If the reply indicates the D-Bus call is successfully done, do nothing.
    // Concierge will call OnVmStopped() eventually.
    if (reply.has_value() && reply.value().success())
      return;

    // StopVm always returns successfully, so the only case where this happens
    // is if the reply is empty, which means Concierge isn't running and ARCVM
    // isn't either.
    LOG(ERROR) << "Failed to stop ARCVM: empty reply.";
    OnArcInstanceStopped(false /* is_system_shutdown */);
  }

  void OnTrimVmMemory(
      TrimVmMemoryCallback callback,
      std::optional<vm_tools::concierge::ReclaimVmMemoryResponse> reply) {
    bool success = false;
    std::string failure_reason;

    if (!reply.has_value()) {
      failure_reason = "Empty response";
    } else {
      const vm_tools::concierge::ReclaimVmMemoryResponse& response =
          reply.value();
      success = response.success();
      if (!success)
        failure_reason = response.failure_reason();
    }

    VLOG(2) << "Finished trimming memory: success=" << success
            << (failure_reason.empty() ? "" : " reason=") << failure_reason;
    std::move(callback).Run(success, failure_reason);
  }

  static void OnArcVmCompleteBootResponse(
      std::optional<vm_tools::concierge::ArcVmCompleteBootResponse> reply) {
    vm_tools::concierge::ArcVmCompleteBootResult result =
        reply.has_value()
            ? reply.value().result()
            : vm_tools::concierge::ArcVmCompleteBootResult::BAD_REQUEST;

    VLOG(2) << "ArcVmCompleteBoot: result=" << result;
    if (result != vm_tools::concierge::ArcVmCompleteBootResult::SUCCESS)
      LOG(WARNING) << "Failed ArcVmCompleteBoot: result=" << result;
  }

  std::unique_ptr<ArcVmClientAdapterDelegate> delegate_;

  // A cryptohome ID of the primary profile.
  cryptohome::Identification cryptohome_id_;
  // A hash of the primary profile user ID.
  std::string user_id_hash_;
  // A serial number for the current profile.
  std::string serial_number_;

  StartParams start_params_;
  bool should_notify_observers_ = false;
  int64_t current_cid_ = kInvalidCid;

  // A timer that fires after ARCVM startup is suspected to be blocked by
  // patchpanel service startup.
  std::unique_ptr<base::OneShotTimer> patchpanel_timer_;

  FileSystemStatusRewriter file_system_status_rewriter_for_testing_;

  // The delegate is owned by ArcSessionRunner.
  raw_ptr<DemoModeDelegate> demo_mode_delegate_ = nullptr;

  // For callbacks.
  base::WeakPtrFactory<ArcVmClientAdapter> weak_factory_{this};
};

std::unique_ptr<ArcClientAdapter> CreateArcVmClientAdapter() {
  return std::make_unique<ArcVmClientAdapter>();
}

std::unique_ptr<ArcClientAdapter> CreateArcVmClientAdapterForTesting(
    const FileSystemStatusRewriter& rewriter) {
  return std::make_unique<ArcVmClientAdapter>(rewriter);
}

void SetArcVmBootNotificationServerAddressForTesting(
    const std::string& new_address,
    base::TimeDelta connect_timeout_limit,
    base::TimeDelta connect_sleep_duration_initial) {
  sockaddr_un* address =
      const_cast<sockaddr_un*>(GetArcVmBootNotificationServerAddress());
  DCHECK_GE(sizeof(address->sun_path), new_address.size());
  DCHECK_GT(connect_timeout_limit, connect_sleep_duration_initial);

  memset(address->sun_path, 0, sizeof(address->sun_path));
  // |new_address| may contain '\0' if it is an abstract socket address, so use
  // memcpy instead of strcpy.
  memcpy(address->sun_path, new_address.data(), new_address.size());

  g_connect_timeout_limit_for_testing = connect_timeout_limit;
  g_connect_sleep_duration_initial_for_testing = connect_sleep_duration_initial;
}

void SetArcVmBootNotificationServerFdForTesting(std::optional<int> fd) {
  g_boot_notification_server_fd = fd;
}

std::vector<std::string> GenerateUpgradePropsForTesting(
    const UpgradeParams& upgrade_params,
    const std::string& serial_number,
    const std::string& prefix) {
  return GenerateUpgradeProps(upgrade_params, serial_number, prefix);
}

void SetArcVmClientAdapterDelegateForTesting(  // IN-TEST
    ArcClientAdapter* adapter,
    std::unique_ptr<ArcVmClientAdapterDelegate> delegate) {
  static_cast<ArcVmClientAdapter*>(adapter)
      ->set_delegate_for_testing(  // IN-TEST
          std::move(delegate));
}

}  // namespace arc
