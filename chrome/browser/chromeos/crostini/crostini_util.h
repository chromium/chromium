// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UTIL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/chromeos/crostini/crostini_simple_types.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace aura {
class Window;
}  // namespace aura

namespace base {
class FilePath;
}  // namespace base

namespace views {
class Widget;
}  // namespace views

class Profile;

namespace crostini {

// TODO(crbug.com/1092657): kCrostiniDeletedTerminalId can be removed after M86.
// We use an arbitrary well-formed extension id for the Terminal app, this
// is equal to GenerateId("Terminal").
extern const char kCrostiniDeletedTerminalId[];
// web_app::GenerateAppIdFromURL(
//     GURL("chrome-untrusted://terminal/html/terminal.html"))
extern const char kCrostiniTerminalSystemAppId[];

extern const char kCrostiniDefaultVmName[];
extern const char kCrostiniDefaultContainerName[];
extern const char kCrostiniDefaultUsername[];
// In order to be compatible with sync folder id must match standard.
// Generated using crx_file::id_util::GenerateId("LinuxAppsFolder")
extern const char kCrostiniFolderId[];
extern const char kCrostiniDefaultImageServerUrl[];
extern const char kCrostiniStretchImageAlias[];
extern const char kCrostiniBusterImageAlias[];
extern const char kCrostiniDlcName[];

extern const base::FilePath::CharType kHomeDirectory[];

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

struct LinuxPackageInfo;

// A unique identifier for our containers.
struct ContainerId {
  ContainerId(std::string vm_name, std::string container_name) noexcept;

  static ContainerId GetDefault();

