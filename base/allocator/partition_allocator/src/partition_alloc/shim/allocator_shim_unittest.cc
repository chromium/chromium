// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/shim/allocator_shim.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <memory>
#include <new>
#include <sstream>
#include <type_traits>
#include <vector>

#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc.h"
#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/memory/page_size.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(IS_WIN)
#include <windows.h>

#include <malloc.h>
#elif PA_BUILDFLAG(IS_APPLE)
#include <malloc/malloc.h>

#include "partition_alloc/shim/allocator_interception_apple.h"
#include "partition_alloc/third_party/apple_apsl/malloc.h"
#else
#include <malloc.h>
#endif

#if !PA_BUILDFLAG(IS_WIN)
#include <unistd.h>
#endif

#if PA_BUILDFLAG(PA_LIBC_GLIBC)
extern "C" void* __libc_memalign(size_t align, size_t s);
#endif

#if PA_BUILDFLAG( \
    ENABLE_ALLOCATOR_SHIM_PARTITION_ALLOC_DISPATCH_WITH_ADVANCED_CHECKS_SUPPORT)
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc_with_advanced_checks.h"
#endif

// Weak as this is a C11 function, which is not always available. It is also not
// always defined in stdlib.h, for instance on Android prior to API level 28.
extern "C" {
void* __attribute__((weak)) aligned_alloc(size_t alignment, size_t size);
}

namespace allocator_shim {
namespace {

using testing::_;
using testing::MockFunction;

extern AllocatorDispatch g_mock_dispatch;

// Special sentinel values used for testing GetSizeEstimate() interception.
const char kTestSizeEstimateData[] = "test_value";
constexpr void* kTestSizeEstimateAddress = (void*)kTestSizeEstimateData;
constexpr size_t kTestSizeEstimate = 1234;

class AllocatorShimTest : public testing::Test {
 public:
  AllocatorShimTest() = default;

  static size_t Hash(const void* ptr) {
    return reinterpret_cast<uintptr_t>(ptr) % MaxSizeTracked();
  }

  static void* MockAlloc(size_t size, void* context) {
    if (instance_ && size < MaxSizeTracked()) {
      ++(instance_->allocs_intercepted_by_size[size]);
    }
    return g_mock_dispatch.next->alloc_function(size, context);
  }

  static void* MockAllocUnchecked(size_t size, void* context) {
    if (instance_ && size < MaxSizeTracked()) {
      ++(instance_->allocs_intercepted_by_size[size]);
    }
    return g_mock_dispatch.next->alloc_unchecked_function(size, context);
  }

  static void* MockAllocZeroInit(size_t n, size_t size, void* context) {
    const size_t real_size = n * size;
    if (instance_ && real_size < MaxSizeTracked()) {
      ++(instance_->zero_allocs_intercepted_by_size[real_size]);
    }
    return g_mock_dispatch.next->alloc_zero_initialized_function(n, size,
                                                                 context);
  }

  static void* MockAllocZeroInitUnchecked(size_t n,
                                          size_t size,
                                          void* context) {
    const size_t real_size = n * size;
    if (instance_ && real_size < MaxSizeTracked()) {
      ++(instance_->zero_allocs_intercepted_by_size[real_size]);
    }
    return g_mock_dispatch.next->alloc_zero_initialized_unchecked_function(
        n, size, context);
  }

  static void* MockAllocAligned(size_t alignment, size_t size, void* context) {
    if (instance_) {
      if (size < MaxSizeTracked()) {
        ++(instance_->allocs_intercepted_by_size[size]);
      }
      if (alignment < MaxSizeTracked()) {
        ++(instance_->allocs_intercepted_by_alignment[alignment]);
      }
    }
    return g_mock_dispatch.next->alloc_aligned_function(alignment, size,
                                                        context);
  }

  static void* MockRealloc(void* address, size_t size, void* context) {
    if (instance_) {
      // Size 0xFEED is a special sentinel for the NewHandlerConcurrency test.
      // Hitting it for the first time will cause a failure, causing the
      // invocation of the std::new_handler.
      if (size == 0xFEED) {
        thread_local bool did_fail_realloc_0xfeed_once = false;
        if (!did_fail_realloc_0xfeed_once) {
          did_fail_realloc_0xfeed_once = true;
          return nullptr;
        }
        return address;
      }

      if (size < MaxSizeTracked()) {
        ++(instance_->reallocs_intercepted_by_size[size]);
      }
      ++instance_->reallocs_intercepted_by_addr[Hash(address)];
    }
    return g_mock_dispatch.next->realloc_function(address, size, context);
  }

  static void* MockReallocUnchecked(void* address, size_t size, void* context) {
    if (instance_) {
      // Size 0xFEED is a special sentinel for the NewHandlerConcurrency test.
      // Hitting it for the first time will cause a failure, causing the
      // invocation of the std::new_handler.
      if (size == 0xFEED) {
        thread_local bool did_fail_realloc_0xfeed_once = false;
        if (!did_fail_realloc_0xfeed_once) {
          did_fail_realloc_0xfeed_once = true;
          return nullptr;
        }
        return address;
      }

      if (size < MaxSizeTracked()) {
        ++(instance_->reallocs_intercepted_by_size[size]);
      }
      ++instance_->reallocs_intercepted_by_addr[Hash(address)];
    }
    return g_mock_dispatch.next->realloc_unchecked_function(address, size,
                                                            context);
  }

  static void MockFree(void* address, void* context) {
    if (instance_) {
      ++instance_->frees_intercepted_by_addr[Hash(address)];
    }
    g_mock_dispatch.next->free_function(address, context);
  }

  static void MockFreeWithSize(void* ptr, size_t size, void* context) {
    if (instance_) {
      ++instance_->frees_intercepted_by_addr[Hash(ptr)];
      if (size < MaxSizeTracked()) {
        ++(instance_->frees_intercepted_by_size[size]);
      }
    }
    g_mock_dispatch.next->free_with_size_function(ptr, size, context);
  }

  static void MockFreeWithAlignment(void* ptr,
                                    size_t alignment,
                                    void* context) {
    if (instance_) {
      ++instance_->frees_intercepted_by_addr[Hash(ptr)];
      if (alignment < MaxSizeTracked()) {
        ++(instance_->frees_intercepted_by_alignment[alignment]);
      }
    }
    g_mock_dispatch.next->free_with_alignment_function(ptr, alignment, context);
  }

  static void MockFreeWithSizeAndAlignment(void* ptr,
                                           size_t size,
                                           size_t alignment,
                                           void* context) {
    if (instance_) {
      ++instance_->frees_intercepted_by_addr[Hash(ptr)];
      if (size < MaxSizeTracked()) {
        ++(instance_->frees_intercepted_by_size[size]);
      }
      if (alignment < MaxSizeTracked()) {
        ++(instance_->frees_intercepted_by_alignment[alignment]);
      }
    }
    g_mock_dispatch.next->free_with_size_and_alignment_function(
        ptr, size, alignment, context);
  }

  static size_t MockGetSizeEstimate(void* address, void* context) {
    // Special testing values for GetSizeEstimate() interception.
    if (address == kTestSizeEstimateAddress) {
      return kTestSizeEstimate;
    }
    return g_mock_dispatch.next->get_size_estimate_function(address, context);
  }

  static bool MockClaimedAddress(void* address, void* context) {
    // The same as MockGetSizeEstimate.
    if (address == kTestSizeEstimateAddress) {
      return true;
    }
    return g_mock_dispatch.next->claimed_address_function(address, context);
  }

  static size_t MockGoodSize(size_t size, void* context) { return size; }

  static unsigned MockBatchMalloc(size_t size,
                                  void** results,
                                  unsigned num_requested,
                                  void* context) {
    if (instance_) {
      instance_->batch_mallocs_intercepted_by_size[size] =
          instance_->batch_mallocs_intercepted_by_size[size] + num_requested;
    }
    return g_mock_dispatch.next->batch_malloc_function(size, results,
                                                       num_requested, context);
  }

  static void MockBatchFree(void** to_be_freed,
                            unsigned num_to_be_freed,
                            void* context) {
    if (instance_) {
      for (unsigned i = 0; i < num_to_be_freed; ++i) {
        ++instance_->batch_frees_intercepted_by_addr[Hash(
            PA_UNSAFE_TODO(to_be_freed[i]))];
      }
    }
    g_mock_dispatch.next->batch_free_function(to_be_freed, num_to_be_freed,
                                              context);
  }

  static void MockTryFreeDefault(void* ptr, void* context) {
    if (instance_) {
      ++instance_->frees_intercepted_by_addr[Hash(ptr)];
    }
    g_mock_dispatch.next->try_free_default_function(ptr, context);
  }

  static void* MockAlignedMalloc(size_t size, size_t alignment, void* context) {
    if (instance_ && size < MaxSizeTracked()) {
      ++instance_->allocs_intercepted_by_size[size];
    }
    if (alignment < MaxSizeTracked()) {
      ++(instance_->allocs_intercepted_by_alignment[alignment]);
    }
    return g_mock_dispatch.next->aligned_malloc_function(size, alignment,
                                                         context);
  }

