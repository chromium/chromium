// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_WINDOW_PREDICTOR_WINDOW_PREDICTOR_H_
#define CHROME_BROWSER_ASH_ARC_WINDOW_PREDICTOR_WINDOW_PREDICTOR_H_

#include <memory>
#include <vector>

#include "ash/components/arc/mojom/app.mojom.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/window_predictor/arc_predictor_app_launch_handler.h"
#include "chrome/browser/ash/arc/window_predictor/window_predictor_utils.h"

namespace arc {

// Predict ARC app window initial launch window parameters and launch
// corresponding ARC ghost window.
// TODO(sstan): Finish window parameters related features.
class WindowPredictor {
 public:
  static WindowPredictor* GetInstance();

  WindowPredictor(const WindowPredictor&) = delete;
  WindowPredictor& operator=(const WindowPredictor&) = delete;

  // Create ARC app ghost window and add the corresponding to the launching
  // list, it will be launched after ARC ready.
  bool LaunchArcAppWithGhostWindow(
      Profile* profile,
      const std::string& app_id,
      const ArcAppListPrefs::AppInfo& app_info,
      const apps::IntentPtr& intent,
      int event_flags,
      GhostWindowType window_type,
      WindowPredictorUseCase use_case,
      const arc::mojom::WindowInfoPtr& window_info);

  // Get predict app window info by app id and existed window info.
  arc::mojom::WindowInfoPtr PredictAppWindowInfo(
      const ArcAppListPrefs::AppInfo& app_info,
      arc::mojom::WindowInfoPtr window_info);

  bool IsAppPendingLaunch(Profile* profile, const std::string& app_id);

 private:
  friend class base::NoDestructor<WindowPredictor>;
  WindowPredictor();
  ~WindowPredictor();

  int32_t launch_counter = 0;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_WINDOW_PREDICTOR_WINDOW_PREDICTOR_H_
