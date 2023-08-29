// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_SAVE_DESK_BUTTON_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_SAVE_DESK_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/style/pill_button.h"
#include "ash/wm/overview/overview_focusable_view.h"
#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class ASH_EXPORT SavedDeskSaveDeskButton : public PillButton,
                                           public OverviewFocusableView {
 public:
  METADATA_HEADER(SavedDeskSaveDeskButton);

  enum class Type {
    // Button that saves current desk as template.
    kSaveAsTemplate = 0,

    // Button that saves current desk for later.
    kSaveForLater,
  };

  SavedDeskSaveDeskButton(base::RepeatingClosure callback,
                          const std::u16string& text,
                          Type button_type,
                          const gfx::VectorIcon* icon);
  SavedDeskSaveDeskButton(const SavedDeskSaveDeskButton&) = delete;
  SavedDeskSaveDeskButton& operator=(const SavedDeskSaveDeskButton&) = delete;
  ~SavedDeskSaveDeskButton() override;

  Type button_type() const { return button_type_; }

 private:
  // OverviewFocusableView:
  views::View* GetView() override;
  void MaybeActivateFocusedView() override;
  void MaybeCloseFocusedView(bool primary_action) override;
  void MaybeSwapFocusedView(bool right) override;
  void OnFocusableViewFocused() override;
  void OnFocusableViewBlurred() override;

  // PillButton:
  void OnFocus() override;
  void OnBlur() override;

  base::RepeatingClosure callback_;
  Type button_type_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_SAVE_DESK_BUTTON_H_
