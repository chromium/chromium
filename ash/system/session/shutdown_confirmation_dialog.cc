// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/session/shutdown_confirmation_dialog.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
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

namespace {

// Default width of the dialog in DIPs.
constexpr int kDefaultWidth = 448;

}  // namespace

ShutdownConfirmationDialog::ShutdownConfirmationDialog(
    int window_title_text_id,
    int dialog_text_id,
    base::OnceClosure on_accept_callback,
    base::OnceClosure on_cancel_callback) {
  SetTitle(l10n_util::GetStringUTF16(window_title_text_id));
  SetShowCloseButton(false);
  SetModalType(ui::mojom::ModalType::kSystem);

  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(IDS_ASH_SHUTDOWN_CONFIRMATION_OK_BUTTON));
  SetButtonLabel(
      ui::mojom::DialogButton::kCancel,
      l10n_util::GetStringUTF16(IDS_ASH_SHUTDOWN_CONFIRMATION_CANCEL_BUTTON));
  SetAcceptCallback(std::move(on_accept_callback));
  SetCancelCallback(std::move(on_cancel_callback));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kText)));

  label_ = new views::Label;
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetMultiLine(true);
  label_->SetText(l10n_util::GetStringUTF16(dialog_text_id));
  AddChildView(label_.get());

  // Parent the dialog widget to the PowerButtonAnimationContainer
  int container_id = kShellWindowId_PowerButtonAnimationContainer;
  views::Widget* widget = CreateDialogWidget(
      this, nullptr,
      Shell::GetContainer(ash::Shell::GetRootWindowForNewWindows(),
                          container_id));
  widget->Show();
}

ShutdownConfirmationDialog::~ShutdownConfirmationDialog() = default;

gfx::Size ShutdownConfirmationDialog::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(
      kDefaultWidth,
      GetLayoutManager()->GetPreferredHeightForWidth(this, kDefaultWidth));
}

}  // namespace ash
