// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_STYLE_UTIL_H_
#define ASH_STYLE_STYLE_UTIL_H_

#include <optional>

#include "ash/ash_export.h"
#include "ui/color/color_id.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"

namespace ui {
class ColorProvider;
}  // namespace ui

namespace views {
class Background;
class Button;
class FocusRing;
class InkDrop;
class InkDropHighlight;
class InkDropRipple;
class View;

namespace corewm {
class TooltipViewAura;
}  // namespace corewm

}  // namespace views

namespace ash {

class ASH_EXPORT StyleUtil {
 public:
  // InkDrop attributes that can be configured through
  // ConfigureInkDropAttributes. Including the base color, inkdrop and highlight
  // opacity.
  enum InkDropAttributes {
    kBaseColor = 1,
    kInkDropOpacity = 1 << 1,
    kHighlightOpacity = 1 << 2
  };

  static constexpr float kLightInkDropOpacity = 0.08f;
  static constexpr float kDarkInkDropOpacity = 0.06f;

  static float GetInkDropOpacity();

  // Creates an InkDrop instance for `host`. All styles are configured to show
  // the highlight when the ripple is visible.
  static std::unique_ptr<views::InkDrop> CreateInkDrop(
      views::Button* host,
      bool highlight_on_hover = false,
      bool highlight_on_focus = false);

  // Crates an InkDropRipple instance for `host` with `insets`.
  static std::unique_ptr<views::InkDropRipple> CreateInkDropRipple(
      const gfx::Insets& insets,
      const views::View* host,
      SkColor background_color = gfx::kPlaceholderColor);

  // Creates an InkDropHighlight instance for `host`.
  static std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight(
      const views::View* host,
      SkColor background_color);

  // Sets attributes(e.g, insets) for creating the inkdrop ripple. Note, A
  // FloodFillInkDropRipple will be created for the given `host`.
  static void SetRippleParams(
      views::View* host,
      const gfx::Insets& insets,
      SkColor background_color = gfx::kPlaceholderColor);

  // Sets up the inkdrop for the given `button`. Including setting the callback
  // for InkDrop, Ripple, Highlight. Inside the callback functions, they will
  // setup whether to show the highlight on hover or focus, inkdrop color,
  // opacity etc.
  static void SetUpInkDropForButton(
      views::Button* button,
      const gfx::Insets& ripple_insets = gfx::Insets(),
      bool highlight_on_hover = false,
      bool highlight_on_focus = false,
      SkColor background_color = gfx::kPlaceholderColor);

  // Configures the InkDropAttributes for the given `view` based on
  // `attributes`. Note, `attributes` is a bitmask from InkDropAttributes.
  static void ConfigureInkDropAttributes(
      views::View* view,
      uint32_t attributes,
      SkColor background_color = gfx::kPlaceholderColor);

  // Sets up the focus ring and its color for `view`. `halo_inset` is the
  // adjustment from the visible border of the host view to render the focus
  // ring. If it is not given, then the default (-0.5 * thickness) will be used.
  static views::FocusRing* SetUpFocusRingForView(
      views::View* view,
      std::optional<int> halo_inset = std::nullopt);

  static void InstallRoundedCornerHighlightPathGenerator(
      views::View* view,
      const gfx::RoundedCornersF& corners);

  // Creates a background that fills the canvas with a fully rounded rect whose
  // rounded corner radius is set to the half of the minimum dimension of view's
  // local bounds. The background is painted in the color specified by the
  // view's ColorProvider and the given color identifier.
  static std::unique_ptr<views::Background>
  CreateThemedFullyRoundedRectBackground(ui::ColorId color_id);

  static std::unique_ptr<views::corewm::TooltipViewAura>
  CreateAshStyleTooltipView();

  // Creates a shadow colors map with given color provider.
  static ui::Shadow::ElevationToColorsMap CreateShadowElevationToColorsMap(
      const ui::ColorProvider* color_provider);

 private:
  StyleUtil() = default;
  StyleUtil(const StyleUtil&) = delete;
  StyleUtil& operator=(const StyleUtil&) = delete;
  ~StyleUtil() = default;
};

}  // namespace ash

#endif  // ASH_STYLE_STYLE_UTIL_H_
