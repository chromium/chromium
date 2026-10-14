// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_HOVER_TEXT_BUBBLE_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_HOVER_TEXT_BUBBLE_VIEW_H_

#include <array>
#include <string>
#include <utility>

#include "ash/ash_export.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/font_list.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/widget/widget.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

// View for the Hover Text floating bubble.
class ASH_EXPORT HoverTextBubbleView : public views::View,
                                       public views::BubbleDialogDelegate {
  METADATA_HEADER(HoverTextBubbleView, views::View)

 public:
  HoverTextBubbleView();
  HoverTextBubbleView(const HoverTextBubbleView&) = delete;
  HoverTextBubbleView& operator=(const HoverTextBubbleView&) = delete;
  ~HoverTextBubbleView() override;

  void Update(const std::u16string& text,
              double font_scale,
              HoverTextFontStyle font_style);

  std::u16string GetTextForTesting() const;
  const gfx::FontList& GetFontListForTesting() const;
  int GetFontSizePixelsForTesting() const;
  std::string GetFontFamilyForTesting() const;
  views::Label* label_for_testing() const { return label_; }

  // views::BubbleDialogDelegate:
  views::View* GetContentsView() override;
  void Init() override;

  // views::View:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

 private:
  static constexpr size_t kNumFontStyles =
      std::to_underlying(HoverTextFontStyle::kMaxValue) + 1;

  raw_ptr<views::Label> label_ = nullptr;
  int last_font_size_px_ = -1;
  HoverTextFontStyle last_font_style_ = HoverTextFontStyle::kSystemSans;
  std::array<int, kNumFontStyles> resolved_candidate_index_by_style_;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, HoverTextBubbleView, views::View)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::HoverTextBubbleView)

#endif  // ASH_SYSTEM_ACCESSIBILITY_HOVER_TEXT_BUBBLE_VIEW_H_
