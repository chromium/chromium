// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_COMPAT_MODE_RESIZE_CONFIRMATION_DIALOG_VIEW_H_
#define ASH_COMPONENTS_ARC_COMPAT_MODE_RESIZE_CONFIRMATION_DIALOG_VIEW_H_

#include "base/functional/callback_forward.h"
#include "ui/views/layout/box_layout_view.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class MdTextButton;
class Checkbox;
}  // namespace views

namespace arc {

// Callback to notify user's confirmation for allowing to resize the app.
// If user accept it, the callback is invoked with 1st argument true.
// Otherwise, with false.
// If the user marked the "Don't ask me again", 2nd argument will be true.
using ResizeConfirmationCallback = base::OnceCallback<void(bool, bool)>;

class ResizeConfirmationDialogView : public views::BoxLayoutView {
 public:
  // TestApi is used only in tests to get internal views.
  class TestApi {
   public:
    explicit TestApi(ResizeConfirmationDialogView* view) : view_(view) {}

    views::MdTextButton* accept_button() const { return view_->accept_button_; }
    views::MdTextButton* cancel_button() const { return view_->cancel_button_; }
    views::Checkbox* do_not_ask_checkbox() const {
      return view_->do_not_ask_checkbox_;
    }

   private:
    ResizeConfirmationDialogView* const view_;
  };

  explicit ResizeConfirmationDialogView(ResizeConfirmationCallback callback);
  ResizeConfirmationDialogView(const ResizeConfirmationDialogView&) = delete;
  ResizeConfirmationDialogView& operator=(const ResizeConfirmationDialogView&) =
      delete;
  ~ResizeConfirmationDialogView() override;

  // Shows confirmation dialog for asking user if really want to enable resizing
  // for the resize-locked ARC app.
  static void Show(aura::Window* parent, ResizeConfirmationCallback callback);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void AddedToWidget() override;
  void OnThemeChanged() override;

 private:
  std::unique_ptr<views::View> MakeContentsView();
  std::unique_ptr<views::View> MakeButtonsView();

  void OnButtonClicked(bool accept);

  ResizeConfirmationCallback callback_;

  views::Checkbox* do_not_ask_checkbox_{nullptr};
  views::MdTextButton* accept_button_{nullptr};
  views::MdTextButton* cancel_button_{nullptr};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_COMPAT_MODE_RESIZE_CONFIRMATION_DIALOG_VIEW_H_
