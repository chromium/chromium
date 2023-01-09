// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_frame_data.h"

namespace cc {

bool operator==(const SkottieFrameData& frame_l,
                const SkottieFrameData& frame_r) {
  return frame_l.image == frame_r.image && frame_l.quality == frame_r.quality;
}

}  // namespace cc
