// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APPS_LAUNCH_H_
#define CHROME_BROWSER_APPS_APPS_LAUNCH_H_

#include <string>

class GURL;
class Profile;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace apps {

// TODO(crbug.com/966288): Move these methods into LaunchService.

// Tries to open |app_id|, and prompts the user if the app is disabled. Returns
// true if the app was successfully opened and false otherwise.
bool OpenApplicationWithReenablePrompt(Profile* profile,
                                       const std::string& app_id,
                                       const base::CommandLine& command_line,
                                       const base::FilePath& current_directory);

// Returns true if |url| was successfully opened in a window, and false
// otherwise.
bool OpenAppShortcutWindow(Profile* profile, const GURL& url);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APPS_LAUNCH_H_
