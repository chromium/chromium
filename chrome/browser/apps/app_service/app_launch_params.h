// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_LAUNCH_PARAMS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_LAUNCH_PARAMS_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace apps {

struct AppLaunchParams {
  AppLaunchParams(const std::string& app_id,
                  LaunchContainer container,
                  WindowOpenDisposition disposition,
                  apps::LaunchSource launch_source,
                  int64_t display_id = display::kInvalidDisplayId);

  AppLaunchParams(const std::string& app_id,
                  LaunchContainer container,
                  WindowOpenDisposition disposition,
                  apps::LaunchSource launch_source,
                  int64_t display_id,
                  const std::vector<base::FilePath>& files,
                  const IntentPtr& intentPtr);

  AppLaunchParams(const std::string& app_id,
                  LaunchContainer container,
                  WindowOpenDisposition disposition,
                  const GURL& override_url,
                  apps::LaunchSource launch_source,
                  int64_t display_id,
                  const std::vector<base::FilePath>& files,
                  const IntentPtr& intentPtr);

  AppLaunchParams(const AppLaunchParams&) = delete;
  AppLaunchParams& operator=(const AppLaunchParams&) = delete;
  AppLaunchParams(AppLaunchParams&&);
  AppLaunchParams& operator=(AppLaunchParams&&);

  ~AppLaunchParams();

  // The app to launch.
  std::string app_id;

  // An id that can be passed to an app when launched in order to support
  // multiple shelf items per app.
  std::string launch_id;

  // The container type to launch the application in.
  LaunchContainer container;

  // If container is TAB, this field controls how the tab is opened.
  // If container is WINDOW, NEW_WINDOW will force a new window to be opened.
  WindowOpenDisposition disposition;

  // If non-empty, use override_url in place of the application's launch url.
  GURL override_url;

  // If non-empty, use override_boudns in place of the application's default
  // position and dimensions.
  gfx::Rect override_bounds;

  // If non-empty, use override_app_name in place of generating one normally.
  std::string override_app_name;

  // The id from the restore data to restore the browser window.
  int32_t restore_id = 0;

  // If non-empty, information from the command line may be passed on to the
  // application.
  base::CommandLine command_line;

  // If non-empty, the current directory from which any relative paths on the
  // command line should be expanded from.
  base::FilePath current_directory;

  // Record where the app is launched from for tracking purpose.
  // Different app may have their own enumeration of sources.
  apps::LaunchSource launch_source;

  // The id of the display from which the app is launched.
  // display::kInvalidDisplayId means that the default display for new windows
  // will be used. See `display::Screen` for details.
  int64_t display_id;

  // The files the application was launched with. Empty if the application was
  // not launched with files.
  std::vector<base::FilePath> launch_files;

  // The intent the application was launched with. Empty if the application was
  // not launched with intent.
  IntentPtr intent;

  // When PWA is launched as a URL handler, the URL that we should launch the
  // PWA to. Null when it's not a URL handler launch.
  std::optional<GURL> url_handler_launch_url;

  // When a PWA is launched as a protocol handler, the protocol URL that we
  // should translate and then launch the PWA to. Null when it's not a protocol
  // handler launch.
  std::optional<GURL> protocol_handler_launch_url;

  // Whether or not to have the resulting Browser be omitted from session
  // restore.
  bool omit_from_session_restore = false;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_LAUNCH_PARAMS_H_
