// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/style/typography.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_error_dialog.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_warn_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace policy {

FilesPolicyDialogFactory* factory_;

FilesPolicyDialog::FilesPolicyDialog(size_t file_count,
                                     dlp::FileAction action,
                                     gfx::NativeWindow modal_parent)
    : action_(action), file_count_(file_count) {
  ui::ModalType modal =
      modal_parent ? ui::MODAL_TYPE_WINDOW : ui::MODAL_TYPE_SYSTEM;

  set_margins(gfx::Insets::TLBR(24, 0, 20, 0));

  SetModalType(modal);
}

FilesPolicyDialog::~FilesPolicyDialog() = default;

views::Widget* FilesPolicyDialog::CreateWarnDialog(
    OnDlpRestrictionCheckedCallback callback,
    const std::vector<DlpConfidentialFile>& files,
    dlp::FileAction action,
    gfx::NativeWindow modal_parent,
    absl::optional<DlpFileDestination> destination) {
  if (factory_) {
    return factory_->CreateWarnDialog(std::move(callback), files, action,
                                      modal_parent, destination);
  }

  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      std::make_unique<FilesPolicyWarnDialog>(
          std::move(callback), files, action, modal_parent, destination),
      /*context=*/nullptr, /*parent=*/modal_parent);
  widget->Show();
  return widget;
}

views::Widget* FilesPolicyDialog::CreateErrorDialog(
    const std::map<DlpConfidentialFile, Policy>& files,
    dlp::FileAction action,
    gfx::NativeWindow modal_parent) {
  if (factory_) {
    return factory_->CreateErrorDialog(files, action, modal_parent);
  }

  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      std::make_unique<FilesPolicyErrorDialog>(files, action, modal_parent),
      /*context=*/nullptr, /*parent=*/modal_parent);
  widget->Show();
  return widget;
}

// static
void FilesPolicyDialog::SetFactory(FilesPolicyDialogFactory* factory) {
  delete factory_;
  factory_ = factory;
}

void FilesPolicyDialog::SetupScrollView() {
  // Call the parent class to setup the element. Do not remove.
  PolicyDialogBase::SetupScrollView();
  views::BoxLayout* layout = scroll_view_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets::TLBR(8, 8, 8, 24),
          /*between_child_spacing=*/0));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
}

views::Label* FilesPolicyDialog::AddTitle(const std::u16string& title) {
  // Call the parent class to setup the element. Do not remove.
  views::Label* title_label = PolicyDialogBase::AddTitle(title);
  title_label->SetFontList(
      ash::TypographyProvider::Get()->ResolveTypographyToken(
          ash::TypographyToken::kCrosTitle1));
  return title_label;
}

views::Label* FilesPolicyDialog::AddMessage(const std::u16string& message) {
  if (message.empty()) {
    // Some dialogs, like the mixed error dialogs don't have a single message,
    // but add the error description inside the scrollable list, so skip adding
    // the element altogether.
    return nullptr;
  }
  // Call the parent class to setup the element. Do not remove.
  views::Label* message_label = PolicyDialogBase::AddMessage(message);
  message_label->SetFontList(
      ash::TypographyProvider::Get()->ResolveTypographyToken(
          ash::TypographyToken::kCrosBody1));
  return message_label;
}

void FilesPolicyDialog::AddConfidentialRow(const gfx::ImageSkia& icon,
                                           const std::u16string& title) {
  DCHECK(scroll_view_container_);
  views::View* row =
      scroll_view_container_->AddChildView(std::make_unique<views::View>());
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::TLBR(10, 16, 10, 16), /*between_child_spacing=*/16));

  AddRowIcon(icon, row);

  views::Label* title_label = AddRowTitle(title, row);
  title_label->SetFontList(
      ash::TypographyProvider::Get()->ResolveTypographyToken(
          ash::TypographyToken::kCrosBody1));
}

BEGIN_METADATA(FilesPolicyDialog, PolicyDialogBase)
END_METADATA

}  // namespace policy
