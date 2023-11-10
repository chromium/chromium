// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/strings/string_util.h"

#include <cstring>

namespace partition_alloc::internal::base::strings {

const char* FindLastOf(const char* text, const char* characters) {
  size_t length = strlen(text);
  const char* ptr = text + length - 1;
  while (ptr >= text) {
    if (strchr(characters, *ptr)) {
      return ptr;
    }
    --ptr;
  }
  return nullptr;
}

const char* FindLastNotOf(const char* text, const char* characters) {
  size_t length = strlen(text);
  const char* ptr = text + length - 1;
  while (ptr >= text) {
    if (!strchr(characters, *ptr)) {
      return ptr;
    }
    --ptr;
  }
  return nullptr;
}

}  // namespace partition_alloc::internal::base::strings