  static void* MockAlignedMallocUnchecked(size_t size,
                                          size_t alignment,
                                          void* context) {
    if (instance_ && size < MaxSizeTracked()) {
      ++instance_->allocs_intercepted_by_size[size];
    }
    if (alignment < MaxSizeTracked()) {
      ++(instance_->allocs_intercepted_by_alignment[alignment]);
    }
    return g_mock_dispatch.next->aligned_malloc_unchecked_function(
        size, alignment, context);
  }

  static void* MockAlignedRealloc(void* address,
                                  size_t size,
                                  size_t alignment,
                                  void* context) {
    if (instance_) {
      if (size < MaxSizeTracked()) {
        ++instance_->aligned_reallocs_intercepted_by_size[size];
      }
      ++instance_->aligned_reallocs_intercepted_by_addr[Hash(address)];
    }
    return g_mock_dispatch.next->aligned_realloc_function(address, size,
                                                          alignment, context);
  }

  static void* MockAlignedReallocUnchecked(void* address,
                                           size_t size,
                                           size_t alignment,
                                           void* context) {
    if (instance_) {
      if (size < MaxSizeTracked()) {
        ++instance_->aligned_reallocs_intercepted_by_size[size];
      }
      ++instance_->aligned_reallocs_intercepted_by_addr[Hash(address)];
    }
    return g_mock_dispatch.next->aligned_realloc_unchecked_function(
        address, size, alignment, context);
  }

  static void MockAlignedFree(void* address, void* context) {
    if (instance_) {
      ++instance_->frees_intercepted_by_addr[Hash(address)];
    }
    g_mock_dispatch.next->aligned_free_function(address, context);
  }

  static void NewHandler() {
    if (!instance_) {
      return;
    }
    instance_->num_new_handler_calls.fetch_add(1, std::memory_order_relaxed);
  }

  int32_t GetNumberOfNewHandlerCalls() {
    return instance_->num_new_handler_calls.load(std::memory_order_acquire);
  }

  void SetUp() override {
    allocs_intercepted_by_size.resize(MaxSizeTracked());
    allocs_intercepted_by_alignment.resize(MaxSizeTracked());
    zero_allocs_intercepted_by_size.resize(MaxSizeTracked());
    reallocs_intercepted_by_size.resize(MaxSizeTracked());
    reallocs_intercepted_by_addr.resize(MaxSizeTracked());
    frees_intercepted_by_addr.resize(MaxSizeTracked());
    frees_intercepted_by_size.resize(MaxSizeTracked());
    frees_intercepted_by_alignment.resize(MaxSizeTracked());
    batch_mallocs_intercepted_by_size.resize(MaxSizeTracked());
    batch_frees_intercepted_by_addr.resize(MaxSizeTracked());
    aligned_reallocs_intercepted_by_size.resize(MaxSizeTracked());
    aligned_reallocs_intercepted_by_addr.resize(MaxSizeTracked());
    num_new_handler_calls.store(0, std::memory_order_release);
    instance_ = this;

#if PA_BUILDFLAG(IS_APPLE)
    InitializeAllocatorShim();
#endif
  }

  void TearDown() override {
    instance_ = nullptr;
#if PA_BUILDFLAG(IS_APPLE)
    UninterceptMallocZonesForTesting();
#endif
  }

  static size_t MaxSizeTracked() {
#if PA_BUILDFLAG(IS_IOS)
    // TODO(crbug.com/40129080): 64-bit iOS uses a page size that is larger than
    // SystemPageSize(), causing this test to make larger allocations, relative
    // to SystemPageSize().
    return 6 * partition_alloc::internal::SystemPageSize();
#else
    return 2 * partition_alloc::internal::SystemPageSize();
#endif
  }

 protected:
  std::vector<size_t> allocs_intercepted_by_size;
  std::vector<size_t> allocs_intercepted_by_alignment;
  std::vector<size_t> zero_allocs_intercepted_by_size;
  std::vector<size_t> reallocs_intercepted_by_size;
  std::vector<size_t> reallocs_intercepted_by_addr;
  std::vector<size_t> frees_intercepted_by_addr;
  std::vector<size_t> frees_intercepted_by_size;
  std::vector<size_t> frees_intercepted_by_alignment;
  std::vector<size_t> batch_mallocs_intercepted_by_size;
  std::vector<size_t> batch_frees_intercepted_by_addr;
  std::vector<size_t> aligned_reallocs_intercepted_by_size;
  std::vector<size_t> aligned_reallocs_intercepted_by_addr;
  std::atomic<uint32_t> num_new_handler_calls;

 private:
  static AllocatorShimTest* instance_;
};

class ThreadDelegateForNewHandlerTest : public base::PlatformThread::Delegate {
 public:
  explicit ThreadDelegateForNewHandlerTest(base::WaitableEvent* event)
      : event_(event) {}

  void ThreadMain() override {
    event_->Wait();
    void* temp = malloc(1);
    void* res = realloc(temp, 0xFEED);
    EXPECT_EQ(temp, res);
  }

 private:
  base::WaitableEvent* event_;
};

AllocatorShimTest* AllocatorShimTest::instance_ = nullptr;

AllocatorDispatch g_mock_dispatch = {
    &AllocatorShimTest::MockAlloc,          /* alloc_function */
    &AllocatorShimTest::MockAllocUnchecked, /* alloc_unchecked_function */
    &AllocatorShimTest::MockAllocZeroInit, /* alloc_zero_initialized_function */
    &AllocatorShimTest::
        MockAllocZeroInitUnchecked, /* alloc_zero_initialized_unchecked_function
                                     */
    &AllocatorShimTest::MockAllocAligned,      /* alloc_aligned_function */
    &AllocatorShimTest::MockRealloc,           /* realloc_function */
    &AllocatorShimTest::MockReallocUnchecked,  /* realloc_unchecked_function */
    &AllocatorShimTest::MockFree,              /* free_function */
    &AllocatorShimTest::MockFreeWithSize,      /* free_with_size_function */
    &AllocatorShimTest::MockFreeWithAlignment, /* free_with_alignment_function
                                                */
    &AllocatorShimTest::
        MockFreeWithSizeAndAlignment, /* free_with_size_and_alignment_function
                                       */
    &AllocatorShimTest::MockGetSizeEstimate, /* get_size_estimate_function */
    &AllocatorShimTest::MockGoodSize,        /* good_size */
    &AllocatorShimTest::MockClaimedAddress,  /* claimed_address_function */
    &AllocatorShimTest::MockBatchMalloc,     /* batch_malloc_function */
    &AllocatorShimTest::MockBatchFree,       /* batch_free_function */
    &AllocatorShimTest::MockTryFreeDefault,  /* try_free_default_function */
    &AllocatorShimTest::MockAlignedMalloc,   /* aligned_malloc_function */
    &AllocatorShimTest::MockAlignedMallocUnchecked,
    /* aligned_malloc_unchecked_function */
    &AllocatorShimTest::MockAlignedRealloc, /* aligned_realloc_function */
    &AllocatorShimTest::MockAlignedReallocUnchecked,
    /* aligned_realloc_unchecked_function */
    &AllocatorShimTest::MockAlignedFree, /* aligned_free_function */
    nullptr,                             /* next */
};

TEST_F(AllocatorShimTest, InterceptLibcSymbols) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  void* alloc_ptr = malloc(19);
  ASSERT_NE(nullptr, alloc_ptr);
  ASSERT_GE(allocs_intercepted_by_size[19], 1u);

  void* zero_alloc_ptr = calloc(2, 23);
  ASSERT_NE(nullptr, zero_alloc_ptr);
  ASSERT_GE(zero_allocs_intercepted_by_size[2 * 23], 1u);

#if !PA_BUILDFLAG(IS_WIN)
  void* posix_memalign_ptr = nullptr;
  int res = posix_memalign(&posix_memalign_ptr, 256, 59);
  ASSERT_EQ(0, res);
  ASSERT_NE(nullptr, posix_memalign_ptr);
  ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(posix_memalign_ptr) % 256);
  ASSERT_GE(allocs_intercepted_by_alignment[256], 1u);
  ASSERT_GE(allocs_intercepted_by_size[59], 1u);

  // (p)valloc() are not defined on Android. pvalloc() is a GNU extension,
  // valloc() is not in POSIX.
#if !PA_BUILDFLAG(IS_ANDROID)
  const size_t kPageSize = partition_alloc::internal::base::GetPageSize();
  void* valloc_ptr = valloc(61);
  ASSERT_NE(nullptr, valloc_ptr);
  ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(valloc_ptr) % kPageSize);
  ASSERT_GE(allocs_intercepted_by_alignment[kPageSize], 1u);
  ASSERT_GE(allocs_intercepted_by_size[61], 1u);
#endif  // !PA_BUILDFLAG(IS_ANDROID)

#endif  // !PA_BUILDFLAG(IS_WIN)

#if !PA_BUILDFLAG(IS_WIN) && !PA_BUILDFLAG(IS_APPLE)
  void* memalign_ptr = memalign(128, 53);
  ASSERT_NE(nullptr, memalign_ptr);
  ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(memalign_ptr) % 128);
  ASSERT_GE(allocs_intercepted_by_alignment[128], 1u);
  ASSERT_GE(allocs_intercepted_by_size[53], 1u);

