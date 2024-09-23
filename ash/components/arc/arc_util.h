// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_ARC_UTIL_H_
#define ASH_COMPONENTS_ARC_ARC_UTIL_H_

// This file contains utility to see ARC functionality status controlled by
// outside of ARC, e.g. CommandLine flag, attribute of global data/state,
// users' preferences, and FeatureList.

#include <stdint.h>

#include <deque>
#include <string>
#include <vector>

#include "ash/components/arc/session/arc_vm_data_migration_status.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chromeos/dbus/common/dbus_callback.h"

namespace aura {
class Window;
}  // namespace aura

namespace user_manager {
class User;
}  // namespace user_manager

class PrefService;

namespace arc {

// This enum should be synced with CpuRestrictionState in
// src/third_party/cros_system_api/dbus/vm_concierge/concierge_service.proto
enum class CpuRestrictionState {
  // The CPU usage is relaxed.
  CPU_RESTRICTION_FOREGROUND = 0,
  // The CPU usage is tightly restricted.
  CPU_RESTRICTION_BACKGROUND = 1,
};

enum class UpstartOperation {
  JOB_START = 0,
  JOB_STOP,
  // This sends STOP D-Bus message, then sends START. Unlike 'initctl restart',
  // this starts the job even when the job hasn't been started yet (and
  // therefore the stop operation fails.)
  JOB_STOP_AND_START,
};

// Enum for configuring ureadahead mode of operation during ARC boot process for
// both host and guest.
enum class ArcUreadaheadMode {
  // ARC ureadahead is in readahead mode for normal user boot flow.
  READAHEAD = 0,
  // ARC ureadahead is turned on for generate mode in data collector flow.
  GENERATE,
  // ARC ureadahead is turned off for disabled mode.
  DISABLED,
};

// Upstart Job Description
struct JobDesc {
  // Explicit ctor/dtor declaration is necessary for complex struct. See
  // https://cs.chromium.org/chromium/src/tools/clang/plugins/FindBadConstructsConsumer.cpp
  JobDesc(const std::string& job_name,
          UpstartOperation operation,
          const std::vector<std::string>& environment);
  ~JobDesc();
  JobDesc(const JobDesc& other);

