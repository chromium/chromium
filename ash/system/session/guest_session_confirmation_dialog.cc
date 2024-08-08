// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/session/guest_session_confirmation_dialog.h"

#include <memory>

#include "ash/public/cpp/window_backdrop.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(GuestSessionConfirmationDialog,
                                      kGuestSessionConfirmationDialogId);

GuestSessionConfirmationDialog* GuestSessionConfirmationDialog::g_dialog_ =
    nullptr;

GuestSessionConfirmationDialog::~GuestSessionConfirmationDialog() = default;

// static
void GuestSessionConfirmationDialog::Show() {
  // Avoid duplicate dialogs.
  if (g_dialog_) {
    return;
  }

  // dialog_ will be released when the dialog is closed.
  g_dialog_ = new GuestSessionConfirmationDialog();

  std::unique_ptr<ui::DialogModel> dialog_model =
      ui::DialogModel::Builder(std::make_unique<ui::DialogModelDelegate>())
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_GUEST_SESSION_CONFIRMATION_DIALOG_TITLE))
          .AddOkButton(
              base::BindOnce(&GuestSessionConfirmationDialog::OnConfirm,
                             g_dialog_->weak_ptr_factory_.GetWeakPtr()),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_GUEST_SESSION_CONFIRMATION_DIALOG_SIGN_OUT)))
          .AddCancelButton(
              base::DoNothing(),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_GUEST_SESSION_CONFIRMATION_DIALOG_CANCEL)))
          .AddParagraph(ui::DialogModelLabel(l10n_util::GetStringUTF16(
              IDS_GUEST_SESSION_CONFIRMATION_DIALOG_TEXT)))
          .SetDialogDestroyingCallback(
              base::BindOnce(&GuestSessionConfirmationDialog::OnDialogClosing,
                             g_dialog_->weak_ptr_factory_.GetWeakPtr()))
          .Build();

  g_dialog_->dialog_model_ = dialog_model.get();

  auto bubble = views::BubbleDialogModelHost::CreateModal(
      std::move(dialog_model), ui::mojom::ModalType::kSystem);
  bubble->SetOwnedByWidget(true);
  views::Widget* widget =
      views::DialogDelegate::CreateDialogWidget(std::move(bubble),
                                                /*context=*/nullptr,
                                                /*parent=*/nullptr);
  widget->Show();

  // TODO(crbug.com/1016828): Remove/update this after the dialog behavior on
  // Chrome OS is defined.
  WindowBackdrop::Get(widget->GetNativeWindow())
      ->SetBackdropType(WindowBackdrop::BackdropType::kSemiOpaque);
}

GuestSessionConfirmationDialog::GuestSessionConfirmationDialog() = default;

void GuestSessionConfirmationDialog::OnConfirm() {
  should_logout_ = true;
}

void GuestSessionConfirmationDialog::OnDialogClosing() {
  dialog_model_ = nullptr;

  if (should_logout_) {
    Shell::Get()->session_controller()->RequestSignOut();
  }

  delete this;
  g_dialog_ = nullptr;
}

}  // namespace ash
