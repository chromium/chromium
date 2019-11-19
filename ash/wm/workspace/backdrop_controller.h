// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_BACKDROP_CONTROLLER_H_
#define ASH_WM_WORKSPACE_BACKDROP_CONTROLLER_H_

#include <memory>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/public/cpp/wallpaper_controller_observer.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_observer.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ui {
class EventHandler;
}

namespace ash {

// A backdrop which gets created for a container |window| and which gets
// stacked behind the top level, activatable window that meets the following
// criteria.
//
// 1) Has a aura::client::kHasBackdrop property = true.
// 2) Active ARC window when the spoken feedback is enabled.
// 3) In tablet mode:
//        - Bottom-most snapped window in splitview,
//        - Top-most activatable window if splitview is inactive.
class ASH_EXPORT BackdropController : public AccessibilityObserver,
                                      public OverviewObserver,
                                      public SplitViewObserver,
                                      public WallpaperControllerObserver,
                                      public TabletModeObserver {
 public:
  explicit BackdropController(aura::Window* container);
  ~BackdropController() override;

  void OnWindowAddedToLayout();
  void OnWindowRemovedFromLayout();
  void OnChildWindowVisibilityChanged();
  void OnWindowStackingChanged();
  void OnPostWindowStateTypeChange();
  void OnDisplayMetricsChanged();

  // Called when the desk content is changed in order to update the state of the
  // backdrop even if overview mode is active.
  void OnDeskContentChanged();

  // Update the visibility of, and restack the backdrop relative to
  // the other windows in the container.
  void UpdateBackdrop();

  // Pauses backdrop updates until the returned object goes out of scope.
  base::ScopedClosureRunner PauseUpdates();

  // Returns the current visible top level window in the container.
  aura::Window* GetTopmostWindowWithBackdrop();

  aura::Window* backdrop_window() { return backdrop_window_; }

  // OverviewObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnding(OverviewSession* overview_session) override;
  void OnOverviewModeEndingAnimationComplete(bool canceled) override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // SplitViewObserver:
  void OnSplitViewStateChanged(SplitViewController::State previous_state,
                               SplitViewController::State state) override;
  void OnSplitViewDividerPositionChanged() override;

  // WallpaperControllerObserver:
  void OnWallpaperPreviewStarted() override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

 private:
  friend class WorkspaceControllerTestApi;

  // Reenables updates previously pause by calling PauseUpdates().
  void RestoreUpdates();

  void UpdateBackdropInternal();

  void EnsureBackdropWidget(BackdropWindowMode mode);

  void UpdateAccessibilityMode();

  void Layout();

  bool WindowShouldHaveBackdrop(aura::Window* window);

  // Show the backdrop window.
  void Show();

  // Hide the backdrop window. If |destroy| is true, the backdrop widget will be
  // destroyed, otherwise it'll be just hidden.
  void Hide(bool destroy, bool animate = true);

  // Returns true if the backdrop window should be fullscreen. It should not be
  // fullscreen only if 1) split view is active and 2) there is only one snapped
  // window and 3) the snapped window is the topmost window which should have
  // the backdrop.
  bool BackdropShouldFullscreen();

  // Gets the bounds for the backdrop window if it should not be fullscreen.
  // It's the case for splitview mode, if there is only one snapped window, the
  // backdrop should not cover the non-snapped side of the screen, thus the
  // backdrop bounds should be the bounds of the snapped window.
  gfx::Rect GetBackdropBounds();

  // Sets the animtion type of |backdrop_window_| to |type|.
  void SetBackdropAnimationType(int type);

  aura::Window* root_window_;

  // The backdrop which covers the rest of the screen.
  std::unique_ptr<views::Widget> backdrop_;

  // aura::Window for |backdrop_|.
  aura::Window* backdrop_window_ = nullptr;

  // The container of the window that should have a backdrop.
  aura::Window* container_;

  // Event hanlder used to implement actions for accessibility.
  std::unique_ptr<ui::EventHandler> backdrop_event_handler_;
  ui::EventHandler* original_event_handler_ = nullptr;

  // If true, skip updating background. Used to avoid recursive update
  // when updating the window stack, or delay hiding the backdrop
  // in overview mode.
  bool pause_update_ = false;

  base::WeakPtrFactory<BackdropController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BackdropController);
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_BACKDROP_CONTROLLER_H_
