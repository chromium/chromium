// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// https://dev.chromium.org/developers/testing/no-compile-tests

#include <string>
#include <type_traits>

#include "base/memory/structured_shared_memory.h"
#include "base/memory/read_only_shared_memory_region.h"

namespace base {

namespace {

struct NotTriviallyCopyable {
  std::string data;
};

static_assert(!std::is_trivially_copyable_v<NotTriviallyCopyable>);

}  // namespace

void MustBeTriviallyCopyable() {
  StructuredSharedMemory<NotTriviallyCopyable>::Create();  // expected-error@base/memory/structured_shared_memory.h:* {{no matching function for call to 'AssertSafeToMap'}}
  StructuredSharedMemory<NotTriviallyCopyable>::MapReadOnlyRegion(
      ReadOnlySharedMemoryRegion());  // expected-error@base/memory/structured_shared_memory.h:* {{no matching function for call to 'AssertSafeToMap'}}
}

}  // namespace base
