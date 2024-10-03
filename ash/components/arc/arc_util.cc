// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/components/arc/arc_util.h"

#include <algorithm>
#include <cstdio>
#include <optional>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_vm_data_migration_status.h"
#include "ash/constants/ash_switches.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/date_helper.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "chromeos/version/version_loader.h"
#include "components/exo/shell_surface_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/display/types/display_constants.h"

namespace arc {

namespace {

// This is for finch. See also crbug.com/633704 for details.
// TODO(hidehiko): More comments of the intention how this works, when
// we unify the commandline flags.
BASE_FEATURE(kEnableArcFeature, "EnableARC", base::FEATURE_DISABLED_BY_DEFAULT);

// Possible values for --arc-availability flag.
constexpr char kAvailabilityNone[] = "none";
constexpr char kAvailabilityInstalled[] = "installed";
constexpr char kAvailabilityOfficiallySupported[] = "officially-supported";
constexpr char kAlwaysStartWithNoPlayStore[] =
    "always-start-with-no-play-store";
constexpr char kManualStart[] = "manual";

constexpr const char kCrosSystemPath[] = "/usr/bin/crossystem";

// ArcUreadaheadMode param value strings.
constexpr char kReadahead[] = "readahead";
constexpr char kGenerate[] = "generate";
constexpr char kDisabled[] = "disabled";

constexpr const char kArcvmInstallAndroidImageDlc[] =
    "arcvm_2dinstall_2dandroid_2dimage_2ddlc";

// 10 minutes in ms.
constexpr int kArcvmInstallAndroidImageDlcTimeoutMs = 10 * 60 * 1000;

// Decodes a job name that may have "_2d" e.g. |kArcCreateDataJobName|
// and returns a decoded string.
std::string DecodeJobName(const std::string& raw_job_name) {
  constexpr const char* kFind = "_2d";
  std::string decoded(raw_job_name);
  base::ReplaceSubstringsAfterOffset(&decoded, 0, kFind, "-");
  return decoded;
}

// Called when the Upstart operation started in ConfigureUpstartJobs is
// done. Handles the fatal error (if any) and then starts the next job.
void OnConfigureUpstartJobs(std::deque<JobDesc> jobs,
                            chromeos::VoidDBusMethodCallback callback,
                            bool result) {
  const std::string job_name = DecodeJobName(jobs.front().job_name);
  const bool is_start = (jobs.front().operation == UpstartOperation::JOB_START);

  if (!result && is_start) {
    LOG(ERROR) << "Failed to start " << job_name;
    // TODO(khmel): Record UMA for this case.
    std::move(callback).Run(false);
    return;
  }

  VLOG(1) << job_name
          << (is_start ? " started" : (result ? " stopped " : " not running?"));
  jobs.pop_front();
  ConfigureUpstartJobs(std::move(jobs), std::move(callback));
}

int64_t GetRequiredDiskImageSizeForArcVmDataMigrationInBytes(
    uint64_t android_data_size_in_bytes) {
  // Reserved disk space for virtio-blk /data disk image (128 MB). Defined in
  // the guest's arc-mkfs-blk-data.
  constexpr uint64_t kReservedDiskSpaceInBytes = 128ULL << 20;
  return android_data_size_in_bytes * 11ULL / 10ULL + kReservedDiskSpaceInBytes;
}

void OnStaleArcVmStopped(
    EnsureStaleArcVmAndArcVmUpstartJobsStoppedCallback callback,
    std::optional<vm_tools::concierge::StopVmResponse> response) {
  // Successful response is returned even when the VM is not running. See
  // Service::StopVm() in platform2/vm_tools/concierge/service.cc.
  if (!response.has_value() || !response->success()) {
    LOG(ERROR) << "StopVm failed: "
               << (response.has_value() ? response->failure_reason()
                                        : "No D-Bus response.");
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(true);
}

void OnConciergeServiceAvailable(
    const std::string& user_id_hash,
    EnsureStaleArcVmAndArcVmUpstartJobsStoppedCallback callback,
    bool available) {
  if (!available) {
    LOG(ERROR) << "ConciergeService is not available";
    std::move(callback).Run(false);
    return;
  }
  vm_tools::concierge::StopVmRequest request;
  request.set_name(kArcVmName);
  request.set_owner_id(user_id_hash);
  ash::ConciergeClient::Get()->StopVm(
      request, base::BindOnce(&OnStaleArcVmStopped, std::move(callback)));
}

void OnStaleArcVmUpstartJobsStopped(
    const std::string& user_id_hash,
    EnsureStaleArcVmAndArcVmUpstartJobsStoppedCallback callback,
    bool stopped) {
  if (!stopped) {
    LOG(ERROR) << "Failed to stop stale ARCVM Upstart jobs";
    std::move(callback).Run(false);
    return;
  }
  if (!ash::ConciergeClient::Get()) {
    LOG(ERROR) << "ConciergeClient is not available";
    std::move(callback).Run(false);
    return;
  }
  ash::ConciergeClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
      &OnConciergeServiceAvailable, user_id_hash, std::move(callback)));
}

}  // namespace

