// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_BACKDROP_CONTROLLER_H_
#define ASH_WM_WORKSPACE_BACKDROP_CONTROLLER_H_

#include <memory>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_observer.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Widget;
}  // namespace views

namespace ui {
class EventHandler;
}  // namespace ui

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
                                      public WindowBackdrop::Observer {
 public:
  explicit BackdropController(aura::Window* container);

  BackdropController(const BackdropController&) = delete;
  BackdropController& operator=(const BackdropController&) = delete;

  ~BackdropController() override;

  void OnWindowAddedToLayout(aura::Window* window);
  void OnWindowRemovedFromLayout(aura::Window* window);
  void OnChildWindowVisibilityChanged(aura::Window* window);
  void OnWindowStackingChanged(aura::Window* window);
  void OnPostWindowStateTypeChange(aura::Window* window);
  void OnDisplayMetricsChanged();
  void OnTabletModeChanged();

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

  // Hides the backdrop window for taking the informed restore screenshot in
  // order to not include it in the screenshot.
  void HideOnTakingInformedRestoreScreenshot();

  aura::Window* backdrop_window() { return backdrop_window_; }

  aura::Window* window_having_backdrop() { return window_having_backdrop_; }

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

  // WindowBackdrop::Observer:
  void OnWindowBackdropPropertyChanged(aura::Window* window) override;

 private:
  class WindowAnimationWaiter;
  friend class WorkspaceControllerTestApi;

  // Reenables updates previously pause by calling PauseUpdates().
  void RestoreUpdates();

  void UpdateBackdropInternal();

  void EnsureBackdropWidget();

  void UpdateAccessibilityMode();

  void Layout();

  bool WindowShouldHaveBackdrop(aura::Window* window);

  // Show the backdrop window if the |window_having_backdrop_| is not animating,
  // otherwise it will wait for that animation to finish. If it can show the
  // backdrop, it will update its bounds and stacking order before its shown.
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

  // If |window_having_backdrop_| is animating such that we shouldn't update the
  // backdrop until that animation is complete, starts observing this animation
  // (if not already done) and returns true. Returns false otherwise.
  bool MaybeWaitForWindowAnimation();

  // Updates the layout of the backdrop if one exists and is visible.
  void MaybeUpdateLayout();

  // Returns true if changes to |window| may require updating the backdrop
  // visibility and availability.
  bool DoesWindowCauseBackdropUpdates(aura::Window* window) const;

  raw_ptr<aura::Window> root_window_;

  // The backdrop which covers the rest of the screen.
  std::unique_ptr<views::Widget> backdrop_;

  // aura::Window for |backdrop_|.
  raw_ptr<aura::Window, DanglingUntriaged> backdrop_window_ = nullptr;

  // The window for which a backdrop has been installed.
  raw_ptr<aura::Window, DanglingUntriaged> window_having_backdrop_ = nullptr;

  // The container of the window that should have a backdrop.
  raw_ptr<aura::Window> container_;

  // If |window_having_backdrop_| is animating while we're trying to show the
  // backdrop, we postpone showing it until the animation completes.
  std::unique_ptr<WindowAnimationWaiter> window_animation_waiter_;

  // Event hanlder used to implement actions for accessibility.
  std::unique_ptr<ui::EventHandler> backdrop_event_handler_;
  raw_ptr<ui::EventHandler, DanglingUntriaged> original_event_handler_ =
      nullptr;

  // If true, skip updating background. Used to avoid recursive update
  // when updating the window stack, or delay hiding the backdrop
  // in overview mode.
  bool pause_update_ = false;

  // If true, we're inside the stack of `Hide()`. This is used to avoid
  // recursively calling `Hide()` which may lead to destroying the backdrop
  // widget while it's still being hidden. https://crbug.com/1368587.
  bool is_hiding_backdrop_ = false;

  base::ScopedMultiSourceObservation<WindowBackdrop, WindowBackdrop::Observer>
      window_backdrop_observations_{this};

  base::WeakPtrFactory<BackdropController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_BACKDROP_CONTROLLER_H_
