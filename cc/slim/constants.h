// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_CONSTANTS_H_
#define CC_SLIM_CONSTANTS_H_

#include <cstdint>

// This file contains constants that may turn into settings in the future.

namespace cc::slim {

// Wait for this number of conseuctive begin frame that are not needed before
// stop requesting begin frames. This is to avoid situations where slim keep
// togging begin frame request every frame.
inline constexpr uint32_t kNumUnneededBeginFrameBeforeStop = 4u;

// Max texture size using software mode. This is an arbitrary limit but is meant
// to be similar to the limits on max GPU texture size.
inline constexpr int kSoftwareMaxTextureSize = 16 * 1024;

// Keep tracking of layer occlusion if both x and y dimensions are greater than
// this.
inline constexpr int kMinimumOcclusionTrackingDimension = 160;

}  // namespace cc::slim

#endif  // CC_SLIM_CONSTANTS_H_