  std::string vm_name;
  std::string container_name;
};

bool operator<(const ContainerId& lhs, const ContainerId& rhs) noexcept;
bool operator==(const ContainerId& lhs, const ContainerId& rhs) noexcept;
inline bool operator!=(const ContainerId& lhs,
                       const ContainerId& rhs) noexcept {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& ostream,
                         const ContainerId& container_id);

// Checks if user profile is able to a crostini app with a given app_id.
bool IsUninstallable(Profile* profile, const std::string& app_id);

// Returns whether the default Crostini VM is running for the user.
bool IsCrostiniRunning(Profile* profile);

// Whether the user is able to perform a container upgrade.
bool ShouldAllowContainerUpgrade(Profile* profile);

// Returns whether default Crostini container should be configured according to
// the configuration specified by CrostiniAnsiblePlaybook user policy.
bool ShouldConfigureDefaultContainer(Profile* profile);

// Returns whether a dialog from Crostini is blocking the immediate launch.
bool MaybeShowCrostiniDialogBeforeLaunch(Profile* profile,
                                         CrostiniResult result);

using LaunchArg = absl::variant<storage::FileSystemURL, std::string>;

// Launch a Crostini App with a given set of files, given as absolute paths in
// the container. For apps which can only be launched with a single file,
// launch multiple instances.
void LaunchCrostiniApp(Profile* profile,
                       const std::string& app_id,
                       int64_t display_id,
                       const std::vector<LaunchArg>& args = {},
                       CrostiniSuccessCallback callback = base::DoNothing());

// Retrieves cryptohome_id from profile.
std::string CryptohomeIdForProfile(Profile* profile);

// Retrieves username from profile.
std::string DefaultContainerUserNameForProfile(Profile* profile);

// Returns the mount directory within the container where paths from the Chrome
// OS host such as within Downloads are shared with the container.
base::FilePath ContainerChromeOSBaseDirectory();

// The Terminal opens Crosh but overrides the Browser's app_name so that we can
// identify it as the Crostini Terminal. In the future, we will also use these
// for Crostini apps marked Terminal=true in their .desktop file.
std::string AppNameFromCrostiniAppId(const std::string& id);

// Returns nullopt for a non-Crostini app name.
base::Optional<std::string> CrostiniAppIdFromAppName(
    const std::string& app_name);

// Returns a list of ports currently being forwarded in Crostini as a JSON
// object.
std::string GetActivePortListAsJSON(Profile* profile);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CrostiniUISurface { kSettings = 0, kAppList = 1, kCount };

// See chrome/browser/ui/views/crostini for implementation of the ShowXXX
// functions below.

// Shows the Crostini Uninstaller dialog.
void ShowCrostiniUninstallerView(Profile* profile,
                                 CrostiniUISurface ui_surface);
bool IsCrostiniRecoveryViewShowing();

// Shows the Crostini App installer dialog.
void ShowCrostiniAppInstallerView(Profile* profile,
                                  const LinuxPackageInfo& package_info);
// Shows the Crostini force-close dialog. If |app_name| is nonempty, the dialog
// will include the window's name as text. Returns a handle to that dialog, so
// that we can add observers to the dialog itself.
views::Widget* ShowCrostiniForceCloseDialog(
    const std::string& app_name,
    views::Widget* closable_widget,
    base::OnceClosure force_close_callback);
// Shows the Crostini Termina Upgrade dialog (for blocking crostini start until
// Termina version matches).
void ShowCrostiniUpdateComponentView(Profile* profile,
                                     CrostiniUISurface ui_surface);
// Shows the ui with the error message when installing a package fails.
void ShowCrostiniPackageInstallFailureView(const std::string& error_message);

// Shows the Crostini Container Upgrade dialog (for running upgrades in the
// container).
void ShowCrostiniUpdateFilesystemView(Profile* profile,
                                      CrostiniUISurface ui_surface);
// Show the Crostini Container Upgrade dialog after a delay
// (CloseCrostiniUpdateFilesystemView will cancel the next dialog show).
void PrepareShowCrostiniUpdateFilesystemView(Profile* profile,
                                             CrostiniUISurface ui_surface);
// Closes the current CrostiniUpdateFilesystemView or ensures that the view will
// not open until PrepareShowCrostiniUpdateFilesystemView is called again.
void CloseCrostiniUpdateFilesystemView();

// Show the Crostini Software Config dialog (for installing Ansible and
// applying an Ansible playbook in the container).
void ShowCrostiniAnsibleSoftwareConfigView(Profile* profile);

// Show the Crostini Recovery dialog when Crostini is still running after a
// Chrome crash. The user must either restart the VM, or launch a terminal.
void ShowCrostiniRecoveryView(Profile* profile,
                              CrostiniUISurface ui_surface,
                              const std::string& app_id,
                              int64_t display_id,
                              const std::vector<LaunchArg>& args,
                              CrostiniSuccessCallback callback);

// Add a newly created LXD container to the kCrostiniContainers pref
void AddNewLxdContainerToPrefs(Profile* profile,
                               const ContainerId& container_id);

// Remove a newly deleted LXD container from the kCrostiniContainers pref, and
// deregister its apps and mime types.
void RemoveLxdContainerFromPrefs(Profile* profile,
                                 const ContainerId& container_id);

// Returns a string to be displayed in a notification with the estimated time
// left for an operation to run which started and time |start| and is current
// at |percent| way through.
std::u16string GetTimeRemainingMessage(base::TimeTicks start, int percent);

// Returns a pref value stored for a specific container.
const base::Value* GetContainerPrefValue(Profile* profile,
                                         const ContainerId& container_id,
                                         const std::string& key);

// Sets a pref value for a specific container.
void UpdateContainerPref(Profile* profile,
                         const ContainerId& container_id,
                         const std::string& key,
                         base::Value value);

const ContainerId& DefaultContainerId();

bool IsCrostiniWindow(const aura::Window* window);

void RecordAppLaunchHistogram(CrostiniAppLaunchAppType app_type);
void RecordAppLaunchResultHistogram(CrostiniAppLaunchAppType type,
                                    crostini::CrostiniResult reason);

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UTIL_H_
