// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_warn_dialog.h"

#include <memory>
#include <string>
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
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/grid_layout.h"

namespace policy {

namespace {

// The corner radius.
constexpr int kDialogCornerRadius = 12;

// The insets of the margins (top, left, bottom, right).
constexpr gfx::Insets kMarginInsets(20, 24, 20, 24);

// The size of the managed icon.
constexpr int kManagedIconSize = 32;

// The size of the favicon.
constexpr int kFaviconSize = 20;

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

// The spacing between the managed icon and the title.
constexpr int kAfterIconSpacing = 16;

// The spacing between the title and the body.
constexpr int kAfterTitleSpacing = 16;

// The spacing between the body and the confidential content list.
constexpr int kBodyContentSpacing = 16;

// The width of the padding column between the icon and the title of the
// confidential content list.
constexpr int kConfidentialContentListHorizontalPadding = 16;

// The padding on the top and bottom of the confidential list.
constexpr int kConfidentialContentListTopBottomPadding = 8;

// The padding on the top and bottom of each confidential content row.
constexpr int kConfidentialContentListVerticalPadding = 6;

// The line height of the confidential content title label.
constexpr int kConfidentialContentLineHeight = 20;

// Maximum height of the confidential content scrollable list.
// This can hold seven rows.
constexpr int kConfidentialContentListMaxHeight = 240;

// The spacing between body label and the buttons.
constexpr int kAfterBodySpacing = 20;

// Returns the OK button label for |restriction|.
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
    case DlpWarnDialog::Restriction::kScreenShare:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_SCREEN_SHARE_WARN_CONTINUE_BUTTON);
  }
}

// Returns the Cancel button label for |restriction|.
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
    case DlpWarnDialog::Restriction::kScreenShare:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_SCREEN_SHARE_WARN_CANCEL_BUTTON);
  }
}

// Returns the title for |restriction|.
const std::u16string GetTitle(DlpWarnDialog::Restriction restriction) {
  switch (restriction) {
    case DlpWarnDialog::Restriction::kScreenCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_SCREEN_CAPTURE_WARN_TITLE);
    case DlpWarnDialog::Restriction::kVideoCapture:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_VIDEO_CAPTURE_WARN_TITLE);
    case DlpWarnDialog::Restriction::kPrinting:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_PRINTING_WARN_TITLE);
    case DlpWarnDialog::Restriction::kScreenShare:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_SCREEN_SHARE_WARN_TITLE);
  }
}

// Returns the message for |restriction|.
const std::u16string GetMessage(DlpWarnDialog::DlpWarnDialogOptions options) {
  switch (options.restriction) {
    case DlpWarnDialog::Restriction::kScreenCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_SCREEN_CAPTURE_WARN_MESSAGE);
    case DlpWarnDialog::Restriction::kVideoCapture:
      return l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_VIDEO_CAPTURE_WARN_MESSAGE);
    case DlpWarnDialog::Restriction::kPrinting:
      return l10n_util::GetStringUTF16(IDS_POLICY_DLP_PRINTING_WARN_MESSAGE);
    case DlpWarnDialog::Restriction::kScreenShare:
      DCHECK(options.application_title.has_value());
      return l10n_util::GetStringFUTF16(
          IDS_POLICY_DLP_SCREEN_SHARE_WARN_MESSAGE,
          options.application_title.value());
  }
}

// Adds a managed icon and a padding row to the warn dialog's layout.
void AddManagedIcon(views::GridLayout* layout) {
  ash::ColorProvider* color_provider = ash::ColorProvider::Get();
  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  // Add the managed icon
  views::ImageView* managed_icon =
      layout->AddView(std::make_unique<views::ImageView>());
  managed_icon->SetImage(gfx::CreateVectorIcon(
      vector_icons::kBusinessIcon, kManagedIconSize,
      color_provider->GetContentLayerColor(
          ash::ColorProvider::ContentLayerType::kIconColorPrimary)));
  // Add the padding after the managed icon
  layout->AddPaddingRow(views::GridLayout::kFixedSize, kAfterIconSpacing);
}

