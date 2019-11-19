// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/macros.h"
#include "ui/aura/window_observer.h"

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
      public aura::WindowObserver,
      public KeyboardControllerObserver {
 public:
  explicit SystemModalContainerLayoutManager(aura::Window* container);
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

  aura::Window* modal_window() {
    return !modal_windows_.empty() ? modal_windows_.back() : nullptr;
  }

  // The container that owns the layout manager.
  aura::Window* container_;

  // WindowDimmer used to dim windows behind the modal window(s) being shown in
  // |container_|.
  std::unique_ptr<WindowDimmer> window_dimmer_;

  // A stack of modal windows. Only the topmost can receive events.
  std::vector<aura::Window*> modal_windows_;

  // Windows contained in this set are centered. Windows are automatically
  // added to this based on IsBoundsCentered().
  std::set<const aura::Window*> windows_to_center_;

  DISALLOW_COPY_AND_ASSIGN(SystemModalContainerLayoutManager);
};

}  // namespace ash

#endif  // ASH_WM_SYSTEM_MODAL_CONTAINER_LAYOUT_MANAGER_H_
