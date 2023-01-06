// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/kiosk_app_instruction_bubble.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/functional/callback_helpers.h"
#include "components/strings/grit/components_strings.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"

namespace ash {
namespace {

// The preferred internal width of the kiosk app instruction bubble.
constexpr int kBubblePreferredInternalWidth = 150;

views::BubbleBorder::Arrow GetArrow(ShelfAlignment alignment) {
  switch (alignment) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      return views::BubbleBorder::BOTTOM_LEFT;
    case ShelfAlignment::kLeft:
      return views::BubbleBorder::LEFT_TOP;
    case ShelfAlignment::kRight:
      return views::BubbleBorder::RIGHT_TOP;
  }
  return views::BubbleBorder::Arrow::NONE;
}

gfx::Insets GetBubbleInsets() {
  gfx::Insets insets = GetTrayBubbleInsets();
  insets.set_bottom(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  return insets;
}

}  // namespace

KioskAppInstructionBubble::KioskAppInstructionBubble(views::View* anchor,
                                                     ShelfAlignment alignment)
    : views::BubbleDialogDelegateView(anchor, views::BubbleBorder::NONE) {
  set_close_on_deactivate(false);
  const int bubble_margin = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL);
  set_margins(gfx::Insets(bubble_margin));
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetArrow(GetArrow(alignment));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Set up the title view.
  title_ = AddChildView(std::make_unique<views::Label>());
  TrayPopupUtils::SetLabelFontList(title_,
                                   TrayPopupUtils::FontStyle::kSmallTitle);
  title_->SetText(l10n_util::GetStringUTF16(IDS_SHELF_KIOSK_APP_INSTRUCTION));
  title_->SetMultiLine(true);
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  views::DialogDelegate::CreateDialogWidget(
      this, nullptr /* context */,
      Shell::GetContainer(
          anchor_widget()->GetNativeWindow()->GetRootWindow(),
          kShellWindowId_LockScreenRelatedContainersContainer) /* parent */);

  auto bubble_border =
      std::make_unique<views::BubbleBorder>(arrow(), GetShadow());
  bubble_border->set_insets(GetBubbleInsets());
  bubble_border->SetCornerRadius(
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kHigh));
  GetBubbleFrameView()->SetBubbleBorder(std::move(bubble_border));
  GetBubbleFrameView()->SetBackgroundColor(GetBackgroundColor());
}

KioskAppInstructionBubble::~KioskAppInstructionBubble() = default;

void KioskAppInstructionBubble::OnThemeChanged() {
  BubbleDialogDelegateView::OnThemeChanged();

  SkColor label_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  title_->SetEnabledColor(label_color);
}

void KioskAppInstructionBubble::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kStaticText;
  node_data->SetName(l10n_util::GetStringUTF8(IDS_SHELF_KIOSK_APP_INSTRUCTION));
}

gfx::Size KioskAppInstructionBubble::CalculatePreferredSize() const {
  const int bubble_margin = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL);
  const int width = kBubblePreferredInternalWidth + 2 * bubble_margin;
  return gfx::Size(width, GetHeightForWidth(width));
}

}  // namespace ash