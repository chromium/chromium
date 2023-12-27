// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_RESIZE_SHADOW_H_
#define ASH_WM_RESIZE_SHADOW_H_

#include <memory>

#include "ash/public/cpp/resize_shadow_type.h"
#include "ash/style/ash_color_id.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/hit_test.h"
#include "ui/color/color_provider_source_observer.h"

namespace aura {
class Window;
}
namespace ui {
class Layer;
}
namespace gfx {
class Rect;
}

namespace ash {

// A class to render the resize edge effect when the user moves their mouse
// over a sizing edge. This is just a visual effect; the actual resize is
// handled by the EventFilter.
class ResizeShadow : public ui::ColorProviderSourceObserver {
 public:
  // Resize shadow parameters. Default params values are unresizable window
  // shadow.
  struct InitParams {
    // The width of the resize shadow that appears on edge of the window.
    int thickness = 8;
    // The corner radius of the resize shadow.
    int shadow_corner_radius = 2;
    // The corner radius of the window.
    int window_corner_radius = 2;
    // The opacity of the resize shadow.
    float opacity = 0.6f;
    // The color of the resize shadow.
    absl::variant<SkColor, ui::ColorId> color = kColorAshResizeShadowColor;
    // Controls whether the resize shadow shall respond to hit testing or not.
    bool hit_test_enabled = true;
    int hide_duration_ms = 100;
    // True if the resize shadow is configured for a window with rounded
    // corners.
    bool is_for_rounded_window = false;
  };

  ResizeShadow(aura::Window* window,
               const InitParams& params,
               ResizeShadowType type);
  ResizeShadow(const ResizeShadow&) = delete;
  ResizeShadow& operator=(const ResizeShadow&) = delete;
  ~ResizeShadow() override;

  bool visible() const { return visible_; }
  bool is_for_rounded_window() const { return params_.is_for_rounded_window; }

  int GetLastHitTestForTest() const { return last_hit_test_; }
  const ui::Layer* GetLayerForTest() const { return layer_.get(); }
  ResizeShadowType GetResizeShadowTypeForTest() const { return type_; }

 private:
  friend class ResizeShadowController;

  // ui::ColorProviderSourceObserver:
  void OnColorProviderChanged() override;

  // Called when the observed window is parented to a root window.
  void OnWindowParentToRootWindow();

  void UpdateShadowLayer();

  // Shows resize effects for one or more edges based on a |hit_test| code, such
  // as HTRIGHT or HTBOTTOMRIGHT.
  void ShowForHitTest(int hit_test = HTNOWHERE);

  // Hides all resize effects.
  void Hide();

  // Reparents |layer_| so that it's behind the layer of |window_|.
  void ReparentLayer();

  // Updates bounds and visibility of |layer_|.
  void UpdateBoundsAndVisibility();
  void UpdateBounds(const gfx::Rect& window_bounds);

  // Updates the |last_hist_test_| with given |hit_test| code.
  void UpdateHitTest(int hit_test);

  // The window associated with this shadow. Guaranteed to be alive for the
  // lifetime of `this`.
  raw_ptr<aura::Window> window_;

  // The layer to which the shadow is drawn. The layer is stacked beneath the
  // layer of |window_|.
  std::unique_ptr<ui::Layer> layer_;

  // Hit test value from last call to ShowForHitTest().  Used to prevent
  // repeatedly triggering the same animations for the same hit.
  int last_hit_test_ = HTNOWHERE;

  InitParams params_;

  // The type of the resize shadow. Used to identify variations of resize
  // shadow.
  ResizeShadowType type_;

  bool visible_ = false;
};

}  // namespace ash

#endif  // ASH_WM_RESIZE_SHADOW_H_
