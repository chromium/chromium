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
#include <vector>

#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc.h"
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
  AllocatorShimTest() : testing::Test() {}

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

  static void* MockAllocAligned(size_t alignment, size_t size, void* context) {
    if (instance_) {
      if (size < MaxSizeTracked()) {
        ++(instance_->aligned_allocs_intercepted_by_size[size]);
      }
      if (alignment < MaxSizeTracked()) {
        ++(instance_->aligned_allocs_intercepted_by_alignment[alignment]);
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
        ++instance_->batch_frees_intercepted_by_addr[Hash(to_be_freed[i])];
      }
    }
    g_mock_dispatch.next->batch_free_function(to_be_freed, num_to_be_freed,
                                              context);
  }

  static void MockFreeDefiniteSize(void* ptr, size_t size, void* context) {
    if (instance_) {
      ++instance_->frees_intercepted_by_addr[Hash(ptr)];
      ++instance_->free_definite_sizes_intercepted_by_size[size];
    }
    g_mock_dispatch.next->free_definite_size_function(ptr, size, context);
  }

  static void MockTryFreeDefault(void* ptr, void* context) {
    if (instance_) {
      ++instance_->frees_intercepted_by_addr[Hash(ptr)];
    }
    g_mock_dispatch.next->try_free_default_function(ptr, context);
  }

  static void* MockAlignedMalloc(size_t size, size_t alignment, void* context) {
    if (instance_ && size < MaxSizeTracked()) {
      ++instance_->aligned_mallocs_intercepted_by_size[size];
    }
    return g_mock_dispatch.next->aligned_malloc_function(size, alignment,
                                                         context);
  }

  static void* MockAlignedMallocUnchecked(size_t size,
                                          size_t alignment,
                                          void* context) {
    if (instance_ && size < MaxSizeTracked()) {
      ++instance_->aligned_mallocs_intercepted_by_size[size];
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
      ++instance_->aligned_frees_intercepted_by_addr[Hash(address)];
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
    zero_allocs_intercepted_by_size.resize(MaxSizeTracked());
    aligned_allocs_intercepted_by_size.resize(MaxSizeTracked());
    aligned_allocs_intercepted_by_alignment.resize(MaxSizeTracked());
    reallocs_intercepted_by_size.resize(MaxSizeTracked());
    reallocs_intercepted_by_addr.resize(MaxSizeTracked());
    frees_intercepted_by_addr.resize(MaxSizeTracked());
    batch_mallocs_intercepted_by_size.resize(MaxSizeTracked());
    batch_frees_intercepted_by_addr.resize(MaxSizeTracked());
    free_definite_sizes_intercepted_by_size.resize(MaxSizeTracked());
    aligned_mallocs_intercepted_by_size.resize(MaxSizeTracked());
    aligned_reallocs_intercepted_by_size.resize(MaxSizeTracked());
    aligned_reallocs_intercepted_by_addr.resize(MaxSizeTracked());
    aligned_frees_intercepted_by_addr.resize(MaxSizeTracked());
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
  std::vector<size_t> zero_allocs_intercepted_by_size;
  std::vector<size_t> aligned_allocs_intercepted_by_size;
  std::vector<size_t> aligned_allocs_intercepted_by_alignment;
  std::vector<size_t> reallocs_intercepted_by_size;
  std::vector<size_t> reallocs_intercepted_by_addr;
  std::vector<size_t> frees_intercepted_by_addr;
  std::vector<size_t> batch_mallocs_intercepted_by_size;
  std::vector<size_t> batch_frees_intercepted_by_addr;
  std::vector<size_t> free_definite_sizes_intercepted_by_size;
  std::vector<size_t> aligned_mallocs_intercepted_by_size;
  std::vector<size_t> aligned_reallocs_intercepted_by_size;
  std::vector<size_t> aligned_reallocs_intercepted_by_addr;
  std::vector<size_t> aligned_frees_intercepted_by_addr;
  std::atomic<uint32_t> num_new_handler_calls;

 private:
  static AllocatorShimTest* instance_;
};

struct TestStruct1 {
  uint32_t ignored;
  uint8_t ignored_2;
};

struct TestStruct2 {
  uint64_t ignored;
  uint8_t ignored_3;
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
    &AllocatorShimTest::MockAllocAligned,  /* alloc_aligned_function */
    &AllocatorShimTest::MockRealloc,       /* realloc_function */
    &AllocatorShimTest::MockReallocUnchecked, /* realloc_unchecked_function */
    &AllocatorShimTest::MockFree,             /* free_function */
    &AllocatorShimTest::MockGetSizeEstimate,  /* get_size_estimate_function */
    &AllocatorShimTest::MockGoodSize,         /* good_size */
    &AllocatorShimTest::MockClaimedAddress,   /* claimed_address_function */
    &AllocatorShimTest::MockBatchMalloc,      /* batch_malloc_function */
    &AllocatorShimTest::MockBatchFree,        /* batch_free_function */
    &AllocatorShimTest::MockFreeDefiniteSize, /* free_definite_size_function */
    &AllocatorShimTest::MockTryFreeDefault,   /* try_free_default_function */
    &AllocatorShimTest::MockAlignedMalloc,    /* aligned_malloc_function */
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
  ASSERT_GE(aligned_allocs_intercepted_by_alignment[256], 1u);
  ASSERT_GE(aligned_allocs_intercepted_by_size[59], 1u);

  // (p)valloc() are not defined on Android. pvalloc() is a GNU extension,
  // valloc() is not in POSIX.
#if !PA_BUILDFLAG(IS_ANDROID)
  const size_t kPageSize = partition_alloc::internal::base::GetPageSize();
  void* valloc_ptr = valloc(61);
  ASSERT_NE(nullptr, valloc_ptr);
  ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(valloc_ptr) % kPageSize);
  ASSERT_GE(aligned_allocs_intercepted_by_alignment[kPageSize], 1u);
  ASSERT_GE(aligned_allocs_intercepted_by_size[61], 1u);
#endif  // !PA_BUILDFLAG(IS_ANDROID)

#endif  // !PA_BUILDFLAG(IS_WIN)

#if !PA_BUILDFLAG(IS_WIN) && !PA_BUILDFLAG(IS_APPLE)
  void* memalign_ptr = memalign(128, 53);
  ASSERT_NE(nullptr, memalign_ptr);
  ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(memalign_ptr) % 128);
  ASSERT_GE(aligned_allocs_intercepted_by_alignment[128], 1u);
  ASSERT_GE(aligned_allocs_intercepted_by_size[53], 1u);

