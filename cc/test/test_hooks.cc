// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_hooks.h"

namespace cc {

TestHooks::TestHooks() = default;

TestHooks::~TestHooks() = default;

DrawResult TestHooks::PrepareToDrawOnThread(
    LayerTreeHostImpl* host_impl,
    LayerTreeHostImpl::FrameData* frame_data,
    DrawResult draw_result) {
  return draw_result;
}

std::unique_ptr<RasterBufferProvider> TestHooks::CreateRasterBufferProvider(
    LayerTreeHostImpl* host_impl) {
  return host_impl->LayerTreeHostImpl::CreateRasterBufferProvider();
}

std::unique_ptr<BeginMainFrameMetrics> TestHooks::GetBeginMainFrameMetrics() {
  return nullptr;
}

}  // namespace cc
