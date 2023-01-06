// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/remove_query_confirmation_dialog.h"

#include <memory>
#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/view_shadow.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "base/functional/bind.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
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

RemoveQueryConfirmationDialog::RemoveQueryConfirmationDialog(
    RemovalConfirmationCallback confirm_callback,
    const std::u16string& result_title)
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
  title_->SetTextStyle(views::style::STYLE_EMPHASIZED);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetAutoColorReadabilityEnabled(false);
  // Needs to paint to layer so it's stacked above `this` view.
  title_->SetPaintToLayer();
  title_->layer()->SetFillsBoundsOpaquely(false);
  // Ignore labels for accessibility - the accessible name is defined for the
  // whole dialog view.
  title_->GetViewAccessibility().OverrideIsIgnored(true);

  // Add dialog body.
  body_ =
      AddChildView(std::make_unique<views::Label>(l10n_util::GetStringFUTF16(
          IDS_REMOVE_ZERO_STATE_SUGGESTION_DETAILS, result_title)));
  body_->SetProperty(views::kMarginsKey,
                     gfx::Insets::TLBR(kMarginBetweenTitleAndBody, 0,
                                       kMarginBetweenBodyAndButtons, 0));
  body_->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  body_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  body_->SetMultiLine(true);
  body_->SetAllowCharacterBreak(true);
  body_->SetAutoColorReadabilityEnabled(false);
  // Needs to paint to layer so it's stacked above `this` view.
  body_->SetPaintToLayer();
  body_->layer()->SetFillsBoundsOpaquely(false);
  // Ignore labels for accessibility - the accessible name is defined for the
  // whole dialog view.
  body_->GetViewAccessibility().OverrideIsIgnored(true);

  auto run_callback = [](RemoveQueryConfirmationDialog* dialog, bool accept) {
    if (!dialog->confirm_callback_)
      return;

    if (accept) {
      Shell::Get()
          ->accessibility_controller()
          ->TriggerAccessibilityAlertWithMessage(
              l10n_util::GetStringUTF8(IDS_REMOVE_SUGGESTION_ANNOUNCEMENT));
    }

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
      l10n_util::GetStringUTF16(IDS_APP_CANCEL),
      PillButton::Type::kDefaultWithoutIcon, nullptr));
  accept_button_ = button_row->AddChildView(std::make_unique<ash::PillButton>(
      views::Button::PressedCallback(
          base::BindRepeating(run_callback, base::Unretained(this), true)),
      l10n_util::GetStringUTF16(IDS_REMOVE_SUGGESTION_BUTTON_LABEL),
      PillButton::Type::kPrimaryWithoutIcon, nullptr));
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
  SetBorder(std::make_unique<views::HighlightBorder>(
      kDialogRoundedCornerRadius,
      views::HighlightBorder::Type::kHighlightBorder1,
      /*use_light_colors=*/false));
  title_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  body_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
}

void RemoveQueryConfirmationDialog::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  if (!GetVisible())
    return;
  node_data->role = ax::mojom::Role::kAlertDialog;
  node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kClick);
  node_data->SetName(base::JoinString(
      {l10n_util::GetStringUTF16(IDS_REMOVE_ZERO_STATE_SUGGESTION_TITLE),
       l10n_util::GetStringUTF16(IDS_REMOVE_ZERO_STATE_SUGGESTION_DETAILS)},
      u", "));
}

}  // namespace ash
