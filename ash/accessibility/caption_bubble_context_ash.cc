// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/caption_bubble_context_ash.h"

#include "ash/shell.h"
#include "ash/wm/work_area_insets.h"

namespace ash {
namespace captions {

CaptionBubbleContextAsh::CaptionBubbleContextAsh() = default;

CaptionBubbleContextAsh::~CaptionBubbleContextAsh() = default;

absl::optional<gfx::Rect> CaptionBubbleContextAsh::GetBounds() const {
  return WorkAreaInsets::ForWindow(Shell::GetRootWindowForNewWindows())
      ->user_work_area_bounds();
}

bool CaptionBubbleContextAsh::IsActivatable() const {
  return false;
}

}  // namespace captions
}  // namespace ash
