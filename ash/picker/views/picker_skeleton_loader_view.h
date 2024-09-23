// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SKELETON_LOADER_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SKELETON_LOADER_VIEW_H_

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/image_view.h"

namespace views {
class AnimationAbortHandle;
}

namespace ash {

// A view for rendering a skeleton loader for Picker.
// The initial state is a blank view.
// Calling `StartAnimation` will render a skeleton loader animation after a
// delay. Calling `StopAnimation` will reset this back to a blank view.
class ASH_EXPORT PickerSkeletonLoaderView : public views::ImageView {
  METADATA_HEADER(PickerSkeletonLoaderView, views::ImageView)

 public:
  PickerSkeletonLoaderView();
  PickerSkeletonLoaderView(const PickerSkeletonLoaderView&) = delete;
  PickerSkeletonLoaderView& operator=(const PickerSkeletonLoaderView&) = delete;
  ~PickerSkeletonLoaderView() override;

  // Starts the loading animation after `initial_delay`.
  // If this is called while an animation is happening, the existing animation
  // is stopped and a new one is started.
  void StartAnimationAfter(base::TimeDelta initial_delay = base::Seconds(0));

  // Stops the loading animation.
  void StopAnimation();

  bool HasStartedAnimationForTesting() const;

 private:
  void StartAnimation();

  base::OneShotTimer animation_start_timer_;
  std::unique_ptr<views::AnimationAbortHandle> abort_handle_;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, PickerSkeletonLoaderView, views::ImageView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::PickerSkeletonLoaderView)

#endif  // ASH_PICKER_VIEWS_PICKER_SKELETON_LOADER_VIEW_H_
