// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_RESIZE_SHADOW_CONTROLLER_H_
#define ASH_WM_RESIZE_SHADOW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace ash {
class ResizeShadow;

// ResizeShadowController observes changes to resizable windows and shows
// a resize handle visual effect when the cursor is near the edges.
class ASH_EXPORT ResizeShadowController : public aura::WindowObserver {
 public:
  ResizeShadowController();
  ResizeShadowController(const ResizeShadowController&) = delete;
  ResizeShadowController& operator=(const ResizeShadowController&) = delete;
  ~ResizeShadowController() override;

  // Shows the appropriate shadow for a given |window| and |hit_test| location.
  void ShowShadow(aura::Window* window, int hit_test);

  // Hides the shadow for a |window|, if it has one.
  void HideShadow(aura::Window* window);

  // Hides all shadows.
  void HideAllShadows();

  // aura::WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;

  ResizeShadow* GetShadowForWindowForTest(aura::Window* window);

 private:
  // Creates a shadow for a given window and returns it.  |window_shadows_|
  // owns the memory.
  ResizeShadow* CreateShadow(aura::Window* window);

  // Returns the resize shadow for |window| or NULL if no shadow exists.
  ResizeShadow* GetShadowForWindow(aura::Window* window);

  base::flat_map<aura::Window*, std::unique_ptr<ResizeShadow>> window_shadows_;

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      windows_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_RESIZE_SHADOW_CONTROLLER_H_
