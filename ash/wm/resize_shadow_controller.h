// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_RESIZE_SHADOW_CONTROLLER_H_
#define ASH_WM_RESIZE_SHADOW_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"

namespace ui {
class ColorProviderSourceObserver;
class Decoration;
}  // namespace ui

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
  ui::Decoration* GetDecorationForWindowForTest(aura::Window* window);
  ui::ColorProviderSourceObserver* GetColorObserverForWindowForTest(
      aura::Window* window);

 private:
  class WindowResizeShadow;

  // Removes all shadows.
  void RemoveAllShadows();

  // Recreates a shadow for `window` if none is registered or its type / corner
  // configuration changed.
  void RecreateShadowIfNeeded(aura::Window* window);

  WindowResizeShadow* GetWindowResizeShadow(aura::Window* window) const;

  // Returns the resize shadow for `window` or nullptr if none exists.
  ResizeShadow* GetShadowForWindow(aura::Window* window) const;

  // Reparents `window`'s shadow layer directly beneath `window->layer()`.
  void ReparentShadowLayer(aura::Window* window) const;

  // Updates bounds, opacity, and visibility of `window`'s shadow layer.
  void UpdateShadowBoundsAndVisibility(aura::Window* window,
                                       const gfx::Rect& window_bounds) const;

  // Update shadow visibility for a given |window|.
  void UpdateShadowVisibility(aura::Window* window, bool visible) const;
  bool ShouldShowShadowForWindow(aura::Window* window) const;

  base::flat_map<aura::Window*, std::unique_ptr<WindowResizeShadow>>
      window_shadows_;

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      windows_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_RESIZE_SHADOW_CONTROLLER_H_
