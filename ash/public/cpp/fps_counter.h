// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_FPS_COUNTER_H_
#define ASH_PUBLIC_CPP_FPS_COUNTER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/time/time.h"
#include "ui/compositor/compositor_observer.h"

namespace ui {
class Compositor;
}

namespace ash {

// FpsCounter is used to measures the smoothness of animations applied in one
// operation, while AnimationMetricsRepoter measures smoothness of one
// particular animation.  For example, overview animation consists of multiple
// animations, such as wallpaper, windows and label etc. This allows us to
// measure such animations as a whole.
class ASH_PUBLIC_EXPORT FpsCounter : public ui::CompositorObserver {
 public:
  explicit FpsCounter(ui::Compositor* compositor);
  ~FpsCounter() override;

  // Comptues smoothness based on the updated frame number in compositor and the
  // duration between creation and the invocation time. Returns -1 if it cannot
  // compute the smoothness (when frame_count rolled over, or it finihed
  // immediately).
  int ComputeSmoothness();

  // ui::CompositorObserver:
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // Use this to update histogram even with zero animation.
  static void SetForceReportZeroAnimationForTest(bool value);

 private:
  ui::Compositor* compositor_ = nullptr;
  int start_frame_number_ = 0;
  base::TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(FpsCounter);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_FPS_COUNTER_H_
