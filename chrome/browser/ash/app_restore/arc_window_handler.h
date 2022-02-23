// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_ARC_WINDOW_HANDLER_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_ARC_WINDOW_HANDLER_H_

#include "ash/components/arc/mojom/app.mojom-forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/wm_helper.h"

namespace app_restore {
struct AppRestoreData;
}  // namespace app_restore

namespace ash {
namespace full_restore {

// The ArcWindowHandler class provides control for ARC ghost window.
class ArcWindowHandler : public exo::WMHelper::LifetimeManager::Observer {
  // Map from window_session_id to exo::ClientControlledShellSurface.
  using ShellSurfaceMap =
      std::map<int, std::unique_ptr<exo::ClientControlledShellSurface>>;
  // Map from window_session_id to arc::mojom::WindowInfoPtr.
  using WindowInfoMap = std::map<int, arc::mojom::WindowInfoPtr>;

  // This class populates the exo::ShellSurfaceBase to PropertyHandler by
  // the corresponding window session id.
  class WindowSessionResolver : public exo::WMHelper::AppPropertyResolver {
   public:
    explicit WindowSessionResolver(ArcWindowHandler* handler);
    WindowSessionResolver(const WindowSessionResolver&) = delete;
    WindowSessionResolver& operator=(const WindowSessionResolver&) = delete;
    ~WindowSessionResolver() override = default;

    // exo::WMHelper::AppPropertyResolver:
    void PopulateProperties(
        const Params& params,
        ui::PropertyHandler& out_properties_container) override;

   private:
    ArcWindowHandler* handler_;
  };

 public:
  // This class is used to notify observers that AppInstance is connected.
  class Observer : public base::CheckedObserver {
   public:
    // Observer for app instance connection ready.
    virtual void OnAppInstanceConnected() {}

    // Observer for ghost window close event.
    virtual void OnWindowCloseRequested(int window_id) {}

   protected:
    ~Observer() override = default;
  };

  ArcWindowHandler();
  ArcWindowHandler(const ArcWindowHandler&) = delete;
  ArcWindowHandler& operator=(const ArcWindowHandler&) = delete;
  ~ArcWindowHandler() override;

  // Returns true if the ghost window is created and launched. Otherwise,
  // returns false.
  bool LaunchArcGhostWindow(const std::string& app_id,
                            int32_t session_id,
                            ::app_restore::AppRestoreData* restore_data);

  void CloseWindow(int session_id);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer);

  void OnAppInstanceConnected();

  void OnWindowInfoUpdated(int window_id,
                           int state,
                           int64_t display_id,
                           gfx::Rect bounds);

  int ghost_window_pop_count() { return ghost_window_pop_count_; }

  // exo::WMHelper::LifetimeManager::Observer:
  void OnDestroyed() override;

 private:
  bool is_app_instance_connected_ = false;

  int ghost_window_pop_count_ = 0;

  // Map window session id to ClientControlledShellSurface.
  ShellSurfaceMap session_id_to_shell_surface_;

  // Map window session id to pending window info. Before ARC app instance
  // connection establish, all of window info update will be saved here as
  // pending update info.
  WindowInfoMap session_id_to_pending_window_info_;

  base::ObserverList<Observer> observer_list_;

  base::WeakPtrFactory<ArcWindowHandler> weak_ptr_factory_{this};
};

}  // namespace full_restore
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_ARC_WINDOW_HANDLER_H_
