// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/strings/string_util.h"

#include <cstring>

#include "partition_alloc/partition_alloc_base/compiler_specific.h"

namespace partition_alloc::internal::base::strings {

const char* FindLastOf(const char* text, const char* characters) {
  size_t length = strlen(text);
  const char* ptr = PA_UNSAFE_TODO(text + length - 1);
  while (ptr >= text) {
    if (PA_UNSAFE_TODO(strchr(characters, *ptr))) {
      return ptr;
    }
    PA_UNSAFE_TODO(--ptr);
  }
  return nullptr;
}

const char* FindLastNotOf(const char* text, const char* characters) {
  size_t length = strlen(text);
  const char* ptr = PA_UNSAFE_TODO(text + length - 1);
  while (ptr >= text) {
    if (!PA_UNSAFE_TODO(strchr(characters, *ptr))) {
      return ptr;
    }
    PA_UNSAFE_TODO(--ptr);
  }
  return nullptr;
}

}  // namespace partition_alloc::internal::base::strings
