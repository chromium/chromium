// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_WINDOW_PREDICTOR_ARC_PREDICTOR_APP_LAUNCH_HANDLER_H_
#define CHROME_BROWSER_ASH_ARC_WINDOW_PREDICTOR_ARC_PREDICTOR_APP_LAUNCH_HANDLER_H_

#include "ash/components/arc/mojom/app.mojom.h"
#include "chrome/browser/ash/app_restore/app_launch_handler.h"
#include "components/app_restore/restore_data.h"

namespace arc {

enum class GhostWindowType;

// A customized AppLaunchHandler to launch the pending apps when
// they are ready. For ARC apps, it will use the launch optimization
// policy to control the system resource usage.
class ArcPredictorAppLaunchHandler : public ash::AppLaunchHandler {
 public:
  ArcPredictorAppLaunchHandler();
  ArcPredictorAppLaunchHandler(const ArcPredictorAppLaunchHandler&) = delete;
  ArcPredictorAppLaunchHandler& operator=(const ArcPredictorAppLaunchHandler&) =
      delete;
  ~ArcPredictorAppLaunchHandler() override;

  void AddPendingApp(const std::string& app_id,
                     int event_flags,
                     GhostWindowType window_type,
                     arc::mojom::WindowInfoPtr window_info);

  void RecordRestoredAppLaunch(apps::AppTypeName app_type_name) override;

 protected:
  base::WeakPtr<ash::AppLaunchHandler> GetWeakPtrAppLaunchHandler() override;

 private:
  base::WeakPtrFactory<ArcPredictorAppLaunchHandler> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_WINDOW_PREDICTOR_ARC_PREDICTOR_APP_LAUNCH_HANDLER_H_
