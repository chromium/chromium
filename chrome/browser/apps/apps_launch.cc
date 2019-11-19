// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/apps_launch.h"

#include "chrome/browser/apps/platform_apps/platform_app_launch.h"

namespace apps {

bool OpenApplicationWithReenablePrompt(
    Profile* profile,
    const std::string& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory) {
  return OpenExtensionApplicationWithReenablePrompt(
      profile, app_id, command_line, current_directory);
}

bool OpenAppShortcutWindow(Profile* profile, const GURL& url) {
  return OpenExtensionAppShortcutWindow(profile, url);
}

}  // namespace apps
