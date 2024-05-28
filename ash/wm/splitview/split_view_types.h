// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_TYPES_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_TYPES_H_

namespace ash {

// Specifies the snap position for split view.
//
// For primary screen orientation:
//  - For landscape screen orientation, `kPrimary` corresponds to the
//  left-snapped position and `kSecondary` corresponds to the right-snapped
//  position.
//  - For portrait screen orientation, `kPrimary` corresponds to the top-snapped
//  position and `kSecondary` corresponds to the bottom-snapped position.
//
// For non-primary screen orientation:
//  - For landscape screen orientation, `kPrimary` corresponds to the
//  right-snapped position and `kSecondary` corresponds to the left-snapped
//  position.
//  - For portrait screen orientation, `kPrimary` corresponds to the
//  bottom-snapped position and `kSecondary` corresponds to the top-snapped
//  position.
enum class SnapPosition { kNone, kPrimary, kSecondary };

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_TYPES_H_
