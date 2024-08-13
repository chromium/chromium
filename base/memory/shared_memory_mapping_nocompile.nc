// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// https://dev.chromium.org/developers/testing/no-compile-tests

#include <string>
#include <type_traits>
#include <utility>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"

namespace base {

namespace {

struct NotTriviallyCopyable {
  std::string data;
};

static_assert(!std::is_trivially_copyable_v<NotTriviallyCopyable>);

struct NotLockFree {
  int large_array[1024] = {};
};

using NotLockFreeAtomic = std::atomic<NotLockFree>;

static_assert(std::is_trivially_copyable_v<NotLockFreeAtomic>);
static_assert(!NotLockFreeAtomic::is_always_lock_free);

}  // namespace

void RequireTriviallyCopyable() {
  auto mapped_region =
      ReadOnlySharedMemoryRegion::Create(sizeof(NotTriviallyCopyable));
  WritableSharedMemoryMapping write_map = std::move(mapped_region.mapping);
  write_map.GetMemoryAs<NotTriviallyCopyable>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAs'}}
  write_map.GetMemoryAsSpan<NotTriviallyCopyable>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  write_map.GetMemoryAsSpan<NotTriviallyCopyable>(1);  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  ReadOnlySharedMemoryMapping read_map = mapped_region.region.Map();
  read_map.GetMemoryAs<NotTriviallyCopyable>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAs'}}
  read_map.GetMemoryAsSpan<NotTriviallyCopyable>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  read_map.GetMemoryAsSpan<NotTriviallyCopyable>(1);  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
}

void RequireLockFreeAtomic() {
  auto mapped_region =
      ReadOnlySharedMemoryRegion::Create(sizeof(NotLockFreeAtomic));
  WritableSharedMemoryMapping write_map = std::move(mapped_region.mapping);
  write_map.GetMemoryAs<NotLockFreeAtomic>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAs'}}
  write_map.GetMemoryAsSpan<NotLockFreeAtomic>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  write_map.GetMemoryAsSpan<NotLockFreeAtomic>(1);  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  ReadOnlySharedMemoryMapping read_map = mapped_region.region.Map();
  read_map.GetMemoryAs<NotLockFreeAtomic>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAs'}}
  read_map.GetMemoryAsSpan<NotLockFreeAtomic>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  read_map.GetMemoryAsSpan<NotLockFreeAtomic>(1);  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
}

}  // namespace base