// Adds a title label and a padding row to the warn dialog's layout.
void AddTitle(views::GridLayout* layout,
              DlpWarnDialog::Restriction restriction) {
  ash::ColorProvider* color_provider = ash::ColorProvider::Get();
  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  // Add the title
  views::Label* title_label =
      layout->AddView(std::make_unique<views::Label>(GetTitle(restriction)));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetAllowCharacterBreak(true);
  title_label->SetEnabledColor(color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kTextColorPrimary));
  title_label->SetFontList(gfx::FontList({kFontName}, gfx::Font::NORMAL,
                                         kTitleFontSize,
                                         gfx::Font::Weight::MEDIUM));
  title_label->SetLineHeight(kTitleLineHeight);
  // Add padding after the title
  layout->AddPaddingRow(views::GridLayout::kFixedSize, kAfterTitleSpacing);
}

// Adds icon and title pair of the |confidential_content| to the container's
// layout.
void AddConfidentialContentRow(views::GridLayout* layout,
                               DlpConfidentialContent confidential_content) {
  layout->AddPaddingRow(views::GridLayout::kFixedSize,
                        kConfidentialContentListVerticalPadding);
  ash::ColorProvider* color_provider = ash::ColorProvider::Get();
  // Add the icon
  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  views::ImageView* icon =
      layout->AddView(std::make_unique<views::ImageView>());
  icon->SetImageSize(gfx::Size(kFaviconSize, kFaviconSize));
  icon->SetImage(confidential_content.icon);
  // Add the title
  views::Label* title = layout->AddView(
      std::make_unique<views::Label>(confidential_content.title));
  title->SetMultiLine(true);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetAllowCharacterBreak(true);
  title->SetEnabledColor(color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kTextColorSecondary));
  title->SetFontList(gfx::FontList({kFontName}, gfx::Font::NORMAL,
                                   kBodyFontSize, gfx::Font::Weight::NORMAL));
  title->SetLineHeight(kConfidentialContentLineHeight);
  title->SizeToFit(360);
  layout->AddPaddingRow(views::GridLayout::kFixedSize,
                        kConfidentialContentListVerticalPadding);
}

// If the |confidential_contents| is not empty, adds a padding row and a
// scrollable view listing the given contents to the warn dialog's layout.
// TODO(crbug.com/1264464): Adjust the width of the scrollable list
void MaybeAddConfidentialContent(
    views::GridLayout* layout,
    const DlpConfidentialContents& confidential_contents) {
  if (confidential_contents.IsEmpty())
    return;
  // First add padding between the message and the list
  layout->AddPaddingRow(views::GridLayout::kFixedSize, kBodyContentSpacing);
  // Create a scrollable view to hold the list and add all the items
  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  views::ScrollView* scroll_view =
      layout->AddView(std::make_unique<views::ScrollView>());
  scroll_view->ClipHeightTo(0, kConfidentialContentListMaxHeight);
  views::View* container =
      scroll_view->SetContents(std::make_unique<views::View>());
  views::GridLayout* scrollable_layout =
      container->SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* column_set = scrollable_layout->AddColumnSet(kColumnSetId);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::ColumnSize::kUsePreferred,
                        /*fixed_width=*/kFaviconSize, /*min_width=*/0);
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                               kConfidentialContentListHorizontalPadding);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                        /*resize_percent=*/1.0,
                        views::GridLayout::ColumnSize::kUsePreferred,
                        /*fixed_width=*/0, /*min_width=*/0);

  scrollable_layout->AddPaddingRow(views::GridLayout::kFixedSize,
                                   kConfidentialContentListTopBottomPadding);
  for (const DlpConfidentialContent& confidential_content :
       confidential_contents.GetContents())
    AddConfidentialContentRow(scrollable_layout, confidential_content);
  scrollable_layout->AddPaddingRow(views::GridLayout::kFixedSize,
                                   kConfidentialContentListTopBottomPadding);
}

