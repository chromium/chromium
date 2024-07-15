// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_buffer.h"

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "cc/paint/paint_op.h"
#include "cc/test/transfer_cache_test_helper.h"

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOGGING_FATAL); }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  std::vector<uint8_t> scratch_buffer;
  cc::TransferCacheTestHelper transfer_cache_helper;
  cc::PaintOp::DeserializeOptions options{
      .transfer_cache = &transfer_cache_helper,
      .scratch_buffer = scratch_buffer};
  cc::PaintOpBuffer::MakeFromMemory(data, size, options);

  return 0;
}