#if PA_BUILDFLAG(IS_POSIX) && !PA_BUILDFLAG(IS_ANDROID)
  void* pvalloc_ptr = pvalloc(67);
  ASSERT_NE(nullptr, pvalloc_ptr);
  ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(pvalloc_ptr) % kPageSize);
  ASSERT_GE(aligned_allocs_intercepted_by_alignment[kPageSize], 1u);
  // pvalloc rounds the size up to the next page.
  ASSERT_GE(aligned_allocs_intercepted_by_size[kPageSize], 1u);
#endif  // PA_BUILDFLAG(IS_POSIX) && !PA_BUILDFLAG(IS_ANDROID)

#endif  // !PA_BUILDFLAG(IS_WIN) && !PA_BUILDFLAG(IS_APPLE)

// See allocator_shim_override_glibc_weak_symbols.h for why we intercept
// internal libc symbols.
#if PA_BUILDFLAG(PA_LIBC_GLIBC) && PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  void* libc_memalign_ptr = __libc_memalign(512, 56);
  ASSERT_NE(nullptr, memalign_ptr);
  ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(libc_memalign_ptr) % 512);
  ASSERT_GE(aligned_allocs_intercepted_by_alignment[512], 1u);
  ASSERT_GE(aligned_allocs_intercepted_by_size[56], 1u);
#endif

  char* realloc_ptr = static_cast<char*>(malloc(10));
  strcpy(realloc_ptr, "foobar");
  void* old_realloc_ptr = realloc_ptr;
  realloc_ptr = static_cast<char*>(realloc(realloc_ptr, 73));
  ASSERT_GE(reallocs_intercepted_by_size[73], 1u);
  ASSERT_GE(reallocs_intercepted_by_addr[Hash(old_realloc_ptr)], 1u);
  ASSERT_EQ(0, strcmp(realloc_ptr, "foobar"));

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

TEST_F(AllocatorShimTest, InterceptLibcSymbolsFreeDefiniteSize) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  void* alloc_ptr = malloc(19);
  ASSERT_NE(nullptr, alloc_ptr);
  ASSERT_GE(allocs_intercepted_by_size[19], 1u);

  ChromeMallocZone* default_zone =
      reinterpret_cast<ChromeMallocZone*>(malloc_default_zone());
  default_zone->free_definite_size(malloc_default_zone(), alloc_ptr, 19);
  ASSERT_GE(free_definite_sizes_intercepted_by_size[19], 1u);
  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}
#endif  // PA_BUILDFLAG(IS_APPLE) &&
        // !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#if PA_BUILDFLAG(IS_WIN)
