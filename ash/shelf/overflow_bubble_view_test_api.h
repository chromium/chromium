// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_OVERFLOW_BUBBLE_VIEW_TEST_API_H_
#define ASH_SHELF_OVERFLOW_BUBBLE_VIEW_TEST_API_H_

#include "base/macros.h"

namespace views {
class BubbleFrameView;
}

namespace ash {
class OverflowBubbleView;

class OverflowBubbleViewTestAPI {
 public:
  explicit OverflowBubbleViewTestAPI(OverflowBubbleView* bubble_view);
  ~OverflowBubbleViewTestAPI();

  // Returns the NonClientFrameView for the bubble.
  views::BubbleFrameView* GetBubbleFrameView();

 private:
  OverflowBubbleView* bubble_view_;

  DISALLOW_COPY_AND_ASSIGN(OverflowBubbleViewTestAPI);
};

}  // namespace ash

#endif  // ASH_SHELF_OVERFLOW_BUBBLE_VIEW_TEST_API_H_