// Adds dialog body to the warn dialog's layout, that consists of the main
// message determined by |restriction| and optionally a scrollable list of
// confidential content.
void AddBody(views::GridLayout* layout,
             DlpWarnDialog::DlpWarnDialogOptions options) {
  ash::ColorProvider* color_provider = ash::ColorProvider::Get();
  // Add the message
  layout->StartRow(views::GridLayout::kFixedSize, kColumnSetId);
  views::Label* message =
      layout->AddView(std::make_unique<views::Label>(GetMessage(options)));
  message->SetMultiLine(true);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message->SetAllowCharacterBreak(true);
  message->SetEnabledColor(color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kTextColorSecondary));
  message->SetFontList(gfx::FontList({kFontName}, gfx::Font::NORMAL,
                                     kBodyFontSize, gfx::Font::Weight::NORMAL));
  message->SetLineHeight(kBodyLineHeight);
  message->SizeToFit(kDialogWidth);
  // Add the confidential content list, if applicable
  MaybeAddConfidentialContent(layout, options.confidential_contents);
  // Add padding after the entire dialog body
  layout->AddPaddingRow(views::GridLayout::kFixedSize, kAfterBodySpacing);
}

}  // namespace

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

// static
void DlpWarnDialog::ShowDlpPrintWarningDialog(
    OnDlpRestrictionChecked callback) {
  ShowDlpWarningDialog(std::move(callback),
                       DlpWarnDialogOptions(Restriction::kPrinting));
}

// static
void DlpWarnDialog::ShowDlpScreenCaptureWarningDialog(
    OnDlpRestrictionChecked callback,
    const DlpConfidentialContents& confidential_contents) {
  ShowDlpWarningDialog(
      std::move(callback),
      DlpWarnDialogOptions(Restriction::kScreenCapture, confidential_contents));
}

// static
void DlpWarnDialog::ShowDlpVideoCaptureWarningDialog(
    OnDlpRestrictionChecked callback,
    const DlpConfidentialContents& confidential_contents) {
  ShowDlpWarningDialog(
      std::move(callback),
      DlpWarnDialogOptions(Restriction::kVideoCapture, confidential_contents));
}

// static
void DlpWarnDialog::ShowDlpScreenShareWarningDialog(
    OnDlpRestrictionChecked callback,
    const DlpConfidentialContents& confidential_contents,
    const std::u16string& application_title) {
  ShowDlpWarningDialog(
      std::move(callback),
      DlpWarnDialogOptions(Restriction::kScreenShare, confidential_contents,
                           application_title));
}

// static
void DlpWarnDialog::ShowDlpWarningDialog(OnDlpRestrictionChecked callback,
                                         DlpWarnDialogOptions options) {
  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      new DlpWarnDialog(std::move(callback), options),
      /*context=*/nullptr, /*parent=*/nullptr);
  widget->Show();
}

DlpWarnDialog::DlpWarnDialog(OnDlpRestrictionChecked callback,
                             DlpWarnDialogOptions options) {
  auto split = base::SplitOnceCallback(std::move(callback));
  SetAcceptCallback(base::BindOnce(std::move(split.first), true));
  SetCancelCallback(base::BindOnce(std::move(split.second), false));

  SetModalType(ui::MODAL_TYPE_SYSTEM);

  SetShowCloseButton(false);
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 GetDialogButtonOkLabel(options.restriction));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 GetDialogButtonCancelLabel(options.restriction));

  set_fixed_width(kDialogWidth);
  set_corner_radius(kDialogCornerRadius);
  set_margins(kMarginInsets);

  views::GridLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* cs = layout_manager->AddColumnSet(kColumnSetId);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred, kDialogWidth, 0);
  AddManagedIcon(layout_manager);
  AddTitle(layout_manager, options.restriction);
  AddBody(layout_manager, options);
}

BEGIN_METADATA(DlpWarnDialog, views::DialogDelegateView)
END_METADATA

}  // namespace policy
