// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(MEMORY_TOOL_REPLACES_ALLOCATOR)

#include <cstdlib>
#include <memory>
#include <optional>
#include <vector>

#include "partition_alloc/partition_alloc.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/containers/span.h"
#include "partition_alloc/partition_alloc_hooks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc::test {

namespace {

std::vector<AllocationNotificationData>* g_allocation_data = nullptr;

size_t g_malloc_override_hook_called = 0;
size_t g_free_override_hook_called = 0;
size_t g_realloc_override_hook_called = 0;

size_t g_malloc_observer_hook_called = 0;
size_t g_free_observer_hook_called = 0;

bool MallocOverrideHook(void** out,
                        AllocFlags flags,
                        size_t size,
                        const char* type_name,
                        std::optional<size_t> alignment,
                        bool use_brp) {
  ++g_malloc_override_hook_called;
  void* result = ContainsFlags(flags, AllocFlags::kZeroFill) ? calloc(1, size)
                                                             : malloc(size);
  AllocationNotificationData data(result, size, type_name);
  // TODO(553120509, 555037375): make `use_brp` available for
  // AsanBackupRefPtrService and V2, and add the test to see the `use_brp`.
  data.set_brp_enabled(use_brp);
  g_allocation_data->push_back(data);
  *out = data.address();
  return true;
}

bool FreeOverrideHook(void* address, FreeFlags flags) {
  ++g_free_override_hook_called;
  free(address);
  bool found = false;
  for (auto it = g_allocation_data->begin(); it != g_allocation_data->end();
       ++it) {
    if (it->address() == address) {
      g_allocation_data->erase(it);
      found = true;
      break;
    }
  }
  PA_CHECK(found) << "g_allocation_data must contain the address to be freed.";
  return true;
}

bool ReallocOverrideHook(size_t* out, void* address) {
  ++g_realloc_override_hook_called;
  return false;
}

constexpr size_t kReallocOverrideReturns32ReturnsSize = 32u;

bool ReallocOverrideReturns32Hook(size_t* out, void* address) {
  ++g_realloc_override_hook_called;
  *out = kReallocOverrideReturns32ReturnsSize;
  return true;
}

void ResetHookCalled() {
  g_malloc_override_hook_called = g_realloc_override_hook_called =
      g_free_override_hook_called = 0;
  g_malloc_observer_hook_called = g_free_observer_hook_called = 0;
}

void MallocObserverHook(const AllocationNotificationData& data) {
  ++g_malloc_observer_hook_called;
}

void FreeObserverHook(const FreeNotificationData& data) {
  ++g_free_observer_hook_called;
}

}  // namespace

class PAMemoryToolReplacesTest : public testing::Test {
 public:
  PAMemoryToolReplacesTest() = default;
  ~PAMemoryToolReplacesTest() override = default;

  void SetUp() override { g_allocation_data = &allocation_data_; }

  void TearDown() override {
    g_allocation_data = nullptr;
    ClearAllocationData();
    ResetHookCalled();
  }

  std::unique_ptr<PartitionAllocator> CreateAllocator() {
    std::unique_ptr<PartitionAllocator> allocator =
        std::make_unique<PartitionAllocator>();
    allocator->init({});
    return allocator;
  }

 private:
  void ClearAllocationData() {
    while (!allocation_data_.empty()) {
      auto data = allocation_data_.back();
      free(data.address());
      allocation_data_.pop_back();
    }
  }

  std::vector<AllocationNotificationData> allocation_data_;
};

struct CalledCount {
  size_t malloc = 0u;
  size_t free = 0u;
  size_t realloc = 0u;
};

struct ScopedObserverHook {
  ScopedObserverHook(
      PartitionAllocHooks::AllocationObserverHook allocation_observer_hook,
      PartitionAllocHooks::FreeObserverHook free_observer_hook) {
    PartitionAllocHooks::SetObserverHooks(allocation_observer_hook,
                                          free_observer_hook);
  }
  ~ScopedObserverHook() {
    PartitionAllocHooks::SetObserverHooks(nullptr, nullptr);
  }
};

struct ScopedOverrideHook {
  ScopedOverrideHook(
      PartitionAllocHooks::AllocationOverrideHook allocation_override_hook,
      PartitionAllocHooks::FreeOverrideHook free_override_hook,
      PartitionAllocHooks::ReallocOverrideHook realloc_override_hook) {
    PartitionAllocHooks::SetOverrideHooks(
        allocation_override_hook, free_override_hook, realloc_override_hook);
  }
  ~ScopedOverrideHook() {
    PartitionAllocHooks::SetOverrideHooks(nullptr, nullptr, nullptr);
  }
};

