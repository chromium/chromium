// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_SYSTEM_DIALOG_DELEGATE_VIEW_H_
#define ASH_STYLE_SYSTEM_DIALOG_DELEGATE_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/style/system_shadow.h"
#include "base/functional/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

// The contents of a dialog that displays information or prompts the user input.
// A dialog may include an icon, a title, a description, additional content, and
// a button container. The button container typically contains an accept button
// and a cancel button, but it may also include an additional view. The layout
// of the dialog with all the elements is shown below:
// +----------------------------------------------------+
// |  +----+                                            |
// |  |    |- Icon                                      |
// |  +----+                                            |
// |                                                    |
// |  Title                                             |
// |                                                    |
// |  Description text                                  |
// |  +----------------------------------------------+  |
// |  |           Additional content                 |  |
// |  +----------------------------------------------+  |
// |  +-----+                    +--------+ +--------+  |
// |  |     |- Additional view   | Cancel | |   OK   |  |
// |  +-----+                    +--------+ +--------+  |
// +----------------------------------------------------+
//
// The dialog would display all or some of above elements, depending on the
// clients' needs.
class ASH_EXPORT SystemDialogDelegateView : public views::WidgetDelegateView {
 public:
  METADATA_HEADER(SystemDialogDelegateView);

  SystemDialogDelegateView();
  SystemDialogDelegateView(const SystemDialogDelegateView&) = delete;
  SystemDialogDelegateView& operator=(const SystemDialogDelegateView&) = delete;
  ~SystemDialogDelegateView() override;

  // Sets the leading icon of the dialog. There is no icon by default.
  void SetIcon(const gfx::VectorIcon& icon);

  // Sets title and description text. There will be no title or description if
  // their texts are empty.
  void SetTitleText(const std::u16string& title);
  void SetDescription(const std::u16string& description);
  void SetDescriptionAccessibleName(const std::u16string& accessible_name);

  // Sets the text of accept and cancel buttons. The default accept button text
  // is "OK", and cancel button is "Cancel".
  void SetAcceptButtonText(const std::u16string& accept_text);
  void SetCancelButtonText(const std::u16string& cancel_text);

  // Sets accept and cancel button callbacks. If the callback is not set,
  // clicking the corresponding button would only close the dialog without
  // performing any additional actions.
  void SetAcceptCallback(base::OnceClosure accept_callback) {
    accept_callback_ = std::move(accept_callback);
  }
  void SetCancelCallback(base::OnceClosure cancel_callback) {
    cancel_callback_ = std::move(cancel_callback);
  }

  // Sets dialog close callback. The close callback is called when the dialog is
  // closed without clicking the accept or cancel button. For example, when the
  // dialog's parent window is destroyed.
  void SetCloseCallback(base::OnceClosure close_callback) {
    close_callback_ = std::move(close_callback);
  }

  // Sets the additional content view.
  template <typename T>
  T* SetAdditionalContentView(std::unique_ptr<T> view) {
    T* raw_ptr = view.get();
    SetAdditionalContentInternal(std::move(view));
    return raw_ptr;
  }

  // Sets the additional view in the button container.
  template <typename T>
  T* SetAdditionalViewInButtonRow(std::unique_ptr<T> view) {
    T* raw_ptr = view.get();
    SetAdditionalViewInButtonRowInternal(std::move(view));
    return raw_ptr;
  }

  // Sets the cross axis alignment of current additional content which is center
  // aligned by default.
  void SetAdditionalContentCrossAxisAlignment(views::LayoutAlignment alignment);

  // views::WidgetDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnWidgetInitialized() override;
  void OnWorkAreaChanged() override;

 protected:
  virtual void UpdateDialogSize();

 private:
  class ButtonContainer;

  // Internal methods of adding the additional views into the dialog.
  void SetAdditionalContentInternal(std::unique_ptr<views::View> view);
  void SetAdditionalViewInButtonRowInternal(std::unique_ptr<views::View> view);

  // The actual callbacks of accept and cancel buttons. When the accept/cancel
  // button is clicked, the corresponding `accept_callback_`/`cancel_callback_`
  // will be called if exists and the dialog will be closed.
  void Accept();
  void Cancel();

  // The callback when the dialog will be closed.
  void Close();

  // Run the given `callback` and close the dialog with `closed_reason`.
  void RunCallbackAndCloseDialog(base::OnceClosure callback,
                                 views::Widget::ClosedReason closed_reason);

  // The callbacks of the buttons and closing dialog.
  base::OnceClosure accept_callback_;
  base::OnceClosure cancel_callback_;
  base::OnceClosure close_callback_;

  // The view of each element owned by the dialog.
  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> description_ = nullptr;
  raw_ptr<views::View> additional_content_ = nullptr;
  raw_ptr<ButtonContainer> button_container_ = nullptr;

  // The dialog shadow.
  std::unique_ptr<SystemShadow> shadow_;

  // Indicates if the dialog is being closed.
  bool closing_dialog_ = false;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT,
                   SystemDialogDelegateView,
                   views::WidgetDelegateView)
VIEW_BUILDER_PROPERTY(const gfx::VectorIcon&, Icon, const gfx::VectorIcon&)
VIEW_BUILDER_PROPERTY(const std::u16string&, TitleText)
VIEW_BUILDER_PROPERTY(const std::u16string&, Description)
VIEW_BUILDER_PROPERTY(const std::u16string&, DescriptionAccessibleName)
VIEW_BUILDER_PROPERTY(const std::u16string&, AcceptButtonText)
VIEW_BUILDER_PROPERTY(const std::u16string&, CancelButtonText)
VIEW_BUILDER_PROPERTY(base::OnceClosure, AcceptCallback)
VIEW_BUILDER_PROPERTY(base::OnceClosure, CancelCallback)
VIEW_BUILDER_PROPERTY(base::OnceClosure, CloseCallback)
VIEW_BUILDER_VIEW_TYPE_PROPERTY(views::View, AdditionalContentView)
VIEW_BUILDER_VIEW_TYPE_PROPERTY(views::View, AdditionalViewInButtonRow)
VIEW_BUILDER_PROPERTY(views::LayoutAlignment,
                      AdditionalContentCrossAxisAlignment)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::SystemDialogDelegateView)

#endif  // ASH_STYLE_SYSTEM_DIALOG_DELEGATE_VIEW_H_
