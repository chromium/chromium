// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHELL_INTEGRATION_WIN_H_
#define CHROME_BROWSER_SHELL_INTEGRATION_WIN_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"

namespace shell_integration {
namespace win {

// Initiates an OS shell flow which (if followed by the user) should set
// Chrome as the default browser. Returns false if the flow cannot be
// initialized, if it is not supported (introduced for Windows 8) or if the
// user cancels the operation. This is a blocking call and requires a FILE
// thread. If Chrome is already default browser, no interactive dialog will be
// shown and this method returns true.
bool SetAsDefaultBrowserUsingIntentPicker();

// Initiates the interaction with the system settings for the default browser.
// The function takes care of making sure |on_finished_callback| will get called
// exactly once when the interaction is finished.
void SetAsDefaultBrowserUsingSystemSettings(
    const base::Closure& on_finished_callback);

// Initiates an OS shell flow which (if followed by the user) should set
// Chrome as the default handler for |protocol|. Returns false if the flow
// cannot be initialized, if it is not supported (introduced for Windows 8)
// or if the user cancels the operation. This is a blocking call and requires
// a FILE thread. If Chrome is already default for |protocol|, no interactive
// dialog will be shown and this method returns true.
bool SetAsDefaultProtocolClientUsingIntentPicker(const std::string& protocol);

// Initiates the interaction with the system settings for the default handler of
// |protocol|. The function takes care of making sure |on_finished_callback|
// will get called exactly once when the interaction is finished.
void SetAsDefaultProtocolClientUsingSystemSettings(
    const std::string& protocol,
    const base::Closure& on_finished_callback);

// Generates an application user model ID (AppUserModelId) for a given app
// name and profile path. The returned app id is in the format of
// "|app_name|[.<profile_id>]". "profile_id" is appended when user override
// the default value.
// Note: If the app has an installation specific suffix (e.g. on user-level
// Chrome installs), |app_name| should already be suffixed, this method will
// then further suffix it with the profile id as described above.
base::string16 GetAppModelIdForProfile(const base::string16& app_name,
                                       const base::FilePath& profile_path);

// Generates an application user model ID (AppUserModelId) for Chromium by
// calling GetAppModelIdForProfile() with ShellUtil::GetAppId() as app_name.
base::string16 GetChromiumModelIdForProfile(const base::FilePath& profile_path);

// Returns the taskbar pin state of Chrome via the IsPinnedToTaskbarCallback.
// The first bool is true if the state could be calculated, and the second bool
// is true if Chrome is pinned to the taskbar.
// The ConnectionErrorCallback is called instead if something wrong happened
// with the connection to the remote process.
using ConnectionErrorCallback = base::Closure;
using IsPinnedToTaskbarCallback = base::Callback<void(bool, bool)>;
void GetIsPinnedToTaskbarState(
    const ConnectionErrorCallback& on_error_callback,
    const IsPinnedToTaskbarCallback& result_callback);

// Migrates existing chrome taskbar pins by tagging them with correct app id.
// see http://crbug.com/28104. Migrates taskbar pins via a task.
void MigrateTaskbarPins();

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

}  // namespace win
}  // namespace shell_integration

#endif  // CHROME_BROWSER_SHELL_INTEGRATION_WIN_H_
