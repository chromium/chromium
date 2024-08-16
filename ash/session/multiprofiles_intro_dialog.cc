// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/multiprofiles_intro_dialog.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Default width of the dialog.
constexpr int kDefaultWidth = 448;

}  // namespace

// static
void MultiprofilesIntroDialog::Show(OnAcceptCallback on_accept) {
  MultiprofilesIntroDialog* dialog_view =
      new MultiprofilesIntroDialog(std::move(on_accept));
  dialog_view->InitDialog();
  views::DialogDelegate::CreateDialogWidget(
      dialog_view, Shell::GetRootWindowForNewWindows(), nullptr);
  views::Widget* widget = dialog_view->GetWidget();
  DCHECK(widget);
  widget->Show();
}

gfx::Size MultiprofilesIntroDialog::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(
      kDefaultWidth,
      GetLayoutManager()->GetPreferredHeightForWidth(this, kDefaultWidth));
}

MultiprofilesIntroDialog::MultiprofilesIntroDialog(OnAcceptCallback on_accept)
    : never_show_again_checkbox_(new views::Checkbox(
          l10n_util::GetStringUTF16(IDS_ASH_DIALOG_DONT_SHOW_AGAIN))),
      on_accept_(std::move(on_accept)) {
  never_show_again_checkbox_->SetChecked(true);
  SetModalType(ui::mojom::ModalType::kSystem);
  SetTitle(l10n_util::GetStringUTF16(IDS_ASH_MULTIPROFILES_INTRO_HEADLINE));
  SetShowCloseButton(false);
  SetAcceptCallback(base::BindOnce(
      [](MultiprofilesIntroDialog* dialog) {
        std::move(dialog->on_accept_)
            .Run(true, dialog->never_show_again_checkbox_->GetChecked());
      },
      base::Unretained(this)));
  SetCancelCallback(base::BindOnce(
      [](MultiprofilesIntroDialog* dialog) {
        std::move(dialog->on_accept_).Run(false, false);
      },
      base::Unretained(this)));
}

MultiprofilesIntroDialog::~MultiprofilesIntroDialog() = default;

void MultiprofilesIntroDialog::InitDialog() {
  const views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetBorder(views::CreateEmptyBorder(provider->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kControl)));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  // Explanation string
  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_MULTIPROFILES_INTRO_MESSAGE));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(label);
  AddChildView(never_show_again_checkbox_.get());
}

}  // namespace ash
