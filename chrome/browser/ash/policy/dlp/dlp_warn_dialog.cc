// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_warn_dialog.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/style/color_provider.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"

namespace policy {

namespace {

// The corner radius.
constexpr int kDialogCornerRadius = 12;

// The insets of the margins.
constexpr gfx::Insets kMarginInsets(24, 24, 20, 24);

// The size of the managed icon.
constexpr int kManagedIconSize = 32;

// The font used for in the dialog.
constexpr char kFontName[] = "Roboto";

// The font size of the text.
constexpr int kBodyFontSize = 14;

// The line height of the text.
constexpr int kBodyLineHeight = 20;

// The font size of the title.
constexpr int kTitleFontSize = 16;

// The line height of the title.
constexpr int kTitleLineHeight = 24;

// The width of the dialog.
constexpr int kDialogWidth = 360;

// Id of the column in the grid layout.
constexpr int kColumnSetId = 0;

// The spacing between the managed icon and the title label.
constexpr int kIconTitleSpacing = 16;

// The spacing between the title and body labels.
constexpr int kTitleBodySpacing = 8;

// The spacing between body label and the buttons.
constexpr int kBodyButtonsSpacing = 36;

// Returns the ID of the message or the title for the notification based on
// |allowance| and |for_title|.
const std::u16string GetDialogButtonOkLabel(
    DlpWarnDialog::Restriction restriction) {
  switch (restriction) {
    case DlpWarnDialog::Restriction::kScreenCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_SCREEN_CAPTURE_WARN_CONTINUE_BUTTON);
    case DlpWarnDialog::Restriction::kVideoCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_VIDEO_CAPTURE_WARN_CONTINUE_BUTTON);
    case DlpWarnDialog::Restriction::kPrinting:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_PRINTING_WARN_CONTINUE_BUTTON);
  }
}

const std::u16string GetDialogButtonCancelLabel(
    DlpWarnDialog::Restriction restriction) {
  switch (restriction) {
    case DlpWarnDialog::Restriction::kScreenCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_SCREEN_CAPTURE_WARN_CANCEL_BUTTON);
    case DlpWarnDialog::Restriction::kVideoCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_VIDEO_CAPTURE_WARN_CANCEL_BUTTON);
    case DlpWarnDialog::Restriction::kPrinting:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_PRINTING_WARN_CANCEL_BUTTON);
  }
}

const std::u16string GetDialogTitle(DlpWarnDialog::Restriction restriction) {
  switch (restriction) {
    case DlpWarnDialog::Restriction::kScreenCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_SCREEN_CAPTURE_WARN_TITLE);
    case DlpWarnDialog::Restriction::kVideoCapture:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_VIDEO_CAPTURE_WARN_TITLE);
    case DlpWarnDialog::Restriction::kPrinting:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_PRINTING_WARN_TITLE);
  }
}

const std::u16string GetDialogBody(DlpWarnDialog::Restriction restriction) {
  switch (restriction) {
    case DlpWarnDialog::Restriction::kScreenCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_SCREEN_CAPTURE_WARN_MESSAGE);
    case DlpWarnDialog::Restriction::kVideoCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_VIDEO_CAPTURE_WARN_MESSAGE);
    case DlpWarnDialog::Restriction::kPrinting:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_PRINTING_WARN_MESSAGE);
  }
}

std::unique_ptr<views::ImageView> InitializeIcon() {
  ash::ColorProvider* color_provider = ash::ColorProvider::Get();
  std::unique_ptr<views::ImageView> managed_icon =
      std::make_unique<views::ImageView>();
  managed_icon->SetImage(gfx::CreateVectorIcon(
      vector_icons::kBusinessIcon, kManagedIconSize,
      color_provider->GetContentLayerColor(
          ash::ColorProvider::ContentLayerType::kIconColorPrimary)));
  return managed_icon;
}

std::unique_ptr<views::Label> InitializeTitle(
    DlpWarnDialog::Restriction restriction) {
  ash::ColorProvider* color_provider = ash::ColorProvider::Get();
  std::unique_ptr<views::Label> title_label =
      std::make_unique<views::Label>(GetDialogTitle(restriction));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetAllowCharacterBreak(true);
  title_label->SetEnabledColor(color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kTextColorPrimary));
  title_label->SetFontList(gfx::FontList({kFontName}, gfx::Font::NORMAL,
                                         kTitleFontSize,
                                         gfx::Font::Weight::MEDIUM));
  title_label->SetLineHeight(kTitleLineHeight);
  return title_label;
}

std::unique_ptr<views::Label> InitializeBody(
    DlpWarnDialog::Restriction restriction) {
  ash::ColorProvider* color_provider = ash::ColorProvider::Get();
  std::unique_ptr<views::Label> body =
      std::make_unique<views::Label>(GetDialogBody(restriction));
  body->SetMultiLine(true);
  body->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  body->SetAllowCharacterBreak(true);
  body->SetEnabledColor(color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kTextColorSecondary));
  body->SetFontList(gfx::FontList({kFontName}, gfx::Font::NORMAL, kBodyFontSize,
                                  gfx::Font::Weight::NORMAL));
  body->SetLineHeight(kBodyLineHeight);
  body->SizeToFit(kDialogWidth);
  return body;
}

}  // namespace

DlpWarnDialog::DlpWarnDialog(base::OnceClosure accept_callback,
                             base::OnceClosure cancel_callback,
                             Restriction restriction) {
  SetAcceptCallback(std::move(accept_callback));
  SetCancelCallback(std::move(cancel_callback));

  InitializeView(restriction);
}

void DlpWarnDialog::InitializeView(Restriction restriction) {
  SetModalType(ui::MODAL_TYPE_SYSTEM);

  SetShowCloseButton(false);
  SetButtonLabel(ui::DIALOG_BUTTON_OK, GetDialogButtonOkLabel(restriction));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 GetDialogButtonCancelLabel(restriction));

  set_fixed_width(kDialogWidth);
  set_corner_radius(kDialogCornerRadius);
  set_margins(kMarginInsets);

  views::GridLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  views::ColumnSet* cs = layout_manager->AddColumnSet(kColumnSetId);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred, kDialogWidth, 0);

  layout_manager->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  layout_manager->AddView(InitializeIcon());
  layout_manager->AddPaddingRow(views::GridLayout::kFixedSize,
                                kIconTitleSpacing);

  layout_manager->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  layout_manager->AddView(InitializeTitle(restriction));
  layout_manager->AddPaddingRow(views::GridLayout::kFixedSize,
                                kTitleBodySpacing);
  layout_manager->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  layout_manager->AddView(InitializeBody(restriction));
  layout_manager->AddPaddingRow(views::GridLayout::kFixedSize,
                                kBodyButtonsSpacing);
}

BEGIN_METADATA(DlpWarnDialog, views::DialogDelegateView)
END_METADATA

}  // namespace policy
