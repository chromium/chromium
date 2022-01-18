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

enum class DictationBubbleIconType;

// View for the Dictation bubble.
class ASH_EXPORT DictationBubbleView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(DictationBubbleView);
  DictationBubbleView();
  DictationBubbleView(const DictationBubbleView&) = delete;
  DictationBubbleView& operator=(const DictationBubbleView&) = delete;
  ~DictationBubbleView() override;

  // Updates the visibility of all child views. Also updates the text content
  // of `label_` and updates the size of this view.
  void Update(DictationBubbleIconType icon,
              const absl::optional<std::u16string>& text);

  // views::BubbleDialogDelegateView:
  void Init() override;
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  std::u16string GetTextForTesting();
  bool IsStandbyImageVisibleForTesting();
  bool IsMacroSucceededImageVisibleForTesting();
  bool IsMacroFailedImageVisibleForTesting();

 private:
  std::unique_ptr<views::Label> CreateLabel(const std::u16string& text);

  // Owned by the views hierarchy.
  // An image that is shown when Dictation is standing by.
  views::ImageView* standby_image_ = nullptr;
  // An image that is shown when a macro is successfully run.
  views::ImageView* macro_succeeded_image_ = nullptr;
  // An image that is shown when a macro fails to run.
  views::ImageView* macro_failed_image_ = nullptr;
  // A label that displays non-final speech results.
  views::Label* label_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   DictationBubbleView,
                   views::BubbleDialogDelegateView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::DictationBubbleView)

#endif  // ASH_SYSTEM_ACCESSIBILITY_DICTATION_BUBBLE_VIEW_H_
