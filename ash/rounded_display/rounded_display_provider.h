// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_PROVIDER_H_
#define ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_PROVIDER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class RoundedDisplayGutter;
class RoundedDisplayGutterFactory;
class RoundedDisplayProviderTestApi;
class RoundedDisplayHost;

// Provides an API to create software-based rounded corners for a given display.
// It manages the rounded corners for the display and renders them by submitting
// independent compositor frame to the display compositor.
class ASH_EXPORT RoundedDisplayProvider {
 public:
  // Strategy decides the direction in which RoundedDisplayGutters are created.
  enum class Strategy {
    // Overlay gutters are created is the scanout direction of the display
    // panel.
    kScanout,
    // Overlay gutters are created is the direction that is perpendicular to the
    // scanout direction of display_panel.
    kOther
  };

  static std::unique_ptr<RoundedDisplayProvider> Create(int64_t display_id);

  RoundedDisplayProvider(
      const int64_t display_id,
      std::unique_ptr<RoundedDisplayGutterFactory> gutter_factory);

  RoundedDisplayProvider(const RoundedDisplayProvider&) = delete;
  RoundedDisplayProvider& operator=(const RoundedDisplayProvider&) = delete;

  ~RoundedDisplayProvider();

  // Creates RoundedDisplayGutter based on the specified `strategy` for the
  // display and initializes the `RoundedDisplayHost`. Note: Need to call
  // `UpdateRoundedDisplaySurface()` to show the rounded-display masks.
  void Init(const gfx::RoundedCornersF& panel_radii, Strategy strategy);

  // Update the window hierarchy to which the `host_window_` is attached to.
  // Needed to be called when the window_tree_host for the display is updated
  // and swapped with the window_tree_host of another display.
  void UpdateHostParent();

  bool UpdateRoundedDisplaySurface();

  int64_t display_id() const { return display_id_; }

 private:
  friend class RoundedDisplayProviderTestApi;

  // Returns gutters in draw order. Gutters in the front are drawn on top.
  // Note: `DeleteGutters()` and `CreateGutters()` invalidates the pointers.
  void GetGuttersInDrawOrder(std::vector<RoundedDisplayGutter*>& gutters) const;

  bool ShouldSubmitNewCompositorFrame(const display::Display& display) const;

  // Creates RoundedDisplayGutters if needed and returns true if we created
  // gutters else returns false.
  // To minimize the use of overlay planes, we only create a gutter if it has
  // at least a single non-zero corner mask drawn into it.
  // For example, for a display that doesn't have rounded bottom edges, and
  // based on the `strategy_`, we need to create upper and lower OverlayGutters,
  // we will skip the creation of the lower OverlayGutter.
  bool CreateGutters(const display::Display& display,
                     const gfx::RoundedCornersF& panel_radii);

  // Initialize the `RoundedDisplayHost`, attach the `host_window_` to the
  // window hierarchy of the display.
  void InitializeHost();

  // The id of the display for which the provider creates rounded corners
  // for.
  const int64_t display_id_;

  // The current display state for which rounded display is enabled.
  float current_device_scale_factor_ = 0.0;
  display::Display::Rotation current_logical_rotation_ =
      display::Display::Rotation::ROTATE_0;
  gfx::RoundedCornersF current_panel_radii_;

  // The specified strategy to determine direction of overlay gutters.
  Strategy strategy_ = Strategy::kScanout;

  // Stores the overlay gutters that are created based on the `strategy_`.
  std::vector<std::unique_ptr<RoundedDisplayGutter>> overlay_gutters_;

  // OverlayRoundedDisplayGutter creation is delegated to this factory.
  std::unique_ptr<RoundedDisplayGutterFactory> gutter_factory_;

  // Represents the surface on which the `host_` render the mask textures of the
  // rounded-display corners.
  std::unique_ptr<aura::Window> host_window_;

  // Responsible to render the mask textures by submitting compositor frames.
  std::unique_ptr<RoundedDisplayHost> host_;

  base::WeakPtrFactory<RoundedDisplayProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_PROVIDER_H_
