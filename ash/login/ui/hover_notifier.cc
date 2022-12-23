// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/hover_notifier.h"

#include "ui/views/view.h"

namespace ash {

HoverNotifier::HoverNotifier(views::View* target_view, const OnHover& on_hover)
    : target_view_(target_view), on_hover_(on_hover) {
  target_view_->AddPreTargetHandler(this);
  on_hover_.Run(target_view_->IsMouseHovered());
}

HoverNotifier::~HoverNotifier() {
  target_view_->RemovePreTargetHandler(this);
}

void HoverNotifier::OnEvent(ui::Event* event) {
  bool has_hover = target_view_->IsMouseHovered();
  if (has_hover == had_hover_) {
    return;
  }

  had_hover_ = has_hover;
  on_hover_.Run(has_hover);
}

}  // namespace ash
