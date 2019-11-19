// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/session_aborted_dialog.h"

#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Default width of the dialog.
constexpr int kDefaultWidth = 448;

}  // namespace

// static
void SessionAbortedDialog::Show(const std::string& user_email) {
  SessionAbortedDialog* dialog_view = new SessionAbortedDialog();
  dialog_view->InitDialog(user_email);
  views::DialogDelegate::CreateDialogWidget(
      dialog_view, Shell::GetRootWindowForNewWindows(), nullptr);
  views::Widget* widget = dialog_view->GetWidget();
  DCHECK(widget);
  widget->Show();

  // Since this is the last thing the user ever sees, we also hide all system
  // trays from the screen.
  std::vector<RootWindowController*> controllers =
      Shell::GetAllRootWindowControllers();
  for (RootWindowController* controller : controllers) {
    controller->shelf()->SetAutoHideBehavior(SHELF_AUTO_HIDE_ALWAYS_HIDDEN);
  }
}

bool SessionAbortedDialog::Accept() {
  Shell::Get()->session_controller()->RequestSignOut();
  return true;
}

int SessionAbortedDialog::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

ui::ModalType SessionAbortedDialog::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 SessionAbortedDialog::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_MULTIPROFILES_SESSION_ABORT_HEADLINE);
}

bool SessionAbortedDialog::ShouldShowCloseButton() const {
  return false;
}

gfx::Size SessionAbortedDialog::CalculatePreferredSize() const {
  return gfx::Size(
      kDefaultWidth,
      GetLayoutManager()->GetPreferredHeightForWidth(this, kDefaultWidth));
}

SessionAbortedDialog::SessionAbortedDialog() {
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(
          IDS_ASH_MULTIPROFILES_SESSION_ABORT_BUTTON_LABEL));
}

SessionAbortedDialog::~SessionAbortedDialog() = default;

void SessionAbortedDialog::InitDialog(const std::string& user_email) {
  const views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetBorder(views::CreateEmptyBorder(
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT)));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Explanation string.
  views::Label* label = new views::Label(
      l10n_util::GetStringFUTF16(IDS_ASH_MULTIPROFILES_SESSION_ABORT_MESSAGE,
                                 base::ASCIIToUTF16(user_email)));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  AddChildView(label);
}

}  // namespace ash