bool IsArcAvailable() {
  const auto* command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(ash::switches::kArcAvailability)) {
    const std::string value =
        command_line->GetSwitchValueASCII(ash::switches::kArcAvailability);
    DCHECK(value == kAvailabilityNone || value == kAvailabilityInstalled ||
           value == kAvailabilityOfficiallySupported)
        << "Unknown flag value: " << value;
    return value == kAvailabilityOfficiallySupported ||
           (value == kAvailabilityInstalled &&
            base::FeatureList::IsEnabled(kEnableArcFeature));
  }

  // For transition, fallback to old flags.
  // TODO(hidehiko): Remove this and clean up whole this function, when
  // session_manager supports a new flag.
  return command_line->HasSwitch(ash::switches::kEnableArc) ||
         (command_line->HasSwitch(ash::switches::kArcAvailable) &&
          base::FeatureList::IsEnabled(kEnableArcFeature));
}

bool IsArcVmEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kEnableArcVm);
}

bool IsArcVmDlcEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kEnableArcVmDlc);
}

int GetArcAndroidSdkVersionAsInt() {
  const auto arc_version_str =
      chromeos::version_loader::GetArcAndroidSdkVersion();
  if (!arc_version_str) {
    // Expected in tests and linux-chromeos that don't have /etc/lsb-release.
    LOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS())
        << "ARC SDK version is unknown";
    return kMaxArcVersion;
  }
  int arc_version;
  if (!base::StringToInt(*arc_version_str, &arc_version)) {
    LOG(WARNING) << "ARC SDK version is not a number: " << *arc_version_str;
    return kMaxArcVersion;
  }
  return arc_version;
}

bool IsArcVmRtVcpuEnabled(uint32_t cpus) {
  // TODO(kansho): remove switch after tast test use Finch instead.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kEnableArcVmRtVcpu)) {
    return true;
  }
  if (cpus == 2 && base::FeatureList::IsEnabled(kRtVcpuDualCore)) {
    return true;
  }
  if (cpus > 2 && base::FeatureList::IsEnabled(kRtVcpuQuadCore)) {
    return true;
  }
  return false;
}

bool IsArcVmUseHugePages() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kArcVmUseHugePages);
}

bool IsArcVmDevConfIgnored() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kIgnoreArcVmDevConf);
}

bool IsArcUseDevCaches() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kArcUseDevCaches);
}

ArcUreadaheadMode GetArcUreadaheadMode(
    std::string_view ureadahead_mode_switch) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ureadahead_mode_switch)) {
    return ArcUreadaheadMode::READAHEAD;
  }
  ArcUreadaheadMode mode = ArcUreadaheadMode::READAHEAD;
  const std::string value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ureadahead_mode_switch);
  if (value == kReadahead) {
    mode = ArcUreadaheadMode::READAHEAD;
  } else if (value == kGenerate) {
    mode = ArcUreadaheadMode::GENERATE;
  } else if (value == kDisabled) {
    mode = ArcUreadaheadMode::DISABLED;
  } else {
    LOG(FATAL) << "Invalid parameter " << value << " for "
               << ureadahead_mode_switch;
  }
  return mode;
}

bool ShouldArcAlwaysStart() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             ash::switches::kArcStartMode) == kAlwaysStartWithNoPlayStore;
}

bool ShouldArcAlwaysStartWithNoPlayStore() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             ash::switches::kArcStartMode) == kAlwaysStartWithNoPlayStore;
}

bool ShouldArcStartManually() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             ash::switches::kArcStartMode) == kManualStart;
}

bool ShouldShowOptInForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kArcForceShowOptInUi);
}

bool IsRobotOrOfflineDemoAccountMode() {
  return user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsLoggedInAsManagedGuestSession();
}

