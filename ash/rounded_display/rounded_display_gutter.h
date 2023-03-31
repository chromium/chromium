// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_GUTTER_H_
#define ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_GUTTER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/frame_sink/ui_resource.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Canvas;
}  // namespace gfx

namespace ash {

// A RoundedDisplayGutter represents a collection of rounded corners of a
// display. A gutter is a texture factory that knows how to draw the
// corresponding masks for all rounded corners on a single texture. Generated
// textures are in target (pixel) space.
class ASH_EXPORT RoundedDisplayGutter {
 public:
  // RoundedCorner represents one of the rounded corners of a display.
  class RoundedCorner {
   public:
    // Enumerates the position of RoundedCorner on a display.
    enum Position {
      kUpperLeft = 1u << 0,
      kUpperRight = 1u << 1,
      kLowerLeft = 1u << 2,
      kLowerRight = 1u << 3
    };

    RoundedCorner(Position position, int radius, const gfx::Point& origin);

    RoundedCorner(const RoundedCorner&) = delete;
    RoundedCorner& operator=(const RoundedCorner&) = delete;

    RoundedCorner(RoundedCorner&&);
    RoundedCorner& operator=(RoundedCorner&&);

    ~RoundedCorner();

    int radius() const { return radius_; }

    Position position() const { return position_; }

    // Returns the bounds of the corners in display coordinates.
    const gfx::Rect& bounds() const { return bounds_in_pixels_; }

    bool DoesPaint() const;

    // Paints a rounded corner on the canvas. We decide how to paint the corner
    // based on the RoundedCorner position.
    void Paint(gfx::Canvas* canvas) const;

   private:
    void PaintCornerHelper(gfx::Canvas* canvas) const;

    Position position_;

    // Radius of the corner in pixels.
    int radius_;

    // Bounds of the corner in the display.
    gfx::Rect bounds_in_pixels_;
  };

  static std::unique_ptr<RoundedDisplayGutter> CreateGutter(
      std::vector<RoundedCorner>&& corners,
      bool is_overlay);

  RoundedDisplayGutter(std::vector<RoundedCorner>&& corners, bool is_overlay);

  RoundedDisplayGutter(const RoundedDisplayGutter&) = delete;
  RoundedDisplayGutter& operator=(const RoundedDisplayGutter&) = delete;

  ~RoundedDisplayGutter();

  // Returns the bounds in pixels of the gutter in display coordinates.
  const gfx::Rect& bounds() const;

  const std::vector<RoundedCorner>& GetGutterCorners() const {
    return corners_;
  }

  bool NeedsOverlays() const { return is_overlay_; }

  // Returns a unique identifier that can be used to consistently map a
  // texture back to the type of gutter.
  // For example, texture generated for gutter for top-left rounded corner and a
  // texture generated for gutter with bottom-left rounded corner are identical
  // in terms of size, buffer format etc but will have unique ui_source_id.
  UiSourceId ui_source_id() const;

  // Paints all the corner's mask textures on the canvas.
  void Paint(gfx::Canvas* canvas) const;

 private:
  gfx::Rect CalculateGutterBounds() const;
  UiSourceId CalculateUiSourceId() const;

  UiSourceId ui_source_id_ = kInvalidUiSourceId;

  // The rounded display corners that the gutter draws.
  const std::vector<RoundedCorner> corners_;

  // True if the textures should be rendered using hardware overlays system.
  bool is_overlay_ = true;

  // Bounds of gutter in display coordinates.
  gfx::Rect bounds_in_pixels_;
};

}  // namespace ash

#endif  // ASH_ROUNDED_DISPLAY_ROUNDED_DISPLAY_GUTTER_H_
