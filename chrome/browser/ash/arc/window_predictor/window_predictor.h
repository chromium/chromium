// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_WINDOW_PREDICTOR_WINDOW_PREDICTOR_H_
#define CHROME_BROWSER_ASH_ARC_WINDOW_PREDICTOR_WINDOW_PREDICTOR_H_

#include <memory>

#include "ash/components/arc/mojom/app.mojom.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/arc/window_predictor/arc_predictor_app_launch_handler.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"

class Profile;

namespace arc {

// Predict ARC app window initial launch window parameters and launch
// corresponding ARC ghost window.
// TODO(sstan): Finish window parameters related features.
class WindowPredictor {
 public:
  static WindowPredictor* GetInstance();

  WindowPredictor(const WindowPredictor&) = delete;
  WindowPredictor& operator=(const WindowPredictor&) = delete;

  // Create App Launch Handler.
  void MaybeCreateAppLaunchHandler(Profile* profile);

  // Get predict app window info by app id and existed window info.
  arc::mojom::WindowInfoPtr PredictAppWindowInfo(
      const ArcAppListPrefs::AppInfo& app_info,
      arc::mojom::WindowInfoPtr window_info);

  ArcPredictorAppLaunchHandler* app_launch_handler() {
    return app_launch_handler_.get();
  }

 private:
  friend class base::NoDestructor<WindowPredictor>;
  WindowPredictor();
  ~WindowPredictor();

  std::unique_ptr<ArcPredictorAppLaunchHandler> app_launch_handler_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_WINDOW_PREDICTOR_WINDOW_PREDICTOR_H_
