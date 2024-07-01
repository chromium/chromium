// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_ARC_GHOST_WINDOW_DELEGATE_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_ARC_GHOST_WINDOW_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_handler.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "ui/gfx/geometry/rect.h"

namespace ash::full_restore {

// The ArcGhostWindowDelegate class is a self controlled shell surface delegate
// to handle ARC ghost windows, e.g. when window bounds or window state is
// changed, notify ARC.
class ArcGhostWindowDelegate
    : public exo::ClientControlledShellSurface::Delegate,
      public ArcGhostWindowHandler::Observer {
 public:
  ArcGhostWindowDelegate(exo::ClientControlledShellSurface* shell_surface,
                         int window_id,
                         const std::string& app_id,
                         int64_t display_id,
                         const gfx::Rect& bounds,
                         chromeos::WindowStateType window_state);
  ~ArcGhostWindowDelegate() override;

  // exo::ClientControlledShellSurface::Delegate
  void OnGeometryChanged(const gfx::Rect& geometry) override;

  void OnStateChanged(chromeos::WindowStateType old_state_type,
                      chromeos::WindowStateType new_state) override;

  void OnBoundsChanged(chromeos::WindowStateType current_state,
                       chromeos::WindowStateType requested_state,
                       int64_t display_id,
                       const gfx::Rect& bounds_in_screen,
                       bool is_resize,
                       int bounds_change,
                       bool is_adjusted_bounds) override;

  void OnDragStarted(int component) override;

  void OnDragFinished(int x, int y, bool canceled) override;

  void OnZoomLevelChanged(exo::ZoomChange zoom_change) override;

  // ArcGhostWindowHandler::Observer
  void OnAppInstanceConnected() override;

  void OnWindowCloseRequested(int window_id) override;

  void OnAppStatesUpdate(const std::string& app_id,
                         bool ready,
                         bool need_fixup) override;

 private:
  bool SetDisplayId(int64_t display_id);
  void UpdateWindowInfoToArc();

  int window_id_;
  std::string app_id_;
  gfx::Rect bounds_;
  bool pending_close_;
  int64_t display_id_;
  double scale_factor_;
  chromeos::WindowStateType window_state_;
  raw_ptr<exo::ClientControlledShellSurface> shell_surface_;

  base::ScopedObservation<ArcGhostWindowHandler,
                          ArcGhostWindowHandler::Observer>
      observation_{this};
};

}  // namespace ash::full_restore

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_ARC_GHOST_WINDOW_DELEGATE_H_
