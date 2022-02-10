// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_WINDOW_PREDICTOR_WINDOW_PREDICTOR_UTILS_H_
#define CHROME_BROWSER_ASH_ARC_WINDOW_PREDICTOR_WINDOW_PREDICTOR_UTILS_H_

#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/components/arc/mojom/app.mojom.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"

class Profile;

namespace app_restore {
struct AppRestoreData;
}

namespace arc {

// Create ARC app ghost window and add the corresponding to the launching list,
// it will be launched after ARC ready.
bool LaunchArcAppWithGhostWindow(Profile* profile,
                                 const std::string& app_id,
                                 const ArcAppListPrefs::AppInfo& app_info,
                                 int event_flags,
                                 arc::UserInteractionType user_interaction_type,
                                 const arc::mojom::WindowInfoPtr& window_info);

// Is the the window info provide enough data to create corresponding ARC ghost
// window.
bool CanLaunchGhostWindowByRestoreData(
    const app_restore::AppRestoreData& restore_data);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_WINDOW_PREDICTOR_WINDOW_PREDICTOR_UTILS_H_
