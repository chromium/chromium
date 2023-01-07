// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_LAUNCHER_H_
#define APPS_LAUNCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "extensions/common/api/app_runtime.h"
#include "extensions/common/constants.h"

class GURL;

namespace base {
class CommandLine;
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
namespace api {
namespace app_runtime {
struct ActionData;
}
}
}

namespace apps {

// Launches the platform app |app|. Creates appropriate launch data for the
// |command_line| fields present. |app| and |context| must not be NULL. An empty
// |command_line| means there is no launch data. If non-empty,
// |current_directory| is used to expand any relative paths on the command line.
// |source| is one of the enumerated values which trace how the app is launched.
void LaunchPlatformAppWithCommandLine(content::BrowserContext* context,
                                      const extensions::Extension* app,
                                      const base::CommandLine& command_line,
                                      const base::FilePath& current_directory,
                                      extensions::AppLaunchSource source);

// As above but includes |launch_id|, an id that can be passed to
// an app when launched in order to support multiple shelf items per app.
void LaunchPlatformAppWithCommandLineAndLaunchId(
    content::BrowserContext* context,
    const extensions::Extension* app,
    const std::string& launch_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory,
    extensions::AppLaunchSource source);

// Launches the platform app |app| by issuing an onLaunched event with the
// contents of |file_path| available through the launch data.
void LaunchPlatformAppWithPath(content::BrowserContext* context,
                               const extensions::Extension* app,
                               const base::FilePath& file_path);

// Launches the platform app |app| by issuing an onLaunched event with the
// contents of |file_paths| available through the launch data.
void LaunchPlatformAppWithFilePaths(
    content::BrowserContext* context,
    const extensions::Extension* app,
    const std::vector<base::FilePath>& file_paths);

// Launches the platform app |app| with the specific |action_data|.
void LaunchPlatformAppWithAction(
    content::BrowserContext* context,
    const extensions::Extension* app,
    extensions::api::app_runtime::ActionData action_data);

// Launches the platform app |app|. |source| tells us how the app is launched.
void LaunchPlatformApp(content::BrowserContext* context,
                       const extensions::Extension* app,
                       extensions::AppLaunchSource source);

// Launches the platform app |app| with |handler_id| and the contents of
// |file_paths| available through the launch data. |handler_id| corresponds to
// the id of the file_handlers item in the manifest that resulted in a match
// that triggered this launch.
void LaunchPlatformAppWithFileHandler(
    content::BrowserContext* context,
    const extensions::Extension* app,
    const std::string& handler_id,
    const std::vector<base::FilePath>& file_paths);

// Launches the platform app |app| with |handler_id|, |url| and |referrer_url|
// available through the launch data. |handler_id| corresponds to the id of the
// file_handlers item in the manifest that resulted in a match that triggered
// this launch.
void LaunchPlatformAppWithUrl(content::BrowserContext* context,
                              const extensions::Extension* app,
                              const std::string& handler_id,
                              const GURL& url,
                              const GURL& referrer_url);

void RestartPlatformApp(content::BrowserContext* context,
                        const extensions::Extension* app);

}  // namespace apps

#endif  // APPS_LAUNCHER_H_
