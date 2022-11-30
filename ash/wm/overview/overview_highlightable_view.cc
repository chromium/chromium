// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_highlightable_view.h"

#include "ui/views/view.h"

namespace ash {

bool OverviewHighlightableView::MaybeActivateHighlightedViewOnOverviewExit(
    OverviewSession* overview_session) {
  return false;
}

void OverviewHighlightableView::SetHighlightVisibility(bool visible) {
  if (visible == is_highlighted_)
    return;

  is_highlighted_ = visible;
  if (is_highlighted_)
    OnViewHighlighted();
  else
    OnViewUnhighlighted();
}

gfx::Point OverviewHighlightableView::GetMagnifierFocusPointInScreen() {
  return GetView()->GetBoundsInScreen().CenterPoint();
}

}  // namespace ash
