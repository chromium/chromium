// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_OVERSCAN_CALIBRATOR_H_
#define ASH_DISPLAY_OVERSCAN_CALIBRATOR_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class Layer;
}

namespace ash {

// This is used to show the visible feedback to the user's operations for
// calibrating display overscan settings.
class ASH_EXPORT OverscanCalibrator : public ui::LayerDelegate,
                                      public display::DisplayObserver {
 public:
  OverscanCalibrator(const display::Display& target_display,
                     const gfx::Insets& initial_insets);

  OverscanCalibrator(const OverscanCalibrator&) = delete;
  OverscanCalibrator& operator=(const OverscanCalibrator&) = delete;

  ~OverscanCalibrator() override;

  // Commits the current insets data to the system.
  void Commit();

  // Reset the overscan insets to default value.  If the display has
  // overscan, the default value is the display's default overscan
  // value. Otherwise, the default value is the old |initial_insets_|.
  void Reset();

  // Updates the insets and redraw the visual feedback.
  void UpdateInsets(const gfx::Insets& insets);

  const gfx::Insets& insets() const { return insets_; }

  // ui::LayerDelegate overrides:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  // DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

 private:
  void UpdateUILayer();

  // The target display.
  display::Display display_;

  // The current insets.
  gfx::Insets insets_;

  // The insets initially given. Stored so we can undo the insets later.
  gfx::Insets initial_insets_;

  // Whether the current insets are committed to the system or not.
  bool committed_;

  // The visualization layer for the current calibration region.
  std::unique_ptr<ui::Layer> calibration_layer_;

  // Register for DisplayObserver callbacks.
  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_DISPLAY_OVERSCAN_CALIBRATOR_H_
