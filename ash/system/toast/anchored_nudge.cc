// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/anchored_nudge.h"

#include "ash/public/cpp/shelf_types.h"
#include "ash/style/system_toast_style.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/layout/flex_layout.h"

namespace ash {

AnchoredNudge::AnchoredNudge(Delegate* delegate,
                             const AnchoredNudgeData& nudge_data)
    : views::BubbleDialogDelegateView(nudge_data.anchor_view,
                                      nudge_data.arrow,
                                      views::BubbleBorder::NO_SHADOW),
      delegate_(delegate),
      id_(nudge_data.id) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_color(SK_ColorTRANSPARENT);
  set_margins(gfx::Insets());
  set_close_on_deactivate(false);
  SetLayoutManager(std::make_unique<views::FlexLayout>());
  toast_contents_view_ = AddChildView(std::make_unique<SystemToastStyle>(
      nudge_data.dismiss_callback, nudge_data.text, nudge_data.dismiss_text));
}

AnchoredNudge::~AnchoredNudge() {
  // Make sure `delegate_` knows that the nudge has been closed, for cases where
  // the nudge wasn't closed through the manager (e.g. widget destroyed by
  // test).
  delegate_->OnNudgeClosed(id_);
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

BEGIN_METADATA(AnchoredNudge, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace ash
