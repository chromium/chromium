// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_BUBBLE_VIEW_IDS_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_BUBBLE_VIEW_IDS_H_

namespace ash::video_conference {

// IDs used for the main views that comprise the video conference bubble view.
// Use these for easy access to the views during the unittests. Note that these
// IDs are only guaranteed to be unique inside the bubble view.
enum BubbleViewID {
  // Main outermost bubble view, what's actually launched from the tray. Start
  // from 1 because 0 is the default view ID.
  kMainBubbleView = 1,

  // The "return to app" UI, which is a child of `kMainBubbleView`.
  kReturnToApp,

  // Container view for all "set-value" VC effects, a child of
  // `kMainBubbleView`.
  kSetValueEffectsView,

  // Container view for a single "set-value" VC effect, a child of
  // `kSetValueEffectsView`.
  kSingleSetValueEffectView,

  // Container view for all "toggle" VC effects, a child of `kMainBubbleView`.
  kToggleEffectsView,

  // Button for toggling an individual "toggle" VC effect, a child of
  // `kToggleEffectsContainer`.
  kToggleEffectsButton,

  // Buttons for setting an individual value of a "set-value" VC effect,
  // children of `kSetValueEffectsContainer`. Since the number of values for a
  // given effect can't be known at compile-time, an allowable range of IDs
  // [`kSetValueButtonMin`..`kSetValueButtonMax`] is allocated, with IDs clamped
  // at kSetValueButtonMax. If a specific ID doesn't matter, then just use
  // `kSetValueButton`.
  kSetValueButtonMin = kToggleEffectsButton + 1,
  kSetValueButton = kSetValueButtonMin,
  kSetValueButtonMax = kSetValueButtonMin + 100,
  kNextAvailableId = kSetValueButtonMax + 1,
};

}  // namespace ash::video_conference

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_BUBBLE_VIEW_IDS_H_
