// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_edit_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace arc::input_overlay {
namespace {
// UI specs.
constexpr SkColor kBackgroundColor = gfx::kGoogleGrey200;
constexpr int kDotsIconSize = 20;
constexpr int kDotsButtonSize = 32;
constexpr SkColor kDotsIconColor = gfx::kGoogleGrey900;
constexpr int kFocusRingStrokeWidth = 4;

}  // namespace

class ActionEditButton::CircleBackground : public views::Background {
 public:
  explicit CircleBackground(SkColor color) { SetNativeControlColor(color); }
  ~CircleBackground() override = default;

  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    int radius = view->bounds().width() / 2;
    gfx::PointF center(radius, radius);
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(get_color());
    canvas->DrawCircle(center, radius, flags);
  }
};

ActionEditButton::ActionEditButton(PressedCallback callback)
    : views::ImageButton(std::move(callback)) {
  auto dots_icon = gfx::CreateVectorIcon(ash::kVerticalDotsIcon, kDotsIconSize,
                                         kDotsIconColor);
  SetImage(views::Button::STATE_NORMAL, dots_icon);
  SetAccessibleName(base::UTF8ToUTF16(GetClassName()));
  SetSize(gfx::Size(kDotsButtonSize, kDotsButtonSize));
  SetBackground(std::make_unique<CircleBackground>(kBackgroundColor));
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  ash::StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                        /*highlight_on_hover=*/true,
                                        /*highlight_on_focus=*/true);

  views::FocusRing* focus_ring = views::FocusRing::Get(this);
  auto* color_provider = ash::AshColorProvider::Get();
  DCHECK(focus_ring);
  DCHECK(color_provider);
  if (!focus_ring || !color_provider) {
    return;
  }
  focus_ring->SetColorId(ui::kColorAshFocusRing);
  focus_ring->SetHaloThickness(kFocusRingStrokeWidth);
  focus_ring->SetPathGenerator(
      std::make_unique<views::CircleHighlightPathGenerator>(gfx::Insets(0)));
}

ActionEditButton::~ActionEditButton() = default;

}  // namespace arc::input_overlay
