// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_BUBBLE_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_BUBBLE_VIEW_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

enum class MouseKeysBubbleIconType;

// View for the MouseKeys bubble.
class ASH_EXPORT MouseKeysBubbleView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(MouseKeysBubbleView, views::BubbleDialogDelegateView)

 public:
  MouseKeysBubbleView();
  MouseKeysBubbleView(const MouseKeysBubbleView&) = delete;
  MouseKeysBubbleView& operator=(const MouseKeysBubbleView&) = delete;
  ~MouseKeysBubbleView() override;

  // Updates the visibility of all child views. Also updates the text content
  // of `label_` and updates the size of this view.
  void Update(MouseKeysBubbleIconType icon,
              const std::optional<std::u16string>& text);

  // views::BubbleDialogDelegateView:
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;

  std::u16string_view GetTextForTesting() const;
  views::ImageView* GetMouseButtonChangeIconForTesting() const;
  views::ImageView* GetMouseDragIconForTesting() const;

 private:
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::ImageView> mouse_button_change_icon_ = nullptr;
  raw_ptr<views::ImageView> mouse_drag_icon_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   MouseKeysBubbleView,
                   views::BubbleDialogDelegateView)
END_VIEW_BUILDER
}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::MouseKeysBubbleView)

#endif  // ASH_SYSTEM_ACCESSIBILITY_MOUSE_KEYS_MOUSE_KEYS_BUBBLE_VIEW_H_
