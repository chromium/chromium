// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/overflow_bubble_view_test_api.h"

#include "ash/shelf/overflow_bubble_view.h"
#include "ash/shelf/shelf_view.h"

namespace ash {

OverflowBubbleViewTestAPI::OverflowBubbleViewTestAPI(
    OverflowBubbleView* bubble_view)
    : bubble_view_(bubble_view) {}

OverflowBubbleViewTestAPI::~OverflowBubbleViewTestAPI() = default;

views::BubbleFrameView* OverflowBubbleViewTestAPI::GetBubbleFrameView() {
  return bubble_view_->GetBubbleFrameView();
}

}  // namespace ash
