// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_UI_ACCESSIBILITY_PANEL_LAYOUT_MANAGER_H_
#define ASH_ACCESSIBILITY_UI_ACCESSIBILITY_PANEL_LAYOUT_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/shell_observer.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/layout_manager.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}

namespace ash {

// AccessibilityPanelLayoutManager manages the container window used for the
// ChromeVox spoken feedback panel, which sits at the top of the display. It
// insets the display work area bounds when ChromeVox is visible. The ChromeVox
// panel is created by Chrome because spoken feedback is implemented by an
// extension. Exported for test.
class ASH_EXPORT AccessibilityPanelLayoutManager
    : public aura::LayoutManager,
      public display::DisplayObserver,
      public ::wm::ActivationChangeObserver,
      public ShellObserver {
 public:
  // Height of the panel in DIPs. Public for test.
  static constexpr int kDefaultPanelHeight = 35;

  AccessibilityPanelLayoutManager();

  AccessibilityPanelLayoutManager(const AccessibilityPanelLayoutManager&) =
      delete;
  AccessibilityPanelLayoutManager& operator=(
      const AccessibilityPanelLayoutManager&) = delete;

  ~AccessibilityPanelLayoutManager() override;

  // Controls the panel's visibility and location.
  void SetAlwaysVisible(bool always_visible);
  void SetPanelBounds(const gfx::Rect& bounds, AccessibilityPanelState state);

  // aura::LayoutManager:
  void OnWindowResized() override {}
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override;
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override;
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

  // DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override {}
  void OnDisplaysRemoved(const display::Displays& removed_displays) override {}
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // ::wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // ShellObserver:
  void OnFullscreenStateChanged(bool is_fullscreen,
                                aura::Window* container) override;

  aura::Window* panel_window_for_test() { return panel_window_; }

 private:
  // Updates the panel window bounds.
  void UpdateWindowBounds();

  // Sets cached height of the accessibility panel.
  void UpdateWorkAreaForPanelHeight();

  // The panel being managed (e.g. the ChromeVoxPanel's native aura window).
  raw_ptr<aura::Window> panel_window_ = nullptr;

  // Window bounds when not in fullscreen
  gfx::Rect panel_bounds_ = gfx::Rect(0, 0, 0, 0);

  display::ScopedDisplayObserver display_observer_{this};

  // Determines whether panel is hidden when browser is in fullscreen.
  bool always_visible_ = false;

  // Determines how the panel_bounds_ are used when displaying the panel.
  AccessibilityPanelState panel_state_ = AccessibilityPanelState::BOUNDED;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_UI_ACCESSIBILITY_PANEL_LAYOUT_MANAGER_H_
