// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_SET_VALUE_EFFECTS_VIEW_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_SET_VALUE_EFFECTS_VIEW_H_

#include "ui/views/view.h"

namespace ash {

class VideoConferenceTrayController;

namespace video_conference {

// The set-value effects view, that resides in the video conference bubble. It
// functions as a "factory" that constructs and hosts selector-views for effects
// that are set to one of several integral values. The selector-views host the
// individual effects and are registered with the
// `VideoConferenceTrayEffectsManager`, which is in turn owned by the passed-in
// controller.
class SetValueEffectsView : public views::View {
 public:
  explicit SetValueEffectsView(VideoConferenceTrayController* controller);
  SetValueEffectsView(const SetValueEffectsView&) = delete;
  SetValueEffectsView& operator=(const SetValueEffectsView&) = delete;
  ~SetValueEffectsView() override = default;
};

}  // namespace video_conference

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_SET_VALUE_EFFECTS_VIEW_H_
