// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_raster_query_queue.h"

namespace cc {

FakeRasterQueryQueue::FakeRasterQueryQueue(
    viz::RasterContextProvider* const worker_context_provider,
    bool oop_rasterization_enabled)
    : RasterQueryQueue(worker_context_provider, oop_rasterization_enabled) {}

FakeRasterQueryQueue::~FakeRasterQueryQueue() = default;

bool FakeRasterQueryQueue::CheckRasterFinishedQueries() {
  return false;
}

}  // namespace cc
