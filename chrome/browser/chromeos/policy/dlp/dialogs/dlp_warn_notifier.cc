// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dialogs/dlp_warn_notifier.h"

#include <cstddef>
#include <memory>

#include "base/containers/cxx20_erase.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/dlp_warn_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

namespace policy {

DlpWarnNotifier::DlpWarnNotifier() = default;

DlpWarnNotifier::~DlpWarnNotifier() {
  for (views::Widget* widget : widgets_) {
    widget->RemoveObserver(this);
    widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

void DlpWarnNotifier::OnWidgetDestroying(views::Widget* widget) {
  RemoveWidget(widget);
}

void DlpWarnNotifier::ShowDlpPrintWarningDialog(
    OnDlpRestrictionCheckedCallback callback) {
  ShowDlpWarningDialog(std::move(callback),
                       DlpWarnDialog::DlpWarnDialogOptions(
                           DlpWarnDialog::Restriction::kPrinting));
}

void DlpWarnNotifier::ShowDlpScreenCaptureWarningDialog(
    OnDlpRestrictionCheckedCallback callback,
    const DlpConfidentialContents& confidential_contents) {
  ShowDlpWarningDialog(
      std::move(callback),
      DlpWarnDialog::DlpWarnDialogOptions(
          DlpWarnDialog::Restriction::kScreenCapture, confidential_contents));
}

void DlpWarnNotifier::ShowDlpVideoCaptureWarningDialog(
    OnDlpRestrictionCheckedCallback callback,
    const DlpConfidentialContents& confidential_contents) {
  ShowDlpWarningDialog(
      std::move(callback),
      DlpWarnDialog::DlpWarnDialogOptions(
          DlpWarnDialog::Restriction::kVideoCapture, confidential_contents));
}

base::WeakPtr<views::Widget> DlpWarnNotifier::ShowDlpFilesWarningDialog(
    OnDlpRestrictionCheckedCallback callback,
    const std::vector<DlpConfidentialFile>& confidential_files,
    const DlpFileDestination& files_destination,
    DlpFilesController::FileAction files_action,
    gfx::NativeWindow modal_parent) {
  return ShowDlpWarningDialog(
      std::move(callback),
      DlpWarnDialog::DlpWarnDialogOptions(DlpWarnDialog::Restriction::kFiles,
                                          confidential_files, files_destination,
                                          files_action),
      modal_parent);
}

base::WeakPtr<views::Widget> DlpWarnNotifier::ShowDlpScreenShareWarningDialog(
    OnDlpRestrictionCheckedCallback callback,
    const DlpConfidentialContents& confidential_contents,
    const std::u16string& application_title) {
  return ShowDlpWarningDialog(std::move(callback),
                              DlpWarnDialog::DlpWarnDialogOptions(
                                  DlpWarnDialog::Restriction::kScreenShare,
                                  confidential_contents, application_title));
}

int DlpWarnNotifier::ActiveWarningDialogsCountForTesting() const {
  return widgets_.size();
}

base::WeakPtr<views::Widget> DlpWarnNotifier::ShowDlpWarningDialog(
    OnDlpRestrictionCheckedCallback callback,
    DlpWarnDialog::DlpWarnDialogOptions options,
    gfx::NativeWindow modal_parent) {
  views::Widget* widget;
  if (options.restriction == PolicyDialogBase::Restriction::kFiles) {
    widget = views::DialogDelegate::CreateDialogWidget(
        std::make_unique<FilesPolicyDialog>(
            std::move(callback), options.confidential_files,
            options.files_destination.value(), options.files_action.value(),
            modal_parent),
        /*context=*/nullptr, /*parent=*/modal_parent);
  } else {  // on-screen restriction
    widget = views::DialogDelegate::CreateDialogWidget(
        std::make_unique<DlpWarnDialog>(std::move(callback), options),
        /*context=*/nullptr, /*parent=*/nullptr);
  }

  widget->Show();
  // We disable the dialog's hide animations after showing it so that it doesn't
  // end up showing in the screenshots, video recording, or screen share.
  widget->GetNativeWindow()->SetProperty(aura::client::kAnimationsDisabledKey,
                                         true);
  // We set the dialog as the current capture window as it should be the target
  // for all input events.
  widget->GetNativeWindow()->SetCapture();
  widget->AddObserver(this);
  widgets_.push_back(widget);
  return widget->GetWeakPtr();
}

void DlpWarnNotifier::RemoveWidget(views::Widget* widget) {
  widget->RemoveObserver(this);
  base::EraseIf(widgets_, [=](views::Widget* widget_ptr) -> bool {
    return widget_ptr == widget;
  });
}

}  // namespace policy
