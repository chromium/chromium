// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_DICTATION_BUBBLE_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_DICTATION_BUBBLE_VIEW_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ui {
struct AXNodeData;
}  // namespace ui

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

// View for the Dictation bubble.
class ASH_EXPORT DictationBubbleView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(DictationBubbleView);
  DictationBubbleView();
  DictationBubbleView(const DictationBubbleView&) = delete;
  DictationBubbleView& operator=(const DictationBubbleView&) = delete;
  ~DictationBubbleView() override;

  // Updates the visibility and text content of `label_`. Also updates the size
  // of this view.
  void Update(const absl::optional<std::u16string>& text);

  // views::BubbleDialogDelegateView:
  void Init() override;
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  std::u16string GetTextForTesting();

 private:
  std::unique_ptr<views::ImageView> CreateIcon();
  std::unique_ptr<views::Label> CreateLabel(const std::u16string& text);

  // Owned by the views hierarchy.
  views::ImageView* image_view_ = nullptr;
  views::Label* label_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   DictationBubbleView,
                   views::BubbleDialogDelegateView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::DictationBubbleView)

#endif  // ASH_SYSTEM_ACCESSIBILITY_DICTATION_BUBBLE_VIEW_H_
