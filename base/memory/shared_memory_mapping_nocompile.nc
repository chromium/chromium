// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// https://dev.chromium.org/developers/testing/no-compile-tests

#include <atomic>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/raw_span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/safe_ref.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/weak_ptr.h"

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

// Assert that common smart pointer types fail the is_trivially_copyable test.
// This ensures that they can't be used in a shared memory mapping, without
// needing explicit handling in SharedMemorySafetyChecker. If one of these
// asserts fails because a type is now trivially copyable, replace it with a
// SharedMemorySafetyChecker specialization in shared_memory_safety_checker.h
// that disallows the type.
static_assert(!std::is_trivially_copyable_v<std::unique_ptr<int>>);
static_assert(!std::is_trivially_copyable_v<raw_ptr<int>>);
static_assert(!std::is_trivially_copyable_v<raw_ref<int>>);
static_assert(!std::is_trivially_copyable_v<raw_span<int>>);
static_assert(!std::is_trivially_copyable_v<base::SafeRef<int>>);
static_assert(!std::is_trivially_copyable_v<base::WeakPtr<int>>);

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

void NoPointers() {
  auto mapped_region = ReadOnlySharedMemoryRegion::Create(sizeof(int*));
  WritableSharedMemoryMapping write_map = std::move(mapped_region.mapping);
  write_map.GetMemoryAs<int*>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAs'}}
  write_map.GetMemoryAsSpan<int*>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  write_map.GetMemoryAsSpan<int*>(1);  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  ReadOnlySharedMemoryMapping read_map = mapped_region.region.Map();
  read_map.GetMemoryAs<int*>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAs'}}
  read_map.GetMemoryAsSpan<int*>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  read_map.GetMemoryAsSpan<int*>(1);  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
}

void NoFunctionPointers() {
  using FunctionPtr = void (*)();
  auto mapped_region = ReadOnlySharedMemoryRegion::Create(sizeof(FunctionPtr));
  WritableSharedMemoryMapping write_map = std::move(mapped_region.mapping);
  write_map.GetMemoryAs<FunctionPtr>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAs'}}
  write_map.GetMemoryAsSpan<FunctionPtr>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  write_map.GetMemoryAsSpan<FunctionPtr>(1);  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  ReadOnlySharedMemoryMapping read_map = mapped_region.region.Map();
  read_map.GetMemoryAs<FunctionPtr>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAs'}}
  read_map.GetMemoryAsSpan<FunctionPtr>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  read_map.GetMemoryAsSpan<FunctionPtr>(1);  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
}

void NoMemberFunctionPointers() {
  using MemberFunctionPtr = size_t (std::string::*)() const;
  auto mapped_region = ReadOnlySharedMemoryRegion::Create(sizeof(MemberFunctionPtr));
  WritableSharedMemoryMapping write_map = std::move(mapped_region.mapping);
  write_map.GetMemoryAs<MemberFunctionPtr>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAs'}}
  write_map.GetMemoryAsSpan<MemberFunctionPtr>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  write_map.GetMemoryAsSpan<MemberFunctionPtr>(1);  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  ReadOnlySharedMemoryMapping read_map = mapped_region.region.Map();
  read_map.GetMemoryAs<MemberFunctionPtr>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAs'}}
  read_map.GetMemoryAsSpan<MemberFunctionPtr>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  read_map.GetMemoryAsSpan<MemberFunctionPtr>(1);  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
}

void NoAtomicPointers() {
  using AtomicPtr = std::atomic<int*>;
  auto mapped_region = ReadOnlySharedMemoryRegion::Create(sizeof(AtomicPtr));
  WritableSharedMemoryMapping write_map = std::move(mapped_region.mapping);
  write_map.GetMemoryAs<AtomicPtr>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAs'}}
  write_map.GetMemoryAsSpan<AtomicPtr>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  write_map.GetMemoryAsSpan<AtomicPtr>(1);  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  ReadOnlySharedMemoryMapping read_map = mapped_region.region.Map();
  read_map.GetMemoryAs<AtomicPtr>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAs'}}
  read_map.GetMemoryAsSpan<AtomicPtr>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  read_map.GetMemoryAsSpan<AtomicPtr>(1);  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
}

void NoArraysOfBannedTypes() {
  using Array = NotLockFreeAtomic[2];
  auto mapped_region = ReadOnlySharedMemoryRegion::Create(sizeof(Array));
  WritableSharedMemoryMapping write_map = std::move(mapped_region.mapping);
  write_map.GetMemoryAs<Array>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAs'}}
  write_map.GetMemoryAsSpan<Array>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  write_map.GetMemoryAsSpan<Array>(1);  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  ReadOnlySharedMemoryMapping read_map = mapped_region.region.Map();
  read_map.GetMemoryAs<Array>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAs'}}
  read_map.GetMemoryAsSpan<Array>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  read_map.GetMemoryAsSpan<Array>(1);  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
}

void NoStdArraysOfBannedTypes() {
  using Array = std::array<NotLockFreeAtomic, 2>;
  auto mapped_region = ReadOnlySharedMemoryRegion::Create(sizeof(Array));
  WritableSharedMemoryMapping write_map = std::move(mapped_region.mapping);
  write_map.GetMemoryAs<Array>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAs'}}
  write_map.GetMemoryAsSpan<Array>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  write_map.GetMemoryAsSpan<Array>(1);  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  ReadOnlySharedMemoryMapping read_map = mapped_region.region.Map();
  read_map.GetMemoryAs<Array>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAs'}}
  read_map.GetMemoryAsSpan<Array>();  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
  read_map.GetMemoryAsSpan<Array>(1);  // expected-error@base/memory/shared_memory_mapping_nocompile.nc:* {{no matching member function for call to 'GetMemoryAsSpan'}}
}

}  // namespace base
