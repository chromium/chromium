// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include "chrome/browser/ash/extensions/file_manager/search_by_pattern.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  extensions::CreateFnmatchQuery({reinterpret_cast<const char*>(data), size});
  return 0;
}
