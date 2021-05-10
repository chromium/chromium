// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/random.h"

#include "base/allocator/partition_allocator/partition_lock.h"
#include "base/rand_util.h"

namespace base {
namespace {

internal::PartitionLock g_lock = {};

// Using XorShift128+, which is simple and widely used. See
// https://en.wikipedia.org/wiki/Xorshift#xorshift+ for details.
struct RandomContext {
  bool initialized;

  uint64_t a;
  uint64_t b;
};

RandomContext g_context GUARDED_BY(g_lock);

RandomContext& GetRandomContext() EXCLUSIVE_LOCKS_REQUIRED(g_lock) {
  if (UNLIKELY(!g_context.initialized)) {
    g_context.a = RandUint64();
    g_context.b = RandUint64();
    g_context.initialized = true;
  }

  return g_context;
}

}  // namespace

uint32_t RandomValue() {
  internal::ScopedGuard<true> guard(g_lock);
  RandomContext& x = GetRandomContext();

  uint64_t t = x.a;
  const uint64_t s = x.b;

  x.a = s;
  t ^= t << 23;
  t ^= t >> 17;
  t ^= s ^ (s >> 26);
  x.b = t;

  // The generator usually returns an uint64_t, truncate it.
  //
  // It is noted in this paper (https://arxiv.org/abs/1810.05313) that the
  // lowest 32 bits fail some statistical tests from the Big Crush
  // suite. Use the higher ones instead.
  return (t + s) >> 32;
}

void SetMmapSeedForTesting(uint64_t seed) {
  internal::ScopedGuard<true> guard(g_lock);
  RandomContext& x = GetRandomContext();
  x.a = x.b = static_cast<uint32_t>(seed);
  x.initialized = true;
}

}  // namespace base
