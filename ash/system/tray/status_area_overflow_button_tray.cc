// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/status_area_overflow_button_tray.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/view.h"

namespace ash {

namespace {
constexpr int kAnimationDurationMs = 250;
}  // namespace

StatusAreaOverflowButtonTray::IconView::IconView()
    : slide_animation_(std::make_unique<gfx::SlideAnimation>(this)) {
  slide_animation_->Reset(1.0);
  slide_animation_->SetTweenType(gfx::Tween::EASE_OUT);
  slide_animation_->SetSlideDuration(
      base::TimeDelta::FromMilliseconds(kAnimationDurationMs));

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  gfx::ImageSkia image = gfx::CreateVectorIcon(
      kOverflowShelfLeftIcon, ShelfConfig::Get()->shelf_icon_color());
  SetImage(image);

  const int vertical_padding = (kTrayItemSize - image.height()) / 2;
  const int horizontal_padding = (kTrayItemSize - image.width()) / 2;
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(vertical_padding, horizontal_padding)));

  UpdateRotation();
}

StatusAreaOverflowButtonTray::IconView::~IconView() {}

void StatusAreaOverflowButtonTray::IconView::ToggleState(State state) {
  slide_animation_->End();
  if (state == CLICK_TO_EXPAND)
    slide_animation_->Show();
  else if (state == CLICK_TO_COLLAPSE)
    slide_animation_->Hide();

  // TODO(tengs): Currently, the collpase/expand animation is not fully spec'd,
  // so skip it for now.
  slide_animation_->End();
}

void StatusAreaOverflowButtonTray::IconView::AnimationEnded(
    const gfx::Animation* animation) {
  UpdateRotation();
}

void StatusAreaOverflowButtonTray::IconView::AnimationProgressed(
    const gfx::Animation* animation) {
  UpdateRotation();
}

void StatusAreaOverflowButtonTray::IconView::AnimationCanceled(
    const gfx::Animation* animation) {
  UpdateRotation();
}

void StatusAreaOverflowButtonTray::IconView::UpdateRotation() {
  double progress = slide_animation_->GetCurrentValue();

  gfx::Transform transform;
  double center = kTrayItemSize / 2.0;
  transform.Translate(gfx::Vector2d(center, center));
  transform.RotateAboutZAxis(180.0 * progress);
  transform.Translate(gfx::Vector2d(-center, -center));

  SetTransform(transform);
}

StatusAreaOverflowButtonTray::StatusAreaOverflowButtonTray(Shelf* shelf)
    : TrayBackgroundView(shelf), icon_(new IconView()) {
  tray_container()->AddChildView(icon_);
}

StatusAreaOverflowButtonTray::~StatusAreaOverflowButtonTray() {}

void StatusAreaOverflowButtonTray::ClickedOutsideBubble() {}

base::string16 StatusAreaOverflowButtonTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      state_ == CLICK_TO_COLLAPSE ? IDS_ASH_STATUS_AREA_OVERFLOW_BUTTON_COLLAPSE
                                  : IDS_ASH_STATUS_AREA_OVERFLOW_BUTTON_EXPAND);
}

void StatusAreaOverflowButtonTray::HideBubbleWithView(
    const TrayBubbleView* bubble_view) {}

void StatusAreaOverflowButtonTray::Initialize() {
  TrayBackgroundView::Initialize();

  // TODO(tengs): Make this tray button visible when the device is in tablet
  // mode and the status area width exceeds the maximum desirable width.
  SetVisiblePreferred(false);
}

bool StatusAreaOverflowButtonTray::PerformAction(const ui::Event& event) {
  // TODO(tengs): Toggle the visibility of other trays based on the button state
  // and the amount of available width in the shelf.

  state_ = state_ == CLICK_TO_COLLAPSE ? CLICK_TO_EXPAND : CLICK_TO_COLLAPSE;
  icon_->ToggleState(state_);

  return false;
}

const char* StatusAreaOverflowButtonTray::GetClassName() const {
  return "StatusAreaOverflowButtonTray";
}

}  // namespace ash
