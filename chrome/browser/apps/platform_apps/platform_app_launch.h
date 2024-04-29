// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_PLATFORM_APP_LAUNCH_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_PLATFORM_APP_LAUNCH_H_

#include <string>

#include "build/build_config.h"

class GURL;
class Profile;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace apps {

// Tries to open an application window. If the app is specified to start in a
// tab returns false to specify default processing. Returns true if |app_id| was
// successfully opened in a window, and false otherwise.
bool OpenExtensionApplicationWindow(Profile* profile,
                                    const std::string& app_id,
                                    const base::CommandLine& command_line,
                                    const base::FilePath& current_directory);

// If the user set a pref indicating that the app should open in a tab, open an
// application tab.
// Returns web contents if |app_id| was successfully opened in a tab, and
// nullptr otherwise.
content::WebContents* OpenExtensionApplicationTab(Profile* profile,
                                                  const std::string& app_id);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Opens the deprecated Chrome Apps flow if |app_id| refers to a Chrome App and
// Chrome Apps are deprecated on the |profile|. Returns true if that was the
// case, or false otherwise.
bool OpenDeprecatedApplicationPrompt(Profile* profile,
                                     const std::string& app_id);
#endif

// Tries to open |app_id|, and prompts the user if the app is disabled. Returns
// true if the app was successfully opened and false otherwise.
// Handles the case If |app_id| is a disabled or terminated platform app.
bool OpenExtensionApplicationWithReenablePrompt(
    Profile* profile,
    const std::string& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory);

// Tries to open an application window by app's |url|.
// Returns web contents if |url| was successfully opened in a window, and
// nullptr otherwise.
content::WebContents* OpenExtensionAppShortcutWindow(Profile* profile,
                                                     const GURL& url);

// Records the restored app launch for UMA.
void RecordExtensionAppLaunchOnTabRestored(Profile* profile, const GURL& url);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_PLATFORM_APP_LAUNCH_H_
