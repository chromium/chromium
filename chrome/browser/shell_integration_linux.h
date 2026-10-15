// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHELL_INTEGRATION_LINUX_H_
#define CHROME_BROWSER_SHELL_INTEGRATION_LINUX_H_

#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/files/safe_base_name.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#error shell_integration_linux is for desktop linux only.
#endif

namespace base {
class CommandLine;
class Environment;
}

namespace web_app {
struct DesktopActionInfo;
}

namespace shell_integration_linux {

// Gets the name for use as the res_name of the window's WM_CLASS property.
std::string GetProgramClassName();

// Gets the name for use as the res_class of the window's WM_CLASS property.
std::string GetProgramClassClass();

// Returns name of the browser icon (without a path or file extension).
std::string GetIconName();

// Returns the contents of an existing .desktop file installed in the system.
// Searches the "applications" subdirectory of each XDG data directory for a
// file named |desktop_filename|. If the file is found, populates |output| with
// its contents and returns true. Else, returns false.
bool GetExistingShortcutContents(base::Environment* env,
                                 const base::FilePath& desktop_filename,
                                 std::string* output);

// Returns the base name for .desktop file based on |name|, sanitized for
// security with no whitespace, and ensures it will be a unique file in the
// directory at base::DIR_USER_DESKTOP. This call is not thread-safe - multiple
// callers from different threads with the same argument may get the same base
// name.
// Returns a std::nullopt if base::DIR_USER_DESKTOP is not defined or a unique
// name could not be found.
std::optional<base::SafeBaseName> GetUniqueWebShortcutFilename(
    const std::string& name);

// Returns a list of filenames for all existing .desktop files corresponding to
// on |profile_path| in a given |directory|.
std::vector<base::FilePath> GetExistingProfileShortcutFilenames(
    const base::FilePath& profile_path,
    const base::FilePath& directory);

// Returns contents for .desktop file based on |url| and |title|. If
// |no_display| is true, the shortcut will not be visible to the user in menus.
std::string GetDesktopFileContents(
    const base::FilePath& chrome_exe_path,
    const std::string& app_name,
    const GURL& url,
    const std::string& extension_id,
    const std::u16string& title,
    const std::string& icon_name,
    const base::FilePath& profile_path,
    const std::string& categories,
    const std::string& mime_type,
    bool no_display,
    const std::string& run_on_os_login_mode,
    std::set<web_app::DesktopActionInfo> action_info);

// Returns contents for .desktop file that executes command_line. This is a more
// general form of GetDesktopFileContents. If |no_display| is true, the shortcut
// will not be visible to the user in menus.
std::string GetDesktopFileContentsForCommand(
    const base::CommandLine& command_line,
    const std::string& app_name,
    const GURL& url,
    const std::u16string& title,
    const std::string& icon_name,
    const std::string& categories,
    const std::string& mime_type,
    bool no_display,
    std::string_view extra_mime_types,
    std::set<web_app::DesktopActionInfo> action_info);

// Returns contents for the .desktop file that launches the browser with the
// profile at `profile_path`. `desktop_id` is used as the StartupWMClass, so it
// must be the profile's per-profile desktop entry ID (see
// GetPerProfileDesktopId()).
std::string GetDesktopFileContentsForProfile(const base::FilePath& chrome_exe,
                                             const base::FilePath& profile_path,
                                             const std::string& desktop_id,
                                             const std::u16string& title,
                                             const base::FilePath& icon_path);

// Returns contents for a .desktop file that launches chrome at the given url
// using the given profile, referencing the given icon. The file has the given
// title & icon.
// This will CHECK-fail if the url is not valid, the profile path is empty, or
// the icon path is empty.
std::string GetDesktopFileContentsForUrlShortcut(
    const std::string& title,
    const GURL& url,
    const base::FilePath& icon_path,
    const base::FilePath& profile_path);

// Returns contents for .directory file named |title| with icon |icon_name|. If
// |icon_name| is empty, will use the Chrome icon.
std::string GetDirectoryFileContents(const std::u16string& title,
                                     const std::string& icon_name);

// Returns the filename for a .xml file, corresponding to a given |app_id|,
// which is passed to `xdg-mime` to register one or more custom MIME types in
// Linux.
base::FilePath GetMimeTypesRegistrationFilename(
    const base::FilePath& profile_path,
    const webapps::AppId& app_id);

// Returns the contents of a .xml file as specified by |file_handlers|, which is
// passed to `xdg-mime` to register one or more custom MIME types in Linux.
std::string GetMimeTypesRegistrationFileContents(
    const apps::FileHandlers& file_handlers);

// Returns the mime type handler for direct launch.
std::string GetDirectLaunchMimeTypeHandler();

// Windows that correspond to web apps need to have a deterministic (and
// different) WMClass than normal chrome windows so the window manager groups
// them as a separate application.
std::string GetWMClassFromAppName(std::string app_name);

// Wayland version of GetWMClassFromAppName explained above.
// The XDG application ID must match the name of the desktop entry file, where
// the latter looks like 'chrome-<web app id>-<profile name>.desktop'.
std::string GetXdgAppIdForWebApp(std::string app_name,
                                 const base::FilePath& profile_path);

// Returns the desktop entry ID (the .desktop file basename without the
// extension) that identifies browser windows of the profile at
// `profile_path`, e.g. "google-chrome-Profile_1". Returns an empty string for
// the default profile and for profiles outside the default user data
// directory; those always use the browser's main desktop entry.
std::string GetPerProfileDesktopId(const base::FilePath& profile_path);

// Returns the per-profile desktop entry ID for `profile_path` if per-profile
// window identities are enabled and a desktop entry with that ID is known to be
// installed. Otherwise returns an empty string, in which case the profile's
// windows use the browser's main identity. Browser windows with a per-profile
// ID use it as both the X11 WM_CLASS res_name and the Wayland app_id; the X11
// res_class is left unchanged so desktop environments that don't know about
// the per-profile entry still fall back to the main browser entry.
// Must be called on the UI thread.
std::string GetInstalledPerProfileDesktopId(const base::FilePath& profile_path);

// Records whether a desktop entry with the per-profile desktop entry ID for
// `profile_path` is installed. Recording the same state again is a no-op, since
// detection and desktop entry updates may report a state that is already
// known. Must be called on the UI thread.
void SetPerProfileDesktopEntryInstalled(const base::FilePath& profile_path,
                                        bool installed);

// Registers `callback` to be run with the profile path whenever
// SetPerProfileDesktopEntryInstalled() changes whether a profile has an
// installed per-profile desktop entry, so that open browser windows can update
// their identity. Must be called on the UI thread.
base::CallbackListSubscription RegisterPerProfileDesktopEntryChangedCallback(
    base::RepeatingCallback<void(const base::FilePath&)> callback);

// Checks the XDG data directories for installed per-profile desktop entries
// for each of `profile_paths`, and records the results with
// SetPerProfileDesktopEntryInstalled(). Does nothing if per-profile window
// identities are disabled. Blocking; must be called on the UI thread during
// startup, before any browser windows are created.
void DetectInstalledPerProfileDesktopEntries(
    const std::vector<base::FilePath>& profile_paths);

// Helper to launch xdg scripts. We don't want them to ask any questions on the
// terminal etc. The function returns true if the utility launches and exits
// cleanly, in which case |exit_code| returns the utility's exit code.
// thread_restrictions.h assumes it to be in shell_integration namespace.
bool LaunchXdgUtility(const std::vector<std::string>& argv, int* exit_code);

namespace internal {

// Exposed for testing.  Clients should use the corresponding functions in
// shell_integration_linux instead.
std::string GetProgramClassName(const base::CommandLine& command_line,
                                const std::string& desktop_file_name);
std::string GetProgramClassClass(const base::CommandLine& command_line,
                                 const std::string& desktop_file_name);
std::string GetPerProfileDesktopId(const base::FilePath& profile_path,
                                   const base::FilePath& default_user_data_dir,
                                   const std::string& desktop_file_name);

// Get the value of NoDisplay from the [Desktop Entry] section of a .desktop
// file, given in |shortcut_contents|. If the key is not found, returns false.
bool GetNoDisplayFromDesktopFile(const std::string& shortcut_contents);

// Gets the path to the Chrome executable or wrapper script.
// Returns an empty path if the executable path could not be found, which should
// never happen.
base::FilePath GetChromeExePath();

// Get the value of |key| from the [Desktop Entry] section of a .desktop file,
// given in |shortcut_contents|. If the key is not found, returns an empty
// string.
std::string GetDesktopEntryStringValueFromFromDesktopFileForTest(
    const std::string& key,
    const std::string& shortcut_contents);

}  // namespace internal

}  // namespace shell_integration_linux

#endif  // CHROME_BROWSER_SHELL_INTEGRATION_LINUX_H_