#if PA_BUILDFLAG(IS_POSIX) && !PA_BUILDFLAG(IS_ANDROID)
  void* pvalloc_ptr = pvalloc(67);
  ASSERT_NE(nullptr, pvalloc_ptr);
  ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(pvalloc_ptr) % kPageSize);
  ASSERT_GE(allocs_intercepted_by_alignment[kPageSize], 1u);
  // pvalloc rounds the size up to the next page.
  ASSERT_GE(allocs_intercepted_by_size[kPageSize], 1u);
#endif  // PA_BUILDFLAG(IS_POSIX) && !PA_BUILDFLAG(IS_ANDROID)

#endif  // !PA_BUILDFLAG(IS_WIN) && !PA_BUILDFLAG(IS_APPLE)

// See allocator_shim_override_glibc_weak_symbols.h for why we intercept
// internal libc symbols.
#if PA_BUILDFLAG(PA_LIBC_GLIBC) && PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  void* libc_memalign_ptr = __libc_memalign(512, 56);
  ASSERT_NE(nullptr, memalign_ptr);
  ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(libc_memalign_ptr) % 512);
  ASSERT_GE(allocs_intercepted_by_alignment[512], 1u);
  ASSERT_GE(allocs_intercepted_by_size[56], 1u);
#endif

  // TODO(crbug.com/407932921) Support Apple platforms.
#if !BUILDFLAG(IS_APPLE)
  // See above, it is a weak symbol.
#pragma clang diagnostic push  // Can be removed once our min-sdk is >= 28.
#pragma clang diagnostic ignored "-Wunguarded-availability"
  if (aligned_alloc) {
    void* aligned_alloc_ptr = aligned_alloc(128, 32);
#pragma clang diagnostic pop
    ASSERT_NE(nullptr, aligned_alloc_ptr);
    ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(aligned_alloc_ptr) % 128);
    ASSERT_GE(allocs_intercepted_by_alignment[128], 1u);
    ASSERT_GE(allocs_intercepted_by_size[32], 1u);
  }
#endif  // !BUILDFLAG(IS_APPLE)

  char* realloc_ptr = static_cast<char*>(malloc(10));
  PA_UNSAFE_TODO(strcpy(realloc_ptr, "foobar"));
  void* old_realloc_ptr = realloc_ptr;
  realloc_ptr = static_cast<char*>(realloc(realloc_ptr, 73));
  ASSERT_GE(reallocs_intercepted_by_size[73], 1u);
  ASSERT_GE(reallocs_intercepted_by_addr[Hash(old_realloc_ptr)], 1u);
  ASSERT_EQ(0, PA_UNSAFE_TODO(strcmp(realloc_ptr, "foobar")));

  free(alloc_ptr);
  ASSERT_GE(frees_intercepted_by_addr[Hash(alloc_ptr)], 1u);

  free(zero_alloc_ptr);
  ASSERT_GE(frees_intercepted_by_addr[Hash(zero_alloc_ptr)], 1u);

#if !PA_BUILDFLAG(IS_WIN) && !PA_BUILDFLAG(IS_APPLE)
  free(memalign_ptr);
  ASSERT_GE(frees_intercepted_by_addr[Hash(memalign_ptr)], 1u);

#if PA_BUILDFLAG(IS_POSIX) && !PA_BUILDFLAG(IS_ANDROID)
  free(pvalloc_ptr);
  ASSERT_GE(frees_intercepted_by_addr[Hash(pvalloc_ptr)], 1u);
#endif  // PA_BUILDFLAG(IS_POSIX) && !PA_BUILDFLAG(IS_ANDROID)

#endif  // !PA_BUILDFLAG(IS_WIN) && !PA_BUILDFLAG(IS_APPLE)

#if !PA_BUILDFLAG(IS_WIN)
  free(posix_memalign_ptr);
  ASSERT_GE(frees_intercepted_by_addr[Hash(posix_memalign_ptr)], 1u);

#if !PA_BUILDFLAG(IS_ANDROID)
  free(valloc_ptr);
  ASSERT_GE(frees_intercepted_by_addr[Hash(valloc_ptr)], 1u);
#endif  // !PA_BUILDFLAG(IS_ANDROID)

#endif  // !PA_BUILDFLAG(IS_WIN)

#if PA_BUILDFLAG(PA_LIBC_GLIBC) && PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  free(libc_memalign_ptr);
  ASSERT_GE(frees_intercepted_by_addr[Hash(memalign_ptr)], 1u);
#endif

  free(realloc_ptr);
  ASSERT_GE(frees_intercepted_by_addr[Hash(realloc_ptr)], 1u);

  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);

  void* non_hooked_ptr = malloc(4095);
  ASSERT_NE(nullptr, non_hooked_ptr);
  ASSERT_EQ(0u, allocs_intercepted_by_size[4095]);
  free(non_hooked_ptr);
}

// PartitionAlloc-Everywhere does not support batch_malloc / batch_free.
#if PA_BUILDFLAG(IS_APPLE) && !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
TEST_F(AllocatorShimTest, InterceptLibcSymbolsBatchMallocFree) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  unsigned count = 13;
  std::vector<void*> results;
  results.resize(count);
  unsigned result_count = malloc_zone_batch_malloc(malloc_default_zone(), 99,
                                                   results.data(), count);
  ASSERT_EQ(count, result_count);

  // TODO(erikchen): On macOS 10.12+, batch_malloc in the default zone may
  // forward to another zone, which we've also shimmed, resulting in
  // MockBatchMalloc getting called twice as often as we'd expect. This
  // re-entrancy into the allocator shim is a bug that needs to be fixed.
  // https://crbug.com/693237.
  // ASSERT_EQ(count, batch_mallocs_intercepted_by_size[99]);

  std::vector<void*> results_copy(results);
  malloc_zone_batch_free(malloc_default_zone(), results.data(), count);
  for (void* result : results_copy) {
    ASSERT_GE(batch_frees_intercepted_by_addr[Hash(result)], 1u);
  }
  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}

TEST_F(AllocatorShimTest, InterceptLibcSymbolsFreeWithSize) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  void* alloc_ptr = malloc(19);
  ASSERT_NE(nullptr, alloc_ptr);
  ASSERT_GE(allocs_intercepted_by_size[19], 1u);

  ChromeMallocZone* default_zone =
      reinterpret_cast<ChromeMallocZone*>(malloc_default_zone());
  default_zone->free_definite_size(malloc_default_zone(), alloc_ptr, 19);
  ASSERT_GE(frees_intercepted_by_size[19], 1u);
  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}
#endif  // PA_BUILDFLAG(IS_APPLE) &&
        // !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#if PA_BUILDFLAG(IS_WIN)
TEST_F(AllocatorShimTest, InterceptUcrtAlignedAllocationSymbols) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  constexpr size_t kAlignment = 32;
  void* alloc_ptr = _aligned_malloc(123, kAlignment);
  EXPECT_GE(allocs_intercepted_by_size[123], 1u);

  void* new_alloc_ptr = _aligned_realloc(alloc_ptr, 1234, kAlignment);
  EXPECT_GE(aligned_reallocs_intercepted_by_size[1234], 1u);
  EXPECT_GE(aligned_reallocs_intercepted_by_addr[Hash(alloc_ptr)], 1u);

  _aligned_free(new_alloc_ptr);
  EXPECT_GE(frees_intercepted_by_addr[Hash(new_alloc_ptr)], 1u);

  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}

TEST_F(AllocatorShimTest, AlignedReallocSizeZeroFrees) {
  void* alloc_ptr = _aligned_malloc(123, 16);
  ASSERT_TRUE(alloc_ptr);
  alloc_ptr = _aligned_realloc(alloc_ptr, 0, 16);
  ASSERT_TRUE(!alloc_ptr);
}
#endif  // PA_BUILDFLAG(IS_WIN)

// PartitionAlloc disallows large allocations to avoid errors with int
// overflows.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
struct TooLarge {
  char padding1[1UL << 31];
  int padding2;
};

TEST_F(AllocatorShimTest, NewNoThrowTooLarge) {
  char* too_large_array = new (std::nothrow) char[(1UL << 31) + 100];
  EXPECT_EQ(nullptr, too_large_array);

  TooLarge* too_large_struct = new (std::nothrow) TooLarge;
  EXPECT_EQ(nullptr, too_large_struct);
}
#endif

// This test exercises the case of concurrent OOM failure, which would end up
// invoking std::new_handler concurrently. This is to cover the CallNewHandler()
// paths of allocator_shim.cc and smoke-test its thread safey.
// The test creates kNumThreads threads. Each of them mallocs some memory, and
// then does a realloc(<new memory>, 0xFEED).
// The shim intercepts such realloc and makes it fail only once on each thread.
// We expect to see excactly kNumThreads invocations of the new_handler.
TEST_F(AllocatorShimTest, NewHandlerConcurrency) {
  const int kNumThreads = 32;
  base::PlatformThreadHandle threads[kNumThreads];

  // The WaitableEvent here is used to attempt to trigger all the threads at
  // the same time, after they have been initialized.
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  ThreadDelegateForNewHandlerTest mock_thread_main(&event);

  for (auto& thread : threads) {
    base::PlatformThread::Create(0, &mock_thread_main, &thread);
  }

  std::set_new_handler(&AllocatorShimTest::NewHandler);
  SetCallNewHandlerOnMallocFailure(true);  // It's going to fail on realloc().
  InsertAllocatorDispatch(&g_mock_dispatch);
  event.Signal();
  for (auto& thread : threads) {
    base::PlatformThread::Join(thread);
  }
  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
  ASSERT_EQ(kNumThreads, GetNumberOfNewHandlerCalls());
}

