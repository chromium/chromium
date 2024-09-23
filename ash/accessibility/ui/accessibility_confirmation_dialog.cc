// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/ui/accessibility_confirmation_dialog.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace ash {

AccessibilityConfirmationDialog::AccessibilityConfirmationDialog(
    const std::u16string& window_title_text,
    const std::u16string& dialog_text,
    const std::u16string& confirm_text,
    const std::u16string& cancel_text,
    base::OnceClosure on_accept_callback,
    base::OnceClosure on_cancel_callback,
    base::OnceClosure on_close_callback) {
  SetModalType(ui::mojom::ModalType::kSystem);
  SetTitle(window_title_text);
  SetButtonLabel(ui::mojom::DialogButton::kOk, confirm_text);
  SetButtonLabel(ui::mojom::DialogButton::kCancel, cancel_text);
  SetAcceptCallback(std::move(on_accept_callback));
  SetCancelCallback(std::move(on_cancel_callback));
  SetCloseCallback(std::move(on_close_callback));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kText)));
  std::unique_ptr<views::Label> label =
      std::make_unique<views::Label>(dialog_text);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(std::move(label));

  // Parent the dialog widget to the LockSystemModalContainer to ensure that it
  // will get displayed on respective lock/signin or OOBE screen.
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  int container_id = kShellWindowId_SystemModalContainer;
  if (session_controller->IsUserSessionBlocked() ||
      session_controller->GetSessionState() ==
          session_manager::SessionState::OOBE) {
    container_id = kShellWindowId_LockSystemModalContainer;
  }

  views::Widget* widget = CreateDialogWidget(
      this, nullptr,
      Shell::GetContainer(Shell::GetPrimaryRootWindow(), container_id));
  widget->Show();
}

AccessibilityConfirmationDialog::~AccessibilityConfirmationDialog() = default;

bool AccessibilityConfirmationDialog::ShouldShowCloseButton() const {
  return false;
}

base::WeakPtr<AccessibilityConfirmationDialog>
AccessibilityConfirmationDialog::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
