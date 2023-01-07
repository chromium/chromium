// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/hud_header_view.h"

#include "ash/hud_display/hud_constants.h"
#include "ash/hud_display/hud_display.h"
#include "ash/hud_display/hud_properties.h"
#include "ash/hud_display/solid_source_background.h"
#include "ash/hud_display/tab_strip.h"
#include "base/bind.h"
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
#include "ui/views/layout/layout_manager.h"

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
 public:
  METADATA_HEADER(SettingsButton);

  explicit SettingsButton(views::Button::PressedCallback callback)
      : views::ImageButton(callback) {
    SetImage(views::Button::ButtonState::STATE_NORMAL,
             gfx::CreateVectorIcon(vector_icons::kSettingsIcon,
                                   kHUDSettingsIconSize, kHUDDefaultColor));
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

BEGIN_METADATA(SettingsButton, views::ImageButton)
END_METADATA

// Basically FillLayout that matches host size to the given data view.
// Padding will take the rest of the host view to the right.
class HUDHeaderLayout : public views::LayoutManager {
 public:
  HUDHeaderLayout(const views::View* data_view, views::View* padding)
      : data_view_(data_view), padding_(padding) {}

  HUDHeaderLayout(const HUDHeaderLayout&) = delete;
  HUDHeaderLayout& operator=(const HUDHeaderLayout&) = delete;

  ~HUDHeaderLayout() override = default;

  // views::LayoutManager:
  void Layout(views::View* host) override;
  gfx::Size GetPreferredSize(const views::View* host) const override;

 private:
  const views::View* data_view_;
  views::View* padding_;
};

gfx::Size HUDHeaderLayout::GetPreferredSize(const views::View* host) const {
  return data_view_->GetPreferredSize() + padding_->GetPreferredSize();
}

void HUDHeaderLayout::Layout(views::View* host) {
  // Assume there are only 3 child views (data, background and padding).
  DCHECK_EQ(host->children().size(), 3U);

  const gfx::Size preferred_size = data_view_->GetPreferredSize();

  for (auto* child : host->children()) {
    if (child != padding_) {
      child->SetPosition({0, 0});
      child->SetSize(preferred_size);
    }
  }
  // Layout padding
  padding_->SetPosition({preferred_size.width(), 0});
  padding_->SetSize({host->width() - preferred_size.width(), host->height()});
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// HUDHeaderView

BEGIN_METADATA(HUDHeaderView, views::View)
END_METADATA

HUDHeaderView::HUDHeaderView(HUDDisplayView* hud) {
  // Header is rendered as horizontal container with three children:
  // background, buttons container and padding.

  // Header should have background under the buttons area only.
  // To achieve this we have buttons container view to calculate total width,
  // and special layout manager that matches size of the background view to the
  // size of the buttons.
  //
  // There is additional "padding" view an the end to draw bottom inner
  // rounded piece of background.

  views::View* header_background =
      AddChildView(std::make_unique<views::View>());
  header_background->SetBackground(std::make_unique<SolidSourceBackground>(
      kHUDBackground, kHUDTabOverlayCornerRadius));

  views::View* header_buttons = AddChildView(std::make_unique<views::View>());
  header_buttons
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal))
      ->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kStretch);
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

  SetLayoutManager(std::make_unique<HUDHeaderLayout>(header_buttons, padding));
}

HUDHeaderView::~HUDHeaderView() = default;

}  // namespace hud_display
}  // namespace ash
