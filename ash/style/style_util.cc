// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_util.h"

#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/system_shadow.h"
#include "ash/style/typography.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/corewm/tooltip_view_aura.h"

namespace ash {

namespace {

constexpr int kTooltipRoundedCornerRadius = 6;
constexpr gfx::Insets kTooltipBorderInset = gfx::Insets::VH(5, 8);
constexpr int kTooltipMinLineHeight = 18;
constexpr int kTooltipMaxLines = 3;

// A themed fully rounded rect background whose corner radius equals to the half
// of the minimum dimension of its view's local bounds.
class ThemedFullyRoundedRectBackground : public views::Background {
 public:
  explicit ThemedFullyRoundedRectBackground(ui::ColorId color_id)
      : color_id_(color_id) {}
  ThemedFullyRoundedRectBackground(const ThemedFullyRoundedRectBackground&) =
      delete;
  ThemedFullyRoundedRectBackground& operator=(
      const ThemedFullyRoundedRectBackground&) = delete;
  ~ThemedFullyRoundedRectBackground() override = default;

  // views::Background:
  void OnViewThemeChanged(views::View* view) override {
    SetNativeControlColor(view->GetColorProvider()->GetColor(color_id_));
    view->SchedulePaint();
  }

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    // Draw a fully rounded rect filling in the view's local bounds.
    cc::PaintFlags paint;
    paint.setAntiAlias(true);

    SkColor color = get_color();
    if (!view->GetEnabled()) {
      color = ColorUtil::GetDisabledColor(color);
    }
    paint.setColor(color);

    const gfx::Rect bounds = view->GetLocalBounds();
    // Set the rounded corner radius to the half of the minimum dimension of
    // local bounds.
    const int rounded_corner_radius =
        std::min(bounds.width(), bounds.height()) / 2;
    canvas->DrawRoundRect(bounds, rounded_corner_radius, paint);
  }

 private:
  // Color Id of the background.
  const ui::ColorId color_id_;
};

// A `HighlightPathGenerator` that uses caller-supplied rounded rect corners.
class RoundedCornerHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  explicit RoundedCornerHighlightPathGenerator(
      const gfx::RoundedCornersF& corners)
      : corners_(corners) {}

  RoundedCornerHighlightPathGenerator(
      const RoundedCornerHighlightPathGenerator&) = delete;
  RoundedCornerHighlightPathGenerator& operator=(
      const RoundedCornerHighlightPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  std::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    return gfx::RRectF(rect, corners_);
  }

 private:
  // The user-supplied rounded rect corners.
  const gfx::RoundedCornersF corners_;
};

}  // namespace

// static
float StyleUtil::GetInkDropOpacity() {
  return DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()
             ? kDarkInkDropOpacity
             : kLightInkDropOpacity;
}

// static
std::unique_ptr<views::InkDrop> StyleUtil::CreateInkDrop(
    views::Button* host,
    bool highlight_on_hover,
    bool highlight_on_focus) {
  return views::InkDrop::CreateInkDropForFloodFillRipple(
      views::InkDrop::Get(host), highlight_on_hover, highlight_on_focus);
}

// static
std::unique_ptr<views::InkDropRipple> StyleUtil::CreateInkDropRipple(
    const gfx::Insets& insets,
    const views::View* host,
    SkColor background_color) {
  const std::pair<SkColor, float> base_color_and_opacity =
      AshColorProvider::Get()->GetInkDropBaseColorAndOpacity(background_color);
  return std::make_unique<views::FloodFillInkDropRipple>(
      const_cast<views::InkDropHost*>(views::InkDrop::Get(host)), host->size(),
      insets, views::InkDrop::Get(host)->GetInkDropCenterBasedOnLastEvent(),
      base_color_and_opacity.first, base_color_and_opacity.second);
}

// static
std::unique_ptr<views::InkDropHighlight> StyleUtil::CreateInkDropHighlight(
    const views::View* host,
    SkColor background_color) {
  const std::pair<SkColor, float> base_color_and_opacity =
      AshColorProvider::Get()->GetInkDropBaseColorAndOpacity(background_color);
  auto highlight = std::make_unique<views::InkDropHighlight>(
      gfx::SizeF(host->size()), base_color_and_opacity.first);
  highlight->set_visible_opacity(base_color_and_opacity.second);
  return highlight;
}

// static
void StyleUtil::SetRippleParams(views::View* host,
                                const gfx::Insets& insets,
                                SkColor background_color) {
  views::InkDrop::Get(host)->SetCreateRippleCallback(base::BindRepeating(
      &CreateInkDropRipple, insets, host, background_color));
}