  std::string job_name;
  UpstartOperation operation;
  std::vector<std::string> environment;
};

// Name of the crosvm instance when ARCVM is enabled.
constexpr char kArcVmName[] = "arcvm";

// Android SDK versions. See GetArcAndroidSdkVersionAsInt().
constexpr int kArcVersionP = 28;
constexpr int kArcVersionR = 30;
constexpr int kArcVersionT = 33;
constexpr int kMaxArcVersion = 999;

// How long ARCVM /data migration notification and dialog are dismissible.
constexpr int kArcVmDataMigrationNumberOfDismissibleDays = 30;
constexpr base::TimeDelta kArcVmDataMigrationDismissibleTimeDelta =
    base::Days(kArcVmDataMigrationNumberOfDismissibleDays);

// Names of Upstart jobs that are managed in the ARCVM boot sequence.
// The "_2d" in job names below corresponds to "-". Upstart escapes characters
// that aren't valid in D-Bus object paths with underscore followed by its
// ascii code in hex. So "arc_2dcreate_2ddata" becomes "arc-create-data".
constexpr char kArcVmDataMigratorJobName[] = "arcvm_2ddata_2dmigrator";
constexpr char kArcVmMediaSharingServicesJobName[] =
    "arcvm_2dmedia_2dsharing_2dservices";
constexpr const char kArcVmPerBoardFeaturesJobName[] =
    "arcvm_2dper_2dboard_2dfeatures";
constexpr char kArcVmPreLoginServicesJobName[] =
    "arcvm_2dpre_2dlogin_2dservices";
constexpr char kArcVmPostLoginServicesJobName[] =
    "arcvm_2dpost_2dlogin_2dservices";
constexpr char kArcVmPostVmStartServicesJobName[] =
    "arcvm_2dpost_2dvm_2dstart_2dservices";

// List of Upstart jobs that can outlive ARC sessions (e.g. after Chrome crash,
// Chrome restart on a feature flag change) and thus should be stopped at the
// beginning of the ARCVM boot sequence.
constexpr std::array<const char*, 5> kArcVmUpstartJobsToBeStoppedOnRestart = {
    kArcVmDataMigratorJobName,         kArcVmPreLoginServicesJobName,
    kArcVmPostLoginServicesJobName,    kArcVmPostVmStartServicesJobName,
    kArcVmMediaSharingServicesJobName,
};

// Returns true if ARC is installed and the current device is officially
// supported to run ARC.
// Note that, to run ARC practically, it is necessary to meet more conditions,
// e.g., ARC supports only on Primary User Profile. To see if ARC can actually
// run for the profile etc., arc::ArcSessionManager::IsAllowedForProfile() is
// the function for that purpose. Please see also its comment, too.
// Also note that, ARC singleton classes (e.g. ArcSessionManager,
// ArcServiceManager, ArcServiceLauncher) are instantiated regardless of this
// check, so it is ok to access them directly.
bool IsArcAvailable();

// Returns true if ARC VM is enabled.
// Note: NEVER use this function to distinguish ARC P from R+. For that purpose,
// use GetArcAndroidSdkVersionAsInt() instead. IsArcVmEnabled() returns *false*
// for ARC R container and your code won't work on that configuration.
bool IsArcVmEnabled();

// Returns true if ARC VM DLC is enabled.
bool IsArcVmDlcEnabled();

// This is a thin wrapper around version_loader::GetArcAndroidSdkVersion() and
// returns the version as integer. For example, when the device uses ARC++ P,
// it returns kArcVersionP that is 28, and for ARC++ container R and ARCVM R, it
// returns kArcVersionR or 30. When the version is not a number, e.g. "master",
// or the version is unknown, it returns kMaxArcVersion, a large number.
int GetArcAndroidSdkVersionAsInt();

// Returns true if ARC VM realtime VCPU is enabled.
// |cpus| is the number of logical cores that are currently online on the
// device.
bool IsArcVmRtVcpuEnabled(uint32_t cpus);

// Returns true if ARC VM advised to use Huge Pages for guest memory.
bool IsArcVmUseHugePages();

// Returns true if all development configuration directives in the
// vm_tools/init/arcvm_dev.conf file are ignored during ARCVM start.
bool IsArcVmDevConfIgnored();

// Returns true if ARC is using dev caches for arccachesetup service.
bool IsArcUseDevCaches();

// Returns mode of operation for ureadahead during the ARC boot flow.
// Valid modes are readahead, generate, or disabled.
ArcUreadaheadMode GetArcUreadaheadMode(std::string_view ureadahead_mode_switch);

// Returns true if ARC should always start within the primary user session
// (opted in user or not), and other supported mode such as guest and Kiosk
// mode.
bool ShouldArcAlwaysStart();

// Returns true if ARC should always start with no Play Store availability
// within the primary user session (opted in user or not), and other supported
// mode such as guest and Kiosk mode.
bool ShouldArcAlwaysStartWithNoPlayStore();

// Returns true if ARC should ignore Play Store preference and be started
// manually in tests using autotest API |startArc|.
bool ShouldArcStartManually();

// Returns true if ARC OptIn ui needs to be shown for testing.
bool ShouldShowOptInForTesting();

// Returns true if current user is a robot account user, or offline demo mode
// user.
// These are Public Session users. Note that demo mode, including
// offline demo mode, is implemented as a Public Session - offline demo mode
// is setup offline and it isn't associated with a working robot account.
// As it can return true only when user is already initialized, it implies
// that ARC availability was checked before.
bool IsRobotOrOfflineDemoAccountMode();

// Returns true if ARC is allowed for the given user. Note this should not be
// used as a signal of whether ARC is allowed alone because it only considers
// user meta data. e.g. a user could be allowed for ARC but if the user signs in
// as a secondary user or signs in to create a supervised user, ARC should be
// disabled for such cases.
bool IsArcAllowedForUser(const user_manager::User* user);

// Checks if opt-in verification was disabled by switch in command line.
// In most cases, it is disabled for testing purpose.
bool IsArcOptInVerificationDisabled();

constexpr int kNoTaskId = -1;
constexpr int kSystemWindowTaskId = 0;
// Returns the task id given by the exo shell's application id, or
// std::nullopt if not an ARC window.
std::optional<int> GetWindowTaskId(const aura::Window* window);
std::optional<int> GetTaskIdFromWindowAppId(const std::string& window_app_id);
std::optional<int> GetWindowSessionId(const aura::Window* window);
std::optional<int> GetSessionIdFromWindowAppId(
    const std::string& window_app_id);
std::optional<int> GetWindowTaskOrSessionId(const aura::Window* window);

// Returns true if ARC app icons are forced to cache.
bool IsArcForceCacheAppIcon();

// Returns true if data clean up is requested for each ARC start.
bool IsArcDataCleanupOnStartRequested();

// Returns true in case ARC app sync flow is disabled.
bool IsArcAppSyncFlowDisabled();

// Returns true in case ARC locale sync is disabled.
bool IsArcLocaleSyncDisabled();

// Returns true in case ARC Play Auto Install flow is disabled.
bool IsArcPlayAutoInstallDisabled();

// Returns the Android density that should be used for the given device scale
// factor used on chrome.
int32_t GetLcdDensityForDeviceScaleFactor(float device_scale_factor);

// Gets a system property managed by crossystem. This function can be called
// only with base::MayBlock().
int GetSystemPropertyInt(const std::string& property);

// Starts or stops a job in |jobs| one by one. If starting a job fails, the
// whole operation is aborted and the |callback| is immediately called with
// false. Errors on stopping a job is just ignored with some logs. Once all jobs
// are successfully processed, |callback| is called with true.
void ConfigureUpstartJobs(std::deque<JobDesc> jobs,
                          chromeos::VoidDBusMethodCallback callback);

// Gets the ArcVmDataMigrationStatus profile preference.
ArcVmDataMigrationStatus GetArcVmDataMigrationStatus(PrefService* prefs);

// Gets the ArcVmDatamigrationStrategy profile preference.
ArcVmDataMigrationStrategy GetArcVmDataMigrationStrategy(PrefService* prefs);

// Sets the ArcVmDataMigrationStatus profile preference.
void SetArcVmDataMigrationStatus(PrefService* prefs,
                                 ArcVmDataMigrationStatus status);

// Returns whether ARCVM should use virtio-blk for /data.
bool ShouldUseVirtioBlkData(PrefService* prefs);

// Returns true if ARC should use KeyMint. Returns false if ARC should use
// Keymaster. It is based on the lsb-release value. If missing lsb-release
// value (e.g. in unit tests), it returns false. Use
// `ScopedChromeOSVersionInfo` to set ARC version in unit test, if needed.
bool ShouldUseArcKeyMint();

// Returns true if ARC should use key and ID attestation. It is based on the
// lsb-release value. If missing lsb-release value (e.g. in unit tests), it
// returns false. Use `ScopedChromeOSVersionInfo` to set ARC version in unit
// test, if needed.
bool ShouldUseArcAttestation();

// Returns ARCVM /data migration should be done within how many days. When the
// migration has not started, the value is calculated from the time when the
// ARCVM /data migration notification is shown for the first time. When the
// migration is in progress, the minimum value 1 is returned, which means the
// migration should be done within a day = today.
int GetDaysUntilArcVmDataMigrationDeadline(PrefService* prefs);

// Whether ARCVM /data migration notification and/or dialog should be
// dismissible given the number of days returned by
// GetDaysUntilArcVmDataMigrationDeadline().
bool ArcVmDataMigrationShouldBeDismissible(int days_until_deadline);

// Calculates and returns the desired disk image size for the destination of
// ARCVM /data migration based on the size of the source (existing Android
// /data) and free disk space.
uint64_t GetDesiredDiskImageSizeForArcVmDataMigrationInBytes(
    uint64_t android_data_size_in_bytes,
    uint64_t free_disk_space_in_bytes);

// Calculates and returns how much free disk space should be there to start
// ARCVM /data migration based on the disk space allocated for pre-migration
// Android /data, estimated disk space allocated for migrated /data, and free
// disk space.
uint64_t GetRequiredFreeDiskSpaceForArcVmDataMigrationInBytes(
    uint64_t android_data_size_src_in_bytes,
    uint64_t android_data_size_dest_in_bytes,
    uint64_t free_disk_space_in_bytes);

// Returns true if ARC app permissions should be shown as read-only in the App
// Management page.
bool IsReadOnlyPermissionsEnabled();

// Stops ARCVM instance and ARCVM Upstart jobs that can outlive ARC sessions
// (e.g. after Chrome crash, Chrome restart on a feature flag change).
// `user_id_hash` is the current user's ID hash (= ARCVM's owner ID).
// `callback` is invoked with true when 1) StopJob() is called on each Upstart
// job in `kArcVmUpstartJobsToBeStoppedOnRestart`, and 2) ARCVM is stopped (or
// not running in the first place).
using EnsureStaleArcVmAndArcVmUpstartJobsStoppedCallback =
    base::OnceCallback<void(bool)>;
void EnsureStaleArcVmAndArcVmUpstartJobsStopped(
    const std::string& user_id_hash,
    EnsureStaleArcVmAndArcVmUpstartJobsStoppedCallback callback);

// Returns if Android volumes (DocumentsProviders and Play files) should be
// mounted in the Files app regardless of whether Play Store is enabled or not.
bool ShouldAlwaysMountAndroidVolumesInFilesForTesting();

// Returns true if ARC's activation should be deferred until the user session
// start up tasks are completed.
// This checks the history of first ARC activation timing in recent user
// sessions, and decides whether or not to defer ARC.
// See also b/326065955#comment9 and linked materials for more background.
bool ShouldDeferArcActivationUntilUserSessionStartUpTaskCompletion(
    const PrefService* prefs);

// Records whether first ARC activation is done during user session start up
// in `prefs`. Just to be explicit, `value` == true means the first activation
// is done during the user session start up.
void RecordFirstActivationDuringUserSessionStartUp(PrefService* prefs,
                                                   bool value);

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_ARC_UTIL_H_
