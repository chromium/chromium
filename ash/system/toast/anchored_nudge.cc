// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/anchored_nudge.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/system/toast/system_nudge_view.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

AnchoredNudge::AnchoredNudge(const AnchoredNudgeData& nudge_data)
    : views::BubbleDialogDelegateView(nudge_data.anchor_view,
                                      nudge_data.arrow,
                                      views::BubbleBorder::NO_SHADOW),
      id_(nudge_data.id),
      nudge_click_callback_(std::move(nudge_data.nudge_click_callback)),
      nudge_dismiss_callback_(std::move(nudge_data.nudge_dimiss_callback)) {
  DCHECK(features::IsSystemNudgeV2Enabled());

  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_color(SK_ColorTRANSPARENT);
  set_margins(gfx::Insets());
  set_close_on_deactivate(false);
  SetLayoutManager(std::make_unique<views::FlexLayout>());
  system_nudge_view_ =
      AddChildView(std::make_unique<SystemNudgeView>(nudge_data));
}

AnchoredNudge::~AnchoredNudge() {
  if (!nudge_dismiss_callback_.is_null()) {
    std::move(nudge_dismiss_callback_).Run();
  }
}

views::ImageView* AnchoredNudge::GetImageView() {
  return system_nudge_view_->image_view();
}

const std::u16string& AnchoredNudge::GetBodyText() {
  CHECK(system_nudge_view_->body_label());
  return system_nudge_view_->body_label()->GetText();
}

const std::u16string& AnchoredNudge::GetTitleText() {
  CHECK(system_nudge_view_->title_label());
  return system_nudge_view_->title_label()->GetText();
}

views::LabelButton* AnchoredNudge::GetDismissButton() {
  return system_nudge_view_->dismiss_button();
}

views::LabelButton* AnchoredNudge::GetSecondButton() {
  return system_nudge_view_->second_button();
}

std::unique_ptr<views::NonClientFrameView>
AnchoredNudge::CreateNonClientFrameView(views::Widget* widget) {
  // Create the customized bubble border.
  std::unique_ptr<views::BubbleBorder> bubble_border =
      std::make_unique<views::BubbleBorder>(arrow(),
                                            views::BubbleBorder::NO_SHADOW);
  bubble_border->set_avoid_shadow_overlap(true);

  // TODO(b/279769899): Have insets adjust to shelf alignment, and set their
  // value from a param in AnchoredNudge constructor. The value 16 works for VC
  // tray icons because the icon is 8px away from the shelf top and we need an
  // extra 8 for spacing between the shelf and nudge.
  bubble_border->set_insets(gfx::Insets(16));

  auto frame = BubbleDialogDelegateView::CreateNonClientFrameView(widget);
  static_cast<views::BubbleFrameView*>(frame.get())
      ->SetBubbleBorder(std::move(bubble_border));
  return frame;
}

bool AnchoredNudge::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

bool AnchoredNudge::OnMouseDragged(const ui::MouseEvent& event) {
  return true;
}

void AnchoredNudge::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && !nudge_click_callback_.is_null()) {
    std::move(nudge_click_callback_).Run();
  }
}

void AnchoredNudge::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP: {
      if (!nudge_click_callback_.is_null()) {
        std::move(nudge_click_callback_).Run();
        event->SetHandled();
      }
      return;
    }
    default: {
      // Do nothing.
    }
  }
}

BEGIN_METADATA(AnchoredNudge, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace ash
