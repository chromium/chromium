// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_LABEL_TEXTFIELD_H_
#define ASH_WM_DESKS_LABEL_TEXTFIELD_H_

#include "ash/wm/wm_highlight_item_border.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

// Defines a textfield styled so when it's not focused, it looks like a normal
// label. It provides an API to elide long names.
// TODO(minch): Unify this to ash/style.
class LabelTextfield : public views::Textfield {
 public:
  METADATA_HEADER(LabelTextfield);

  LabelTextfield();
  LabelTextfield(const LabelTextfield&) = delete;
  LabelTextfield& operator=(const LabelTextfield&) = delete;
  ~LabelTextfield() override;

  // The border radius on the text field.
  static constexpr size_t kLabelTextfieldBorderRadius = 4;

  void SetTextAndElideIfNeeded(const std::u16string& text);

  // If this view has focus, make the view's border visible and change
  // background to its active color. If it doesn't have focus, hide the view's
  // border and change background to its default color.
  void UpdateViewAppearance();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnThemeChanged() override;
  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override;

 protected:
  // Owned by this View via `View::border_`. This is just a convenient pointer
  // to it.
  WmHighlightItemBorder* border_ptr_;

  // Full text without being elided.
  std::u16string full_text_;

 private:
  void UpdateBorderState();

  // Returns the background color for this view based on whether it has focus
  // and if the mouse is entering/exiting the view.
  SkColor GetBackgroundColor() const;
};

BEGIN_VIEW_BUILDER(/* no export */, LabelTextfield, views::Textfield)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::LabelTextfield)

#endif  // ASH_WM_DESKS_LABEL_TEXTFIELD_H_
