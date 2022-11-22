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
#include "chromeos/dbus/common/dbus_method_call_status.h"

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

// Enum for configuring ureadahead mode of operation during ARCVM boot process.
enum class ArcVmUreadaheadMode {
  // ARCVM ureadahead is in readahead mode for normal user boot flow.
  READAHEAD = 0,
  // ARCVM ureadahead is turned on for generate mode in data collector flow.
  GENERATE,
  // ARCVM ureadahead is turned off for disabled mode.
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

// Returns true if ureadahead is disabled completely, including host and guest
// parts. See also |GetArcVmUreadaheadMode|.
bool IsUreadaheadDisabled();

// Returns mode of operation for ureadahead during the ARCVM boot flow.
// Valid modes are readahead, generate, or disabled.
ArcVmUreadaheadMode GetArcVmUreadaheadMode();

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

// Returns true if ARC is installed and running ARC kiosk apps on the current
// device is officially supported.
// It doesn't follow that ARC is available for user sessions and
// IsArcAvailable() will return true, although the reverse should be.
// This is used to distinguish special cases when ARC kiosk is available on
// the device, but is not yet supported for regular user sessions.
// In most cases, IsArcAvailable() check should be used instead of this.
// Also not that this function may return true when ARC is not running in
// Kiosk mode, it checks only ARC Kiosk availability.
bool IsArcKioskAvailable();

// Returns true if ARC should run under Kiosk mode for the current profile.
// As it can return true only when user is already initialized, it implies
// that ARC availability was checked before and IsArcKioskAvailable()
// should also return true in that case.
bool IsArcKioskMode();

// Returns true if current user is a robot account user, or offline demo mode
// user.
// These are Public Session and ARC Kiosk users. Note that demo mode, including
// offline demo mode, is implemented as a Public Session - offline demo mode
// is setup offline and it isn't associated with a working robot account.
// As it can return true only when user is already initialized, it implies
// that ARC availability was checked before.
// The check is basically IsArcKioskMode() | IsLoggedInAsPublicSession().
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
// absl::nullopt if not an ARC window.
absl::optional<int> GetWindowTaskId(const aura::Window* window);
absl::optional<int> GetTaskIdFromWindowAppId(const std::string& window_app_id);
absl::optional<int> GetWindowSessionId(const aura::Window* window);
absl::optional<int> GetSessionIdFromWindowAppId(
    const std::string& window_app_id);
absl::optional<int> GetWindowTaskOrSessionId(const aura::Window* window);

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

// Sets the ArcVmDataMigrationStatus profile preference.
void SetArcVmDataMigrationStatus(PrefService* prefs,
                                 ArcVmDataMigrationStatus status);

// Returns whether ARCVM should use virtio-blk for /data.
bool ShouldUseVirtioBlkData(PrefService* prefs);

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_ARC_UTIL_H_