#if PA_BUILDFLAG(IS_WIN)
TEST_F(AllocatorShimTest, ShimReplacesCRTHeapWhenEnabled) {
  ASSERT_EQ(::GetProcessHeap(), reinterpret_cast<HANDLE>(_get_heap_handle()));
}
#endif  // PA_BUILDFLAG(IS_WIN)

#if PA_BUILDFLAG(IS_WIN)
static size_t GetUsableSize(void* ptr) {
  return _msize(ptr);
}
#elif PA_BUILDFLAG(IS_APPLE)
static size_t GetUsableSize(void* ptr) {
  return malloc_size(ptr);
}
#elif PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS) || \
    PA_BUILDFLAG(IS_ANDROID)
static size_t GetUsableSize(void* ptr) {
  return malloc_usable_size(ptr);
}
#else
#define NO_MALLOC_SIZE
#endif

#if !defined(NO_MALLOC_SIZE)
TEST_F(AllocatorShimTest, ShimReplacesMallocSizeWhenEnabled) {
  InsertAllocatorDispatch(&g_mock_dispatch);
  EXPECT_EQ(GetUsableSize(kTestSizeEstimateAddress), kTestSizeEstimate);
  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}

TEST_F(AllocatorShimTest, ShimDoesntChangeMallocSizeWhenEnabled) {
  void* alloc = malloc(16);
  size_t sz = GetUsableSize(alloc);
  EXPECT_GE(sz, 16U);

  InsertAllocatorDispatch(&g_mock_dispatch);
  EXPECT_EQ(GetUsableSize(alloc), sz);
  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);

  free(alloc);
}
#endif  // !defined(NO_MALLOC_SIZE)

