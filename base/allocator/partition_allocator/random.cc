// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/random.h"

#include "base/allocator/partition_allocator/spin_lock.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"

namespace base {

// This is the same PRNG as used by tcmalloc for mapping address randomness;
// see http://burtleburtle.net/bob/rand/smallprng.html.
struct RandomContext {
  subtle::SpinLock lock;
  bool initialized;
  uint32_t a;
  uint32_t b;
  uint32_t c;
  uint32_t d;
};

namespace {

RandomContext* GetRandomContext() {
  static NoDestructor<RandomContext> g_random_context;
  RandomContext* x = g_random_context.get();
  subtle::SpinLock::Guard guard(x->lock);
  if (UNLIKELY(!x->initialized)) {
    const uint64_t r1 = RandUint64();
    const uint64_t r2 = RandUint64();
    x->a = static_cast<uint32_t>(r1);
    x->b = static_cast<uint32_t>(r1 >> 32);
    x->c = static_cast<uint32_t>(r2);
    x->d = static_cast<uint32_t>(r2 >> 32);
    x->initialized = true;
  }
  return x;
}

}  // namespace

uint32_t RandomValue() {
  RandomContext* x = GetRandomContext();
  subtle::SpinLock::Guard guard(x->lock);
#define rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))
  uint32_t e = x->a - rot(x->b, 27);
  x->a = x->b ^ rot(x->c, 17);
  x->b = x->c + x->d;
  x->c = x->d + e;
  x->d = e + x->a;
  return x->d;
#undef rot
}

void SetMmapSeedForTesting(uint64_t seed) {
  RandomContext* x = GetRandomContext();
  subtle::SpinLock::Guard guard(x->lock);
  x->a = x->b = static_cast<uint32_t>(seed);
  x->c = x->d = static_cast<uint32_t>(seed >> 32);
  x->initialized = true;
}

}  // namespace base
