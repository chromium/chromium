// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_focusable_view.h"

#include "ui/views/view.h"

namespace ash {

OverviewItemBase* OverviewFocusableView::GetOverviewItem() {
  return nullptr;
}

bool OverviewFocusableView::MaybeActivateFocusedViewOnOverviewExit(
    OverviewSession* overview_session) {
  return false;
}

void OverviewFocusableView::SetFocused(bool visible) {
  if (visible == is_focused_) {
    return;
  }

  is_focused_ = visible;
  if (is_focused_) {
    OnFocusableViewFocused();
  } else {
    OnFocusableViewBlurred();
  }
}

gfx::Point OverviewFocusableView::GetMagnifierFocusPointInScreen() {
  return GetView()->GetBoundsInScreen().CenterPoint();
}

}  // namespace ash
