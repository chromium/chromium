// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_SET_CAMERA_BACKGROUND_VIEW_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_SET_CAMERA_BACKGROUND_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/camera/camera_effects_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {
class VideoConferenceTrayController;
}

namespace ash::video_conference {
class BubbleView;

// View that is responsible for setting background image from the sysui.
class ASH_EXPORT SetCameraBackgroundView : public views::View {
  METADATA_HEADER(SetCameraBackgroundView, views::View)

 public:
  SetCameraBackgroundView(BubbleView* bubble_view,
                          VideoConferenceTrayController* controller);
  SetCameraBackgroundView(const SetCameraBackgroundView&) = delete;
  SetCameraBackgroundView& operator=(const SetCameraBackgroundView&) = delete;
  ~SetCameraBackgroundView() override;

  void SetBackgroundReplaceUiVisible(bool visible);

  bool IsAnimationPlayingForCreateWithAiButtonForTesting();

 private:
  void OnGetRecentlyUsedBackgroundImages(
      const std::vector<CameraEffectsController::BackgroundImageInfo>&
          background_images);

  raw_ptr<VideoConferenceTrayController> controller_;
  raw_ptr<views::View> recently_used_background_view_;
  raw_ptr<views::View> create_with_image_button_;

  base::WeakPtrFactory<SetCameraBackgroundView> weak_factory_{this};
};

}  // namespace ash::video_conference

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_SET_CAMERA_BACKGROUND_VIEW_H_
