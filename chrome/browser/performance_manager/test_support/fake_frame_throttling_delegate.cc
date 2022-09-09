// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/test_support/fake_frame_throttling_delegate.h"

namespace performance_manager {

void FakeFrameThrottlingDelegate::StartThrottlingAllFrameSinks() {
  *throttling_enabled_ = true;
}
void FakeFrameThrottlingDelegate::StopThrottlingAllFrameSinks() {
  *throttling_enabled_ = false;
}

FakeFrameThrottlingDelegate::FakeFrameThrottlingDelegate(
    bool* throttling_enabled)
    : throttling_enabled_(throttling_enabled) {}

}  // namespace performance_manager