#define EXPECT_OVERRIDE_HOOK_CALLED(...)                               \
  do {                                                                 \
    const CalledCount expected_count = {__VA_ARGS__};                  \
    EXPECT_EQ(expected_count.malloc, g_malloc_override_hook_called);   \
    EXPECT_EQ(expected_count.free, g_free_override_hook_called);       \
    EXPECT_EQ(expected_count.realloc, g_realloc_override_hook_called); \
  } while (0)

#define EXPECT_OBSERVER_HOOK_CALLED(...)                             \
  do {                                                               \
    const CalledCount expected_count = {__VA_ARGS__};                \
    EXPECT_EQ(expected_count.malloc, g_malloc_observer_hook_called); \
    EXPECT_EQ(expected_count.free, g_free_observer_hook_called);     \
  } while (0)

TEST_F(PAMemoryToolReplacesTest, OverrideHookCalled) {
  std::unique_ptr<PartitionAllocator> allocator = CreateAllocator();
  ScopedOverrideHook override_scope(MallocOverrideHook, FreeOverrideHook,
                                    ReallocOverrideHook);
  // (1) firstly, all `override hook called` values are equal to 0.
  EXPECT_OVERRIDE_HOOK_CALLED(.malloc = 0, .free = 0, .realloc = 0);

  // (2) Alloc() causes malloc override hook called 1.
  void* ptr = allocator->root()->Alloc(32);
  EXPECT_TRUE(ptr);
  EXPECT_OVERRIDE_HOOK_CALLED(.malloc = 1u, .free = 0, .realloc = 0);

  // (3) Realloc() will cause Alloc() and Free().
  ptr = allocator->root()->Realloc(ptr, 128, nullptr);
  EXPECT_TRUE(ptr);
  EXPECT_OVERRIDE_HOOK_CALLED(.malloc = 2u, .free = 1u, .realloc = 1);

  // (4) Realloc() with `nullptr` is just `malloc()`.
  void* ptr2 = allocator->root()->Realloc(nullptr, 128, nullptr);
  EXPECT_TRUE(ptr2);
  EXPECT_OVERRIDE_HOOK_CALLED(.malloc = 3u, .free = 1u, .realloc = 1);

  // (5) Free() causes free override hook called 1.
  allocator->root()->Free(ptr);
  EXPECT_OVERRIDE_HOOK_CALLED(.malloc = 3u, .free = 2u, .realloc = 1);

  // (6) Realloc() with `address` and 0-size is just `free()`.
  allocator->root()->Realloc(ptr2, 0, nullptr);
  EXPECT_OVERRIDE_HOOK_CALLED(.malloc = 3u, .free = 3u, .realloc = 1);
}

TEST_F(PAMemoryToolReplacesTest, ObserverHookCalled) {
  std::unique_ptr<PartitionAllocator> allocator = CreateAllocator();
  ScopedObserverHook observer_scope(MallocObserverHook, FreeObserverHook);

  // (1) firstly, all `observer hook called` values are equal to 0.
  EXPECT_OBSERVER_HOOK_CALLED(.malloc = 0, .free = 0);

  // (2) Alloc() causes malloc observer hook called +1.
  void* ptr = allocator->root()->Alloc(32);
  EXPECT_OBSERVER_HOOK_CALLED(.malloc = 1u, .free = 0);

  // (3) Realloc() will cause Alloc() and Free().
  ptr = allocator->root()->Realloc(ptr, 128, nullptr);
  EXPECT_TRUE(ptr);
  EXPECT_OBSERVER_HOOK_CALLED(.malloc = 2u, .free = 1u);

  // (4) Realloc() with `nullptr` is just `malloc()`.
  void* ptr2 = allocator->root()->Realloc(nullptr, 128, nullptr);
  EXPECT_TRUE(ptr2);
  EXPECT_OBSERVER_HOOK_CALLED(.malloc = 3u, .free = 1u);

  // (5) Free() causes free observer hook called +1.
  allocator->root()->Free(ptr);
  EXPECT_OBSERVER_HOOK_CALLED(.malloc = 3u, .free = 2u);

  // (6) Realloc() with `address` and 0-size is just `free()`.
  allocator->root()->Realloc(ptr2, 0, nullptr);
  EXPECT_OBSERVER_HOOK_CALLED(.malloc = 3u, .free = 3u);
}

