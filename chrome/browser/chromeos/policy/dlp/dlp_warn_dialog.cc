// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_warn_dialog.h"

#include <memory>
#include <string>
#include <utility>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/style/color_provider.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace policy {

namespace {

// The corner radius.
constexpr int kDialogCornerRadius = 12;

// The dialog insets.
constexpr auto kMarginInsets = gfx::Insets::TLBR(20, 0, 20, 0);

// The insets in the upper part of the dialog.
constexpr auto kTopPanelInsets = gfx::Insets::TLBR(0, 24, 16, 24);

// The insests in the container holding the list of confidential contents.
constexpr auto kConfidentialListInsets = gfx::Insets::TLBR(8, 24, 8, 24);

// The insets of a single confidential content row.
constexpr auto kConfidentialRowInsets = gfx::Insets::TLBR(6, 0, 6, 0);

// The spacing between the elements in a box layout.
constexpr int kBetweenChildSpacing = 16;

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

// The line height of the confidential content title label.
constexpr int kConfidentialContentLineHeight = 20;

// Maximum height of the confidential content scrollable list.
// This can hold seven rows.
constexpr int kConfidentialContentListMaxHeight = 240;

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

// Constructs and adds the top part of the dialog, containing the managed
// icon, dialog title and the informative text.
void AddGeneralInformation(views::View* upper_panel,
                           DlpWarnDialog::DlpWarnDialogOptions options) {
// TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // When #dark-light-mode flag is disabled (default setting), the color mode is
  // by default set to dark mode. The warn dialog has white background for the
  // default setting, so it should use light mode color palette.
  ash::ScopedLightModeAsDefault scoped_light_mode_as_default;
  ash::ColorProvider* color_provider = ash::ColorProvider::Get();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  views::BoxLayout* layout =
      upper_panel->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kTopPanelInsets,
          kBetweenChildSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  views::ImageView* managed_icon =
      upper_panel->AddChildView(std::make_unique<views::ImageView>());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto color = color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kIconColorPrimary);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
  auto color = SK_ColorGRAY;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  managed_icon->SetImage(gfx::CreateVectorIcon(vector_icons::kBusinessIcon,
                                               kManagedIconSize, color));

  views::Label* title_label = upper_panel->AddChildView(
      std::make_unique<views::Label>(GetTitle(options.restriction)));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetAllowCharacterBreak(true);
// TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
#if BUILDFLAG(IS_CHROMEOS_ASH)
  title_label->SetEnabledColor(color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kTextColorPrimary));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  title_label->SetFontList(gfx::FontList({kFontName}, gfx::Font::NORMAL,
                                         kTitleFontSize,
                                         gfx::Font::Weight::MEDIUM));
  title_label->SetLineHeight(kTitleLineHeight);

  views::Label* message = upper_panel->AddChildView(
      std::make_unique<views::Label>(GetMessage(options)));
  message->SetMultiLine(true);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message->SetAllowCharacterBreak(true);
// TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
#if BUILDFLAG(IS_CHROMEOS_ASH)
  message->SetEnabledColor(color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kTextColorSecondary));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  message->SetFontList(gfx::FontList({kFontName}, gfx::Font::NORMAL,
                                     kBodyFontSize, gfx::Font::Weight::NORMAL));
  message->SetLineHeight(kBodyLineHeight);
}

// TODO(crbug.com/682266) Remove this function.
int GetMaxConfidentialTitleWidth() {
  int total_width = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  int margin_width = kMarginInsets.width() + kConfidentialListInsets.width() +
                     kConfidentialRowInsets.width();
  int image_width = kFaviconSize;
  int spacing = kBetweenChildSpacing;
  return total_width - margin_width - image_width - spacing;
}

// Adds icon and title pair of the |confidential_content| to the container.
void AddConfidentialContentRow(
    views::View* container,
    const DlpConfidentialContent& confidential_content) {
// TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // When #dark-light-mode flag is disabled (default setting), the color mode is
  // by default set to dark mode. The warn dialog has white background for the
  // default setting, so it should use light mode color palette.
  ash::ScopedLightModeAsDefault scoped_light_mode_as_default;
  ash::ColorProvider* color_provider = ash::ColorProvider::Get();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  views::View* row = container->AddChildView(std::make_unique<views::View>());
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kConfidentialRowInsets,
      kBetweenChildSpacing));

  views::ImageView* icon =
      row->AddChildView(std::make_unique<views::ImageView>());
  icon->SetImageSize(gfx::Size(kFaviconSize, kFaviconSize));
  icon->SetImage(confidential_content.icon);

  views::Label* title = row->AddChildView(
      std::make_unique<views::Label>(confidential_content.title));
  title->SetMultiLine(true);
  // TODO(crbug.com/682266) Remove the next line that sets the line size.
  title->SetMaximumWidth(GetMaxConfidentialTitleWidth());
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetAllowCharacterBreak(true);
// TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
#if BUILDFLAG(IS_CHROMEOS_ASH)
  title->SetEnabledColor(color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kTextColorSecondary));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  title->SetFontList(gfx::FontList({kFontName}, gfx::Font::NORMAL,
                                   kBodyFontSize, gfx::Font::Weight::NORMAL));
  title->SetLineHeight(kConfidentialContentLineHeight);
}

// Adds a scrollable child view to |parent| that lists the information from
// |confidential_contents|. No-op if no contents are given.
void MaybeAddConfidentialContent(
    views::View* parent,
    const DlpConfidentialContents& confidential_contents) {
  if (confidential_contents.IsEmpty())
    return;

  views::ScrollView* scroll_view =
      parent->AddChildView(std::make_unique<views::ScrollView>());
  scroll_view->ClipHeightTo(0, kConfidentialContentListMaxHeight);
  views::View* container =
      scroll_view->SetContents(std::make_unique<views::View>());
  views::BoxLayout* layout =
      container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kConfidentialListInsets,
          /*between_child_spacing=*/0));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  for (const DlpConfidentialContent& confidential_content :
       confidential_contents.GetContents()) {
    AddConfidentialContentRow(container, confidential_content);
  }
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

DlpWarnDialog::DlpWarnDialog(OnDlpRestrictionCheckedCallback callback,
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

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  set_corner_radius(kDialogCornerRadius);
  set_margins(kMarginInsets);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  AddGeneralInformation(AddChildView(std::make_unique<views::View>()), options);
  MaybeAddConfidentialContent(this, options.confidential_contents);
}

BEGIN_METADATA(DlpWarnDialog, views::DialogDelegateView)
END_METADATA

}  // namespace policy
