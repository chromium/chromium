// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SCREEN_PINNING_CONTROLLER_H_
#define ASH_WM_SCREEN_PINNING_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "base/memory/raw_ptr.h"
#include "ui/display/manager/display_manager_observer.h"

namespace aura {
class Window;
}

namespace ash {

class WindowDimmer;

template <typename UserData>
class WindowUserData;

// Supports "screen pinning" for ARC++ apps. From the Android docs:
// "Lets you temporarily restrict users from leaving your task or being
// interrupted by notifications. This could be used, for example, if you are
// developing an education app to support high stakes assessment requirements on
// Android, or a single-purpose or kiosk application."
// https://developer.android.com/about/versions/android-5.0.html#ScreenPinning
class ASH_EXPORT ScreenPinningController
    : public display::DisplayManagerObserver,
      aura::WindowObserver {
 public:
  ScreenPinningController();

  ScreenPinningController(const ScreenPinningController&) = delete;
  ScreenPinningController& operator=(const ScreenPinningController&) = delete;

  ~ScreenPinningController() override;

  // Set the `allow_window_stacking_with_pinned_window_` to allow window to
  // stack on top of pinned window. This is only used for OnTask locked session
  // where we still want to surface some popups on top of the pinned window.
  void SetAllowWindowStackingWithPinnedWindow(bool val) {
    allow_window_stacking_with_pinned_window_ = val;
  }

  // Sets a pinned window. It is not allowed to call this when there already
  // is a pinned window.
  void SetPinnedWindow(aura::Window* pinned_window);

  // Returns true if in pinned mode, otherwise false.
  bool IsPinned() const;

  // Returns the pinned window if in pinned mode, or nullptr.
  aura::Window* pinned_window() const { return pinned_window_; }

  // Called when a new window is added to the container which has the pinned
  // window.
  void OnWindowAddedToPinnedContainer(aura::Window* new_window);

  // Called when a window will be removed from the container which has the
  // pinned window.
  void OnWillRemoveWindowFromPinnedContainer(aura::Window* window);

  // Called when a window stacking is changed in the container which has the
  // pinned window.
  void OnPinnedContainerWindowStackingChanged(aura::Window* window);

  // Called when a new window is added to a system modal container.
  void OnWindowAddedToSystemModalContainer(aura::Window* new_window);

  // Called when a window will be removed from a system modal container.
  void OnWillRemoveWindowFromSystemModalContainer(aura::Window* window);

  // Called when a window stacking is changed in a system modal container.
  void OnSystemModalContainerWindowStackingChanged(aura::Window* window);

 private:
  class PinnedContainerWindowObserver;
  class PinnedContainerChildWindowObserver;
  class SystemModalContainerWindowObserver;
  class SystemModalContainerChildWindowObserver;

  // Keeps the pinned window on top of the siblings.
  void KeepPinnedWindowOnTop();

  // Keeps the dim window at bottom of the container.
  void KeepDimWindowAtBottom(aura::Window* container);

  // Creates a WindowDimmer for |container| and places it in |window_dimmers_|.
  // Returns the window from WindowDimmer.
  aura::Window* CreateWindowDimmer(aura::Window* container);

  // Resets internal states when |pinned_window_| exits pinning state, or
  // disappears.
  void ResetWindowPinningState();

  // display::DisplayManagerObserver:
  void OnDidApplyDisplayChanges() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // Called when `container_` is being destroyed.
  void OnContainerDestroying(aura::Window* container);

  // Pinned window should be on top in the parent window.
  raw_ptr<aura::Window> pinned_window_ = nullptr;
  // The container `pinned_container_window_observer_` observes.
  raw_ptr<aura::Window> container_ = nullptr;

  // Owns the WindowDimmers. There is one WindowDimmer for the parent of
  // |pinned_window_| and one for each display other than the display
  // |pinned_window_| is on.
  std::unique_ptr<WindowUserData<WindowDimmer>> window_dimmers_;

  // Set true only when restacking done by this controller.
  bool in_restacking_ = false;

  // Set true to allow windows to stack on top of the pinned window. This is
  // only used for OnTask locked session where we still want to surface some
  // popups on top of the pinned window.
  bool allow_window_stacking_with_pinned_window_ = false;

  // Window observers to translate events for the window to this controller.
  std::unique_ptr<PinnedContainerWindowObserver>
      pinned_container_window_observer_;
  std::unique_ptr<PinnedContainerChildWindowObserver>
      pinned_container_child_window_observer_;
  std::unique_ptr<SystemModalContainerWindowObserver>
      system_modal_container_window_observer_;
  std::unique_ptr<SystemModalContainerChildWindowObserver>
      system_modal_container_child_window_observer_;
};

}  // namespace ash

#endif  // ASH_WM_SCREEN_PINNING_CONTROLLER_H_
