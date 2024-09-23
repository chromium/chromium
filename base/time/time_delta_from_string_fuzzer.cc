// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include "base/time/time.h"
#include "base/time/time_delta_from_string.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view input(reinterpret_cast<const char*>(data), size);
  base::TimeDeltaFromString(input);
  return 0;
}
