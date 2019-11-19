// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_LAUNCH_PARAMS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_LAUNCH_PARAMS_H_

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace apps {

struct AppLaunchParams {
  AppLaunchParams(const std::string& app_id,
                  apps::mojom::LaunchContainer container,
                  WindowOpenDisposition disposition,
                  apps::mojom::AppLaunchSource source,
                  int64_t display_id = display::kInvalidDisplayId);

  AppLaunchParams(const AppLaunchParams& other);

  ~AppLaunchParams();

  // The app to launch.
  std::string app_id;

  // An id that can be passed to an app when launched in order to support
  // multiple shelf items per app.
  std::string launch_id;

  // The container type to launch the application in.
  apps::mojom::LaunchContainer container;

  // If container is TAB, this field controls how the tab is opened.
  WindowOpenDisposition disposition;

  // If non-empty, use override_url in place of the application's launch url.
  GURL override_url;

  // If non-empty, use override_boudns in place of the application's default
  // position and dimensions.
  gfx::Rect override_bounds;

  // If non-empty, use override_app_name in place of generating one normally.
  std::string override_app_name;

  // If non-empty, information from the command line may be passed on to the
  // application.
  base::CommandLine command_line;

  // If non-empty, the current directory from which any relative paths on the
  // command line should be expanded from.
  base::FilePath current_directory;

  // Record where the app is launched from for tracking purpose.
  // Different app may have their own enumeration of sources.
  apps::mojom::AppLaunchSource source;

  // The id of the display from which the app is launched.
  // display::kInvalidDisplayId means that the display does not exist or is not
  // set.
  int64_t display_id;

  // The files the application was launched with. Empty if the application was
  // not launched with files.
  std::vector<base::FilePath> launch_files;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_LAUNCH_PARAMS_H_
