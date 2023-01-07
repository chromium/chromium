// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_raster_query_queue.h"

namespace cc {

FakeRasterQueryQueue::FakeRasterQueryQueue(
    viz::RasterContextProvider* const worker_context_provider)
    : RasterQueryQueue(worker_context_provider) {}

FakeRasterQueryQueue::~FakeRasterQueryQueue() = default;

bool FakeRasterQueryQueue::CheckRasterFinishedQueries() {
  return false;
}

}  // namespace cc
