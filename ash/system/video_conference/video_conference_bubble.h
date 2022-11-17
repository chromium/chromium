// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_BUBBLE_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_BUBBLE_H_

#include "ash/system/tray/tray_bubble_view.h"

namespace ash {

// The bubble that contains controls for camera and microphone effects,
// and for easy navigation to apps performing video/audio capture.
class VideoConferenceBubbleView : public TrayBubbleView {
 public:
  explicit VideoConferenceBubbleView(const InitParams& init_params);

  VideoConferenceBubbleView(const VideoConferenceBubbleView&) = delete;
  VideoConferenceBubbleView& operator=(const VideoConferenceBubbleView&) =
      delete;
  ~VideoConferenceBubbleView() override = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_BUBBLE_H_