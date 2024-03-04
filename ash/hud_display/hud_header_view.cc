// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/hud_header_view.h"

#include <utility>

#include "ash/hud_display/hud_constants.h"
#include "ash/hud_display/hud_display.h"
#include "ash/hud_display/hud_properties.h"
#include "ash/hud_display/solid_source_background.h"
#include "ash/hud_display/tab_strip.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace hud_display {
namespace {

// Draws bottom left rounded background triangle.
class BottomLeftOuterBackground : public views::Background {
 public:
  // Background will have left bottom rounded corner with |top_rounding_radius|.
  BottomLeftOuterBackground(SkColor color, SkScalar top_rounding_radius)
      : inner_radius_(top_rounding_radius) {
    SetNativeControlColor(color);
  }

  BottomLeftOuterBackground(const BottomLeftOuterBackground&) = delete;
  BottomLeftOuterBackground& operator=(const BottomLeftOuterBackground&) =
      delete;

  ~BottomLeftOuterBackground() override = default;

  // views::Background
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    const SkScalar circle_size = inner_radius_ * 2;
    const SkScalar bottom_edge = view->height();

    SkPath path;
    path.moveTo(0, bottom_edge);
    /* |false| will draw straight line to the start of the arc */
    path.arcTo({0, bottom_edge - circle_size, circle_size, bottom_edge}, 90, 90,
               false);
    path.lineTo(0, bottom_edge);

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setBlendMode(SkBlendMode::kSrc);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(get_color());
    canvas->DrawPath(path, flags);
  }

 private:
  SkScalar inner_radius_;
};

// ImageButton with underline
class SettingsButton : public views::ImageButton {
  METADATA_HEADER(SettingsButton, views::ImageButton)

 public:
  explicit SettingsButton(views::Button::PressedCallback callback)
      : views::ImageButton(std::move(callback)) {
    SetImageModel(
        views::Button::ButtonState::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(vector_icons::kSettingsIcon,
                                       kHUDDefaultColor, kHUDSettingsIconSize));
    SetBorder(views::CreateEmptyBorder(kHUDSettingsIconBorder));
    SetProperty(kHUDClickHandler, HTCLIENT);

    SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  }

  SettingsButton(const SettingsButton&) = delete;
  SettingsButton& operator=(const SettingsButton&) = delete;

  ~SettingsButton() override = default;

 protected:
  // ImageButton
  void PaintButtonContents(gfx::Canvas* canvas) override {
    views::ImageButton::PaintButtonContents(canvas);

    SkPath path;
    path.moveTo(0, height());
    path.lineTo(height(), width());

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setBlendMode(SkBlendMode::kSrc);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1);
    flags.setColor(kHUDDefaultColor);
    canvas->DrawPath(path, flags);
  }
};

BEGIN_METADATA(SettingsButton)
END_METADATA

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// HUDHeaderView

BEGIN_METADATA(HUDHeaderView)
END_METADATA

HUDHeaderView::HUDHeaderView(HUDDisplayView* hud) {
  // Header should have background under the buttons area only.
  //
  // There is additional "padding" view an the end to draw bottom inner
  // rounded piece of background.

  views::View* header_buttons =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  header_buttons->SetBackground(std::make_unique<SolidSourceBackground>(
      kHUDBackground, kHUDTabOverlayCornerRadius));

  // Header does not have margin between header and data.
  // Data has its top margin (kHUDGraphsInset).
  header_buttons->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kHUDInset, kHUDInset, 0, kHUDInset)));

  // Add buttons and tab strip.
  header_buttons
      ->AddChildView(std::make_unique<SettingsButton>(base::BindRepeating(
          &HUDDisplayView::OnSettingsToggle, base::Unretained(hud))))
      ->SetTooltipText(u"Trigger Ash HUD Settings");
  tab_strip_ = header_buttons->AddChildView(std::make_unique<HUDTabStrip>(hud));

  // Padding will take the rest of the header and draw bottom inner left
  // background padding.
  views::View* padding = AddChildView(std::make_unique<views::View>());
  padding->SetBackground(std::make_unique<BottomLeftOuterBackground>(
      kHUDBackground, kHUDTabOverlayCornerRadius));
  SetFlexForView(padding, 1);
}

HUDHeaderView::~HUDHeaderView() = default;

}  // namespace hud_display
}  // namespace ash
