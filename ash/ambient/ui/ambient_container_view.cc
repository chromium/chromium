// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_container_view.h"

#include <memory>
#include <utility>

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/metrics/ambient_metrics.h"
#include "ash/ambient/metrics/ambient_session_metrics_recorder.h"
#include "ash/ambient/resources/ambient_animation_static_resources.h"
#include "ash/ambient/ui/ambient_animation_view.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/ambient/ui/photo_view.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/check.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

AmbientContainerView::AmbientContainerView(
    AmbientViewDelegateImpl* delegate,
    AmbientAnimationProgressTracker* progress_tracker,
    std::unique_ptr<AmbientAnimationStaticResources> animation_static_resources,
    AmbientSessionMetricsRecorder* session_metrics_recorder,
    AmbientAnimationFrameRateController* frame_rate_controller) {
  CHECK(delegate);
  CHECK(session_metrics_recorder);
  InitializeCommonSettings();
  View* main_rendering_view = nullptr;
  AmbientUiSettings ui_settings =
      animation_static_resources ? animation_static_resources->GetUiSettings()
                                 : AmbientUiSettings();
  if (animation_static_resources) {
    main_rendering_view = AddChildView(std::make_unique<AmbientAnimationView>(
        delegate, progress_tracker, std::move(animation_static_resources),
        session_metrics_recorder, frame_rate_controller));
  } else {
    main_rendering_view = AddChildView(std::make_unique<PhotoView>(delegate));
    session_metrics_recorder->RegisterScreen(/*animation=*/nullptr);
  }
  orientation_metrics_recorder_ =
      std::make_unique<ambient::AmbientOrientationMetricsRecorder>(
          main_rendering_view, std::move(ui_settings));
}

AmbientContainerView::AmbientContainerView(
    AmbientUiSettings ui_settings,
    std::unique_ptr<views::View> main_rendering_view,
    AmbientSessionMetricsRecorder* session_metrics_recorder) {
  CHECK(main_rendering_view);
  CHECK(session_metrics_recorder);
  InitializeCommonSettings();
  // Set up metrics common to all ambient UIs.
  //
  // TODO(esum): Find a way of recording multi-screen metrics without requiring
  // the caller to pass in a |AmbientSessionMetricsRecorder|. Ideally, we
  // just make a function call here or instantiate a private member as is done
  // for |orientation_metrics_recorder_|.
  session_metrics_recorder->RegisterScreen(/*animation=*/nullptr);
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

BEGIN_METADATA(AmbientContainerView, views::View)
END_METADATA

}  // namespace ash