#if PA_BUILDFLAG(IS_ANDROID)
TEST_F(AllocatorShimTest, InterceptCLibraryFunctions) {
  auto total_counts = [](const std::vector<size_t>& counts) {
    size_t total = 0;
    for (const auto count : counts) {
      total += count;
    }
    return total;
  };
  size_t counts_before;
  size_t counts_after = total_counts(allocs_intercepted_by_size);
  void* ptr;

  InsertAllocatorDispatch(&g_mock_dispatch);

  // <cstdlib>
  counts_before = counts_after;
  ptr = realpath(".", nullptr);
  EXPECT_NE(nullptr, ptr);
  free(ptr);
  counts_after = total_counts(allocs_intercepted_by_size);
  EXPECT_GT(counts_after, counts_before);

  // <cstring>
  counts_before = counts_after;
  ptr = strdup("hello, world");
  EXPECT_NE(nullptr, ptr);
  free(ptr);
  counts_after = total_counts(allocs_intercepted_by_size);
  EXPECT_GT(counts_after, counts_before);

  counts_before = counts_after;
  ptr = PA_UNSAFE_TODO(strndup("hello, world", 5));
  EXPECT_NE(nullptr, ptr);
  free(ptr);
  counts_after = total_counts(allocs_intercepted_by_size);
  EXPECT_GT(counts_after, counts_before);

  // <unistd.h>
  counts_before = counts_after;
  ptr = getcwd(nullptr, 0);
  EXPECT_NE(nullptr, ptr);
  free(ptr);
  counts_after = total_counts(allocs_intercepted_by_size);
  EXPECT_GT(counts_after, counts_before);

  // With component builds on Android, we cannot intercept calls to functions
  // inside another component, in this instance the call to vasprintf() inside
  // libc++. This is not necessarily an issue for allocator shims, as long as we
  // accept that allocations and deallocations will not be matched at all times.
  // It is however essential for PartitionAlloc, which is exercized in the test
  // below.
#ifndef COMPONENT_BUILD
  // Calls vasprintf() indirectly, see below.
  counts_before = counts_after;
  std::stringstream stream;
  stream << std::setprecision(1) << std::showpoint << std::fixed << 1.e38;
  EXPECT_GT(stream.str().size(), 30u);
  counts_after = total_counts(allocs_intercepted_by_size);
  EXPECT_GT(counts_after, counts_before);
#endif  // COMPONENT_BUILD

  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// Non-regression test for crbug.com/1166558.
TEST_F(AllocatorShimTest, InterceptVasprintf) {
  // Printing a float which expands to >=30 characters calls vasprintf() in
  // libc, which we should intercept.
  std::stringstream stream;
  stream << std::setprecision(1) << std::showpoint << std::fixed << 1.e38;
  EXPECT_GT(stream.str().size(), 30u);
  // Should not crash.
}

TEST_F(AllocatorShimTest, InterceptLongVasprintf) {
  char* str = nullptr;
  const char* lorem_ipsum =
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed non risus. "
      "Suspendisse lectus tortor, dignissim sit amet, adipiscing nec, "
      "ultricies sed, dolor. Cras elementum ultrices diam. Maecenas ligula "
      "massa, varius a, semper congue, euismod non, mi. Proin porttitor, orci "
      "nec nonummy molestie, enim est eleifend mi, non fermentum diam nisl sit "
      "amet erat. Duis semper. Duis arcu massa, scelerisque vitae, consequat "
      "in, pretium a, enim. Pellentesque congue. Ut in risus volutpat libero "
      "pharetra tempor. Cras vestibulum bibendum augue. Praesent egestas leo "
      "in pede. Praesent blandit odio eu enim. Pellentesque sed dui ut augue "
      "blandit sodales. Vestibulum ante ipsum primis in faucibus orci luctus "
      "et ultrices posuere cubilia Curae; Aliquam nibh. Mauris ac mauris sed "
      "pede pellentesque fermentum. Maecenas adipiscing ante non diam sodales "
      "hendrerit.";
  int err = asprintf(&str, "%s", lorem_ipsum);
  EXPECT_EQ(err, static_cast<int>(strlen(lorem_ipsum)));
  EXPECT_TRUE(str);
  free(str);
}

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#endif  // PA_BUILDFLAG(IS_ANDROID)

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && PA_BUILDFLAG(IS_APPLE)

// Non-regression test for crbug.com/1291885.
TEST_F(AllocatorShimTest, BatchMalloc) {
  constexpr unsigned kNumToAllocate = 20;
  void* pointers[kNumToAllocate];

  EXPECT_EQ(kNumToAllocate, malloc_zone_batch_malloc(malloc_default_zone(), 10,
                                                     pointers, kNumToAllocate));
  malloc_zone_batch_free(malloc_default_zone(), pointers, kNumToAllocate);
  // Should not crash.
}

TEST_F(AllocatorShimTest, MallocGoodSize) {
  constexpr size_t kTestSize = 100;
  size_t good_size = malloc_good_size(kTestSize);
  EXPECT_GE(good_size, kTestSize);
}

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && PA_BUILDFLAG(IS_APPLE)

TEST_F(AllocatorShimTest, OptimizeAllocatorDispatchTable) {
  const AllocatorDispatch* prev = GetAllocatorDispatchChainHeadForTesting();

  // The nullptr entries are replaced with the functions in the head.
  AllocatorDispatch empty_dispatch{nullptr};
  InsertAllocatorDispatch(&empty_dispatch);
  const AllocatorDispatch* head = GetAllocatorDispatchChainHeadForTesting();
  EXPECT_EQ(head->alloc_function, prev->alloc_function);
  EXPECT_EQ(head->realloc_function, prev->realloc_function);
  EXPECT_EQ(head->free_function, prev->free_function);
  EXPECT_EQ(head->get_size_estimate_function, prev->get_size_estimate_function);
  RemoveAllocatorDispatchForTesting(&empty_dispatch);

  // Partially nullptr and partially non-nullptr.
  AllocatorDispatch non_empty_dispatch{nullptr};
  non_empty_dispatch.get_size_estimate_function =
      AllocatorShimTest::MockGetSizeEstimate;
  InsertAllocatorDispatch(&non_empty_dispatch);
  head = GetAllocatorDispatchChainHeadForTesting();
  EXPECT_EQ(head->alloc_function, prev->alloc_function);
  EXPECT_EQ(head->realloc_function, prev->realloc_function);
  EXPECT_EQ(head->free_function, prev->free_function);
  EXPECT_NE(head->get_size_estimate_function, prev->get_size_estimate_function);
  EXPECT_EQ(head->get_size_estimate_function,
            AllocatorShimTest::MockGetSizeEstimate);
  RemoveAllocatorDispatchForTesting(&non_empty_dispatch);
}

class AllocatorShimCppOperatorTest : public AllocatorShimTest {
  template <typename T>
  static constexpr size_t GetPaddingSize() {
    if (std::is_array_v<T> &&
        !std::is_trivially_destructible_v<std::remove_all_extents_t<T>>) {
#if !PA_BUILDFLAG(IS_APPLE) || !PA_BUILDFLAG(PA_ARCH_CPU_ARM64)
      // Itanium C++ ABI defines a cookie, a region to store an array size, and
      // its size is as follows.
      return std::max(sizeof(size_t), std::alignment_of_v<T>);
#else
      // On ARM Apple devices, they store a pair of integers, one for element
      // size and the other for element count.
      return std::max(sizeof(size_t) * 2, std::alignment_of_v<T>);
#endif  // !PA_BUILDFLAG(IS_APPLE) || !PA_BUILDFLAG(PA_ARCH_CPU_ARM64)
    } else {
      // Cookie is not used.
      return 0;
    }
  }

  template <typename T>
  static constexpr size_t GetAllocSize() {
    return sizeof(T) + GetPaddingSize<T>();
  }

  template <typename T>
  static size_t Hash(const void* ptr) {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    addr -= GetPaddingSize<T>();
    return addr % MaxSizeTracked();
  }

 protected:
  static constexpr size_t GetAllocSize(size_t size, size_t alignment) {
#if !PA_BUILDFLAG(IS_APPLE) || !defined(COMPONENT_BUILD)
    return size;
#else
    return partition_alloc::internal::base::bits::AlignUp(size, alignment);
#endif  // !PA_BUILDFLAG(IS_APPLE) || !defined(COMPONENT_BUILD)
  }

  // Tests `operator new()` and `operator delete()` against `T`.
  template <typename T, bool use_nothrow>
  void NewAndDeleteSingle() {
    InsertAllocatorDispatch(&g_mock_dispatch);

    constexpr auto kSize = GetAllocSize<T>();
    constexpr auto kAlignment = std::alignment_of_v<T>;

    T* new_ptr = use_nothrow ? new (std::nothrow) T : new T;
    ASSERT_NE(nullptr, new_ptr);
    ASSERT_GE(allocs_intercepted_by_size[kSize], 1u);
    ASSERT_EQ(reinterpret_cast<uintptr_t>(new_ptr) % kAlignment, 0);
    given_alignment_on_alloc_ = allocs_intercepted_by_alignment[kAlignment];

    delete new_ptr;
    ASSERT_GE(frees_intercepted_by_addr[Hash<T>(new_ptr)], 1u);
    given_size_on_delete_ = frees_intercepted_by_size[kSize];
    given_alignment_on_delete_ = frees_intercepted_by_alignment[kAlignment];

    RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
  }

  // Tests `operator new[]()` and `operator delete[]()` against `T[3]`.
  template <typename T, bool use_nothrow>
  void NewAndDeleteTriplet() {
    InsertAllocatorDispatch(&g_mock_dispatch);

    constexpr auto kSize = GetAllocSize<T[3]>();
    constexpr auto kAlignment = std::alignment_of_v<T>;

    T* new_ptr = use_nothrow ? new (std::nothrow) T[3] : new T[3];
    ASSERT_NE(nullptr, new_ptr);
    ASSERT_GE(allocs_intercepted_by_size[kSize], 1u);
    ASSERT_EQ(reinterpret_cast<uintptr_t>(new_ptr) % kAlignment, 0);
    given_alignment_on_alloc_ = allocs_intercepted_by_alignment[kAlignment];

    delete[] new_ptr;
    const auto hash = Hash<T[]>(new_ptr);
    ASSERT_GE(frees_intercepted_by_addr[hash], 1u);
    given_size_on_delete_ = frees_intercepted_by_size[kSize];
    given_alignment_on_delete_ = frees_intercepted_by_alignment[kAlignment];

    RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
  }

  // Tests `operator new()` and `operator delete()` against `T`, but indirectly
  // through `std::unique_ptr<T>`.
  template <typename T>
  void MakeUniquePtrSingle() {
    InsertAllocatorDispatch(&g_mock_dispatch);

    constexpr auto kSize = GetAllocSize<T>();
    constexpr auto kAlignment = std::alignment_of_v<T>;

    std::unique_ptr<T> new_ptr = std::make_unique<T>();
    ASSERT_NE(nullptr, new_ptr);
    ASSERT_GE(allocs_intercepted_by_size[kSize], 1u);
    ASSERT_EQ(reinterpret_cast<uintptr_t>(new_ptr.get()) % kAlignment, 0);
    given_alignment_on_alloc_ = allocs_intercepted_by_alignment[kAlignment];

    const auto hash = Hash<T>(new_ptr.get());
    new_ptr.reset();
    ASSERT_GE(frees_intercepted_by_addr[hash], 1u);
    given_size_on_delete_ = frees_intercepted_by_size[kSize];
    given_alignment_on_delete_ = frees_intercepted_by_alignment[kAlignment];

    RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
  }

  // Tests `operator new[]()` and `operator delete[]()` against `T[3]`.
  template <typename T>
  void MakeUniquePtrTriplet() {
    InsertAllocatorDispatch(&g_mock_dispatch);

    constexpr auto kSize = GetAllocSize<T[3]>();
    constexpr auto kAlignment = std::alignment_of_v<T>;

    std::unique_ptr<T[]> new_ptr = std::make_unique<T[]>(3);
    ASSERT_NE(nullptr, new_ptr);
    ASSERT_GE(allocs_intercepted_by_size[kSize], 1u);
    ASSERT_EQ(reinterpret_cast<uintptr_t>(new_ptr.get()) % kAlignment, 0);
    given_alignment_on_alloc_ = allocs_intercepted_by_alignment[kAlignment];

    const auto hash = Hash<T[]>(new_ptr.get());
    new_ptr.reset();
    ASSERT_GE(frees_intercepted_by_addr[hash], 1u);
    given_size_on_delete_ = frees_intercepted_by_size[kSize];
    given_alignment_on_delete_ = frees_intercepted_by_alignment[kAlignment];

    RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
  }

  // Tests `operator new[]()` and `operator delete[]()` against
  // `std::vector<T>`. The allocation is made through `std::allocator<T>`.
  template <typename T>
  void MakeVectorTriplet() {
    InsertAllocatorDispatch(&g_mock_dispatch);

    constexpr auto kSize = sizeof(T) * 3;
    constexpr auto kAlignment = std::alignment_of_v<T>;

    std::vector<T> vec(3);
    ASSERT_NE(nullptr, vec.data());
    ASSERT_GE(allocs_intercepted_by_size[kSize], 1u);
    ASSERT_EQ(reinterpret_cast<uintptr_t>(vec.data()) % kAlignment, 0);
    given_alignment_on_alloc_ = allocs_intercepted_by_alignment[kAlignment];

    const auto hash = Hash<T>(vec.data());
    vec.clear();
    vec.shrink_to_fit();
    ASSERT_GE(frees_intercepted_by_addr[hash], 1u);
    given_size_on_delete_ = frees_intercepted_by_size[kSize];
    given_alignment_on_delete_ = frees_intercepted_by_alignment[kAlignment];

    RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
  }

  bool given_alignment_on_alloc_;
  bool given_size_on_delete_;
  bool given_alignment_on_delete_;
};

// `ASSERT_TRUE` when sized allocation is in use. Otherwise, `ASSERT_FALSE`.
// On Apple component-builds, all deallocations are routed to `try_free_default`
// and size information will be missing.
#if PA_BUILDFLAG(SHIM_SUPPORTS_SIZED_DEALLOC) && \
    (!PA_BUILDFLAG(IS_APPLE) || !defined(COMPONENT_BUILD))
#define ASSERT_TRUE_IFF_SIZED(a) ASSERT_TRUE(a)
#else
#define ASSERT_TRUE_IFF_SIZED(a) ASSERT_FALSE(a)
#endif  // PA_BUILDFLAG(SHIM_SUPPORTS_SIZED_DEALLOC) && (!PA_BUILDFLAG(IS_APPLE)
        // || !defined(COMPONENT_BUILD))

TEST_F(AllocatorShimCppOperatorTest, NewAndDeleteGlobalOperator) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  constexpr auto kSize = 10;

  void* new_ptr = ::operator new(kSize);
  ASSERT_NE(nullptr, new_ptr);
  ASSERT_TRUE(allocs_intercepted_by_size[kSize]);

  ::operator delete(new_ptr);
  ASSERT_TRUE(frees_intercepted_by_addr[AllocatorShimTest::Hash(new_ptr)]);
  ASSERT_FALSE(frees_intercepted_by_size[kSize]);

  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}

TEST_F(AllocatorShimCppOperatorTest, NewAndDeleteGlobalOperatorNoThrow) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  constexpr auto kSize = 10;

  void* new_ptr = ::operator new(kSize, std::nothrow);
  ASSERT_NE(nullptr, new_ptr);
  ASSERT_TRUE(allocs_intercepted_by_size[kSize]);

  ::operator delete(new_ptr, std::nothrow);
  ASSERT_TRUE(frees_intercepted_by_addr[AllocatorShimTest::Hash(new_ptr)]);
  ASSERT_FALSE(frees_intercepted_by_size[kSize]);

  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}

TEST_F(AllocatorShimCppOperatorTest, NewAndDeleteGlobalOperatorAligned) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  constexpr auto kSize = 10;
  constexpr auto kAlignment = 32;

  void* new_ptr = ::operator new(kSize, std::align_val_t(kAlignment));
  ASSERT_NE(nullptr, new_ptr);
  ASSERT_TRUE(allocs_intercepted_by_size[GetAllocSize(kSize, kAlignment)]);
  ASSERT_TRUE(allocs_intercepted_by_alignment[kAlignment]);

  ::operator delete(new_ptr, std::align_val_t(kAlignment));
  ASSERT_TRUE(frees_intercepted_by_addr[AllocatorShimTest::Hash(new_ptr)]);
  ASSERT_FALSE(frees_intercepted_by_size[GetAllocSize(kSize, kAlignment)]);
  ASSERT_TRUE_IFF_SIZED(frees_intercepted_by_alignment[kAlignment]);

  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}

