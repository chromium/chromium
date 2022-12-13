// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_BUBBLE_VIEW_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_BUBBLE_VIEW_H_

#include "ash/system/tray/tray_bubble_view.h"

namespace ash {

class VideoConferenceTrayController;

namespace video_conference {

// The bubble that contains controls for camera and microphone effects,
// and for easy navigation to apps performing video/audio capture.
class BubbleView : public TrayBubbleView {
 public:
  explicit BubbleView(const InitParams& init_params,
                      VideoConferenceTrayController* controller);
  BubbleView(const BubbleView&) = delete;
  BubbleView& operator=(const BubbleView&) = delete;
  ~BubbleView() override = default;
};

}  // namespace video_conference

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_BUBBLE_VIEW_H_