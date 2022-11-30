// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/ui/assistive_accessibility_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ui {
namespace ime {

const gfx::Rect kWindowAnchorRect = gfx::Rect(-100000, -100000, 0, 0);

AssistiveAccessibilityView::AssistiveAccessibilityView(gfx::NativeView parent) {
  DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
  SetCanActivate(false);
  DCHECK(parent);
  set_parent_window(parent);
  set_margins(gfx::Insets());
  set_title_margins(gfx::Insets());
  set_shadow(views::BubbleBorder::NO_SHADOW);

  accessibility_label_ =
      AddChildView(std::make_unique<SuggestionAccessibilityLabel>());
  accessibility_label_->SetLineHeight(0);

  views::Widget* const widget =
      BubbleDialogDelegate::CreateBubble(base::WrapUnique(this));
  // Set the window size to 0 and put it outside screen to make sure users don't
  // see it.
  widget->SetSize(gfx::Size(0, 0));
  SetAnchorRect(kWindowAnchorRect);
  widget->Show();
}

AssistiveAccessibilityView::AssistiveAccessibilityView() = default;
AssistiveAccessibilityView::~AssistiveAccessibilityView() = default;

void AssistiveAccessibilityView::Announce(const std::u16string& message) {
  DCHECK(accessibility_label_);
  if (message.empty())
    return;
  accessibility_label_->Announce(message);
}

BEGIN_METADATA(AssistiveAccessibilityView, views::View)
END_METADATA

}  // namespace ime
}  // namespace ui
