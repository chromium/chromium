// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_RESIZE_SHADOW_H_
#define ASH_WM_RESIZE_SHADOW_H_

#include <optional>
#include <tuple>

#include "ash/ash_export.h"
#include "ash/public/cpp/resize_shadow_type.h"
#include "base/export_template.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/hit_test.h"
#include "ui/decoration/decoration_details.h"
#include "ui/decoration/decoration_source.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace gfx {
class Canvas;
}  // namespace gfx

namespace ash {

// Describes the visual parameters of a window resize shadow for ninebox
// generation and caching.
class ASH_EXPORT ResizeShadowValue {
 public:
  ResizeShadowValue(const gfx::Outsets& outsets,
                    int corner_radius,
                    SkColor color)
      : outsets_(outsets), corner_radius_(corner_radius), color_(color) {}
  ResizeShadowValue(const ResizeShadowValue&) = default;
  ResizeShadowValue& operator=(const ResizeShadowValue&) = default;
  ~ResizeShadowValue() = default;

  const gfx::Outsets& outsets() const { return outsets_; }
  int corner_radius() const { return corner_radius_; }
  SkColor color() const { return color_; }

  friend bool operator==(const ResizeShadowValue&,
                         const ResizeShadowValue&) = default;

  friend bool operator<(const ResizeShadowValue& lhs,
                        const ResizeShadowValue& rhs) {
    return std::make_tuple(lhs.outsets_.top(), lhs.outsets_.left(),
                           lhs.outsets_.bottom(), lhs.outsets_.right(),
                           lhs.corner_radius_, lhs.color_) <
           std::make_tuple(rhs.outsets_.top(), rhs.outsets_.left(),
                           rhs.outsets_.bottom(), rhs.outsets_.right(),
                           rhs.corner_radius_, rhs.color_);
  }

 private:
  gfx::Outsets outsets_;
  int corner_radius_;
  SkColor color_;
};

// Generator for resize shadow decoration rendering and ninebox insets.
struct ASH_EXPORT ResizeShadowGenerator {
  static gfx::Outsets GetMargins(const ResizeShadowValue& value);

  static gfx::Insets GetNineboxApertureInsets(
      const ResizeShadowValue& value,
      const gfx::RoundedCornersF& rounded_corners);

  static void Draw(gfx::Canvas* canvas,
                   const ResizeShadowValue& value,
                   const gfx::RoundedCornersF& rounded_corners,
                   const gfx::Rect& content_rect);
};

using ResizeShadowDetails =
    ui::decoration::DecorationDetails<ResizeShadowValue, ResizeShadowGenerator>;

// DecorationSource that renders the resize edge effect when the user moves
// their mouse over a sizing edge. This is just a visual effect; the actual
// resize is handled by the EventFilter.
class ASH_EXPORT ResizeShadow : public ui::decoration::DecorationSource {
 public:
  DECLARE_SAFE_CAST_TARGET()

  // Resize shadow parameters. Default params values are unresizable window
  // shadow.
  struct InitParams {
    // The width of the resize shadow that appears on edge of the window.
    int thickness = 8;
    // The corner radius of the resize shadow.
    int shadow_corner_radius = 2;
    // The opacity of the resize shadow.
    float opacity = 0.6f;
    // Controls whether the resize shadow shall respond to hit testing or not.
    bool hit_test_enabled = true;
    // True if the resize shadow is configured for a window with large rounded
    // corners.
    bool is_for_large_rounded_corners = false;
  };

  ResizeShadow(const InitParams& params, ResizeShadowType type);
  ResizeShadow(const ResizeShadow&) = delete;
  ResizeShadow& operator=(const ResizeShadow&) = delete;
  ~ResizeShadow() override;

  void SetColor(SkColor color);
  SkColor color() const { return color_; }

  bool visible() const { return visible_; }
  ResizeShadowType type() const { return type_; }
  const InitParams& params() const { return params_; }
  bool is_for_large_rounded_corners() const {
    return params_.is_for_large_rounded_corners;
  }

  int GetLastHitTestForTest() const { return last_hit_test_; }

  // Computes the resize shadow specification for `content_bounds` and
  // `rounded_corners`, or returns std::nullopt if nothing should be drawn.
  std::optional<ResizeShadowValue> GetSpec(
      const gfx::Rect& content_bounds,
      const gfx::RoundedCornersF& rounded_corners) const;

  // ui::decoration::DecorationSource:
  std::optional<Details> GetDetails(
      const gfx::Rect& content_bounds,
      const gfx::RoundedCornersF& rounded_corners) override;

 private:
  friend class ResizeShadowController;

  // Updates active edge outsets for `hit_test` (such as HTRIGHT or
  // HTBOTTOMRIGHT) and marks the shadow visible if any edge is active.
  void ShowForHitTest(int hit_test = HTNOWHERE);

  // Marks the resize shadow hidden while keeping the last active `outsets_` in
  // place so the decoration layer can fade out without snapping inward.
  void Hide();

  InitParams params_;

  // The type of the resize shadow. Used to identify variations of resize
  // shadow.
  ResizeShadowType type_;

  // Hit test value from last call to ShowForHitTest().
  int last_hit_test_ = HTNOWHERE;

  // Outsets for the active resize edges. Retained across Hide() so fade-out
  // animations keep framing the last hovered edges.
  gfx::Outsets outsets_;

  SkColor color_ = SK_ColorBLACK;

  bool visible_ = false;
};

}  // namespace ash

namespace ui::decoration {

// Instantiated in the ash component so that every module shares a single resize
// shadow details cache in component builds.
extern template class EXPORT_TEMPLATE_DECLARE(
    ASH_EXPORT) internal::DecorationCache<ash::ResizeShadowValue,
                                          ash::ResizeShadowGenerator>;

}  // namespace ui::decoration

#endif  // ASH_WM_RESIZE_SHADOW_H_
