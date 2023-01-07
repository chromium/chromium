// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_FRAME_INFO_H_
#define CC_TEST_FAKE_FRAME_INFO_H_

#include "cc/metrics/frame_info.h"

namespace cc {

// Creates and returns a FrameInfo instance with the desired |state|.
FrameInfo CreateFakeFrameInfo(FrameInfo::FrameFinalState state);

}  // namespace cc

#endif  // CC_TEST_FAKE_FRAME_INFO_H_
