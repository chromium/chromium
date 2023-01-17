// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_TOGGLE_EFFECTS_VIEW_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_TOGGLE_EFFECTS_VIEW_H_

#include "ui/views/view.h"

namespace ash {

class VideoConferenceTrayController;

namespace video_conference {

// The toggle effects view, that resides in the video conference bubble. It
// functions as a "factory" that constructs and hosts rows of buttons, with each
// button managing the on/off state for an individual effect. The buttons are
// constructed from effects data gathered from `VcEffectsDelegate` objects that
// host the individual effects and are registered with the
// `VideoConferenceTrayEffectsManager`, which is in turn owned by the passed-in
// controller.
class ToggleEffectsView : public views::View {
 public:
  METADATA_HEADER(ToggleEffectsView);

  ToggleEffectsView(VideoConferenceTrayController* controller,
                    const int parent_width);
  ToggleEffectsView(const ToggleEffectsView&) = delete;
  ToggleEffectsView& operator=(const ToggleEffectsView&) = delete;
  ~ToggleEffectsView() override = default;
};

}  // namespace video_conference

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_TOGGLE_EFFECTS_VIEW_H_
