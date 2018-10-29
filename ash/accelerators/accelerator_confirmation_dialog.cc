// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_confirmation_dialog.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace ash {

AcceleratorConfirmationDialog::AcceleratorConfirmationDialog(
    int window_title_text_id,
    int dialog_text_id,
    base::OnceClosure on_accept_callback)
    : window_title_(l10n_util::GetStringUTF16(window_title_text_id)),
      on_accept_callback_(std::move(on_accept_callback)),
      weak_ptr_factory_(this) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::TEXT, views::TEXT)));
  AddChildView(new views::Label(l10n_util::GetStringUTF16(dialog_text_id)));

  // Parent the dialog widget to the LockSystemModalContainer, or
  // OverlayContainer to ensure that it will get displayed on respective
  // lock/signin or OOBE screen.
  SessionController* session_controller = Shell::Get()->session_controller();
  int container_id = kShellWindowId_SystemModalContainer;
  if (session_controller->GetSessionState() ==
      session_manager::SessionState::OOBE) {
    container_id = kShellWindowId_OverlayContainer;
  } else if (session_controller->IsUserSessionBlocked()) {
    container_id = kShellWindowId_LockSystemModalContainer;
  }

  views::Widget* widget = CreateDialogWidget(
      this, nullptr,
      Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(), container_id));
  widget->Show();
}

AcceleratorConfirmationDialog::~AcceleratorConfirmationDialog() = default;

bool AcceleratorConfirmationDialog::Accept() {
  std::move(on_accept_callback_).Run();
  return true;
}

ui::ModalType AcceleratorConfirmationDialog::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 AcceleratorConfirmationDialog::GetWindowTitle() const {
  return window_title_;
}

base::string16 AcceleratorConfirmationDialog::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_ASH_CONTINUE_BUTTON);
  return views::DialogDelegateView::GetDialogButtonLabel(button);
}

base::WeakPtr<AcceleratorConfirmationDialog>
AcceleratorConfirmationDialog::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
