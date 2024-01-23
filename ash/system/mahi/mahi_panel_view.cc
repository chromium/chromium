// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_view.h"

#include <memory>
#include <string>

#include "ash/public/cpp/style/color_provider.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
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
}

MahiPanelView::~MahiPanelView() = default;

}  // namespace ash
