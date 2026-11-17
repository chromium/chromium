// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dialogs/dlp_warn_notifier.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/dlp_warn_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"

namespace policy {

DlpWarnNotifier::DlpWarnNotifier() = default;

DlpWarnNotifier::~DlpWarnNotifier() = default;

void DlpWarnNotifier::ShowDlpPrintWarningDialog(WarningCallback callback) {
  ShowDlpWarningDialog(std::move(callback),
                       DlpWarnDialog::DlpWarnDialogOptions(
                           DlpWarnDialog::Restriction::kPrinting));
}

void DlpWarnNotifier::ShowDlpScreenCaptureWarningDialog(
    WarningCallback callback,
    const DlpConfidentialContents& confidential_contents) {
  ShowDlpWarningDialog(
      std::move(callback),
      DlpWarnDialog::DlpWarnDialogOptions(
          DlpWarnDialog::Restriction::kScreenCapture, confidential_contents));
}

void DlpWarnNotifier::ShowDlpVideoCaptureWarningDialog(
    WarningCallback callback,
    const DlpConfidentialContents& confidential_contents) {
  ShowDlpWarningDialog(
      std::move(callback),
      DlpWarnDialog::DlpWarnDialogOptions(
          DlpWarnDialog::Restriction::kVideoCapture, confidential_contents));
}

base::WeakPtr<views::Widget> DlpWarnNotifier::ShowDlpScreenShareWarningDialog(
    WarningCallback callback,
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
    WarningCallback callback,
    DlpWarnDialog::DlpWarnDialogOptions options) {
  auto dialog = std::make_unique<DlpWarnDialog>(std::move(callback), options);
  dialog->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  std::unique_ptr<views::Widget> widget =
      base::WrapUnique(views::DialogDelegate::CreateDialogWidget(
          std::move(dialog), /*context=*/nullptr, /*parent=*/nullptr));
  base::WeakPtr<views::Widget> weak_widget = widget->GetWeakPtr();
  ShowWidget(std::move(widget));
  return weak_widget;
}

void DlpWarnNotifier::ShowWidget(std::unique_ptr<views::Widget> widget) {
  widget->Show();
  // We disable the dialog's hide animations after showing it so that it doesn't
  // end up showing in the screenshots, video recording, or screen share.
  widget->GetNativeWindow()->SetProperty(aura::client::kAnimationsDisabledKey,
                                         true);
  // We set the dialog as the current capture window as it should be the target
  // for all input events.
  widget->GetNativeWindow()->SetCapture();
  // This is set only after Show(), because Widget::Show() requires the widget
  // to outlive it. A close during Show() takes the usual asynchronous path, and
  // this callback still runs when the dialog's window is destroyed.
  widget->MakeCloseSynchronous(base::BindOnce(
      &DlpWarnNotifier::RemoveWidget, base::Unretained(this), widget.get()));
  widgets_.push_back(std::move(widget));
}

void DlpWarnNotifier::RemoveWidget(views::Widget* widget,
                                   views::Widget::ClosedReason reason) {
  auto it =
      std::ranges::find(widgets_, widget, &std::unique_ptr<views::Widget>::get);
  CHECK(it != widgets_.end());
  // Take the widget out of `widgets_` before destroying it, so that the list is
  // consistent if the widget's destruction re-enters this class.
  std::unique_ptr<views::Widget> closing_widget = std::move(*it);
  widgets_.erase(it);
}

}  // namespace policy
