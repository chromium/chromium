// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_warn_notifier.h"

#include "chrome/browser/ash/policy/dlp/dlp_warn_dialog.h"
#include "ui/views/widget/widget.h"

namespace policy {

void DlpWarnNotifier::ShowDlpPrintWarningDialog(
    OnDlpRestrictionCheckedCallback callback) const {
  ShowDlpWarningDialog(std::move(callback),
                       DlpWarnDialog::DlpWarnDialogOptions(
                           DlpWarnDialog::Restriction::kPrinting));
}

void DlpWarnNotifier::ShowDlpScreenCaptureWarningDialog(
    OnDlpRestrictionCheckedCallback callback,
    const DlpConfidentialContents& confidential_contents) const {
  ShowDlpWarningDialog(
      std::move(callback),
      DlpWarnDialog::DlpWarnDialogOptions(
          DlpWarnDialog::Restriction::kScreenCapture, confidential_contents));
}

void DlpWarnNotifier::ShowDlpVideoCaptureWarningDialog(
    OnDlpRestrictionCheckedCallback callback,
    const DlpConfidentialContents& confidential_contents) const {
  ShowDlpWarningDialog(
      std::move(callback),
      DlpWarnDialog::DlpWarnDialogOptions(
          DlpWarnDialog::Restriction::kVideoCapture, confidential_contents));
}

void DlpWarnNotifier::ShowDlpScreenShareWarningDialog(
    OnDlpRestrictionCheckedCallback callback,
    const DlpConfidentialContents& confidential_contents,
    const std::u16string& application_title) const {
  ShowDlpWarningDialog(std::move(callback),
                       DlpWarnDialog::DlpWarnDialogOptions(
                           DlpWarnDialog::Restriction::kScreenShare,
                           confidential_contents, application_title));
}

void DlpWarnNotifier::ShowDlpWarningDialog(
    OnDlpRestrictionCheckedCallback callback,
    DlpWarnDialog::DlpWarnDialogOptions options) const {
  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      new DlpWarnDialog(std::move(callback), options),
      /*context=*/nullptr, /*parent=*/nullptr);
  widget->Show();
}

}  // namespace policy
