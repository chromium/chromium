// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "partition_alloc/partition_alloc_base/rand_util.h"

#include <zircon/syscalls.h>

namespace partition_alloc::internal::base {

void RandBytes(void* output, size_t output_length) {
  zx_cprng_draw(output, output_length);
}

}  // namespace partition_alloc::internal::base
