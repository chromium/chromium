// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_view.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/mahi/mahi_constants.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

constexpr int kPanelCornerRadius = 16;

}  // namespace

BEGIN_METADATA(MahiPanelView, views::BoxLayoutView)
END_METADATA

MahiPanelView::MahiPanelView() {
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);
  SetBackground(views::CreateRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevated, kPanelCornerRadius));

  // Create a layer for the view for background blur and rounded corners.
  SetPaintToLayer();
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF{kPanelCornerRadius});
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetIsFastRoundedCorner(true);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  SetBorder(std::make_unique<views::HighlightBorder>(
      kPanelCornerRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow,
      /*insets_type=*/views::HighlightBorder::InsetsType::kHalfInsets));

  auto header = std::make_unique<views::Label>(u"Mahi Panel");
  AddChildView(std::move(header));

  summary_label_ = AddChildView(std::make_unique<views::Label>());

  auto* manager = chromeos::MahiManager::Get();
  if (manager) {
    manager->GetSummary(base::BindOnce(
        [](base::WeakPtr<MahiPanelView> parent, views::Label* summary_label,
           std::u16string summary_text) {
          if (!parent) {
            return;
          }

          summary_label->SetText(summary_text);
        },
        weak_ptr_factory_.GetWeakPtr(), summary_label_));
  } else {
    CHECK_IS_TEST();
  }

  auto feedback_view = std::make_unique<views::BoxLayoutView>();
  feedback_view->SetOrientation(views::BoxLayout::Orientation::kHorizontal);

  // TODO(b/319264190): Replace the string IDs used here with the correct IDs.
  auto thumbs_up_button = std::make_unique<IconButton>(
      base::BindRepeating(&MahiPanelView::OnThumbsUpButtonPressed,
                          weak_ptr_factory_.GetWeakPtr()),
      IconButton::Type::kMedium, &kMahiThumbsUpIcon,
      IDS_ASH_ACCELERATOR_DESCRIPTION_VOLUME_UP);
  thumbs_up_button_ = feedback_view->AddChildView(std::move(thumbs_up_button));

  auto thumbs_down_button = std::make_unique<IconButton>(
      base::BindRepeating(&MahiPanelView::OnThumbsDownButtonPressed,
                          weak_ptr_factory_.GetWeakPtr()),
      IconButton::Type::kMedium, &kMahiThumbsDownIcon,
      IDS_ASH_ACCELERATOR_DESCRIPTION_VOLUME_DOWN);
  thumbs_down_button_ =
      feedback_view->AddChildView(std::move(thumbs_down_button));

  AddChildView(std::move(feedback_view));
}

MahiPanelView::~MahiPanelView() = default;

void MahiPanelView::OnThumbsUpButtonPressed(const ui::Event& event) {
  base::UmaHistogramBoolean(mahi_constants::kMahiFeedbackHistogramName, true);
}

void MahiPanelView::OnThumbsDownButtonPressed(const ui::Event& event) {
  base::UmaHistogramBoolean(mahi_constants::kMahiFeedbackHistogramName, false);
}

}  // namespace ash
