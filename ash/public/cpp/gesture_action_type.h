// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_GESTURE_ACTION_TYPE_H_
#define ASH_PUBLIC_CPP_GESTURE_ACTION_TYPE_H_

namespace ash {

enum GestureActionType {
  GESTURE_UNKNOWN,
  GESTURE_OMNIBOX_PINCH,
  GESTURE_OMNIBOX_SCROLL,
  GESTURE_TABSTRIP_PINCH,
  GESTURE_TABSTRIP_SCROLL,
  GESTURE_BEZEL_SCROLL,
  GESTURE_DESKTOP_SCROLL,
  GESTURE_DESKTOP_PINCH,
  GESTURE_WEBPAGE_PINCH,
  GESTURE_WEBPAGE_SCROLL,
  GESTURE_WEBPAGE_TAP,
  GESTURE_TABSTRIP_TAP,
  GESTURE_BEZEL_DOWN,
  GESTURE_TABSWITCH_TAP,
  GESTURE_TABNOSWITCH_TAP,
  GESTURE_TABCLOSE_TAP,
  GESTURE_NEWTAB_TAP,
  GESTURE_ROOTVIEWTOP_TAP,
  GESTURE_FRAMEMAXIMIZE_TAP,
  GESTURE_FRAMEVIEW_TAP,
  GESTURE_MAXIMIZE_DOUBLETAP,
  // NOTE: Add new action types only immediately above this line. Also,
  // make sure the enum list in tools/histogram/histograms.xml is
  // updated with any change in here.
  GESTURE_ACTION_COUNT
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_GESTURE_ACTION_TYPE_H_