TEST_F(AllocatorShimTest, InterceptUcrtAlignedAllocationSymbols) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  constexpr size_t kAlignment = 32;
  void* alloc_ptr = _aligned_malloc(123, kAlignment);
  EXPECT_GE(aligned_mallocs_intercepted_by_size[123], 1u);

  void* new_alloc_ptr = _aligned_realloc(alloc_ptr, 1234, kAlignment);
  EXPECT_GE(aligned_reallocs_intercepted_by_size[1234], 1u);
  EXPECT_GE(aligned_reallocs_intercepted_by_addr[Hash(alloc_ptr)], 1u);

  _aligned_free(new_alloc_ptr);
  EXPECT_GE(aligned_frees_intercepted_by_addr[Hash(new_alloc_ptr)], 1u);

  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}

TEST_F(AllocatorShimTest, AlignedReallocSizeZeroFrees) {
  void* alloc_ptr = _aligned_malloc(123, 16);
  ASSERT_TRUE(alloc_ptr);
  alloc_ptr = _aligned_realloc(alloc_ptr, 0, 16);
  ASSERT_TRUE(!alloc_ptr);
}
#endif  // PA_BUILDFLAG(IS_WIN)

TEST_F(AllocatorShimTest, InterceptCppSymbols) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  TestStruct1* new_ptr = new TestStruct1;
  ASSERT_NE(nullptr, new_ptr);
  ASSERT_GE(allocs_intercepted_by_size[sizeof(TestStruct1)], 1u);

  TestStruct1* new_array_ptr = new TestStruct1[3];
  ASSERT_NE(nullptr, new_array_ptr);
  ASSERT_GE(allocs_intercepted_by_size[sizeof(TestStruct1) * 3], 1u);

  TestStruct2* new_nt_ptr = new (std::nothrow) TestStruct2;
  ASSERT_NE(nullptr, new_nt_ptr);
  ASSERT_GE(allocs_intercepted_by_size[sizeof(TestStruct2)], 1u);

  TestStruct2* new_array_nt_ptr = new TestStruct2[3];
  ASSERT_NE(nullptr, new_array_nt_ptr);
  ASSERT_GE(allocs_intercepted_by_size[sizeof(TestStruct2) * 3], 1u);

  delete new_ptr;
  ASSERT_GE(frees_intercepted_by_addr[Hash(new_ptr)], 1u);

  delete[] new_array_ptr;
  ASSERT_GE(frees_intercepted_by_addr[Hash(new_array_ptr)], 1u);

  delete new_nt_ptr;
  ASSERT_GE(frees_intercepted_by_addr[Hash(new_nt_ptr)], 1u);

  delete[] new_array_nt_ptr;
  ASSERT_GE(frees_intercepted_by_addr[Hash(new_array_nt_ptr)], 1u);

  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}

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
  ptr = strndup("hello, world", 5);
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

#if PA_BUILDFLAG( \
    ENABLE_ALLOCATOR_SHIM_PARTITION_ALLOC_DISPATCH_WITH_ADVANCED_CHECKS_SUPPORT)

void* MockAllocWithAdvancedChecks(size_t, void*);

void* MockAllocUncheckedWithAdvancedChecks(size_t, void*);

void* MockAllocZeroInitializedWithAdvancedChecks(size_t n, size_t, void*);

void* MockAllocAlignedWithAdvancedChecks(size_t, size_t, void*);

void* MockReallocWithAdvancedChecks(void*, size_t, void*);

void* MockReallocUncheckedWithAdvancedChecks(void*, size_t, void*);

void MockFreeWithAdvancedChecks(void*, void*);

size_t MockGetSizeEstimateWithAdvancedChecks(void*, void*);

size_t MockGoodSizeWithAdvancedChecks(size_t, void*);

bool MockClaimedAddressWithAdvancedChecks(void*, void*);

unsigned MockBatchMallocWithAdvancedChecks(size_t, void**, unsigned, void*);

void MockBatchFreeWithAdvancedChecks(void**, unsigned, void*);

void MockFreeDefiniteSizeWithAdvancedChecks(void*, size_t, void*);

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
    .get_size_estimate_function = &MockGetSizeEstimateWithAdvancedChecks,
    .good_size_function = &MockGoodSizeWithAdvancedChecks,
    .claimed_address_function = &MockClaimedAddressWithAdvancedChecks,
    .batch_malloc_function = &MockBatchMallocWithAdvancedChecks,
    .batch_free_function = &MockBatchFreeWithAdvancedChecks,
    .free_definite_size_function = &MockFreeDefiniteSizeWithAdvancedChecks,
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

void MockFreeDefiniteSizeWithAdvancedChecks(void* address,
                                            size_t size,
                                            void* context) {
  g_mock_free_with_advanced_checks_count++;
  g_mock_dispatch_for_advanced_checks.next->free_definite_size_function(
      address, size, context);
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
