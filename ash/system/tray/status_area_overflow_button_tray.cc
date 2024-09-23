// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/status_area_overflow_button_tray.h"

#include <memory>

#include "ash/constants/tray_background_view_catalog.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/border.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr int kAnimationDurationMs = 250;
constexpr int kTrayWidth = kStatusAreaOverflowButtonSize.width();
constexpr int kTrayHeight = kStatusAreaOverflowButtonSize.height();

}  // namespace

StatusAreaOverflowButtonTray::IconView::IconView()
    : slide_animation_(std::make_unique<gfx::SlideAnimation>(this)) {
  slide_animation_->Reset(1.0);
  slide_animation_->SetTweenType(gfx::Tween::EASE_OUT);
  slide_animation_->SetSlideDuration(base::Milliseconds(kAnimationDurationMs));

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetImage(ui::ImageModel::FromVectorIcon(
      kOverflowShelfRightIcon,
      static_cast<ui::ColorId>(kColorAshIconColorPrimary)));

  // The icon has the default size. Use the size to calculate the border so that
  // the icon is placed at the center of the ink drop.
  const int size = gfx::GetDefaultSizeOfVectorIcon(kOverflowShelfRightIcon);
  const int vertical_padding = (kTrayHeight - size) / 2;
  const int horizontal_padding = (kTrayWidth - size) / 2;
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(vertical_padding, horizontal_padding)));

  UpdateRotation();
}

StatusAreaOverflowButtonTray::IconView::~IconView() {}

void StatusAreaOverflowButtonTray::IconView::ToggleState(State state) {
  slide_animation_->End();
  if (state == CLICK_TO_EXPAND)
    slide_animation_->Show();
  else if (state == CLICK_TO_COLLAPSE)
    slide_animation_->Hide();

  // TODO(b/260253147): Currently, the collapse/expand animation is not fully
  // spec'd, so skip it for now.
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
  gfx::Vector2d center(kTrayWidth / 2.0, kTrayHeight / 2.0);
  transform.Translate(center);
  transform.RotateAboutZAxis(180.0 * progress);
  transform.Translate(gfx::Vector2d(-center.x(), -center.y()));

  SetTransform(transform);
}

BEGIN_METADATA(StatusAreaOverflowButtonTray, IconView)
END_METADATA

StatusAreaOverflowButtonTray::StatusAreaOverflowButtonTray(Shelf* shelf)
    : TrayBackgroundView(
          shelf,
          TrayBackgroundViewCatalogName::kStatusAreaOverflowButton),
      icon_(tray_container()->AddChildView(std::make_unique<IconView>())) {
  SetCallback(base::BindRepeating(
      &StatusAreaOverflowButtonTray::OnButtonPressed, base::Unretained(this)));

  set_use_bounce_in_animation(false);
  // https://b/293650341 `TrayBackgroundView` sets the layer opacity to 0.0 when
  // they're not visible so it can animate the opacity when the visibility
  // changes. Since this view bypasses that logic we need to work around this by
  // setting the opacity ourselves.
  layer()->SetOpacity(1.0);
}

StatusAreaOverflowButtonTray::~StatusAreaOverflowButtonTray() {}

void StatusAreaOverflowButtonTray::ClickedOutsideBubble(
    const ui::LocatedEvent& event) {}

std::u16string StatusAreaOverflowButtonTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(
      state_ == CLICK_TO_COLLAPSE ? IDS_ASH_STATUS_AREA_OVERFLOW_BUTTON_COLLAPSE
                                  : IDS_ASH_STATUS_AREA_OVERFLOW_BUTTON_EXPAND);
}

void StatusAreaOverflowButtonTray::HandleLocaleChange() {}

void StatusAreaOverflowButtonTray::HideBubbleWithView(
    const TrayBubbleView* bubble_view) {}

void StatusAreaOverflowButtonTray::HideBubble(
    const TrayBubbleView* bubble_view) {}

void StatusAreaOverflowButtonTray::Initialize() {
  TrayBackgroundView::Initialize();
  SetVisiblePreferred(false);
}

void StatusAreaOverflowButtonTray::SetVisiblePreferred(bool visible_preferred) {
  // The visibility of the overflow tray button is controlled by the
  // `StatusAreaWidget`, so we bypass all default visibility logic from
  // `TrayBackgroundView`.
  views::View::SetVisible(visible_preferred);
  TrackVisibilityUMA(visible_preferred);
}

void StatusAreaOverflowButtonTray::UpdateAfterStatusAreaCollapseChange() {
  // The visibility of the overflow tray button is controlled by the
  // `StatusAreaWidget`, so we bypass all default visibility logic from
  // `TrayBackgroundView`.
}

void StatusAreaOverflowButtonTray::OnButtonPressed(const ui::Event& event) {
  state_ = state_ == CLICK_TO_COLLAPSE ? CLICK_TO_EXPAND : CLICK_TO_COLLAPSE;
  icon_->ToggleState(state_);
  shelf()->GetStatusAreaWidget()->UpdateCollapseState();
}

void StatusAreaOverflowButtonTray::ResetStateToCollapsed() {
  state_ = CLICK_TO_EXPAND;
  icon_->ToggleState(state_);
}

BEGIN_METADATA(StatusAreaOverflowButtonTray);
END_METADATA

}  // namespace ash
