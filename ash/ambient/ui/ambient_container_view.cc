// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_container_view.h"

#include <memory>
#include <utility>

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/metrics/ambient_metrics.h"
#include "ash/ambient/resources/ambient_animation_static_resources.h"
#include "ash/ambient/ui/ambient_animation_view.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "base/check.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

AmbientContainerView::AmbientContainerView(
    AmbientUiSettings ui_settings,
    std::unique_ptr<views::View> main_rendering_view) {
  CHECK(main_rendering_view);
  InitializeCommonSettings();
  // Set up metrics common to all ambient UIs.
  orientation_metrics_recorder_ =
      std::make_unique<ambient::AmbientOrientationMetricsRecorder>(
          main_rendering_view.get(), std::move(ui_settings));
  AddChildView(std::move(main_rendering_view));
}

AmbientContainerView::~AmbientContainerView() = default;

void AmbientContainerView::InitializeCommonSettings() {
  SetID(AmbientViewID::kAmbientContainerView);
  // TODO(b/139954108): Choose a better dark mode theme color.
  SetBackground(views::CreateSolidBackground(SK_ColorBLACK));
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

BEGIN_METADATA(AmbientContainerView)
END_METADATA

}  // namespace ash
