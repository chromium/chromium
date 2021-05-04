// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_VIEW_H_
#define ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace ash {

// Contains the views for the bubble version of the launcher.
class ASH_EXPORT AppListBubbleView : public views::BubbleDialogDelegateView {
 public:
  AppListBubbleView();
  AppListBubbleView(const AppListBubbleView&) = delete;
  AppListBubbleView& operator=(const AppListBubbleView&) = delete;
  ~AppListBubbleView() override;
};

}  // namespace ash

#endif  // ASH_APP_LIST_BUBBLE_APP_LIST_BUBBLE_VIEW_H_
