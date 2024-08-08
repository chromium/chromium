// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_SYSTEM_DIALOG_DELEGATE_VIEW_H_
#define ASH_STYLE_SYSTEM_DIALOG_DELEGATE_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/style/pill_button.h"
#include "ash/style/system_shadow.h"
#include "base/functional/callback_forward.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
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
// A dialog may include an icon, a title, a description, top and middle contents
// , and a button container. The button container typically contains an
// accept button and a cancel button, but it may also include an additional
// view. The layout of the dialog with all the elements is shown below:
// +----------------------------------------------------+
// |  +----------------------------------------------+  |
// |  |               Top content                    |  |
// |  +----------------------------------------------+  |
// |  +----+                                            |
// |  |    |- Icon                                      |
// |  +----+                                            |
// |                                                    |
// |  Title                                             |
// |                                                    |
// |  Description text                                  |
// |  +----------------------------------------------+  |
// |  |             Middle content                   |  |
// |  +----------------------------------------------+  |
// |  +-----+                    +--------+ +--------+  |
// |  |     |- Additional view   | Cancel | |   OK   |  |
// |  +-----+                    +--------+ +--------+  |
// +----------------------------------------------------+
//
// The dialog would display all or some of above elements, depending on the
// clients' needs.
class ASH_EXPORT SystemDialogDelegateView : public views::WidgetDelegateView {
  METADATA_HEADER(SystemDialogDelegateView, views::WidgetDelegateView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAcceptButtonIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCancelButtonIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDescriptionTextIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTitleTextIdForTesting);

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

  // Sets the visibility of the accept and cancel buttons. Both buttons are
  // visible by default.
  void SetAcceptButtonVisible(bool visible);
  void SetCancelButtonVisible(bool visible);

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

  // Sets the top content view.
  template <typename T>
  T* SetTopContentView(std::unique_ptr<T> view) {
    T* ptr = view.get();
    SetContentInternal(std::move(view), ContentType::kTop);
    return ptr;
  }

  // Sets the middle content view.
  template <typename T>
  T* SetMiddleContentView(std::unique_ptr<T> view) {
    T* ptr = view.get();
    SetContentInternal(std::move(view), ContentType::kMiddle);
    return ptr;
  }

  // Sets the additional view in the button container.
  template <typename T>
  T* SetAdditionalViewInButtonRow(std::unique_ptr<T> view) {
    T* ptr = view.get();
    SetAdditionalViewInButtonRowInternal(std::move(view));
    return ptr;
  }

  // Sets the main axis alignment of the button container which is end aligned
  // by default. Note this will only work if there is no additional view set in
  // the button row. If an additional view is set, the button row will follow
  // the default layout with the additional view at the start and button
  // container at the end.
  void SetButtonContainerAlignment(views::LayoutAlignment alignment);

  // Sets the cross axis alignment of the existing content which is center
  // aligned by default.
  void SetTopContentAlignment(views::LayoutAlignment alignment);
  void SetMiddleContentAlignment(views::LayoutAlignment alignment);

  // Sets the margins for the title label view.
  void SetTitleMargins(const gfx::Insets& margins);

  // views::WidgetDelegateView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnWidgetInitialized() override;
  void OnWorkAreaChanged() override;

  // Helper function to access buttons for tests.
  const PillButton* GetAcceptButtonForTesting() const;
  const PillButton* GetCancelButtonForTesting() const;

 protected:
  virtual void UpdateDialogSize();

 private:
  class ButtonContainer;

  enum class ContentType {
    kTop,     // The content at the top of the dialog.
    kMiddle,  // The content in the middle of description and button container.
  };

  // Get the index of the content with given type in current layout.
  size_t GetContentIndex(ContentType type) const;

  // Internal methods of adding the additional views into the dialog.
  void SetContentInternal(std::unique_ptr<views::View> view, ContentType type);
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
  raw_ptr<ButtonContainer> button_container_ = nullptr;
  base::flat_map<ContentType, raw_ptr<views::View>> contents_;

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
VIEW_BUILDER_VIEW_TYPE_PROPERTY(views::View, TopContentView)
VIEW_BUILDER_VIEW_TYPE_PROPERTY(views::View, MiddleContentView)
VIEW_BUILDER_VIEW_TYPE_PROPERTY(views::View, AdditionalViewInButtonRow)
VIEW_BUILDER_PROPERTY(views::LayoutAlignment, TopContentAlignment)
VIEW_BUILDER_PROPERTY(views::LayoutAlignment, MiddleContentAlignment)
VIEW_BUILDER_PROPERTY(bool, AcceptButtonVisible)
VIEW_BUILDER_PROPERTY(const gfx::Insets&, TitleMargins)
VIEW_BUILDER_PROPERTY(ui::mojom::ModalType, ModalType)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::SystemDialogDelegateView)

#endif  // ASH_STYLE_SYSTEM_DIALOG_DELEGATE_VIEW_H_
