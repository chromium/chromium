// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"

#include <memory>
#include <string>
#include <utility>

#include "build/chromeos_buildflags.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
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
}  // namespace

PolicyDialogBase::PolicyDialogBase(OnDlpRestrictionCheckedCallback callback) {
  auto split = base::SplitOnceCallback(std::move(callback));
  SetAcceptCallback(base::BindOnce(std::move(split.first), true));
  SetCancelCallback(base::BindOnce(std::move(split.second), false));

  SetShowCloseButton(false);

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  set_corner_radius(kDialogCornerRadius);
  set_margins(kMarginInsets);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
}

void PolicyDialogBase::SetupUpperPanel(const std::u16string& title,
                                       const std::u16string& message) {
  upper_panel_ = AddChildView(std::make_unique<views::View>());

  // TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ColorProvider* color_provider = ash::ColorProvider::Get();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  views::BoxLayout* layout =
      upper_panel_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kTopPanelInsets,
          kBetweenChildSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  views::ImageView* managed_icon =
      upper_panel_->AddChildView(std::make_unique<views::ImageView>());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto color = color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kIconColorPrimary);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
  auto color = SK_ColorGRAY;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  managed_icon->SetImage(gfx::CreateVectorIcon(vector_icons::kBusinessIcon,
                                               kManagedIconSize, color));

  views::Label* title_label =
      upper_panel_->AddChildView(std::make_unique<views::Label>(title));
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

  views::Label* message_label =
      upper_panel_->AddChildView(std::make_unique<views::Label>(message));
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_label->SetAllowCharacterBreak(true);
// TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
#if BUILDFLAG(IS_CHROMEOS_ASH)
  message_label->SetEnabledColor(color_provider->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kTextColorSecondary));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  message_label->SetFontList(gfx::FontList({kFontName}, gfx::Font::NORMAL,
                                           kBodyFontSize,
                                           gfx::Font::Weight::NORMAL));
  message_label->SetLineHeight(kBodyLineHeight);
}

void PolicyDialogBase::SetupScrollView() {
  views::ScrollView* scroll_view =
      AddChildView(std::make_unique<views::ScrollView>());
  scroll_view->ClipHeightTo(0, kConfidentialContentListMaxHeight);
  scroll_view_container_ =
      scroll_view->SetContents(std::make_unique<views::View>());
  views::BoxLayout* layout = scroll_view_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kConfidentialListInsets,
          /*between_child_spacing=*/0));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
}

void PolicyDialogBase::AddConfidentialRow(
    const gfx::ImageSkia& confidential_icon,
    const std::u16string& confidential_title) {
  DCHECK(scroll_view_container_);
// TODO(crbug.com/1261496) Enable dynamic UI color & theme in lacros
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ColorProvider* color_provider = ash::ColorProvider::Get();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  views::View* row =
      scroll_view_container_->AddChildView(std::make_unique<views::View>());
  row->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kConfidentialRowInsets,
      kBetweenChildSpacing));

  views::ImageView* icon =
      row->AddChildView(std::make_unique<views::ImageView>());
  icon->SetImageSize(gfx::Size(kFaviconSize, kFaviconSize));
  icon->SetImage(confidential_icon);

  views::Label* title =
      row->AddChildView(std::make_unique<views::Label>(confidential_title));
  title->SetMultiLine(true);
  // TODO(crbug.com/682266) Remove the next line that sets the line size.
  // title->SetMaximumWidth(GetMaxConfidentialTitleWidth());
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

BEGIN_METADATA(PolicyDialogBase, views::DialogDelegateView)
END_METADATA

}  // namespace policy