TEST_F(AllocatorShimCppOperatorTest, NewAndDeleteGlobalOperatorAlignedNoThrow) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  constexpr auto kSize = 10;
  constexpr auto kAlignment = 32;

  void* new_ptr =
      ::operator new(kSize, std::align_val_t(kAlignment), std::nothrow);
  ASSERT_NE(nullptr, new_ptr);
  ASSERT_TRUE(allocs_intercepted_by_size[GetAllocSize(kSize, kAlignment)]);
  ASSERT_TRUE(allocs_intercepted_by_alignment[kAlignment]);

  ::operator delete(new_ptr, std::align_val_t(kAlignment), std::nothrow);
  ASSERT_TRUE(frees_intercepted_by_addr[AllocatorShimTest::Hash(new_ptr)]);
  ASSERT_FALSE(frees_intercepted_by_size[GetAllocSize(kSize, kAlignment)]);
  ASSERT_TRUE_IFF_SIZED(frees_intercepted_by_alignment[kAlignment]);

  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}

// The operator exists only if `-fsized-decallcation` is in use.
#if PA_BUILDFLAG(SHIM_SUPPORTS_SIZED_DEALLOC)
TEST_F(AllocatorShimCppOperatorTest, NewAndDeleteGlobalOperatorSized) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  constexpr auto kSize = 10;

  void* new_ptr = ::operator new(kSize);
  ASSERT_NE(nullptr, new_ptr);
  ASSERT_TRUE(allocs_intercepted_by_size[kSize]);

  ::operator delete(new_ptr, kSize);
  ASSERT_TRUE(frees_intercepted_by_addr[AllocatorShimTest::Hash(new_ptr)]);
  ASSERT_TRUE_IFF_SIZED(frees_intercepted_by_size[kSize]);

  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}

TEST_F(AllocatorShimCppOperatorTest,
       NewAndDeleteGlobalOperatorSizedAndAligned) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  constexpr auto kSize = 10;
  constexpr auto kAlignment = 32;

  void* new_ptr = ::operator new(kSize, std::align_val_t(kAlignment));
  ASSERT_NE(nullptr, new_ptr);
  ASSERT_TRUE(allocs_intercepted_by_size[GetAllocSize(kSize, kAlignment)]);
  ASSERT_TRUE(allocs_intercepted_by_alignment[kAlignment]);

  ::operator delete(new_ptr, kSize, std::align_val_t(kAlignment));
  ASSERT_TRUE(frees_intercepted_by_addr[AllocatorShimTest::Hash(new_ptr)]);
  ASSERT_TRUE_IFF_SIZED(
      frees_intercepted_by_size[GetAllocSize(kSize, kAlignment)]);
  // On Apple component build `try_free_default` is used and alignment
  // information is missing.
#if !PA_BUILDFLAG(IS_APPLE) || !defined(COMPONENT_BUILD)
  ASSERT_TRUE(frees_intercepted_by_alignment[kAlignment]);
#endif  // !PA_BUILDFLAG(IS_APPLE) || !defined(COMPONENT_BUILD)

  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}
#endif  // PA_BUILDFLAG(SHIM_SUPPORTS_SIZED_DEALLOC)

TEST_F(AllocatorShimCppOperatorTest, NewAndDeleteArrayGlobalOperator) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  constexpr auto kSize = 10;

  void* new_ptr = ::operator new[](kSize);
  ASSERT_NE(nullptr, new_ptr);
  ASSERT_TRUE(allocs_intercepted_by_size[kSize]);

  ::operator delete[](new_ptr);
  ASSERT_TRUE(frees_intercepted_by_addr[AllocatorShimTest::Hash(new_ptr)]);
  ASSERT_FALSE(frees_intercepted_by_size[kSize]);

  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}

TEST_F(AllocatorShimCppOperatorTest, NewAndDeleteArrayGlobalOperatorNoThrow) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  constexpr auto kSize = 10;

  void* new_ptr = ::operator new[](kSize, std::nothrow);
  ASSERT_NE(nullptr, new_ptr);
  ASSERT_TRUE(allocs_intercepted_by_size[kSize]);

  ::operator delete[](new_ptr, std::nothrow);
  ASSERT_TRUE(frees_intercepted_by_addr[AllocatorShimTest::Hash(new_ptr)]);
  ASSERT_FALSE(frees_intercepted_by_size[kSize]);

  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}

TEST_F(AllocatorShimCppOperatorTest, NewAndDeleteArrayGlobalOperatorAligned) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  constexpr auto kSize = 10;
  constexpr auto kAlignment = 32;

  void* new_ptr = ::operator new[](kSize, std::align_val_t(kAlignment));
  ASSERT_NE(nullptr, new_ptr);
  ASSERT_TRUE(allocs_intercepted_by_size[GetAllocSize(kSize, kAlignment)]);
  ASSERT_TRUE(allocs_intercepted_by_alignment[kAlignment]);

  ::operator delete[](new_ptr, std::align_val_t(kAlignment));
  ASSERT_TRUE(frees_intercepted_by_addr[AllocatorShimTest::Hash(new_ptr)]);
  ASSERT_FALSE(frees_intercepted_by_size[GetAllocSize(kSize, kAlignment)]);
  ASSERT_TRUE_IFF_SIZED(frees_intercepted_by_alignment[kAlignment]);

  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}

TEST_F(AllocatorShimCppOperatorTest,
       NewAndDeleteArrayGlobalOperatorAlignedNoThrow) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  constexpr auto kSize = 10;
  constexpr auto kAlignment = 32;

  void* new_ptr =
      ::operator new[](kSize, std::align_val_t(kAlignment), std::nothrow);
  ASSERT_NE(nullptr, new_ptr);
  ASSERT_TRUE(allocs_intercepted_by_size[GetAllocSize(kSize, kAlignment)]);
  ASSERT_TRUE(allocs_intercepted_by_alignment[kAlignment]);

  ::operator delete[](new_ptr, std::align_val_t(kAlignment), std::nothrow);
  ASSERT_TRUE(frees_intercepted_by_addr[AllocatorShimTest::Hash(new_ptr)]);
  ASSERT_FALSE(frees_intercepted_by_size[GetAllocSize(kSize, kAlignment)]);
  ASSERT_TRUE_IFF_SIZED(frees_intercepted_by_alignment[kAlignment]);

  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}

// The operator exists only if `-fsized-decallcation` is in use.
#if PA_BUILDFLAG(SHIM_SUPPORTS_SIZED_DEALLOC)
TEST_F(AllocatorShimCppOperatorTest, NewAndDeleteArrayGlobalOperatorSized) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  constexpr auto kSize = 10;

  void* new_ptr = ::operator new[](kSize);
  ASSERT_NE(nullptr, new_ptr);
  ASSERT_TRUE(allocs_intercepted_by_size[kSize]);

  ::operator delete[](new_ptr, kSize);
  ASSERT_TRUE(frees_intercepted_by_addr[AllocatorShimTest::Hash(new_ptr)]);
  ASSERT_TRUE_IFF_SIZED(frees_intercepted_by_size[kSize]);

  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}

TEST_F(AllocatorShimCppOperatorTest,
       NewAndDeleteArrayGlobalOperatorSizedAndAligned) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  constexpr auto kSize = 10;
  constexpr auto kAlignment = 32;

  void* new_ptr = ::operator new[](kSize, std::align_val_t(kAlignment));
  ASSERT_NE(nullptr, new_ptr);
  ASSERT_TRUE(allocs_intercepted_by_size[GetAllocSize(kSize, kAlignment)]);
  ASSERT_TRUE(allocs_intercepted_by_alignment[kAlignment]);

  ::operator delete[](new_ptr, kSize, std::align_val_t(kAlignment));
  ASSERT_TRUE(frees_intercepted_by_addr[AllocatorShimTest::Hash(new_ptr)]);
  ASSERT_TRUE_IFF_SIZED(
      frees_intercepted_by_size[GetAllocSize(kSize, kAlignment)]);
  // On Apple component build `try_free_default` is used and alignment
  // information is missing.
#if !PA_BUILDFLAG(IS_APPLE) || !defined(COMPONENT_BUILD)
  ASSERT_TRUE(frees_intercepted_by_alignment[kAlignment]);
#endif  // !PA_BUILDFLAG(IS_APPLE) || !defined(COMPONENT_BUILD)

  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}
#endif

struct BasicStruct {
  uint32_t ignored;
  uint8_t ignored_2;
};
static_assert(std::is_trivially_destructible_v<BasicStruct>);