TEST_F(PAMemoryToolReplacesTest, BothHookCalled) {
  std::unique_ptr<PartitionAllocator> allocator = CreateAllocator();
  ScopedOverrideHook override_scope(MallocOverrideHook, FreeOverrideHook,
                                    ReallocOverrideHook);
  ScopedObserverHook observer_scope(MallocObserverHook, FreeObserverHook);
  EXPECT_OVERRIDE_HOOK_CALLED(.malloc = 0, .free = 0, .realloc = 0);
  EXPECT_OBSERVER_HOOK_CALLED(.malloc = 0, .free = 0);

  void* ptr1 = allocator->root()->Alloc(32);
  EXPECT_TRUE(ptr1);
  EXPECT_OVERRIDE_HOOK_CALLED(.malloc = 1u, .free = 0, .realloc = 0);
  EXPECT_OBSERVER_HOOK_CALLED(.malloc = 1u, .free = 0);

  void* ptr2 = allocator->root()->Alloc(128);
  EXPECT_TRUE(ptr2);
  EXPECT_OVERRIDE_HOOK_CALLED(.malloc = 2u, .free = 0, .realloc = 0);
  EXPECT_OBSERVER_HOOK_CALLED(.malloc = 2u, .free = 0);

  allocator->root()->Free(ptr2);
  EXPECT_OVERRIDE_HOOK_CALLED(.malloc = 2u, .free = 1u, .realloc = 0);
  EXPECT_OBSERVER_HOOK_CALLED(.malloc = 2u, .free = 1u);

  ptr1 = allocator->root()->Realloc(ptr1, 32, nullptr);
  EXPECT_TRUE(ptr1);
  EXPECT_OVERRIDE_HOOK_CALLED(.malloc = 3u, .free = 2u, .realloc = 1u);
  EXPECT_OBSERVER_HOOK_CALLED(.malloc = 3u, .free = 2u);

  void* ptr3 = allocator->root()->Realloc(nullptr, 128, nullptr);
  EXPECT_TRUE(ptr3);
  EXPECT_OVERRIDE_HOOK_CALLED(.malloc = 4u, .free = 2u, .realloc = 1u);
  EXPECT_OBSERVER_HOOK_CALLED(.malloc = 4u, .free = 2u);

  ptr1 = allocator->root()->Realloc(ptr1, 0, nullptr);
  EXPECT_FALSE(ptr1);
  EXPECT_OVERRIDE_HOOK_CALLED(.malloc = 4u, .free = 3u, .realloc = 1u);
  EXPECT_OBSERVER_HOOK_CALLED(.malloc = 4u, .free = 3u);

  allocator->root()->Free(ptr3);
  EXPECT_OVERRIDE_HOOK_CALLED(.malloc = 4u, .free = 4u, .realloc = 1u);
  EXPECT_OBSERVER_HOOK_CALLED(.malloc = 4u, .free = 4u);
}

TEST_F(PAMemoryToolReplacesTest, ReallocOverrideHookCalled) {
  std::unique_ptr<PartitionAllocator> allocator = CreateAllocator();
  ScopedOverrideHook override_scope(MallocOverrideHook, FreeOverrideHook,
                                    ReallocOverrideReturns32Hook);
  // (1) firstly, all `override hook called` values are equal to 0.
  EXPECT_OVERRIDE_HOOK_CALLED(.malloc = 0, .free = 0, .realloc = 0);

  // (2) Alloc() causes malloc override hook called 1.
  constexpr size_t kAllocSize = 64u;
  void* ptr = allocator->root()->Alloc(kAllocSize);
  EXPECT_TRUE(ptr);
  EXPECT_OVERRIDE_HOOK_CALLED(.malloc = 1u, .free = 0, .realloc = 0);
  // SAFETY: `ptr` was allocated with size `kAllocSize`, so `[ptr, ptr +
  // kAllocSize)` is a valid buffer.
  PA_UNSAFE_BUFFERS(internal::base::span<uint8_t> alloced(
      reinterpret_cast<uint8_t*>(ptr), kAllocSize));
  std::fill(alloced.begin(), alloced.end(), 'a');

  // (3) ReallocOverrideReturns32Hook() makes `old_usable_size` 32u.
  // Only the first 32 bytes are kept.
  constexpr size_t kReallocSize = 64u;
  ptr = allocator->root()->Realloc<AllocFlags::kZeroFill, FreeFlags::kNone>(
      ptr, kReallocSize, nullptr);
  EXPECT_TRUE(ptr);
  EXPECT_OVERRIDE_HOOK_CALLED(.malloc = 2u, .free = 1u, .realloc = 1);

  // SAFETY: `ptr` was reallocated with size `kReallocSize`, so `[ptr, ptr +
  // kReallocSize)` is a valid buffer.
  PA_UNSAFE_BUFFERS(internal::base::span<uint8_t> realloced(
      reinterpret_cast<uint8_t*>(ptr), kReallocSize));
  auto it = std::find(realloced.begin(), realloced.end(), 0);
  EXPECT_EQ(kReallocOverrideReturns32ReturnsSize,
            static_cast<size_t>(std::distance(realloced.begin(), it)));

  // (4) Free() causes free override hook called 1.
  allocator->root()->Free(ptr);
  EXPECT_OVERRIDE_HOOK_CALLED(.malloc = 2u, .free = 2u, .realloc = 1);
}

}  // namespace partition_alloc::test

#endif  // PA_BUILDFLAG(MEMORY_TOOL_REPLACES_ALLOCATOR)