bool IsArcAllowedForUser(const user_manager::User* user) {
  if (!user) {
    VLOG(1) << "No ARC for nullptr user.";
    return false;
  }

  // ARC is only supported for the following cases:
  // - Users have Gaia accounts;
  // - Public Session users;
  //   kPublicAccount check is compatible with IsRobotOrOfflineDemoAccountMode()
  //   above because public account user is always the primary/active user of a
  //   user session.
  if (!user->HasGaiaAccount() &&
      user->GetType() != user_manager::UserType::kPublicAccount) {
    VLOG(1) << "Only users with GAIA account or managed guest session users "
               "are supported in ARC.";
    return false;
  }

  return true;
}

bool IsArcOptInVerificationDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kDisableArcOptInVerification);
}

std::optional<int> GetWindowTaskId(const aura::Window* window) {
  if (!window) {
    return std::nullopt;
  }
  const std::string* window_app_id = exo::GetShellApplicationId(window);
  if (!window_app_id) {
    return std::nullopt;
  }
  return GetTaskIdFromWindowAppId(*window_app_id);
}

std::optional<int> GetTaskIdFromWindowAppId(const std::string& window_app_id) {
  int task_id;
  if (std::sscanf(window_app_id.c_str(), "org.chromium.arc.%d", &task_id) !=
      1) {
    return std::nullopt;
  }
  return task_id;
}

std::optional<int> GetWindowSessionId(const aura::Window* window) {
  if (!window) {
    return std::nullopt;
  }
  const std::string* window_app_id = exo::GetShellApplicationId(window);
  if (!window_app_id) {
    return std::nullopt;
  }
  return GetSessionIdFromWindowAppId(*window_app_id);
}

std::optional<int> GetSessionIdFromWindowAppId(
    const std::string& window_app_id) {
  int session_id;
  if (std::sscanf(window_app_id.c_str(), "org.chromium.arc.session.%d",
                  &session_id) != 1) {
    return std::nullopt;
  }
  return session_id;
}

std::optional<int> GetWindowTaskOrSessionId(const aura::Window* window) {
  auto result = GetWindowTaskId(window);
  if (result) {
    return result;
  }
  return GetWindowSessionId(window);
}

bool IsArcForceCacheAppIcon() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kArcGeneratePlayAutoInstall);
}

bool IsArcDataCleanupOnStartRequested() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kArcDataCleanupOnStart);
}

bool IsArcAppSyncFlowDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kArcDisableAppSync);
}

bool IsArcLocaleSyncDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kArcDisableLocaleSync);
}

bool IsArcPlayAutoInstallDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kArcDisablePlayAutoInstall);
}

int32_t GetLcdDensityForDeviceScaleFactor(float device_scale_factor) {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kArcScale)) {
    const std::string dpi_str =
        command_line->GetSwitchValueASCII(ash::switches::kArcScale);
    int dpi;
    if (base::StringToInt(dpi_str, &dpi)) {
      return dpi;
    }
    VLOG(1) << "Invalid Arc scale set. Using default.";
  }
  // TODO(b/131884992): Remove the logic to update default lcd density once
  // per-display-density is supported.
  constexpr float kEpsilon = 0.001;
  if (std::abs(device_scale_factor - display::kDsf_2_252) < kEpsilon) {
    return 280;
  }
  if (std::abs(device_scale_factor - 2.4f) < kEpsilon) {
    return 280;
  }
  if (std::abs(device_scale_factor - 1.6f) < kEpsilon) {
    return 213;  // TVDPI
  }
  if (std::abs(device_scale_factor - display::kDsf_1_777) < kEpsilon) {
    return 240;  // HDPI
  }
  if (std::abs(device_scale_factor - display::kDsf_1_8) < kEpsilon) {
    return 240;  // HDPI
  }
  if (std::abs(device_scale_factor - display::kDsf_2_666) < kEpsilon) {
    return 320;  // XHDPI
  }

  constexpr float kChromeScaleToAndroidScaleRatio = 0.75f;
  constexpr int32_t kDefaultDensityDpi = 160;
  return static_cast<int32_t>(
      std::max(1.0f, device_scale_factor * kChromeScaleToAndroidScaleRatio) *
      kDefaultDensityDpi);
}

int GetSystemPropertyInt(const std::string& property) {
  std::string output;
  if (!base::GetAppOutput({kCrosSystemPath, property}, &output)) {
    return -1;
  }
  int output_int;
  return base::StringToInt(output, &output_int) ? output_int : -1;
}

