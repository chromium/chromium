// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_forward.h"
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

// The insets in the upper part of the dialog.
constexpr auto kTopPanelInsets = gfx::Insets::TLBR(0, 24, 16, 24);

// The insests in the container holding the list of confidential contents.
constexpr auto kConfidentialListInsets = gfx::Insets::TLBR(8, 24, 8, 24);

// The spacing between the elements in a box layout.
constexpr int kBetweenChildSpacing = 16;

// The size of the managed icon.
constexpr int kManagedIconSize = 32;

// The size of the favicon.
constexpr int kFaviconSize = 20;

// Maximum height of the confidential content scrollable list.
// This can hold seven rows.
constexpr int kConfidentialContentListMaxHeight = 240;
}  // namespace

PolicyDialogBase::PolicyDialogBase() {
  SetShowCloseButton(false);

  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  set_corner_radius(kDialogCornerRadius);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
}

PolicyDialogBase::~PolicyDialogBase() = default;

void PolicyDialogBase::SetupUpperPanel() {
  upper_panel_ = AddChildView(std::make_unique<views::View>());
  views::BoxLayout* layout =
      upper_panel_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kTopPanelInsets,
          kBetweenChildSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  views::ImageView* managed_icon =
      upper_panel_->AddChildView(std::make_unique<views::ImageView>());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto color = ash::ColorProvider::Get()->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kIconColorPrimary);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/40202228) Enable dynamic UI color & theme in lacros
  auto color = SK_ColorGRAY;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  managed_icon->SetImage(gfx::CreateVectorIcon(vector_icons::kBusinessIcon,
                                               kManagedIconSize, color));
}

views::Label* PolicyDialogBase::AddTitle(const std::u16string& title) {
  DCHECK(upper_panel_);

  views::Label* title_label =
      upper_panel_->AddChildView(std::make_unique<views::Label>(title));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetAllowCharacterBreak(true);
// TODO(crbug.com/40202228) Enable dynamic UI color & theme in lacros
#if BUILDFLAG(IS_CHROMEOS_ASH)
  title_label->SetEnabledColor(ash::ColorProvider::Get()->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kTextColorPrimary));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return title_label;
}

views::Label* PolicyDialogBase::AddMessage(const std::u16string& message) {
  DCHECK(upper_panel_);

  views::Label* message_label =
      upper_panel_->AddChildView(std::make_unique<views::Label>(message));
  message_label->SetMultiLine(true);
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_label->SetAllowCharacterBreak(true);
// TODO(crbug.com/40202228) Enable dynamic UI color & theme in lacros
#if BUILDFLAG(IS_CHROMEOS_ASH)
  message_label->SetEnabledColor(
      ash::ColorProvider::Get()->GetContentLayerColor(
          ash::ColorProvider::ContentLayerType::kTextColorSecondary));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return message_label;
}

void PolicyDialogBase::SetupScrollView() {
  views::ScrollView* scroll_view =
      AddChildView(std::make_unique<views::ScrollView>());
  scroll_view->ClipHeightTo(0, kConfidentialContentListMaxHeight);
  scroll_view_container_ =
      scroll_view->SetContents(std::make_unique<views::View>());
  scroll_view_container_->SetID(kScrollViewId);
  views::BoxLayout* layout = scroll_view_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, kConfidentialListInsets,
          /*between_child_spacing=*/0));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
}

void PolicyDialogBase::AddGeneralInformation() {
  SetupUpperPanel();
  AddTitle(GetTitle());
  AddMessage(GetMessage());
}

void PolicyDialogBase::AddRowIcon(const gfx::ImageSkia& icon,
                                  views::View* row) {
  views::ImageView* icon_view =
      row->AddChildView(std::make_unique<views::ImageView>());
  icon_view->SetImageSize(gfx::Size(kFaviconSize, kFaviconSize));
  icon_view->SetImage(icon);
}

views::Label* PolicyDialogBase::AddRowTitle(const std::u16string& title,
                                            views::View* row) {
  views::Label* label =
      row->AddChildView(std::make_unique<views::Label>(title));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
// TODO(crbug.com/40202228) Enable dynamic UI color & theme in lacros
#if BUILDFLAG(IS_CHROMEOS_ASH)
  label->SetEnabledColor(ash::ColorProvider::Get()->GetContentLayerColor(
      ash::ColorProvider::ContentLayerType::kTextColorSecondary));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return label;
}

BEGIN_METADATA(PolicyDialogBase)
END_METADATA

}  // namespace policy
