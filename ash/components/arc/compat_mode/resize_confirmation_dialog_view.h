// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_COMPAT_MODE_RESIZE_CONFIRMATION_DIALOG_VIEW_H_
#define ASH_COMPONENTS_ARC_COMPAT_MODE_RESIZE_CONFIRMATION_DIALOG_VIEW_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class Checkbox;
class LabelButton;
}  // namespace views

namespace ash {
class Checkbox;
}  // namespace ash

namespace arc {

// Callback to notify user's confirmation for allowing to resize the app.
// If user accept it, the callback is invoked with 1st argument true.
// Otherwise, with false.
// If the user marked the "Don't ask me again", 2nd argument will be true.
using ResizeConfirmationCallback = base::OnceCallback<void(bool, bool)>;

class ResizeConfirmationDialogView : public views::BubbleDialogDelegateView,
                                     public views::WidgetObserver {
  METADATA_HEADER(ResizeConfirmationDialogView, views::BubbleDialogDelegateView)

 public:
  // TestApi is used only in tests to get internal views.
  class TestApi {
   public:
    explicit TestApi(ResizeConfirmationDialogView* view) : view_(view) {}

    views::LabelButton* accept_button() const { return view_->accept_button_; }
    views::LabelButton* cancel_button() const { return view_->cancel_button_; }
    void SelectDoNotAskCheckbox();

   private:
    const raw_ptr<ResizeConfirmationDialogView> view_;
  };

  ResizeConfirmationDialogView(views::Widget* parent,
                               ResizeConfirmationCallback callback);
  ResizeConfirmationDialogView(const ResizeConfirmationDialogView&) = delete;
  ResizeConfirmationDialogView& operator=(const ResizeConfirmationDialogView&) =
      delete;
  ~ResizeConfirmationDialogView() override;

  // Shows confirmation dialog for asking user if really want to enable resizing
  // for the resize-locked ARC app.
  static void Show(views::Widget* parent, ResizeConfirmationCallback callback);

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void AddedToWidget() override;
  void OnThemeChanged() override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

 private:
  std::unique_ptr<views::View> MakeContentsView();
  std::unique_ptr<views::View> MakeButtonsView();

  void OnButtonClicked(bool accept, views::Widget::ClosedReason close_reason);

  ResizeConfirmationCallback callback_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  raw_ptr<views::Checkbox> do_not_ask_checkbox_{nullptr};
  raw_ptr<ash::Checkbox> do_not_ask_checkbox_jelly_{nullptr};
  raw_ptr<views::LabelButton> accept_button_{nullptr};
  raw_ptr<views::LabelButton> cancel_button_{nullptr};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_COMPAT_MODE_RESIZE_CONFIRMATION_DIALOG_VIEW_H_
