// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/constants/ambient_theme.h"
#include "ui/views/view.h"

namespace ash {

class AmbientAnimationFrameRateController;
class AmbientAnimationProgressTracker;
class AmbientAnimationStaticResources;
class AmbientMultiScreenMetricsRecorder;
class AmbientViewDelegateImpl;

namespace ambient {
class AmbientOrientationMetricsRecorder;
}  // namespace ambient

// Container view to display all Ambient Mode related views, i.e. photo frame,
// weather info.
class ASH_EXPORT AmbientContainerView : public views::View {
 public:
  METADATA_HEADER(AmbientContainerView);

  // TODO(b/274164306): Remove when slideshow and animation themes are
  // migrated to AmbientUiLauncher. Prefer other overloaded constructor below.
  //
  // |animation_static_resources| contains the Lottie animation file to render
  // along with its accompanying static image assets. If null, that means the
  // slideshow UI should be rendered instead.
  AmbientContainerView(
      AmbientViewDelegateImpl* delegate,
      AmbientAnimationProgressTracker* progress_tracker,
      std::unique_ptr<AmbientAnimationStaticResources>
          animation_static_resources,
      AmbientMultiScreenMetricsRecorder* multi_screen_metrics_recorder,
      AmbientAnimationFrameRateController* frame_rate_controller);

  // |main_rendering_view| should contain the primary content; it becomes a
  // child of |AmbientContainerView|, and |AmbientContainerView| sets up some
  // parameters in the view hierarchy that are common to all ambient UIs.
  AmbientContainerView(
      AmbientTheme theme,
      std::unique_ptr<views::View> main_rendering_view,
      AmbientMultiScreenMetricsRecorder* multi_screen_metrics_recorder);
  ~AmbientContainerView() override;

 private:
  friend class AmbientAshTestBase;

  void InitializeCommonSettings();

  std::unique_ptr<ambient::AmbientOrientationMetricsRecorder>
      orientation_metrics_recorder_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_
