// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_TYPES_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_TYPES_H_

namespace ash {

// Defines the capture type Capture Mode is currently using.
enum class CaptureModeType {
  kImage,
  kVideo,
};

// Defines the source of the capture used by Capture Mode.
enum class CaptureModeSource {
  kFullscreen,
  kRegion,
  kWindow,
};

// The position of the press event during the fine tune phase of a region
// capture session. This will determine what subsequent drag events do to the
// select region.
enum class FineTunePosition {
  // The initial press was outside region. Subsequent drags will do nothing.
  kNone,
  // The initial press was inside the select region. Subsequent drags will
  // move the entire region.
  kCenter,
  // The initial press was on one of the drag affordance circles. Subsequent
  // drags will resize the region. These are sorted clockwise starting at the
  // top left.
  kTopLeft,
  kTopCenter,
  kTopRight,
  kRightCenter,
  kBottomRight,
  kBottomCenter,
  kBottomLeft,
  kLeftCenter,
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_TYPES_H_
