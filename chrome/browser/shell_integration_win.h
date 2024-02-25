// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHELL_INTEGRATION_WIN_H_
#define CHROME_BROWSER_SHELL_INTEGRATION_WIN_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

namespace base::win {
struct ShortcutProperties;
enum class ShortcutOperation;
}  // namespace base::win

namespace shell_integration::win {

struct ShortcutProperties;
enum class ShortcutOperation;

// Initiates the interaction with the system settings for the default browser.
// The function takes care of making sure |on_finished_callback| will get called
// exactly once when the interaction is finished.
void SetAsDefaultBrowserUsingSystemSettings(
    base::OnceClosure on_finished_callback);

// Initiates the interaction with the system settings for the default handler of
// |scheme|. The function takes care of making sure |on_finished_callback|
// will get called exactly once when the interaction is finished.
void SetAsDefaultClientForSchemeUsingSystemSettings(
    const std::string& scheme,
    base::OnceClosure on_finished_callback);

// App windows on Windows have an App User Model Id (AUMI) property. This is set
// in BrowserWindowPropertyManager::UpdateWindowProperties(), when a window is
// opened. Windows desktop shortcuts have an app model property, and this should
// match the open window's AUMI. Windows groups open windows with the same AUMI
// to a taskbar icon. The two methods below are used  to create AUMI's for
// shortcuts and open windows. There are two kinds of windows, Chromium windows,
// i.e., browser windows, and app windows, which include web apps,
// extensions, i.e., windows opened via --app-id or --app.

// GetAppUserModelIdForBrowser constructs an AUMI for a browser window and
// GetAppUserModelIdForApp constructs an AUMI for an app window. Each calls
// ShellUtil::BuildAppUserModelId() to construct the AUMI out of component
// strings.

// Generates an application user model ID (AppUserModelId) for a given
// app name and profile path. The returned app id format is
// "<install_static::GetBaseAppId()>.|app_name|[.<profile_id>]".
// |profile_id| is only appended when it's not the default profile.
std::wstring GetAppUserModelIdForApp(const std::wstring& app_name,
                                     const base::FilePath& profile_path);

// Generates an application user model ID (AppUserModelId) for Chromium by
// calling GetAppUserModelIdImpl() with ShellUtil::GetBrowserModelId() as
// the app_name. The returned app id format is
// "<install_static::GetBaseAppId()>[browser_suffix][.profile_id]"
// |profile_id| is only appended when it's not the default profile.
// browser_suffix is only appended to the BaseAppId if the installer
// has set the kRegisterChromeBrowserSuffix command line switch, e.g.,
// on user-level installs.
std::wstring GetAppUserModelIdForBrowser(const base::FilePath& profile_path);

// Returns the taskbar pin state of Chrome via the IsPinnedToTaskbarCallback.
// The first bool is true if the state could be calculated, and the second bool
// is true if Chrome is pinned to the taskbar.
// The ConnectionErrorCallback is called instead if something wrong happened
// with the connection to the remote process.
using ConnectionErrorCallback = base::OnceClosure;
using IsPinnedToTaskbarCallback = base::OnceCallback<void(bool, bool)>;
void GetIsPinnedToTaskbarState(IsPinnedToTaskbarCallback result_callback);

// Unpins `shortcuts` from the taskbar, and run `completion_callback` when done.
void UnpinShortcuts(const std::vector<base::FilePath>& shortcuts,
                    base::OnceClosure completion_callback);

using CreateOrUpdateShortcutsResultCallback = base::OnceCallback<void(bool)>;
// Based on `operation`, creates or updates each shortcut in `shortcuts` to
// have the properties in the corresponding element of `properties`. Runs
// `callback` when done with a true or false bool indicating success or failure.
void CreateOrUpdateShortcuts(
    const std::vector<base::FilePath>& shortcuts,
    const std::vector<base::win::ShortcutProperties>& properties,
    base::win::ShortcutOperation operation,
    CreateOrUpdateShortcutsResultCallback callback);

// Migrates existing chrome taskbar pins by tagging them with correct app id.
// see http://crbug.com/28104. Migrates taskbar pins via a task and runs
// |completion_callback| on the calling sequence when done.
void MigrateTaskbarPins(base::OnceClosure completion_callback);

// Callback for MigrateTaskbarPins(). Exposed for testing.
void MigrateTaskbarPinsCallback(const base::FilePath& pins_path,
                                const base::FilePath& implicit_apps_path);

// Migrates all shortcuts in |path| which point to |chrome_exe| such that they
// have the appropriate AppUserModelId. Also clears the legacy dual_mode
// property from shortcuts with the default chrome app id.
// Returns the number of shortcuts migrated.
// This method should not be called prior to Windows 7.
// This method is only public for the sake of tests and shouldn't be called
// externally otherwise.
int MigrateShortcutsInPathInternal(const base::FilePath& chrome_exe,
                                   const base::FilePath& path);

}  // namespace shell_integration::win

#endif  // CHROME_BROWSER_SHELL_INTEGRATION_WIN_H_