TEST_F(AllocatorShimCppOperatorTest, NewAndDeleteBasicStruct) {
  NewAndDeleteSingle<BasicStruct, false>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest, NewAndDeleteBasicStructNoThrow) {
  NewAndDeleteSingle<BasicStruct, true>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest, MakeUniquePtrBasicStruct) {
  MakeUniquePtrSingle<BasicStruct>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest, NewAndDeleteBasicStructArray) {
  NewAndDeleteTriplet<BasicStruct, false>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_FALSE(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest, NewAndDeleteBasicStructArrayNoThrow) {
  NewAndDeleteTriplet<BasicStruct, true>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_FALSE(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest, MakeUniquePtrBasicStructArray) {
  MakeUniquePtrTriplet<BasicStruct>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_FALSE(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest, MakeVectorBasicStruct) {
  MakeVectorTriplet<BasicStruct>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}

// Aligned structs can get routed to different operator new/delete, with
// `std::align_val_t` parameters.
struct alignas(32) AlignedStruct {
  char ignored[999];
};
static_assert(std::alignment_of_v<AlignedStruct> == 32);

TEST_F(AllocatorShimCppOperatorTest, NewAndDeleteAlignedStruct) {
  NewAndDeleteSingle<AlignedStruct, false>();
  ASSERT_TRUE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_TRUE_IFF_SIZED(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest, NewAndDeleteAlignedStructNoThrow) {
  NewAndDeleteSingle<AlignedStruct, true>();
  ASSERT_TRUE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_TRUE_IFF_SIZED(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest, MakeUniquePtrAlignedStruct) {
  MakeUniquePtrSingle<AlignedStruct>();
  ASSERT_TRUE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_TRUE_IFF_SIZED(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest, NewAndDeleteAlignedStructArray) {
  NewAndDeleteTriplet<AlignedStruct, false>();
  ASSERT_TRUE(given_alignment_on_alloc_);
  ASSERT_FALSE(given_size_on_delete_);
  ASSERT_TRUE_IFF_SIZED(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest, NewAndDeleteAlignedStructArrayNoThrow) {
  NewAndDeleteTriplet<AlignedStruct, true>();
  ASSERT_TRUE(given_alignment_on_alloc_);
  ASSERT_FALSE(given_size_on_delete_);
  ASSERT_TRUE_IFF_SIZED(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest, MakeUniquePtrAlignedStructArray) {
  MakeUniquePtrTriplet<AlignedStruct>();
  ASSERT_TRUE(given_alignment_on_alloc_);
  ASSERT_FALSE(given_size_on_delete_);
  ASSERT_TRUE_IFF_SIZED(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest, MakeVectorAlignedStruct) {
  MakeVectorTriplet<AlignedStruct>();
  ASSERT_TRUE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_TRUE_IFF_SIZED(given_alignment_on_delete_);
}

// Clang behaves differently on non-trivially destructible types for array
// allocations. More specifically, they allocates an extra space to remember
// length of an array to run destructors of elements.
struct NonTriviallyDestructibleStruct {
  ~NonTriviallyDestructibleStruct() {}  // NOLINT(modernize-use-equals-default)
  uint64_t ignored;
};
static_assert(
    !std::is_trivially_destructible_v<NonTriviallyDestructibleStruct>);

TEST_F(AllocatorShimCppOperatorTest,
       NewAndDeleteNonTriviallyDestructibleStruct) {
  NewAndDeleteSingle<NonTriviallyDestructibleStruct, false>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest,
       NewAndDeleteNonTriviallyDestructibleStructNoThrow) {
  NewAndDeleteSingle<NonTriviallyDestructibleStruct, true>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest,
       MakeUniquePtrNonTriviallyDestructibleStruct) {
  MakeUniquePtrSingle<NonTriviallyDestructibleStruct>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest,
       NewAndDeleteNonTriviallyDestructibleStructArray) {
  NewAndDeleteTriplet<NonTriviallyDestructibleStruct, false>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest,
       NewAndDeleteNonTriviallyDestructibleStructArrayNoThrow) {
  NewAndDeleteTriplet<NonTriviallyDestructibleStruct, true>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest,
       MakeUniquePtrNonTriviallyDestructibleStructArray) {
  MakeUniquePtrTriplet<NonTriviallyDestructibleStruct>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest, MakeVectorNonTriviallyDestructibleStruct) {
  MakeVectorTriplet<NonTriviallyDestructibleStruct>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}

// Padding size is larger on aligned struct.
struct alignas(128) NonTriviallyDestructibleAlignedStruct {
  // NOLINTNEXTLINE(modernize-use-equals-default)
  ~NonTriviallyDestructibleAlignedStruct() {}
  char ignored;
};
static_assert(std::alignment_of_v<NonTriviallyDestructibleAlignedStruct> ==
              128);
static_assert(
    !std::is_trivially_destructible_v<NonTriviallyDestructibleStruct>);

TEST_F(AllocatorShimCppOperatorTest,
       NewAndDeleteNonTriviallyDestructibleAlignedStruct) {
  NewAndDeleteSingle<NonTriviallyDestructibleAlignedStruct, false>();
  ASSERT_TRUE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_TRUE_IFF_SIZED(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest,
       NewAndDeleteNonTriviallyDestructibleAlignedStructNoThrow) {
  NewAndDeleteSingle<NonTriviallyDestructibleAlignedStruct, true>();
  ASSERT_TRUE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_TRUE_IFF_SIZED(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest,
       MakeUniquePtrNonTriviallyDestructibleAlignedStruct) {
  MakeUniquePtrSingle<NonTriviallyDestructibleAlignedStruct>();
  ASSERT_TRUE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_TRUE_IFF_SIZED(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest,
       NewAndDeleteNonTriviallyDestructibleAlignedStructArray) {
  NewAndDeleteTriplet<NonTriviallyDestructibleAlignedStruct, false>();
  ASSERT_TRUE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_TRUE_IFF_SIZED(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest,
       NewAndDeleteNonTriviallyDestructibleAlignedStructArrayNoThrow) {
  NewAndDeleteTriplet<NonTriviallyDestructibleAlignedStruct, true>();
  ASSERT_TRUE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_TRUE_IFF_SIZED(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest,
       MakeUniquePtrNonTriviallyDestructibleAlignedStructArray) {
  MakeUniquePtrTriplet<NonTriviallyDestructibleAlignedStruct>();
  ASSERT_TRUE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_TRUE_IFF_SIZED(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest,
       MakeVectorNonTriviallyDestructibleAlignedStruct) {
  MakeVectorTriplet<NonTriviallyDestructibleAlignedStruct>();
  ASSERT_TRUE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_TRUE_IFF_SIZED(given_alignment_on_delete_);
}

// A class with a virtual destructor can be deleted through
// deleting-destructor.
struct PolymorphicStruct {
  virtual ~PolymorphicStruct() {}  // NOLINT(modernize-use-equals-default)
  uint64_t ignored;
};

TEST_F(AllocatorShimCppOperatorTest, NewAndDeletePolymorphicStruct) {
  NewAndDeleteSingle<PolymorphicStruct, false>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest, NewAndDeletePolymorphicStructNoThrow) {
  NewAndDeleteSingle<PolymorphicStruct, true>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest, MakeUniquePtrPolymorphicStruct) {
  MakeUniquePtrSingle<PolymorphicStruct>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest, NewAndDeletePolymorphicStructArray) {
  NewAndDeleteTriplet<PolymorphicStruct, false>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest,
       NewAndDeletePolymorphicStructArrayNoThrow) {
  NewAndDeleteTriplet<PolymorphicStruct, true>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest, MakeUniquePtrPolymorphicStructArray) {
  MakeUniquePtrTriplet<PolymorphicStruct>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}
TEST_F(AllocatorShimCppOperatorTest, MakeVectorPolymorphicStruct) {
  MakeVectorTriplet<PolymorphicStruct>();
  ASSERT_FALSE(given_alignment_on_alloc_);
  ASSERT_TRUE_IFF_SIZED(given_size_on_delete_);
  ASSERT_FALSE(given_alignment_on_delete_);
}

#if PA_BUILDFLAG( \
    ENABLE_ALLOCATOR_SHIM_PARTITION_ALLOC_DISPATCH_WITH_ADVANCED_CHECKS_SUPPORT)

void* MockAllocWithAdvancedChecks(size_t, void*);

void* MockAllocUncheckedWithAdvancedChecks(size_t, void*);

void* MockAllocZeroInitializedWithAdvancedChecks(size_t n, size_t, void*);

void* MockAllocAlignedWithAdvancedChecks(size_t, size_t, void*);

void* MockReallocWithAdvancedChecks(void*, size_t, void*);

void* MockReallocUncheckedWithAdvancedChecks(void*, size_t, void*);

void MockFreeWithAdvancedChecks(void*, void*);

void MockFreeWithSizeWithAdvancedChecks(void*, size_t, void*);

void MockFreeWithAlignmentWithAdvancedChecks(void*, size_t, void*);

void MockFreeWithSizeAndAlignmentWithAdvancedChecks(void*,
                                                    size_t,
                                                    size_t,
                                                    void*);

size_t MockGetSizeEstimateWithAdvancedChecks(void*, void*);

size_t MockGoodSizeWithAdvancedChecks(size_t, void*);

bool MockClaimedAddressWithAdvancedChecks(void*, void*);

unsigned MockBatchMallocWithAdvancedChecks(size_t, void**, unsigned, void*);

void MockBatchFreeWithAdvancedChecks(void**, unsigned, void*);

void MockTryFreeDefaultWithAdvancedChecks(void*, void*);

void* MockAlignedMallocWithAdvancedChecks(size_t, size_t, void*);

void* MockAlignedMallocUncheckedWithAdvancedChecks(size_t, size_t, void*);

void* MockAlignedReallocWithAdvancedChecks(void*, size_t, size_t, void*);

void* MockAlignedReallocUncheckedWithAdvancedChecks(void*,
                                                    size_t,
                                                    size_t,
                                                    void*);

void MockAlignedFreeWithAdvancedChecks(void*, void*);

std::atomic_size_t g_mock_free_with_advanced_checks_count;

AllocatorDispatch g_mock_dispatch_for_advanced_checks = {
    .alloc_function = &MockAllocWithAdvancedChecks,
    .alloc_unchecked_function = &MockAllocUncheckedWithAdvancedChecks,
    .alloc_zero_initialized_function =
        &MockAllocZeroInitializedWithAdvancedChecks,
    .alloc_aligned_function = &MockAllocAlignedWithAdvancedChecks,
    .realloc_function = &MockReallocWithAdvancedChecks,
    .realloc_unchecked_function = &MockReallocUncheckedWithAdvancedChecks,
    .free_function = &MockFreeWithAdvancedChecks,
    .free_with_size_function = &MockFreeWithSizeWithAdvancedChecks,
    .free_with_alignment_function = &MockFreeWithAlignmentWithAdvancedChecks,
    .free_with_size_and_alignment_function =
        &MockFreeWithSizeAndAlignmentWithAdvancedChecks,
    .get_size_estimate_function = &MockGetSizeEstimateWithAdvancedChecks,
    .good_size_function = &MockGoodSizeWithAdvancedChecks,
    .claimed_address_function = &MockClaimedAddressWithAdvancedChecks,
    .batch_malloc_function = &MockBatchMallocWithAdvancedChecks,
    .batch_free_function = &MockBatchFreeWithAdvancedChecks,
    .try_free_default_function = &MockTryFreeDefaultWithAdvancedChecks,
    .aligned_malloc_function = &MockAlignedMallocWithAdvancedChecks,
    .aligned_malloc_unchecked_function =
        &MockAlignedMallocUncheckedWithAdvancedChecks,
    .aligned_realloc_function = &MockAlignedReallocWithAdvancedChecks,
    .aligned_realloc_unchecked_function =
        &MockAlignedReallocUncheckedWithAdvancedChecks,
    .aligned_free_function = &MockAlignedFreeWithAdvancedChecks,
    .next = nullptr,
};

void* MockAllocWithAdvancedChecks(size_t size, void* context) {
  // no-op.
  return g_mock_dispatch_for_advanced_checks.next->alloc_function(size,
                                                                  context);
}

void* MockAllocUncheckedWithAdvancedChecks(size_t size, void* context) {
  // no-op.
  return g_mock_dispatch_for_advanced_checks.next->alloc_unchecked_function(
      size, context);
}

void* MockAllocZeroInitializedWithAdvancedChecks(size_t n,
                                                 size_t size,
                                                 void* context) {
  // no-op.
  return g_mock_dispatch_for_advanced_checks.next
      ->alloc_zero_initialized_function(n, size, context);
}

void* MockAllocAlignedWithAdvancedChecks(size_t alignment,
                                         size_t size,
                                         void* context) {
  // no-op.
  return g_mock_dispatch_for_advanced_checks.next->alloc_aligned_function(
      alignment, size, context);
}

void* MockReallocWithAdvancedChecks(void* address, size_t size, void* context) {
  // no-op.
  return g_mock_dispatch_for_advanced_checks.next->realloc_function(
      address, size, context);
}

void* MockReallocUncheckedWithAdvancedChecks(void* address,
                                             size_t size,
                                             void* context) {
  // no-op.
  return g_mock_dispatch_for_advanced_checks.next->realloc_unchecked_function(
      address, size, context);
}

void MockFreeWithAdvancedChecks(void* address, void* context) {
  g_mock_free_with_advanced_checks_count++;
  g_mock_dispatch_for_advanced_checks.next->free_function(address, context);
}

void MockFreeWithSizeWithAdvancedChecks(void* address,
                                        size_t size,
                                        void* context) {
  g_mock_free_with_advanced_checks_count++;
  g_mock_dispatch_for_advanced_checks.next->free_with_size_function(
      address, size, context);
}

void MockFreeWithAlignmentWithAdvancedChecks(void* address,
                                             size_t alignment,
                                             void* context) {
  g_mock_free_with_advanced_checks_count++;
  g_mock_dispatch_for_advanced_checks.next->free_with_alignment_function(
      address, alignment, context);
}

void MockFreeWithSizeAndAlignmentWithAdvancedChecks(void* address,
                                                    size_t size,
                                                    size_t alignment,
                                                    void* context) {
  g_mock_free_with_advanced_checks_count++;
  g_mock_dispatch_for_advanced_checks.next
      ->free_with_size_and_alignment_function(address, size, alignment,
                                              context);
}

size_t MockGetSizeEstimateWithAdvancedChecks(void* address, void* context) {
  // no-op.
  return g_mock_dispatch_for_advanced_checks.next->get_size_estimate_function(
      address, context);
}

size_t MockGoodSizeWithAdvancedChecks(size_t size, void* context) {
  // no-op.
  return g_mock_dispatch_for_advanced_checks.next->good_size_function(size,
                                                                      context);
}

bool MockClaimedAddressWithAdvancedChecks(void* address, void* context) {
  // no-op.
  return g_mock_dispatch_for_advanced_checks.next->claimed_address_function(
      address, context);
}

unsigned MockBatchMallocWithAdvancedChecks(size_t size,
                                           void** results,
                                           unsigned num_requested,
                                           void* context) {
  // no-op.
  return g_mock_dispatch_for_advanced_checks.next->batch_malloc_function(
      size, results, num_requested, context);
}

void MockBatchFreeWithAdvancedChecks(void** to_be_freed,
                                     unsigned num_to_be_freed,
                                     void* context) {
  // no-op.
  g_mock_dispatch_for_advanced_checks.next->batch_free_function(
      to_be_freed, num_to_be_freed, context);
}

void MockTryFreeDefaultWithAdvancedChecks(void* address, void* context) {
  // no-op.
  g_mock_dispatch_for_advanced_checks.next->try_free_default_function(address,
                                                                      context);
}

void* MockAlignedMallocWithAdvancedChecks(size_t size,
                                          size_t alignment,
                                          void* context) {
  // no-op.
  return g_mock_dispatch_for_advanced_checks.next->aligned_malloc_function(
      size, alignment, context);
}

void* MockAlignedMallocUncheckedWithAdvancedChecks(size_t size,
                                                   size_t alignment,
                                                   void* context) {
  // no-op.
  return g_mock_dispatch_for_advanced_checks.next
      ->aligned_malloc_unchecked_function(size, alignment, context);
}

void* MockAlignedReallocWithAdvancedChecks(void* address,
                                           size_t size,
                                           size_t alignment,
                                           void* context) {
  // no-op.
  return g_mock_dispatch_for_advanced_checks.next->aligned_realloc_function(
      address, size, alignment, context);
}

void* MockAlignedReallocUncheckedWithAdvancedChecks(void* address,
                                                    size_t size,
                                                    size_t alignment,
                                                    void* context) {
  // no-op.
  return g_mock_dispatch_for_advanced_checks.next
      ->aligned_realloc_unchecked_function(address, size, alignment, context);
}

void MockAlignedFreeWithAdvancedChecks(void* address, void* context) {
  // no-op.
  g_mock_dispatch_for_advanced_checks.next->aligned_free_function(address,
                                                                  context);
}

TEST_F(AllocatorShimTest, InstallDispatchToPartitionAllocWithAdvancedChecks) {
  // To prevent flakiness introduced by sampling-based dispatch inserted,
  // replace the chain head within this test.
  AutoResetAllocatorDispatchChainForTesting chain_reset;

  g_mock_free_with_advanced_checks_count = 0u;

  // Insert a normal dispatch.
  InsertAllocatorDispatch(&g_mock_dispatch);

  // Using `new` and `delete` instead of `malloc()` and `free()`.
  // On `IS_APPLE` platforms, `free()` may be deferred and not reliably
  // testable.
  int* alloc_ptr = new int;
  delete alloc_ptr;

  // `free()` -> `g_mock_dispatch` -> default allocator.
  EXPECT_GE(frees_intercepted_by_addr[Hash(alloc_ptr)], 1u);
  EXPECT_EQ(g_mock_free_with_advanced_checks_count, 0u);

  InstallCustomDispatchForTesting(&g_mock_dispatch_for_advanced_checks);

  alloc_ptr = new int;
  delete alloc_ptr;

  // `free()` -> `g_mock_dispatch` -> `dispatch` -> default allocator.
  EXPECT_GE(frees_intercepted_by_addr[Hash(alloc_ptr)], 1u);
  EXPECT_GE(g_mock_free_with_advanced_checks_count, 1u);

  UninstallCustomDispatch();
  g_mock_free_with_advanced_checks_count = 0u;

  alloc_ptr = new int;
  delete alloc_ptr;

  // `free()` -> `g_mock_dispatch` -> default allocator.
  EXPECT_GE(frees_intercepted_by_addr[Hash(alloc_ptr)], 1u);
  EXPECT_EQ(g_mock_free_with_advanced_checks_count, 0u);

  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}
#endif

}  // namespace
}  // namespace allocator_shim
