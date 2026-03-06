// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time_delta_from_string.h"

#include <stdint.h>

#include <string_view>

#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "base/time/time.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> data) {
  auto input = base::as_string_view(data);
  base::TimeDeltaFromString(input);
  return 0;
}
