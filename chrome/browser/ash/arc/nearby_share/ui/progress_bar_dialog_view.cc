// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/ui/progress_bar_dialog_view.h"

#include <memory>

#include "ash/components/arc/compat_mode/style/arc_color_provider.h"
#include "ash/style/ash_color_id.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/arc/nearby_share/ui/nearby_share_overlay_view.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace arc {

ProgressBarDialogView::ProgressBarDialogView(bool is_multiple_files)
    : is_multiple_files_(is_multiple_files) {
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);
  SetInsideBorderInsets(gfx::Insets(24));
  SetBetweenChildSpacing(
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL));

  // Height of the progress bar at the top of the dialog.
  constexpr int kProgressBarHeight = 4;
  // Width of the progress bar from the left of the dialog.
  constexpr int kProgressBarWidth = 50;
  constexpr int kCornerRadius = 12;

  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW,
      ash::kColorAshDialogBackgroundColor);
  border->SetCornerRadius(kCornerRadius);
  SetBackground(std::make_unique<views::BubbleBackground>(border.get()));
  SetBorder(std::move(border));

  const std::u16string message =
      is_multiple_files
          ? l10n_util::GetStringUTF16(
                IDS_ASH_ARC_NEARBY_SHARE_FILES_PREPARATION_PROGRESS)
          : l10n_util::GetStringUTF16(
                IDS_ASH_ARC_NEARBY_SHARE_FILE_PREPARATION_PROGRESS);
  message_label_ = AddChildView(std::make_unique<views::Label>(message));
  message_label_->SetMultiLine(true);
  message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_label_->SetVerticalAlignment(gfx::ALIGN_TOP);
  AddChildView(message_label_.get());

  progress_bar_ = AddChildView(std::make_unique<views::ProgressBar>());
  progress_bar_->SetPreferredHeight(kProgressBarHeight);
  progress_bar_->SetValue(0.01);  // set small initial value.
  progress_bar_->SetPreferredSize(
      gfx::Size(kProgressBarWidth, kProgressBarHeight));
  progress_bar_->SizeToPreferredSize();
}

ProgressBarDialogView::~ProgressBarDialogView() {
  // Destroy child views before the base overlay where they live is destroyed.
  RemoveAllChildViews();
}

gfx::Size ProgressBarDialogView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  views::LayoutProvider* provider = views::LayoutProvider::Get();

  auto width = provider->GetDistanceMetric(
      views::DistanceMetric::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  return gfx::Size(width,
                   GetLayoutManager()->GetPreferredHeightForWidth(this, width));
}

void ProgressBarDialogView::AddedToWidget() {
  const std::u16string view_name =
      is_multiple_files_
          ? l10n_util::GetStringUTF16(
                IDS_ASH_ARC_NEARBY_SHARE_FILES_PREPARATION_PROGRESS)
          : l10n_util::GetStringUTF16(
                IDS_ASH_ARC_NEARBY_SHARE_FILE_PREPARATION_PROGRESS);
  GetWidget()->GetRootView()->GetViewAccessibility().SetRole(
      ax::mojom::Role::kDialog);
  GetWidget()->GetRootView()->GetViewAccessibility().SetName(view_name);
}

void ProgressBarDialogView::OnThemeChanged() {
  DCHECK(progress_bar_);

  views::BoxLayoutView::OnThemeChanged();
  progress_bar_->SetBackgroundColor(
      GetColorProvider()->GetColor(ash::kColorAshDialogBackgroundColor));
}

void ProgressBarDialogView::Show(aura::Window* parent,
                                 ProgressBarDialogView* view) {
  NearbyShareOverlayView::Show(parent, view);
}

void ProgressBarDialogView::UpdateProgressBarValue(double value) {
  DCHECK(progress_bar_);

  progress_bar_->SetValue(value);
}

double ProgressBarDialogView::GetProgressBarValue() const {
  DCHECK(progress_bar_);

  return progress_bar_->GetValue();
}

void ProgressBarDialogView::UpdateInterpolatedProgressBarValue() {
  DCHECK(progress_bar_);

  constexpr double kStepSize = 0.075;
  constexpr double kStepFactor = 3;  // Larger value = smaller step progression.

  const double value = progress_bar_->GetValue();

  // Reduce interpolation step size as it gets closer to full.
  const double next_value = (value >= kStepSize)
                                ? value + kStepSize / (value * kStepFactor)
                                : kStepSize;
  if (next_value <= 1.0) {
    progress_bar_->SetValue(next_value);
  }
}

}  // namespace arc
