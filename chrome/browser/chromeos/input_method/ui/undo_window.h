// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_UNDO_WINDOW_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_UNDO_WINDOW_H_

#include "chrome/browser/chromeos/input_method/ui/assistive_delegate.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"

namespace ui {
namespace ime {

// Pop up UI for users to undo an autocorrected word.
class UI_CHROMEOS_EXPORT UndoWindow : public views::BubbleDialogDelegateView {
 public:
  explicit UndoWindow(gfx::NativeView parent, AssistiveDelegate* delegate);
  ~UndoWindow() override;

  views::Widget* InitWidget();
  void Hide();
  void Show();

  // Set the position of the undo window at the start of the autocorrected word.
  void SetBounds(const gfx::Rect& word_bounds);

  void SetButtonHighlighted(const AssistiveWindowButton& button,
                            bool highlighted);

  views::Button* GetUndoButtonForTesting();

 protected:
  void OnThemeChanged() override;

 private:
  // views::BubbleDialogDelegateView:
  const char* GetClassName() const override;

  void UndoButtonPressed();

  AssistiveDelegate* delegate_;
  views::LabelButton* undo_button_;

  DISALLOW_COPY_AND_ASSIGN(UndoWindow);
};

}  // namespace ime
}  // namespace ui

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_UI_UNDO_WINDOW_H_
