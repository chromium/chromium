// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_PREVIEW_BUBBLE_H_
#define ASH_PICKER_VIEWS_PICKER_PREVIEW_BUBBLE_H_

#include "ash/ash_export.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"

namespace ash {

class ASH_EXPORT PickerPreviewBubbleView
    : public views::BubbleDialogDelegateView {
  METADATA_HEADER(PickerPreviewBubbleView, views::BubbleDialogDelegateView)

 public:
  explicit PickerPreviewBubbleView(views::View* anchor_view);
  PickerPreviewBubbleView(const PickerPreviewBubbleView&) = delete;
  PickerPreviewBubbleView& operator=(const PickerPreviewBubbleView&) = delete;

  // BubbleDialogDelegateView overrides
  void OnThemeChanged() override;

  void Close();
};
}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_PREVIEW_BUBBLE_H_