// static
void StyleUtil::SetUpInkDropForButton(views::Button* button,
                                      const gfx::Insets& ripple_insets,
                                      bool highlight_on_hover,
                                      bool highlight_on_focus,
                                      SkColor background_color) {
  SetUpFocusRingForView(button);
  views::InkDropHost* const ink_drop = views::InkDrop::Get(button);
  ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
  button->SetHasInkDropActionOnClick(true);
  ink_drop->SetCreateInkDropCallback(base::BindRepeating(
      &CreateInkDrop, button, highlight_on_hover, highlight_on_focus));
  ink_drop->SetCreateRippleCallback(base::BindRepeating(
      &CreateInkDropRipple, ripple_insets, button, background_color));
  ink_drop->SetCreateHighlightCallback(
      base::BindRepeating(&CreateInkDropHighlight, button, background_color));
}

// static
void StyleUtil::ConfigureInkDropAttributes(views::View* view,
                                           uint32_t ripple_config_attributes,
                                           SkColor background_color) {
  const std::pair<SkColor, float> base_color_and_opacity =
      AshColorProvider::Get()->GetInkDropBaseColorAndOpacity(background_color);

  auto* host = views::InkDrop::Get(view);
  if (ripple_config_attributes & kBaseColor)
    host->SetBaseColor(base_color_and_opacity.first);

  if (ripple_config_attributes & kInkDropOpacity)
    host->SetVisibleOpacity(base_color_and_opacity.second);

  if (ripple_config_attributes & kHighlightOpacity)
    host->SetHighlightOpacity(base_color_and_opacity.second);
}

// static
views::FocusRing* StyleUtil::SetUpFocusRingForView(
    views::View* view,
    std::optional<int> halo_inset) {
  DCHECK(view);
  views::FocusRing::Install(view);
  views::FocusRing* focus_ring = views::FocusRing::Get(view);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetColorId(ui::kColorAshFocusRing);
  if (halo_inset)
    focus_ring->SetHaloInset(*halo_inset);
  return focus_ring;
}

// static
void StyleUtil::InstallRoundedCornerHighlightPathGenerator(
    views::View* view,
    const gfx::RoundedCornersF& corners) {
  views::HighlightPathGenerator::Install(
      view, std::make_unique<RoundedCornerHighlightPathGenerator>(corners));
}

// static
std::unique_ptr<views::Background>
StyleUtil::CreateThemedFullyRoundedRectBackground(ui::ColorId color_id) {
  return std::make_unique<ThemedFullyRoundedRectBackground>(color_id);
}

// static
std::unique_ptr<views::corewm::TooltipViewAura>
StyleUtil::CreateAshStyleTooltipView() {
  auto tooltip_view = std::make_unique<views::corewm::TooltipViewAura>();
  // Apply ash style background, border, and font.
  tooltip_view->SetBackground(views::CreateThemedRoundedRectBackground(
      ui::kColorTooltipBackground, kTooltipRoundedCornerRadius));
  tooltip_view->SetBorder(views::CreateEmptyBorder(kTooltipBorderInset));
  tooltip_view->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
      TypographyToken::kCrosAnnotation1));
  tooltip_view->SetMinLineHeight(kTooltipMinLineHeight);
  tooltip_view->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);
  tooltip_view->SetMaxLines(kTooltipMaxLines);
  return tooltip_view;
}

// static
ui::Shadow::ElevationToColorsMap StyleUtil::CreateShadowElevationToColorsMap(
    const ui::ColorProvider* color_provider) {
  ui::Shadow::ElevationToColorsMap colors_map;
  colors_map[SystemShadow::GetElevationFromType(
      SystemShadow::Type::kElevation4)] =
      std::make_pair(
          color_provider->GetColor(ui::kColorShadowValueKeyShadowElevationFour),
          color_provider->GetColor(
              ui::kColorShadowValueAmbientShadowElevationFour));
  colors_map[SystemShadow::GetElevationFromType(
      SystemShadow::Type::kElevation12)] =
      std::make_pair(color_provider->GetColor(
                         ui::kColorShadowValueKeyShadowElevationTwelve),
                     color_provider->GetColor(
                         ui::kColorShadowValueAmbientShadowElevationTwelve));
  colors_map[SystemShadow::GetElevationFromType(
      SystemShadow::Type::kElevation24)] =
      std::make_pair(
          color_provider->GetColor(
              ui::kColorShadowValueKeyShadowElevationTwentyFour),
          color_provider->GetColor(
              ui::kColorShadowValueAmbientShadowElevationTwentyFour));
  return colors_map;
}

}  // namespace ash
