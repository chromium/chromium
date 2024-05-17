// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SYSTEM_MODAL_CONTAINER_LAYOUT_MANAGER_H_
#define ASH_WM_SYSTEM_MODAL_CONTAINER_LAYOUT_MANAGER_H_

#include <memory>
#include <set>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/wm/wm_default_layout_manager.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"

namespace gfx {
class Rect;
}

namespace ash {
class WindowDimmer;

// LayoutManager for the modal window container.
// System modal windows which are centered on the screen will be kept centered
// when the container size changes.
class ASH_EXPORT SystemModalContainerLayoutManager
    : public WmDefaultLayoutManager,
      public display::DisplayObserver,
      public aura::WindowObserver,
      public KeyboardControllerObserver {
 public:
  explicit SystemModalContainerLayoutManager(aura::Window* container);

  SystemModalContainerLayoutManager(const SystemModalContainerLayoutManager&) =
      delete;
  SystemModalContainerLayoutManager& operator=(
      const SystemModalContainerLayoutManager&) = delete;

  ~SystemModalContainerLayoutManager() override;

  bool has_window_dimmer() const { return window_dimmer_ != nullptr; }

  // Overridden from WmDefaultLayoutManager:
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override;
  void OnWindowResized() override;
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnWillRemoveWindowFromLayout(aura::Window* child) override;
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

  // Overridden from aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

  // Overridden from KeyboardControllerObserver:
  void OnKeyboardOccludedBoundsChanged(const gfx::Rect& new_bounds) override;

  // True if the window is either contained by the top most modal window,
  // or contained by its transient children.
  bool IsPartOfActiveModalWindow(aura::Window* window);

  // Activates next modal window if any. Returns false if there
  // are no more modal windows in this layout manager.
  bool ActivateNextModalWindow();

  // Creates modal background window, which is a partially-opaque
  // fullscreen window. If there is already a modal background window,
  // it will bring it the top.
  void CreateModalBackground();

  void DestroyModalBackground();

  // Is the |window| modal background?
  static bool IsModalBackground(aura::Window* window);

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

 private:
  void AddModalWindow(aura::Window* window);

  // Removes |window| from |modal_windows_|. Returns true if |window| was in
  // |modal_windows_|.
  bool RemoveModalWindow(aura::Window* window);

  // Called when a modal window is removed. It will activate another modal
  // window if any, or remove modal screens on all displays.
  void OnModalWindowRemoved(aura::Window* removed);

  // Reposition the dialogs to become visible after the work area changes.
  void PositionDialogsAfterWorkAreaResize();

  // Get the usable bounds rectangle for enclosed dialogs.
  gfx::Rect GetUsableDialogArea() const;

  // Gets the new bounds for a |window| to use which are either centered (if the
  // window was previously centered) or fitted to the screen.
  gfx::Rect GetCenteredAndOrFittedBounds(const aura::Window* window);

  // Returns true if |bounds| is considered centered.
  bool IsBoundsCentered(const gfx::Rect& window_bounds) const;

  // Called to stop observing `window`. It can be called when `window` is
  // removed from the layout or `window` is about to be destroyed. `window` will
  // also be removed from `windows_to_center_` and `modal_windows_` if it's in
  // these lists.
  void StopObservingWindow(aura::Window* window);

  aura::Window* modal_window() {
    return !modal_windows_.empty() ? modal_windows_.back() : nullptr;
  }

  // The container that owns the layout manager.
  raw_ptr<aura::Window> container_;

  // WindowDimmer used to dim windows behind the modal window(s) being shown in
  // |container_|.
  std::unique_ptr<WindowDimmer> window_dimmer_;

  // A stack of modal windows. Only the topmost can receive events.
  std::vector<raw_ptr<aura::Window, VectorExperimental>> modal_windows_;

  // Windows contained in this set are centered. Windows are automatically
  // added to this based on IsBoundsCentered().
  std::set<raw_ptr<const aura::Window, SetExperimental>> windows_to_center_;

  // An observer to update position of modals when display work area changes.
  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_WM_SYSTEM_MODAL_CONTAINER_LAYOUT_MANAGER_H_
