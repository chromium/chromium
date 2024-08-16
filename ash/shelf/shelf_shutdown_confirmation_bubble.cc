// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_shutdown_confirmation_bubble.h"

#include "ash/shelf/login_shelf_button.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace ash {
namespace {

// The insets of the shutdown confirmation bubble. The value of
// Insets-leading-side is taken from the system.
constexpr int kShutdownConfirmationBubbleInsetsBottom = 12;
constexpr int kShutdownConfirmationBubbleInsetsTop = 8;

gfx::Insets GetShutdownConfirmationBubbleInsets(aura::Window* window) {
  gfx::Insets insets = GetTrayBubbleInsets(window);
  insets.set_top(kShutdownConfirmationBubbleInsetsTop);
  insets.set_bottom(kShutdownConfirmationBubbleInsetsBottom);
  return insets;
}

// Histogram for tracking the number of actions on the shelf shutdown
// confirmation bubble.
constexpr char kActionHistogramName[] =
    "Ash.Shelf.ShutdownConfirmationBubble.Action";

}  // namespace

ShelfShutdownConfirmationBubble::ShelfShutdownConfirmationBubble(
    LoginShelfButton* anchor,
    ShelfAlignment alignment,
    base::OnceClosure on_confirm_callback,
    base::OnceClosure on_cancel_callback)
    : ShelfBubble(anchor,
                  alignment,
                  /*for_tooltip=*/false,
                  /*arrow_position=*/std::nullopt),
      anchor_(anchor) {
  DCHECK(on_confirm_callback);
  DCHECK(on_cancel_callback);
  confirm_callback_ = std::move(on_confirm_callback);
  cancel_callback_ = std::move(on_cancel_callback);

  auto* layout_provider = views::LayoutProvider::Get();
  const gfx::Insets kShutdownConfirmationBubbleInsets =
      GetShutdownConfirmationBubbleInsets(
          anchor_widget()->GetNativeWindow()->GetRootWindow());
  const gfx::Insets dialog_insets =
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG);
  set_margins(kShutdownConfirmationBubbleInsets + dialog_insets);
  set_close_on_deactivate(true);
  SetCloseCallback(base::BindOnce(&ShelfShutdownConfirmationBubble::OnClosed,
                                  base::Unretained(this)));

  views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStart);

  // Set up the icon.
  icon_ = AddChildView(std::make_unique<views::ImageView>());
  icon_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, 0,
                        layout_provider->GetDistanceMetric(
                            views::DISTANCE_UNRELATED_CONTROL_VERTICAL),
                        0));

  // Set up the title view.
  title_ = AddChildView(std::make_unique<views::Label>());
  title_->SetMultiLine(true);
  title_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  title_->SetAutoColorReadabilityEnabled(false);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosHeadline1,
                                        *title_);
  title_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_SHUTDOWN_CONFIRMATION_TITLE));
  title_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, 0,
                        layout_provider->GetDistanceMetric(
                            views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT),
                        0));

  // Set up layout row for the buttons of cancellation and confirmation.
  views::View* button_container = AddChildView(std::make_unique<views::View>());

  button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));

  auto cancel_button = std::make_unique<PillButton>(
      base::BindRepeating(&ShelfShutdownConfirmationBubble::OnCancelled,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_CANCEL),
      PillButton::Type::kDefaultWithoutIcon,
      /*icon=*/nullptr);
  cancel_button->SetID(static_cast<int>(ButtonId::kCancel));
  cancel_ = button_container->AddChildView(std::move(cancel_button));

  auto confirm_button = std::make_unique<PillButton>(
      base::BindRepeating(&ShelfShutdownConfirmationBubble::OnConfirmed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_ASH_SHUTDOWN_CONFIRM_BUTTON),
      PillButton::Type::kDefaultWithoutIcon, /*icon=*/nullptr);
  confirm_button->SetID(static_cast<int>(ButtonId::kShutdown));
  confirm_ = button_container->AddChildView(std::move(confirm_button));

  CreateBubble();

  auto bubble_border =
      std::make_unique<views::BubbleBorder>(arrow(), GetShadow());
  bubble_border->set_insets(kShutdownConfirmationBubbleInsets);
  bubble_border->SetCornerRadius(
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kHigh));
  GetBubbleFrameView()->SetBubbleBorder(std::move(bubble_border));
  GetBubbleFrameView()->SetBackgroundColor(GetBackgroundColor());
  // The bubble content size changes after border setting, therefore resize
  // the widget to its content.
  // TODO(crbug.com/41493925): widget should autoresize to its content.
  SizeToContents();
  GetWidget()->Show();

  base::UmaHistogramEnumeration(
      kActionHistogramName,
      ShelfShutdownConfirmationBubble::BubbleAction::kOpened);

  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  GetViewAccessibility().SetName(title_->GetText());
}

ShelfShutdownConfirmationBubble::~ShelfShutdownConfirmationBubble() {
  // In case shutdown confirmation bubble was dismissed, the pointer of the
  // ShelfShutdownConfirmationBubble in LoginShelfView shall be cleaned up.
  if (cancel_callback_) {
    std::move(cancel_callback_).Run();
  }
}

void ShelfShutdownConfirmationBubble::OnThemeChanged() {
  views::View::OnThemeChanged();
  auto* color_provider = AshColorProvider::Get();

  SkColor icon_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  icon_->SetImage(
      gfx::CreateVectorIcon(vector_icons::kWarningOutlineIcon, icon_color));

  SkColor label_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  title_->SetEnabledColor(label_color);

  SkColor button_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonLabelColor);
  cancel_->SetEnabledTextColors(button_color);
  confirm_->SetEnabledTextColors(button_color);
}

std::u16string ShelfShutdownConfirmationBubble::GetAccessibleWindowTitle()
    const {
  return title_->GetText();
}

void ShelfShutdownConfirmationBubble::OnCancelled() {
  dialog_result_ = DialogResult::kCancelled;
  GetWidget()->Close();
}

void ShelfShutdownConfirmationBubble::OnConfirmed() {
  dialog_result_ = DialogResult::kConfirmed;
  GetWidget()->Close();
}

void ShelfShutdownConfirmationBubble::OnClosed() {
  anchor_->SetIsActive(false);
  switch (dialog_result_) {
    case DialogResult::kCancelled:
      ReportBubbleAction(
          ShelfShutdownConfirmationBubble::BubbleAction::kCancelled);
      std::move(cancel_callback_).Run();
      break;
    case DialogResult::kConfirmed:
      ReportBubbleAction(
          ShelfShutdownConfirmationBubble::BubbleAction::kConfirmed);
      std::move(confirm_callback_).Run();
      break;
    case DialogResult::kNone:
      ReportBubbleAction(
          ShelfShutdownConfirmationBubble::BubbleAction::kDismissed);
      break;
  }
}

bool ShelfShutdownConfirmationBubble::ShouldCloseOnPressDown() {
  return true;
}

bool ShelfShutdownConfirmationBubble::ShouldCloseOnMouseExit() {
  return false;
}

void ShelfShutdownConfirmationBubble::ReportBubbleAction(
    ShelfShutdownConfirmationBubble::BubbleAction action) {
  base::UmaHistogramEnumeration(kActionHistogramName, action);
}

}  // namespace ash