JobDesc::JobDesc(const std::string& job_name,
                 UpstartOperation operation,
                 const std::vector<std::string>& environment)
    : job_name(job_name), operation(operation), environment(environment) {}

JobDesc::~JobDesc() = default;

JobDesc::JobDesc(const JobDesc& other) = default;

void ConfigureUpstartJobs(std::deque<JobDesc> jobs,
                          chromeos::VoidDBusMethodCallback callback) {
  if (jobs.empty()) {
    std::move(callback).Run(true);
    return;
  }

  if (jobs.front().operation == UpstartOperation::JOB_STOP_AND_START) {
    // Expand the restart operation into two, stop and start.
    jobs.front().operation = UpstartOperation::JOB_START;
    jobs.push_front({jobs.front().job_name, UpstartOperation::JOB_STOP,
                     jobs.front().environment});
  }

  const auto& job_name = jobs.front().job_name;
  const auto& operation = jobs.front().operation;
  const auto& environment = jobs.front().environment;

  VLOG(1) << (operation == UpstartOperation::JOB_START ? "Starting "
                                                       : "Stopping ")
          << DecodeJobName(job_name);

  auto wrapped_callback = base::BindOnce(&OnConfigureUpstartJobs,
                                         std::move(jobs), std::move(callback));
  switch (operation) {
    case UpstartOperation::JOB_START:
      // DLC installation may take a longer time to respond.
      if (job_name == kArcvmInstallAndroidImageDlc) {
        ash::UpstartClient::Get()->StartJobWithTimeout(
            job_name, environment, std::move(wrapped_callback),
            kArcvmInstallAndroidImageDlcTimeoutMs);
      } else {
        ash::UpstartClient::Get()->StartJob(job_name, environment,
                                            std::move(wrapped_callback));
      }
      break;
    case UpstartOperation::JOB_STOP:
      ash::UpstartClient::Get()->StopJob(job_name, environment,
                                         std::move(wrapped_callback));
      break;
    case UpstartOperation::JOB_STOP_AND_START:
      NOTREACHED();
  }
}

ArcVmDataMigrationStatus GetArcVmDataMigrationStatus(PrefService* prefs) {
  return static_cast<ArcVmDataMigrationStatus>(
      prefs->GetInteger(prefs::kArcVmDataMigrationStatus));
}

ArcVmDataMigrationStrategy GetArcVmDataMigrationStrategy(PrefService* prefs) {
  int value =
      std::max(0, prefs->GetInteger(prefs::kArcVmDataMigrationStrategy));
  if (value > static_cast<int>(ArcVmDataMigrationStrategy::kMaxValue)) {
    LOG(ERROR) << "Unexpected value for ArcVmDataMigrationStrategy pref: "
               << value;
    value = static_cast<int>(ArcVmDataMigrationStrategy::kPrompt);
  }
  return static_cast<ArcVmDataMigrationStrategy>(value);
}

void SetArcVmDataMigrationStatus(PrefService* prefs,
                                 ArcVmDataMigrationStatus status) {
  prefs->SetInteger(prefs::kArcVmDataMigrationStatus, static_cast<int>(status));
}

bool ShouldUseVirtioBlkData(PrefService* prefs) {
  // If kEnableVirtioBlkForData is set, force using virtio-blk /data regardless
  // of the migration status.
  if (base::FeatureList::IsEnabled(kEnableVirtioBlkForData)) {
    return true;
  }

  // Just use virtio-fs when ARCVM /data migration is not enabled.
  if (!base::FeatureList::IsEnabled(kEnableArcVmDataMigration)) {
    return false;
  }

  ArcVmDataMigrationStatus status = GetArcVmDataMigrationStatus(prefs);
  if (status == ArcVmDataMigrationStatus::kFinished) {
    VLOG(1) << "ARCVM /data migration has finished";
    return true;
  }
  VLOG(1) << "ARCVM /data migration hasn't finished yet. Status=" << status;
  return false;
}

bool ShouldUseArcKeyMint() {
  auto version = GetArcAndroidSdkVersionAsInt();
  // TODO(b/308630124): Change to ">= kArcVersionT", when ready to enable
  // KeyMint on ARC V+.
  return version == kArcVersionT && version < kMaxArcVersion &&
         base::FeatureList::IsEnabled(kSwitchToKeyMintOnT) &&
         (!base::CommandLine::ForCurrentProcess()->HasSwitch(
              ash::switches::kArcBlockKeyMint) ||
          base::FeatureList::IsEnabled(kSwitchToKeyMintOnTOverride));
}

