// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"

#include <stdlib.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(PoissonAllocationSamplerTest, MuteHooksWithoutInit) {
  // ScopedMuteHookedSamplesForTesting updates the allocator hooks. Make sure
  // is safe to call from tests that might not call
  // PoissonAllocationSampler::Get() to initialize the rest of the
  // PoissonAllocationSampler.
  EXPECT_FALSE(PoissonAllocationSampler::AreHookedSamplesMuted());
  void* volatile p = nullptr;
  {
    PoissonAllocationSampler::ScopedMuteHookedSamplesForTesting mute_hooks;
    p = malloc(10000);
  }
  free(p);
}

}  // namespace base
