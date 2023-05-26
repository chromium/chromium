// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dialogs/dlp_warn_dialog.h"

#include <memory>
#include <string>
#include <utility>

#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace policy {

DlpWarnDialog::DlpWarnDialogOptions::DlpWarnDialogOptions(
    Restriction restriction)
    : restriction(restriction) {}

DlpWarnDialog::DlpWarnDialogOptions::DlpWarnDialogOptions(
    Restriction restriction,
    DlpConfidentialContents confidential_contents)
    : restriction(restriction), confidential_contents(confidential_contents) {}

DlpWarnDialog::DlpWarnDialogOptions::DlpWarnDialogOptions(
    Restriction restriction,
    DlpConfidentialContents confidential_contents,
    const std::u16string& application_title_)
    : restriction(restriction), confidential_contents(confidential_contents) {
  application_title.emplace(application_title_);
}

DlpWarnDialog::DlpWarnDialogOptions::DlpWarnDialogOptions(
    const DlpWarnDialogOptions& other) = default;

DlpWarnDialog::DlpWarnDialogOptions&
DlpWarnDialog::DlpWarnDialogOptions::operator=(
    const DlpWarnDialogOptions& other) = default;

DlpWarnDialog::DlpWarnDialogOptions::~DlpWarnDialogOptions() = default;

DlpWarnDialog::DlpWarnDialog(OnDlpRestrictionCheckedCallback callback,
                             DlpWarnDialogOptions options)
    : restriction_(options.restriction),
      application_title_(options.application_title),
      contents_(std::move(options.confidential_contents)) {
  SetOnDlpRestrictionCheckedCallback(std::move(callback));

  SetModalType(ui::MODAL_TYPE_SYSTEM);

  SetButtonLabel(ui::DIALOG_BUTTON_OK, GetOkButton());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, GetCancelButton());

  AddGeneralInformation();
  MaybeAddConfidentialRows();
}

DlpWarnDialog::~DlpWarnDialog() = default;

void DlpWarnDialog::AddGeneralInformation() {
  SetupUpperPanel(GetTitle(), GetMessage());
}

void DlpWarnDialog::MaybeAddConfidentialRows() {
  if (contents_.IsEmpty()) {
    return;
  }

  SetupScrollView();
  for (const DlpConfidentialContent& content : contents_.GetContents()) {
    AddConfidentialRow(content.icon, content.title);
  }
}

std::u16string DlpWarnDialog::GetOkButton() {
  switch (restriction_) {
    case DlpWarnDialog::Restriction::kScreenCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_SCREEN_CAPTURE_WARN_CONTINUE_BUTTON);
    case DlpWarnDialog::Restriction::kVideoCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_VIDEO_CAPTURE_WARN_CONTINUE_BUTTON);
    case DlpWarnDialog::Restriction::kPrinting:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_PRINTING_WARN_CONTINUE_BUTTON);
    case DlpWarnDialog::Restriction::kScreenShare:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_SCREEN_SHARE_WARN_CONTINUE_BUTTON);
    case DlpWarnDialog::Restriction::kFiles:
      NOTREACHED();
      return u"";
  }
}

std::u16string DlpWarnDialog::GetCancelButton() {
  switch (restriction_) {
    case DlpWarnDialog::Restriction::kVideoCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_VIDEO_CAPTURE_WARN_CANCEL_BUTTON);
    case DlpWarnDialog::Restriction::kScreenCapture:
    case DlpWarnDialog::Restriction::kPrinting:
    case DlpWarnDialog::Restriction::kScreenShare:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_WARN_CANCEL_BUTTON);
    case DlpWarnDialog::Restriction::kFiles:
      NOTREACHED();
      return u"";
  }
}

std::u16string DlpWarnDialog::GetTitle() {
  switch (restriction_) {
    case DlpWarnDialog::Restriction::kScreenCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_SCREEN_CAPTURE_WARN_TITLE);
    case DlpWarnDialog::Restriction::kVideoCapture:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_VIDEO_CAPTURE_WARN_TITLE);
    case DlpWarnDialog::Restriction::kPrinting:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_PRINTING_WARN_TITLE);
    case DlpWarnDialog::Restriction::kScreenShare:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_SCREEN_SHARE_WARN_TITLE);
    case DlpWarnDialog::Restriction::kFiles:
      NOTREACHED();
      return u"";
  }
}

std::u16string DlpWarnDialog::GetMessage() {
  switch (restriction_) {
    case DlpWarnDialog::Restriction::kScreenCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_SCREEN_CAPTURE_WARN_MESSAGE);
    case DlpWarnDialog::Restriction::kVideoCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_VIDEO_CAPTURE_WARN_MESSAGE);
    case DlpWarnDialog::Restriction::kPrinting:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_PRINTING_WARN_MESSAGE);
    case DlpWarnDialog::Restriction::kScreenShare:
      DCHECK(application_title_.has_value());
      return l10n_util::GetStringFUTF16(
          IDS_POLICY_DLP_SCREEN_SHARE_WARN_MESSAGE, application_title_.value());
    case DlpWarnDialog::Restriction::kFiles:
      NOTREACHED();
      return u"";
  }
}

BEGIN_METADATA(DlpWarnDialog, PolicyDialogBase)
END_METADATA

}  // namespace policy