bool ShouldUseArcAttestation() {
  // Attesation depends on keymint.
  return ShouldUseArcKeyMint() &&
         base::FeatureList::IsEnabled(kEnableArcAttestation);
}

int GetDaysUntilArcVmDataMigrationDeadline(PrefService* prefs) {
  if (GetArcVmDataMigrationStatus(prefs) ==
      ArcVmDataMigrationStatus::kStarted) {
    // If ARCVM /data migration is in progress. Treat it in the same way as
    // cases where the deadline is passed.
    // TODO(b/258278176): Do not call this function when the migration is in
    // progress, or return a different value (0) to provide a dedicated UI.
    return 1;
  }
  const base::Time notification_first_shown_time =
      prefs->GetTime(prefs::kArcVmDataMigrationNotificationFirstShownTime);
  if (notification_first_shown_time == base::Time()) {
    // The preference is uninitialized (the notification has not been shown).
    LOG(ERROR) << "No deadline can be calculated because ARCVM /data migration "
                  "notification has not been shown before";
    return kArcVmDataMigrationNumberOfDismissibleDays;
  }

  auto* date_helper = ash::DateHelper::GetInstance();
  DCHECK(date_helper);
  // Calculate the deadline assuming that the first notification was shown in
  // the current timezone.
  // ash::calendar_utils::kDurationForAdjustingDST is added to take into account
  // days longer than 24 hours due to daylight saving time.
  // For example, if the notification is shown for the first time at
  // 2023-01-01T16:00:00Z and kArcVmDataMigrationNumberOfDismissibleDays is 30,
  // the deadline will be 2023-01-31T00:00:00Z.
  // This function will return 30 until 2023-01-01T23:59:99Z and keep returning
  // 1 from 2023-01-30T00:00:00Z onward.
  const base::Time deadline = date_helper->GetLocalMidnight(
      date_helper->GetLocalMidnight(notification_first_shown_time) +
      kArcVmDataMigrationDismissibleTimeDelta +
      ash::calendar_utils::kDurationForAdjustingDST);
  const base::Time last_local_midnight =
      date_helper->GetLocalMidnight(base::Time::Now());
  const base::TimeDelta delta =
      last_local_midnight < deadline
          ? deadline - last_local_midnight +
                ash::calendar_utils::kDurationForAdjustingDST
          : base::Days(0);
  const int delta_in_days = delta.InDays();
  if (delta_in_days > kArcVmDataMigrationNumberOfDismissibleDays) {
    return kArcVmDataMigrationNumberOfDismissibleDays;
  }
  return std::max(delta_in_days, 1);
}

bool ArcVmDataMigrationShouldBeDismissible(int days_until_deadline) {
  return days_until_deadline > 1;
}

uint64_t GetDesiredDiskImageSizeForArcVmDataMigrationInBytes(
    uint64_t android_data_size_in_bytes,
    uint64_t free_disk_space_in_bytes) {
  // Mask to make the disk image size a multiple of the block size (4096 bytes).
  constexpr uint64_t kDiskImageSizeMaskInBytes = ~((4ULL << 10) - 1);

  // Minimum disk image size for virtio-blk /data (4 GB).
  constexpr uint64_t kMinimumDiskImageSizeInBytes = 4ULL << 30;

  // The default disk image size set by Concierge.
  const uint64_t default_disk_image_size_in_bytes =
      free_disk_space_in_bytes * 9ULL / 10ULL;

  const uint64_t required_disk_image_size_in_bytes =
      GetRequiredDiskImageSizeForArcVmDataMigrationInBytes(
          android_data_size_in_bytes);

  return std::max(default_disk_image_size_in_bytes +
                      required_disk_image_size_in_bytes,
                  kMinimumDiskImageSizeInBytes) &
         kDiskImageSizeMaskInBytes;
}

