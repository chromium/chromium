// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_RASTER_QUERY_QUEUE_H_
#define CC_TEST_FAKE_RASTER_QUERY_QUEUE_H_

#include "cc/raster/raster_query_queue.h"

namespace cc {

// Fake RasterQueryQueue that just no-ops all calls.
class FakeRasterQueryQueue : public RasterQueryQueue {
 public:
  explicit FakeRasterQueryQueue(
      viz::RasterContextProvider* const worker_context_provider);
  ~FakeRasterQueryQueue() override;

  // RasterQueryQueue methods.
  bool CheckRasterFinishedQueries() override;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_RASTER_QUERY_QUEUE_H_
