// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_RESIZE_SHADOW_CONTROLLER_H_
#define ASH_WM_RESIZE_SHADOW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/color_palette.h"

namespace ash {
class ResizeShadow;
enum class ResizeShadowType;

// ResizeShadowController observes changes to resizable windows and shows
// a resize handle visual effect when the cursor is near the edges.
class ASH_EXPORT ResizeShadowController : public aura::WindowObserver {
 public:
  ResizeShadowController();
  ResizeShadowController(const ResizeShadowController&) = delete;
  ResizeShadowController& operator=(const ResizeShadowController&) = delete;
  ~ResizeShadowController() override;

  // Shows the appropriate shadow for a given |window| and |hit_test| location.
  // If the |window| is invisible, the shadow will not shown.
  void ShowShadow(aura::Window* window, int hit_test = HTNOWHERE);

  // Shows all shadows.
  void TryShowAllShadows();

  // Hides the shadow for a |window|, if it has one.
  void HideShadow(aura::Window* window);

  // Hides all shadows.
  void HideAllShadows();

  // Cross fade animation may reorder the window layer so that the shadow layer
  // is on top. We should restack the shadow layer below the window layer.
  void OnCrossFadeAnimationCompleted(aura::Window* window);

  // aura::WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowAddedToRootWindow(aura::Window* window) override;

  void UpdateResizeShadowBoundsOfWindow(aura::Window* window,
                                        const gfx::Rect& bounds);

  ResizeShadow* GetShadowForWindowForTest(aura::Window* window);

 private:
  // Removes all shadows.
  void RemoveAllShadows();

  // Recreates a shadow for a given |window| and the type from the |window|'s
  // property if there's no shadow registered or it has one but its type is
  // different. |window_shadows_| owns the memory.
  void RecreateShadowIfNeeded(aura::Window* window);

  // Returns the resize shadow for |window| or NULL if no shadow exists.
  ResizeShadow* GetShadowForWindow(aura::Window* window) const;

  // Update shadow visibility for a given |window|.
  void UpdateShadowVisibility(aura::Window* window, bool visible) const;
  bool ShouldShowShadowForWindow(aura::Window* window) const;

  base::flat_map<aura::Window*, std::unique_ptr<ResizeShadow>> window_shadows_;

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      windows_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_RESIZE_SHADOW_CONTROLLER_H_
