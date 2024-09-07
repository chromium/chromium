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
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/views/layout/box_layout.h"

namespace policy {

// The insets of a single confidential content row.
constexpr auto kConfidentialRowInsets = gfx::Insets::TLBR(6, 0, 6, 0);

// The font used for in the dialog.
constexpr char kFontName[] = "Roboto";

// The font size of the title.
constexpr int kTitleFontSize = 16;

// The line height of the title.
constexpr int kTitleLineHeight = 24;

// The font size of the text.
constexpr int kBodyFontSize = 14;

// The line height of the text.
constexpr int kBodyLineHeight = 20;

// The spacing between the elements in a box layout.
constexpr int kBetweenChildSpacing = 16;

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

DlpWarnDialog::DlpWarnDialog(WarningCallback callback,
                             DlpWarnDialogOptions options)
    : restriction_(options.restriction),
      application_title_(options.application_title),
      contents_(std::move(options.confidential_contents)) {
  SetWarningCallback(std::move(callback));

  set_margins(gfx::Insets::TLBR(20, 0, 20, 0));

  SetModalType(ui::mojom::ModalType::kSystem);

  SetButtonLabel(ui::mojom::DialogButton::kOk, GetOkButton());
  SetButtonLabel(ui::mojom::DialogButton::kCancel, GetCancelButton());

  AddGeneralInformation();
  MaybeAddConfidentialRows();
}

DlpWarnDialog::~DlpWarnDialog() = default;

void DlpWarnDialog::SetWarningCallback(WarningCallback callback) {
  auto split = base::SplitOnceCallback(std::move(callback));
  SetAcceptCallback(base::BindOnce(std::move(split.first), true));
  SetCancelCallback(base::BindOnce(std::move(split.second), false));
}

views::Label* DlpWarnDialog::AddTitle(const std::u16string& title) {
  // Call the parent class to setup the element. Do not remove.
  views::Label* title_label = PolicyDialogBase::AddTitle(title);
  title_label->SetFontList(gfx::FontList({kFontName}, gfx::Font::NORMAL,
                                         kTitleFontSize,
                                         gfx::Font::Weight::MEDIUM));
  title_label->SetLineHeight(kTitleLineHeight);
  return title_label;
}

views::Label* DlpWarnDialog::AddMessage(const std::u16string& message) {
  // Call the parent class to setup the element. Do not remove.
  views::Label* message_label = PolicyDialogBase::AddMessage(message);
  message_label->SetFontList(gfx::FontList({kFontName}, gfx::Font::NORMAL,
                                           kBodyFontSize,
                                           gfx::Font::Weight::NORMAL));
  message_label->SetLineHeight(kBodyLineHeight);
  return message_label;
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
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
      return u"";
  }
}

void DlpWarnDialog::AddConfidentialRow(const gfx::ImageSkia& icon,
                                       const std::u16string& title) {
  DCHECK(scroll_view_container_);
  views::View* row =
      scroll_view_container_->AddChildView(std::make_unique<views::View>());
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kConfidentialRowInsets,
      kBetweenChildSpacing));

  AddRowIcon(icon, row);

  views::Label* title_label = AddRowTitle(title, row);
  title_label->SetMultiLine(false);
  title_label->SetElideBehavior(gfx::ElideBehavior::FADE_TAIL);
  title_label->SetFontList(gfx::FontList({kFontName}, gfx::Font::NORMAL,
                                         kBodyFontSize,
                                         gfx::Font::Weight::NORMAL));
}

BEGIN_METADATA(DlpWarnDialog)
END_METADATA

}  // namespace policy
