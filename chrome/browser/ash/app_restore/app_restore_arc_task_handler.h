// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_APP_RESTORE_ARC_TASK_HANDLER_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_APP_RESTORE_ARC_TASK_HANDLER_H_

#include <utility>

#include "base/scoped_observation.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/common/buildflags.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace ash {
namespace full_restore {
class ArcWindowHandler;
}  // namespace full_restore

namespace app_restore {

class ArcAppLaunchHandler;

// The AppRestoreArcTaskHandler class observes ArcAppListPrefs, and calls
// app restore clients to update the ARC app launch info when a task is created
// or destroyed. AppRestoreArcTaskHandler is an independent KeyedService so that
// it could be created along with ARC system rather than with desks templates or
// full restore.
class AppRestoreArcTaskHandler : public KeyedService,
                                 public ArcAppListPrefs::Observer,
                                 public arc::ArcSessionManagerObserver {
 public:
  static AppRestoreArcTaskHandler* GetForProfile(Profile* profile);

  explicit AppRestoreArcTaskHandler(Profile* profile);
  AppRestoreArcTaskHandler(const AppRestoreArcTaskHandler&) = delete;
  AppRestoreArcTaskHandler& operator=(const AppRestoreArcTaskHandler&) = delete;
  ~AppRestoreArcTaskHandler() override;

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
  full_restore::ArcWindowHandler* window_handler() {
    return window_handler_.get();
  }
#endif

  ArcAppLaunchHandler* desks_templates_arc_app_launch_handler() {
    return desks_templates_arc_app_launch_handler_.get();
  }
  ArcAppLaunchHandler* full_restore_arc_app_launch_handler() {
    return full_restore_arc_app_launch_handler_.get();
  }

  // ArcAppListPrefs::Observer.
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

 private:
  // KeyedService:
  void Shutdown() override;

  base::ScopedObservation<ArcAppListPrefs, ArcAppListPrefs::Observer>
      arc_prefs_observer_{this};

#if BUILDFLAG(ENABLE_WAYLAND_SERVER)
  std::unique_ptr<full_restore::ArcWindowHandler> window_handler_;
#endif

  // The ArcAppLaunchHandlers, one for each feature that wants to launch ARC
  // apps.
  std::unique_ptr<ArcAppLaunchHandler> desks_templates_arc_app_launch_handler_;
  std::unique_ptr<ArcAppLaunchHandler> full_restore_arc_app_launch_handler_;
};

}  // namespace app_restore
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_APP_RESTORE_ARC_TASK_HANDLER_H_
