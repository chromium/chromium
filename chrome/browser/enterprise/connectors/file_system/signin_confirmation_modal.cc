// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/signin_confirmation_modal.h"

#include "chrome/browser/enterprise/connectors/file_system/signin_experience.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"

namespace {

constexpr int kBusinessIconSize = 24;
constexpr int kModalWidth = 400;
constexpr int kMessageMarginHorizontal = kBusinessIconSize / 4;
constexpr int kMessageMarginRight = kBusinessIconSize / 2;
constexpr int kMessageMarginLeft = kMessageMarginRight + kBusinessIconSize * 2;

}  // namespace

namespace enterprise_connectors {

// static
void FileSystemConfirmationModal::Show(gfx::NativeWindow context,
                                       const std::u16string& title,
                                       const std::u16string& message,
                                       const std::u16string& cancel_button,
                                       const std::u16string& accept_button,
                                       Callback callback,
                                       SigninExperienceTestObserver* observer) {
  auto* confirmation_modal = new FileSystemConfirmationModal(
      title, message, cancel_button, accept_button, std::move(callback));
  auto* modal_view = constrained_window::CreateBrowserModalDialogViews(
      confirmation_modal, context);

  if (observer)
    observer->OnConfirmationDialogCreated(confirmation_modal);
  // The naked new is OK here because CreateBrowserModalDialogViews() takes
  // ownership.
  modal_view->Show();
}

FileSystemConfirmationModal::FileSystemConfirmationModal(
    const std::u16string& title,
    const std::u16string& message,
    const std::u16string& cancel_button,
    const std::u16string& accept_button,
    Callback callback)
    : title_(title), message_(message), callback_(std::move(callback)) {
  SetShowIcon(true);
  SetShowTitle(true);
  SetShowCloseButton(false);
  // Enable and set attributes of the cancel button.
  SetButtonEnabled(ui::DialogButton::DIALOG_BUTTON_CANCEL, true);
  SetCancelCallback(base::BindOnce(&FileSystemConfirmationModal::OnCancellation,
                                   weak_factory_.GetWeakPtr()));
  SetButtonLabel(ui::DialogButton::DIALOG_BUTTON_CANCEL, cancel_button);
  // Enable and set attributes of the accept button.
  SetButtonEnabled(ui::DialogButton::DIALOG_BUTTON_OK, true);
  SetAcceptCallback(base::BindOnce(&FileSystemConfirmationModal::OnConfirmation,
                                   weak_factory_.GetWeakPtr()));
  SetButtonLabel(ui::DialogButton::DIALOG_BUTTON_OK, accept_button);
  // Set the message to be shown.
  std::unique_ptr<views::Label> view = std::make_unique<views::Label>(message_);
  view->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kMessageMarginHorizontal, kMessageMarginLeft,
                        kMessageMarginHorizontal, kMessageMarginRight)));
  view->SetMultiLine(true);
  view->SizeToFit(kModalWidth);
  view->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  SetContentsView(std::move(view));
  // For accessiblity features. This role will read title + message on pop up.
  SetAccessibleRole(ax::mojom::Role::kAlertDialog);
}

FileSystemConfirmationModal::~FileSystemConfirmationModal() = default;

std::u16string FileSystemConfirmationModal::GetWindowTitle() const {
  return title_;
}

ui::ImageModel FileSystemConfirmationModal::GetWindowIcon() {
  // Show the enterprise icon.
  return ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                        ui::kColorAccent, kBusinessIconSize);
}

ui::ModalType FileSystemConfirmationModal::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

}  // namespace enterprise_connectors
