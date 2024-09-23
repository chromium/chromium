// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <nacl/nacl_random.h>
#include <stddef.h>
#include <stdint.h>

#include "base/check_op.h"
#include "base/containers/span.h"

namespace base {

void RandBytes(span<uint8_t> output) {
  while (!output.empty()) {
    size_t nread;
    const int error = nacl_secure_random(output.data(), output.size(), &nread);
    CHECK_EQ(error, 0);
    CHECK_LE(nread, output.size());
    output = output.subspan(nread);
  }
}

}  // namespace base
