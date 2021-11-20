// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/auth_icon_view.h"

#include "ash/login/ui/horizontal_image_sequence_animation_decoder.h"
#include "ash/style/ash_color_provider.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

namespace {

constexpr int kAuthIconSizeDp = 32;

SkColor GetColor(AuthIconView::Color color) {
  switch (color) {
    case AuthIconView::Color::kPrimary:
      return AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary);
    case AuthIconView::Color::kDisabled:
      return AshColorProvider::Get()->GetDisabledColor(
          GetColor(AuthIconView::Color::kPrimary));
    case AuthIconView::Color::kError:
      // TODO(crbug.com/1233614): Either find a system color to match the color
      // in the Fingerprint animation png sequence, or upload new png files with
      // the right color.
      return AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorAlert);
  }
}

}  // namespace

AuthIconView::AuthIconView()
    : AnimatedRoundedImageView(gfx::Size(kAuthIconSizeDp, kAuthIconSizeDp),
                               /*corner_radius=*/0) {}

AuthIconView::~AuthIconView() = default;

void AuthIconView::SetIcon(const gfx::VectorIcon& icon, Color color) {
  SetImage(gfx::CreateVectorIcon(icon, kAuthIconSizeDp, GetColor(color)));
}

void AuthIconView::SetAnimation(int animation_resource_id,
                                base::TimeDelta duration,
                                int num_frames) {
  SetAnimationDecoder(
      std::make_unique<HorizontalImageSequenceAnimationDecoder>(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              animation_resource_id),
          duration, num_frames),
      AnimatedRoundedImageView::Playback::kSingle);
}

// views::View:
void AuthIconView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::ET_GESTURE_TAP &&
      event->type() != ui::ET_GESTURE_TAP_DOWN)
    return;

  if (on_tap_or_click_callback_) {
    on_tap_or_click_callback_.Run();
  }
}

// views::View:
bool AuthIconView::OnMousePressed(const ui::MouseEvent& event) {
  if (on_tap_or_click_callback_) {
    on_tap_or_click_callback_.Run();
    return true;
  }
  return false;
}

}  // namespace ash
