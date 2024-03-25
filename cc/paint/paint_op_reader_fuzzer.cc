// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "cc/paint/paint_op_reader.h"

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOGGING_FATAL); }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  std::vector<uint8_t> scratch_buffer;
  cc::PaintOp::DeserializeOptions options{.scratch_buffer = scratch_buffer};
  cc::PaintOpReader reader(data, size, options,
                           /*enable_security_constraints=*/true);
  sk_sp<cc::PaintFilter> filter;
  reader.Read(&filter);

  return 0;
}
