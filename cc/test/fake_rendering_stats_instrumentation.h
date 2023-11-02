// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_RENDERING_STATS_INSTRUMENTATION_H_
#define CC_TEST_FAKE_RENDERING_STATS_INSTRUMENTATION_H_

#include "cc/debug/rendering_stats_instrumentation.h"

namespace cc {

class FakeRenderingStatsInstrumentation : public RenderingStatsInstrumentation {
 public:
  FakeRenderingStatsInstrumentation() {}
  ~FakeRenderingStatsInstrumentation() override {}
};

}  // namespace cc

#endif  // CC_TEST_FAKE_RENDERING_STATS_INSTRUMENTATION_H_
