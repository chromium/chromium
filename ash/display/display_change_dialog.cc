// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_change_dialog.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace ash {

DisplayChangeDialog::DisplayChangeDialog(
    std::u16string window_title,
    std::u16string timeout_message_with_placeholder,
    base::OnceClosure on_accept_callback,
    CancelCallback on_cancel_callback)
    : timeout_message_with_placeholder_(
          std::move(timeout_message_with_placeholder)),
      on_accept_callback_(std::move(on_accept_callback)),
      on_cancel_callback_(std::move(on_cancel_callback)) {
  SetTitle(window_title);
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_ASH_CONFIRM_BUTTON));

  SetAcceptCallback(base::BindOnce(&DisplayChangeDialog::OnConfirmButtonClicked,
                                   base::Unretained(this)));
  SetCancelCallback(base::BindOnce(&DisplayChangeDialog::OnCancelButtonClicked,
                                   base::Unretained(this)));
  SetModalType(ui::mojom::ModalType::kSystem);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::DialogContentType::kText, views::DialogContentType::kText)));
  label_ =
      AddChildView(std::make_unique<views::Label>(GetRevertTimeoutString()));
  label_->SetMultiLine(true);
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  views::Widget* widget = CreateDialogWidget(
      this, nullptr,
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          kShellWindowId_SystemModalContainer));
  // TODO(baileyberro): Verify behavior in kiosk mode.
  widget->Show();

  timer_.Start(FROM_HERE, base::Seconds(1), this,
               &DisplayChangeDialog::OnTimerTick);
}

DisplayChangeDialog::~DisplayChangeDialog() = default;

void DisplayChangeDialog::OnConfirmButtonClicked() {
  timer_.Stop();
  std::move(on_accept_callback_).Run();
}

void DisplayChangeDialog::OnCancelButtonClicked() {
  timer_.Stop();
  std::move(on_cancel_callback_).Run(/*display_was_removed=*/false);
}

gfx::Size DisplayChangeDialog::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(350, 100);
}

void DisplayChangeDialog::OnTimerTick() {
  if (--timeout_count_ == 0) {
    CancelDialog();
    return;
  }

  label_->SetText(GetRevertTimeoutString());
}

std::u16string DisplayChangeDialog::GetRevertTimeoutString() const {
  const std::u16string timer = ui::TimeFormat::Simple(
      ui::TimeFormat::FORMAT_DURATION, ui::TimeFormat::LENGTH_LONG,
      base::Seconds(timeout_count_));
  return base::ReplaceStringPlaceholders(timeout_message_with_placeholder_,
                                         timer, /*offset=*/nullptr);
}

base::WeakPtr<DisplayChangeDialog> DisplayChangeDialog::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace ash
