// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_vm_data_migration_confirmation_dialog.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace arc {

namespace {

constexpr char kInternalName[] = "ArcVmDataMigrationConfirmationDialog";

// TODO(b/258278176): Replace strings with l10n ones.
constexpr char kDialogButtonOkText[] = "Start update";
constexpr char kDialogButtonNgText[] = "Remind me later";
constexpr char kDialogTitleText[] =
    "Your Chrome tabs and apps will close when the update starts";
constexpr char kDialogMessageText[] =
    "Please save your work and start the update when you're ready.";

}  // namespace

ArcVmDataMigrationConfirmationDialog::ArcVmDataMigrationConfirmationDialog(
    ArcVmDataMigrationConfirmationCallback callback)
    : callback_(std::move(callback)) {
  set_internal_name(kInternalName);
  SetButtons(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 base::UTF8ToUTF16(std::string(kDialogButtonOkText)));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 base::UTF8ToUTF16(std::string(kDialogButtonNgText)));

  InitializeView();

  // Not system modal so that the user can interact with apps before restart.
  SetModalType(ui::MODAL_TYPE_NONE);
  SetOwnedByWidget(true);
  SetShowCloseButton(false);

  const auto* layout_provider = views::LayoutProvider::Get();
  DCHECK(layout_provider);
  set_fixed_width(layout_provider->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  set_margins(layout_provider->GetDialogInsetsForContentType(
      views::DialogContentType::kControl, views::DialogContentType::kControl));

  SetAcceptCallback(
      base::BindOnce(&ArcVmDataMigrationConfirmationDialog::OnButtonClicked,
                     weak_ptr_factory_.GetWeakPtr(), true /* accepted */));
  SetCancelCallback(
      base::BindOnce(&ArcVmDataMigrationConfirmationDialog::OnButtonClicked,
                     weak_ptr_factory_.GetWeakPtr(), false /* accepted */));
}

ArcVmDataMigrationConfirmationDialog::~ArcVmDataMigrationConfirmationDialog() =
    default;

void ArcVmDataMigrationConfirmationDialog::InitializeView() {
  auto view = std::make_unique<views::View>();

  const auto* layout_provider = views::LayoutProvider::Get();
  DCHECK(layout_provider);
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      layout_provider->GetInsetsMetric(views::InsetsMetric::INSETS_DIALOG),
      layout_provider->GetDistanceMetric(
          views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
  view->SetLayoutManager(std::move(layout));

  // TODO(b/258278176): Add an icon once the final design decision is made.

  view->AddChildView(
      views::Builder<views::Label>()
          .SetText(base::UTF8ToUTF16(std::string(kDialogTitleText)))
          .SetTextContext(views::style::CONTEXT_DIALOG_TITLE)
          .SetTextStyle(views::style::STYLE_PRIMARY)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetMultiLine(true)
          .Build());

  view->AddChildView(
      views::Builder<views::Label>()
          .SetText(base::UTF8ToUTF16(std::string(kDialogMessageText)))
          .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
          .SetTextStyle(views::style::STYLE_SECONDARY)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetMultiLine(true)
          .Build());

  SetContentsView(std::move(view));
}

void ArcVmDataMigrationConfirmationDialog::OnButtonClicked(bool accepted) {
  DCHECK(!callback_.is_null());
  std::move(callback_).Run(accepted);
}

void ShowArcVmDataMigrationConfirmationDialog(
    ArcVmDataMigrationConfirmationCallback callback) {
  views::DialogDelegate::CreateDialogWidget(
      std::make_unique<ArcVmDataMigrationConfirmationDialog>(
          std::move(callback)),
      nullptr /* context */, nullptr /* parent */)
      ->Show();
}

}  // namespace arc
