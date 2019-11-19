// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_HIGHLIGHT_VIEW_TEST_API_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_HIGHLIGHT_VIEW_TEST_API_H_

#include "ash/wm/splitview/split_view_highlight_view.h"
#include "base/macros.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// Use the api in this class to test SplitViewHighlightView.
class SplitViewHighlightViewTestApi {
 public:
  explicit SplitViewHighlightViewTestApi(
      SplitViewHighlightView* highlight_view);
  ~SplitViewHighlightViewTestApi();

  views::View* GetLeftTopView();
  views::View* GetRightBottomView();
  views::View* GetMiddleView();

 private:
  SplitViewHighlightView* highlight_view_;

  DISALLOW_COPY_AND_ASSIGN(SplitViewHighlightViewTestApi);
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_HIGHLIGHT_VIEW_TEST_API_H_
