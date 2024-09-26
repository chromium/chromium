// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_APP_RESTORE_ARC_TASK_HANDLER_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_APP_RESTORE_ARC_TASK_HANDLER_H_

#include <utility>

#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/common/buildflags.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace ash {

namespace full_restore {
class ArcGhostWindowHandler;
class FullRestoreAppLaunchHandlerArcAppBrowserTest;
}  // namespace full_restore

namespace app_restore {

class ArcAppSingleRestoreHandler;
class ArcAppQueueRestoreHandler;

namespace {

enum class LauncherType {
  kFullRestore,
  kWindowPredictor,
  kDeskTemplate,
};

using LauncherTag = std::pair<LauncherType, int32_t>;

}  // namespace

// The AppRestoreArcTaskHandler class observes ArcAppListPrefs, and calls
// app restore clients to update the ARC app launch info when a task is created
// or destroyed. AppRestoreArcTaskHandler is an independent KeyedService so that
// it could be created along with ARC system rather than with desks templates or
// full restore.
class AppRestoreArcTaskHandler : public KeyedService,
                                 public ArcAppListPrefs::Observer,
                                 public arc::ArcSessionManagerObserver {
 public:
  explicit AppRestoreArcTaskHandler(Profile* profile);
  AppRestoreArcTaskHandler(const AppRestoreArcTaskHandler&) = delete;
  AppRestoreArcTaskHandler& operator=(const AppRestoreArcTaskHandler&) = delete;
  ~AppRestoreArcTaskHandler() override;

  full_restore::ArcGhostWindowHandler* window_handler() {
    return window_handler_.get();
  }

  // Check if the AppId existed in any arc app launch handler restore queue.
  // When different launch handler which corresponding to different restore
  // purpose trying to restore the same ARC app, it will be confusing ARC that
  // which window info should be applied.
  bool IsAppPendingRestore(const std::string& arc_app_id) const;

  // Get or create full restore arc app queue restore handler.
  ArcAppQueueRestoreHandler* GetFullRestoreArcAppQueueRestoreHandler();

  // Get or create window predictor arc app restore handler by
  // `launch_id`.
  ArcAppSingleRestoreHandler* GetWindowPredictorArcAppRestoreHandler(
      int32_t launch_id);

  // Get or create desk template arc app queue restore handler by `launch_id`.
  ArcAppQueueRestoreHandler* GetDeskTemplateArcAppQueueRestoreHandler(
      int32_t launch_id);
  void ClearDeskTemplateArcAppQueueRestoreHandler(int32_t launch_id);

  // ArcAppListPrefs::Observer.
  void OnAppStatesChanged(const std::string& id,
                          const ArcAppListPrefs::AppInfo& app_info) override;
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const std::string& intent,
                     int32_t session_id) override;
  void OnTaskDestroyed(int task_id) override;
  void OnTaskDescriptionChanged(int32_t task_id,
                                const std::string& label,
                                const arc::mojom::RawIconPngData& icon,
                                uint32_t primary_color,
                                uint32_t status_bar_color) override;
  void OnAppConnectionReady() override;
  void OnAppConnectionClosed() override;
  void OnArcAppListPrefsDestroyed() override;

  // arc::ArcSessionManagerObserver:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

  // Invoked when ChromeShelfController is created.
  void OnShelfReady();

  // KeyedService:
  void Shutdown() override;

 private:
  friend class full_restore::FullRestoreAppLaunchHandlerArcAppBrowserTest;

  ArcAppQueueRestoreHandler* CreateOrGetArcAppQueueRestoreHandler(
      LauncherTag launcher_tag,
      bool call_init_callback);

  ArcAppSingleRestoreHandler* CreateOrGetArcAppSingleRestoreHandler(
      LauncherTag launcher_tag);

  base::ScopedObservation<ArcAppListPrefs, ArcAppListPrefs::Observer>
      arc_prefs_observer_{this};

  std::unique_ptr<full_restore::ArcGhostWindowHandler> window_handler_;

  // Maps LauncherTag to ArcAppQueueRestoreHandlers. Currently FullRestore
  // and DeskTemplate use it.
  std::map<LauncherTag, std::unique_ptr<ArcAppQueueRestoreHandler>>
      arc_app_queue_restore_handlers_;

  // Maps LauncherTag to ArcAppSingleRestoreHandlers. Currently WindowPredictor
  // use it.
  std::map<LauncherTag, std::unique_ptr<ArcAppSingleRestoreHandler>>
      arc_app_single_restore_handlers_;

  // These cache the readiness status of the subsystems needed to launch ARC
  // apps. They are used when new handlers are dynamically created so that the
  // handlers can learn the status of these systems.
  bool arc_play_store_enabled_ = false;
  bool shelf_ready_ = false;
  bool app_connection_ready_ = false;
};

}  // namespace app_restore
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_APP_RESTORE_ARC_TASK_HANDLER_H_
