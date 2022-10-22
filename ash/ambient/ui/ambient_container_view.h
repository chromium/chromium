// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
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
  ~AmbientContainerView() override;

 private:
  friend class AmbientAshTestBase;

  std::unique_ptr<ambient::AmbientOrientationMetricsRecorder>
      orientation_metrics_recorder_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_
