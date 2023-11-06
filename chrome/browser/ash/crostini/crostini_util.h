// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_UTIL_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_UTIL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/crostini/crostini_simple_types.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_launcher.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "third_party/skia/include/core/SkColor.h"

namespace aura {
class Window;
}  // namespace aura

namespace base {
class FilePath;
class TimeTicks;
}  // namespace base

class Profile;

namespace crostini {

inline constexpr char kCrostiniImageAliasPattern[] = "debian/%s";
inline constexpr char kCrostiniContainerDefaultVersion[] = "bookworm";
inline constexpr char kCrostiniContainerFlag[] =
    "crostini-container-install-version";

inline constexpr guest_os::VmType kCrostiniDefaultVmType =
    guest_os::VmType::TERMINA;
inline constexpr char kCrostiniDefaultVmName[] = "termina";
inline constexpr char kCrostiniDefaultContainerName[] = "penguin";
inline constexpr char kCrostiniDefaultUsername[] = "emperor";
inline constexpr char kCrostiniDefaultImageServerUrl[] =
    "https://storage.googleapis.com/cros-containers/%d";
inline constexpr char kCrostiniDlcName[] = "termina-dlc";

inline constexpr base::FilePath::CharType kHomeDirectory[] =
    FILE_PATH_LITERAL("/home/chronos/user");

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CrostiniAppLaunchAppType {
  // An app which isn't in the CrostiniAppRegistry. This shouldn't happen.
  kUnknownApp = 0,

  // The main terminal app.
  kTerminal = 1,

  // An app for which there is something in the CrostiniAppRegistry.
  kRegisteredApp = 2,

  kMaxValue = kRegisteredApp,
};

// Checks if user profile is able to a crostini app with a given app_id.
bool IsUninstallable(Profile* profile, const std::string& app_id);

// Returns whether the default Crostini VM is running for the user.
bool IsCrostiniRunning(Profile* profile);

// Whether the user is able to perform a container upgrade.
bool ShouldAllowContainerUpgrade(Profile* profile);

// Returns whether default Crostini container should be configured according to
// the configuration specified by CrostiniAnsiblePlaybook user policy.
bool ShouldConfigureDefaultContainer(Profile* profile);

// Launch a Crostini App with a given set of files, given as absolute paths in
// the container. For apps which can only be launched with a single file,
// launch multiple instances.
void LaunchCrostiniApp(
    Profile* profile,
    const std::string& app_id,
    int64_t display_id,
    const std::vector<guest_os::LaunchArg>& args = {},
    guest_os::launcher::SuccessCallback callback = base::DoNothing());

void LaunchCrostiniAppWithIntent(
    Profile* profile,
    const std::string& app_id,
    int64_t display_id,
    apps::IntentPtr intent,
    const std::vector<guest_os::LaunchArg>& args = {},
    guest_os::launcher::SuccessCallback callback = base::DoNothing());

// Determine features to enable in the container on app/terminal launches.
std::vector<vm_tools::cicerone::ContainerFeature> GetContainerFeatures();

// Retrieves cryptohome_id from profile.
std::string CryptohomeIdForProfile(Profile* profile);

// Retrieves username from profile.
std::string DefaultContainerUserNameForProfile(Profile* profile);

// Returns the mount directory within the container where paths from the Chrome
// OS host such as within Downloads are shared with the container.
base::FilePath ContainerChromeOSBaseDirectory();

// Returns a list of ports currently being forwarded in Crostini as a JSON
// object.
std::string GetActivePortListAsJSON(Profile* profile);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CrostiniUISurface { kSettings = 0, kAppList = 1, kCount };

// Add a newly created LXD container to the kCrostiniContainers pref
void AddNewLxdContainerToPrefs(Profile* profile,
                               const guest_os::GuestId& container_id);

// Remove a newly deleted LXD container from the kCrostiniContainers pref, and
// deregister its apps and mime types.
void RemoveLxdContainerFromPrefs(Profile* profile,
                                 const guest_os::GuestId& container_id);

// Returns a string to be displayed in a notification with the estimated time
// left for an operation to run which started and time |start| and is current
// at |percent| way through.
std::u16string GetTimeRemainingMessage(base::TimeTicks start, int percent);

SkColor GetContainerBadgeColor(Profile* profile,
                               const guest_os::GuestId& container_id);

void SetContainerBadgeColor(Profile* profile,
                            const guest_os::GuestId& container_id,
                            SkColor badge_color);

bool IsContainerVersionExpired(Profile* profile,
                               const guest_os::GuestId& container_id);

const guest_os::GuestId& DefaultContainerId();

bool IsCrostiniWindow(const aura::Window* window);

void RecordAppLaunchHistogram(CrostiniAppLaunchAppType app_type);
void RecordAppLaunchResultHistogram(CrostiniAppLaunchAppType type,
                                    crostini::CrostiniResult reason);

// Tests whether or not the specified Container is the last one running on it's
// VM. Returns true if the VM should be stopped.
bool ShouldStopVm(Profile* profile, const guest_os::GuestId& container_id);

// Formats a container id the way most UI surfaces identify Crostini containers.
std::string FormatForUi(guest_os::GuestId container_id);

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_UTIL_H_
