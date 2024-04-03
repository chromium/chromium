// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/common/glanceables_progress_bar_view.h"

#include <memory>

#include "ash/glanceables/common/glanceables_view_id.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr auto kProgressBarContainerPreferredSize = gfx::Size(0, 12);
constexpr auto kProgressBarContainerMargins =
    gfx::Insets::VH(0, -kProgressBarContainerPreferredSize.height());
constexpr int kProgressBarThickness = 2;

}  // namespace

GlanceablesProgressBarView::GlanceablesProgressBarView() {
  SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  SetPreferredSize(kProgressBarContainerPreferredSize);
  SetProperty(views::kMarginsKey, kProgressBarContainerMargins);

  progress_bar_ = AddChildView(std::make_unique<views::ProgressBar>());
  progress_bar_->SetPreferredHeight(kProgressBarThickness);
  progress_bar_->SetPreferredCornerRadii(std::nullopt);
  progress_bar_->SetID(base::to_underlying(GlanceablesViewId::kProgressBar));
  progress_bar_->SetBackgroundColorId(cros_tokens::kCrosSysSystemOnBase);
  progress_bar_->SetForegroundColorId(cros_tokens::kCrosSysPrimary);
  progress_bar_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  // Any value outside the range [0, 1] makes the progress bar animation
  // infinite.
  progress_bar_->SetValue(-1);
}

void GlanceablesProgressBarView::UpdateProgressBarVisibility(bool visible) {
  progress_bar_->SetVisible(visible);
}

BEGIN_METADATA(GlanceablesProgressBarView)
END_METADATA

}  // namespace ash
