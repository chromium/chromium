// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/remove_query_confirmation_dialog.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/view_shadow.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/highlight_border.h"
#include "ash/style/pill_button.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kDialogWidth = 360;

constexpr gfx::Insets kDialogContentInsets = gfx::Insets(20, 24);
constexpr float kDialogRoundedCornerRadius = 16.0f;
constexpr int kDialogShadowElevation = 3;

constexpr int kMarginBetweenTitleAndBody = 8;
constexpr int kMarginBetweenBodyAndButtons = 20;
constexpr int kMarginBetweenButtons = 8;

}  // namespace

RemoveQueryConfirmationDialog::RemoveQueryConfirmationDialog(
    const std::u16string& query,
    RemovalConfirmationCallback confirm_callback)
    : confirm_callback_(std::move(confirm_callback)) {
  SetModalType(ui::MODAL_TYPE_WINDOW);

  SetPaintToLayer();
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  view_shadow_ = std::make_unique<ViewShadow>(this, kDialogShadowElevation);
  view_shadow_->SetRoundedCornerRadius(kDialogRoundedCornerRadius);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kDialogContentInsets));

  // Add dialog title.
  title_ = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_REMOVE_ZERO_STATE_SUGGESTION_TITLE)));
  title_->SetTextContext(views::style::CONTEXT_DIALOG_TITLE);
  title_->SetTextStyle(ash::STYLE_EMPHASIZED);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // Needs to paint to layer so it's stacked above `this` view.
  title_->SetPaintToLayer();
  title_->layer()->SetFillsBoundsOpaquely(false);

  // Add dialog body.
  body_ =
      AddChildView(std::make_unique<views::Label>(l10n_util::GetStringFUTF16(
          IDS_REMOVE_ZERO_STATE_SUGGESTION_DETAILS, query)));
  body_->SetProperty(views::kMarginsKey,
                     gfx::Insets(kMarginBetweenTitleAndBody, 0,
                                 kMarginBetweenBodyAndButtons, 0));
  body_->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  body_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  body_->SetMultiLine(true);
  body_->SetAllowCharacterBreak(true);
  // Needs to paint to layer so it's stacked above `this` view.
  body_->SetPaintToLayer();
  body_->layer()->SetFillsBoundsOpaquely(false);

  auto run_callback = [](RemoveQueryConfirmationDialog* dialog, bool accept) {
    if (!dialog->confirm_callback_)
      return;

    std::move(dialog->confirm_callback_).Run(accept);

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
      l10n_util::GetStringUTF16(IDS_APP_CANCEL), PillButton::Type::kIconless,
      nullptr));
  accept_button_ = button_row->AddChildView(std::make_unique<ash::PillButton>(
      views::Button::PressedCallback(
          base::BindRepeating(run_callback, base::Unretained(this), true)),
      l10n_util::GetStringUTF16(IDS_REMOVE_SUGGESTION_BUTTON_LABEL),
      PillButton::Type::kIconlessProminent, nullptr));
}

RemoveQueryConfirmationDialog::~RemoveQueryConfirmationDialog() = default;

const char* RemoveQueryConfirmationDialog::GetClassName() const {
  return "RemoveQueryConfirmationDialog";
}

gfx::Size RemoveQueryConfirmationDialog::CalculatePreferredSize() const {
  const int default_width = kDialogWidth;
  return gfx::Size(default_width, GetHeightForWidth(default_width));
}

void RemoveQueryConfirmationDialog::OnThemeChanged() {
  views::WidgetDelegateView::OnThemeChanged();

  SetBackground(views::CreateRoundedRectBackground(
      AshColorProvider::Get()->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kTransparent80),
      kDialogRoundedCornerRadius));
  SetBorder(std::make_unique<HighlightBorder>(
      kDialogRoundedCornerRadius, HighlightBorder::Type::kHighlightBorder1,
      /*use_light_colors=*/false));
  title_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  body_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
}

}  // namespace ash