uint64_t GetRequiredFreeDiskSpaceForArcVmDataMigrationInBytes(
    uint64_t android_data_size_src_in_bytes,
    uint64_t android_data_size_dest_in_bytes,
    uint64_t free_disk_space_in_bytes) {
  // Mask to make the required free disk space a multiple of 512 MB.
  constexpr uint64_t kRequiredFreeDiskSpaceMaskInBytes = ~((512ULL << 20) - 1);

  // Minimum required free disk space for ARCVM /data migration (1 GB).
  constexpr uint64_t kMinimumRequiredFreeDiskSpaceInBytes = 1ULL << 30;

  const uint64_t required_disk_image_size_in_bytes =
      GetRequiredDiskImageSizeForArcVmDataMigrationInBytes(
          android_data_size_dest_in_bytes);

  const uint64_t maximum_disk_space_overhead_in_bytes =
      required_disk_image_size_in_bytes - android_data_size_dest_in_bytes;

  // Amount of additional disk space required after the migration due to
  // expanded sparse files in Android /data.
  uint64_t android_data_expansion_size_in_bytes = 0;
  if (android_data_size_dest_in_bytes > android_data_size_src_in_bytes) {
    android_data_expansion_size_in_bytes =
        android_data_size_dest_in_bytes - android_data_size_src_in_bytes;
  }

  return (kMinimumRequiredFreeDiskSpaceInBytes +
          maximum_disk_space_overhead_in_bytes +
          android_data_expansion_size_in_bytes) &
         kRequiredFreeDiskSpaceMaskInBytes;
}

bool IsReadOnlyPermissionsEnabled() {
  return base::FeatureList::IsEnabled(arc::kEnableReadOnlyPermissions) &&
         GetArcAndroidSdkVersionAsInt() >= kArcVersionT;
}

void EnsureStaleArcVmAndArcVmUpstartJobsStopped(
    const std::string& user_id_hash,
    EnsureStaleArcVmAndArcVmUpstartJobsStoppedCallback callback) {
  // Stop stale Upstart jobs first. StopVm() is called after
  // ConfigureUpstartJobs() is successfully finished.
  std::deque<JobDesc> jobs;
  for (const char* job : kArcVmUpstartJobsToBeStoppedOnRestart) {
    jobs.emplace_back(job, UpstartOperation::JOB_STOP,
                      std::vector<std::string>());
  }
  ConfigureUpstartJobs(std::move(jobs),
                       base::BindOnce(&OnStaleArcVmUpstartJobsStopped,
                                      user_id_hash, std::move(callback)));
}

bool ShouldAlwaysMountAndroidVolumesInFilesForTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kArcForceMountAndroidVolumesInFiles);
}

bool ShouldDeferArcActivationUntilUserSessionStartUpTaskCompletion(
    const PrefService* prefs) {
  if (!base::FeatureList::IsEnabled(
          kDeferArcActivationUntilUserSessionStartUpTaskCompletion)) {
    return false;
  }

  const int max_window_size = kDeferArcActivationHistoryWindow.Get();
  const int threshold = kDeferArcActivationHistoryThreshold.Get();
  if (max_window_size < 0 || threshold < 0) {
    LOG(ERROR) << "Unexpected negative value(s): " << max_window_size << ", "
               << threshold;
    return false;
  }

  // Look at recent (at most) `histogram_window` sessions, and if ARC is
  // activated during user session start up tasks more than or equals to
  // `history_threshold` times, we'll immediately activate ARC.
  // I.e., if ARC is activated during user session start up tasks less than
  // `history_threshold` times, we'll defer the ARC activation until
  // the user session start up task completion.
  const auto& history =
      prefs->GetList(prefs::kArcFirstActivationDuringUserSessionStartUpHistory);
  const size_t window_size = std::min<size_t>(history.size(), max_window_size);
  base::span<const base::Value> history_window(history.end() - window_size,
                                               history.end());
  return base::ranges::count(history_window, base::Value(true)) < threshold;
}

void RecordFirstActivationDuringUserSessionStartUp(PrefService* prefs,
                                                   bool value) {
  if (!base::FeatureList::IsEnabled(
          kDeferArcActivationUntilUserSessionStartUpTaskCompletion)) {
    return;
  }

  const int window_size = kDeferArcActivationHistoryWindow.Get();
  if (window_size < 0) {
    LOG(ERROR) << "Unexpected negative window_size: " << window_size;
    return;
  }

  ScopedListPrefUpdate update(
      prefs, prefs::kArcFirstActivationDuringUserSessionStartUpHistory);
  auto& history = update.Get();
  // Limit the size up to the history_window.
  history.Append(value);
  if (history.size() >= static_cast<size_t>(window_size)) {
    history.erase(history.begin(), history.end() - window_size);
  }
}

}  // namespace arc
