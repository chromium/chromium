// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_BUBBLE_VIEW_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"

namespace views {
class View;
}  // namespace views

namespace ash {

class VideoConferenceTrayController;

namespace video_conference {

using MediaApps = std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr>;

// The bubble that contains controls for camera and microphone effects,
// and for easy navigation to apps performing video/audio capture.
class ASH_EXPORT BubbleView : public TrayBubbleView {
 public:
  explicit BubbleView(const InitParams& init_params,
                      const MediaApps& media_apps,
                      VideoConferenceTrayController* controller);
  BubbleView(const BubbleView&) = delete;
  BubbleView& operator=(const BubbleView&) = delete;
  ~BubbleView() override;

  // views::View:
  void AddedToWidget() override;
  void ChildPreferredSizeChanged(View* child) override;

  // TrayBubbleView:
  bool CanActivate() const override;

 private:
  // Unowned by `BubbleView`.
  raw_ptr<VideoConferenceTrayController, ExperimentalAsh> controller_;

  const MediaApps& media_apps_;
};

}  // namespace video_conference

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_BUBBLE_VIEW_H_