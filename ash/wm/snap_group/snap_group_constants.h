// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SNAP_GROUP_SNAP_GROUP_CONSTANTS_H_
#define ASH_WM_SNAP_GROUP_SNAP_GROUP_CONSTANTS_H_

namespace ash {

// This threshold determines if a new window can replace an existing one snapped
// on the same side within a snap group, based on the difference in their snap
// ratios.
// TODO(b/346624805): Finalize and rename the threshold value and update the
// comments above.
inline constexpr float kSnapToReplaceRatioDiffThreshold = 0.33f;

}  // namespace ash

#endif  // ASH_WM_SNAP_GROUP_SNAP_GROUP_CONSTANTS_H_
