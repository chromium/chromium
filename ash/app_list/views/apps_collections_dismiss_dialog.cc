// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/apps_collections_dismiss_dialog.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/ash_typography.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_shadow.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kDialogWidth = 360;

constexpr gfx::Insets kDialogContentInsets = gfx::Insets::VH(20, 24);
constexpr float kDialogRoundedCornerRadius = 16.0f;
constexpr int kDialogShadowElevation = 3;

constexpr int kMarginBetweenTitleAndBody = 8;
constexpr int kMarginBetweenBodyAndButtons = 20;
constexpr int kMarginBetweenButtons = 8;

}  // namespace

AppsCollectionsDismissDialog::AppsCollectionsDismissDialog(
    base::OnceClosure confirm_callback)
    : confirm_callback_(std::move(confirm_callback)) {
  SetModalType(ui::mojom::ModalType::kWindow);

  SetPaintToLayer();
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  view_shadow_ =
      std::make_unique<views::ViewShadow>(this, kDialogShadowElevation);
  view_shadow_->SetRoundedCornerRadius(kDialogRoundedCornerRadius);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kDialogContentInsets));

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysBaseElevated, kDialogRoundedCornerRadius));

  SetBorder(std::make_unique<views::HighlightBorder>(
      kDialogRoundedCornerRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));

  // Add dialog title.
  title_ =
      AddChildView(std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_APPS_COLLECTIONS_DISMISS_DIALOG_TITLE)));
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosTitle1, *title_);
  title_->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetAutoColorReadabilityEnabled(false);
  // Needs to paint to layer so it's stacked above `this` view.
  title_->SetPaintToLayer();
  title_->layer()->SetFillsBoundsOpaquely(false);
  // Ignore labels for accessibility - the accessible name is defined for the
  // whole dialog view.
  title_->GetViewAccessibility().SetIsIgnored(true);

  // Add dialog body.
  auto* body =
      AddChildView(std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_APPS_COLLECTIONS_DISMISS_DIALOG_SUBTITLE)));
  body->SetProperty(views::kMarginsKey,
                    gfx::Insets::TLBR(kMarginBetweenTitleAndBody, 0,
                                      kMarginBetweenBodyAndButtons, 0));
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody1, *body);
  body->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  body->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  body->SetMultiLine(true);
  body->SetAllowCharacterBreak(true);
  body->SetAutoColorReadabilityEnabled(false);
  // Needs to paint to layer so it's stacked above `this` view.
  body->SetPaintToLayer();
  body->layer()->SetFillsBoundsOpaquely(false);
  // Ignore labels for accessibility - the accessible name is defined for the
  // whole dialog view.
  body->GetViewAccessibility().SetIsIgnored(true);

  auto run_callback = [](AppsCollectionsDismissDialog* dialog, bool accept) {
    if (!dialog->confirm_callback_) {
      return;
    }
    if (accept) {
      std::move(dialog->confirm_callback_).Run();
    }

    dialog->GetWidget()->CloseWithReason(
        accept ? views::Widget::ClosedReason::kAcceptButtonClicked
               : views::Widget::ClosedReason::kCancelButtonClicked);
  };

  // Add button row.
  auto* button_row = AddChildView(std::make_unique<views::View>());
  button_row
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kMarginBetweenButtons))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);

  cancel_button_ = button_row->AddChildView(std::make_unique<ash::PillButton>(
      views::Button::PressedCallback(
          base::BindRepeating(run_callback, base::Unretained(this), false)),
      l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_APPS_COLLECTIONS_DISMISS_DIALOG_CANCEL),
      PillButton::Type::kDefaultWithoutIcon, nullptr));
  accept_button_ = button_row->AddChildView(std::make_unique<ash::PillButton>(
      views::Button::PressedCallback(
          base::BindRepeating(run_callback, base::Unretained(this), true)),
      l10n_util::GetStringUTF16(
          IDS_ASH_LAUNCHER_APPS_COLLECTIONS_DISMISS_DIALOG_EXIT),
      PillButton::Type::kPrimaryWithoutIcon, nullptr));

  GetViewAccessibility().SetRole(ax::mojom::Role::kAlertDialog);
  GetViewAccessibility().SetName(base::JoinString(
      {l10n_util::GetStringUTF16(
           IDS_ASH_LAUNCHER_APPS_COLLECTIONS_DISMISS_DIALOG_TITLE),
       l10n_util::GetStringUTF16(
           IDS_ASH_LAUNCHER_APPS_COLLECTIONS_DISMISS_DIALOG_SUBTITLE)},
      u", "));
}

AppsCollectionsDismissDialog::~AppsCollectionsDismissDialog() {}

gfx::Size AppsCollectionsDismissDialog::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int default_width = kDialogWidth;
  return gfx::Size(
      default_width,
      GetLayoutManager()->GetPreferredHeightForWidth(this, default_width));
}

BEGIN_METADATA(AppsCollectionsDismissDialog)
END_METADATA

}  // namespace ash
