// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SNAP_GROUP_SNAP_GROUP_CONSTANTS_H_
#define ASH_WM_SNAP_GROUP_SNAP_GROUP_CONSTANTS_H_

namespace ash {

// This threshold determines if a new window can replace an existing one snapped
// on the same side within a snap group, based on the difference in their snap
// ratios.
// TODO(michelefan): This is a placeholder value. We will work with UX to
// fianlize the threshold.
constexpr float kSnapToReplaceRatioDiffThreshold = 0.1f;

}  // namespace ash

#endif  // ASH_WM_SNAP_GROUP_SNAP_GROUP_CONSTANTS_H_
