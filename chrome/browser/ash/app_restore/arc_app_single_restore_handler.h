// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_ARC_APP_SINGLE_RESTORE_HANDLER_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_ARC_APP_SINGLE_RESTORE_HANDLER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_handler.h"
#include "chrome/browser/ash/arc/window_predictor/window_predictor_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/intent.h"

namespace ash::app_restore {

// ArcAppSingleRestoreHandler class restore single ARC app with ghost window
// directly.
class ArcAppSingleRestoreHandler
    : public full_restore::ArcGhostWindowHandler::Observer {
 public:
  ArcAppSingleRestoreHandler();
  ~ArcAppSingleRestoreHandler() override;

  ArcAppSingleRestoreHandler(const ArcAppSingleRestoreHandler&) = delete;
  ArcAppSingleRestoreHandler& operator=(const ArcAppSingleRestoreHandler&) =
      delete;

  // Launch the ARC app and corresponding ghost window by the given launch
  // parameters. It's expected only called once.
  void LaunchGhostWindowWithApp(Profile* profile,
                                const std::string& app_id,
                                apps::IntentPtr intent,
                                int event_flags,
                                arc::GhostWindowType window_type,
                                arc::mojom::WindowInfoPtr window_info);

  bool IsAppPendingRestore(const std::string& app_id) const;

  void OnShelfReady();

  // full_restore::ArcGhostWindowHandler::Observer:
  void OnWindowCloseRequested(int window_id) override;
  void OnAppStatesUpdate(const std::string& app_id,
                         bool ready,
                         bool need_fixup) override;
  void OnGhostWindowHandlerDestroy() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ArcAppSingleRestoreHandlerTest,
                           NotLaunchIfShelfNotReady);
  FRIEND_TEST_ALL_PREFIXES(ArcAppSingleRestoreHandlerTest,
                           PendingLaunchIfShelfHasReady);
  FRIEND_TEST_ALL_PREFIXES(ArcAppSingleRestoreHandlerTest,
                           NullBoundsNotCauseCrash);

  // Called when ARC app has ready. It's expected only called once.
  void SendAppLaunchRequestToARC();

  // For test usage.
  raw_ptr<full_restore::ArcGhostWindowHandler> ghost_window_handler_ = nullptr;

  raw_ptr<Profile> profile_;
  std::optional<std::string> app_id_;
  bool is_cancelled_ = false;

  apps::IntentPtr intent_;
  int32_t event_flags_;
  int32_t window_id_;
  apps::WindowInfoPtr window_info_;

  bool is_shelf_ready_ = false;
  base::OnceClosure not_ready_callback_;

  base::ScopedObservation<full_restore::ArcGhostWindowHandler,
                          full_restore::ArcGhostWindowHandler::Observer>
      observation_{this};

  base::WeakPtrFactory<ArcAppSingleRestoreHandler> weak_ptr_factory_{this};
};

}  // namespace ash::app_restore

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_ARC_APP_SINGLE_RESTORE_HANDLER_H_
