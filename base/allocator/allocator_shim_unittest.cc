// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/allocator_shim.h"

#include <stdlib.h>
#include <string.h>

#include <atomic>
#include <iomanip>
#include <memory>
#include <new>
#include <sstream>
#include <vector>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/memory/page_size.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_local.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <malloc.h>
#include <windows.h>
#elif defined(OS_APPLE)
#include <malloc/malloc.h>
#include "base/allocator/allocator_interception_mac.h"
#include "base/mac/mac_util.h"
#include "third_party/apple_apsl/malloc.h"
#else
#include <malloc.h>
#endif

#if !defined(OS_WIN)
#include <unistd.h>
#endif

#if defined(LIBC_GLIBC)
extern "C" void* __libc_memalign(size_t align, size_t s);
#endif

namespace base {
namespace allocator {
namespace {

using testing::_;
using testing::MockFunction;

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

  static void* MockAlloc(const AllocatorDispatch* self,
                         size_t size,
                         void* context) {
    if (instance_ && size < MaxSizeTracked())
      ++(instance_->allocs_intercepted_by_size[size]);
    return self->next->alloc_function(self->next, size, context);
  }

  static void* MockAllocUnchecked(const AllocatorDispatch* self,
                                  size_t size,
                                  void* context) {
    if (instance_ && size < MaxSizeTracked())
      ++(instance_->allocs_intercepted_by_size[size]);
    return self->next->alloc_unchecked_function(self->next, size, context);
  }

  static void* MockAllocZeroInit(const AllocatorDispatch* self,
                                 size_t n,
                                 size_t size,
                                 void* context) {
    const size_t real_size = n * size;
    if (instance_ && real_size < MaxSizeTracked())
      ++(instance_->zero_allocs_intercepted_by_size[real_size]);
    return self->next->alloc_zero_initialized_function(self->next, n, size,
                                                       context);
  }

  static void* MockAllocAligned(const AllocatorDispatch* self,
                                size_t alignment,
                                size_t size,
                                void* context) {
    if (instance_) {
      if (size < MaxSizeTracked())
        ++(instance_->aligned_allocs_intercepted_by_size[size]);
      if (alignment < MaxSizeTracked())
        ++(instance_->aligned_allocs_intercepted_by_alignment[alignment]);
    }
    return self->next->alloc_aligned_function(self->next, alignment, size,
                                              context);
  }

  static void* MockRealloc(const AllocatorDispatch* self,
                           void* address,
                           size_t size,
                           void* context) {
    if (instance_) {
      // Size 0xFEED a special sentinel for the NewHandlerConcurrency test.
      // Hitting it for the first time will cause a failure, causing the
      // invocation of the std::new_handler.
      if (size == 0xFEED) {
        if (!instance_->did_fail_realloc_0xfeed_once->Get()) {
          instance_->did_fail_realloc_0xfeed_once->Set(true);
          return nullptr;
        }
        return address;
      }

      if (size < MaxSizeTracked())
        ++(instance_->reallocs_intercepted_by_size[size]);
      ++instance_->reallocs_intercepted_by_addr[Hash(address)];
    }
    return self->next->realloc_function(self->next, address, size, context);
  }

  static void MockFree(const AllocatorDispatch* self,
                       void* address,
                       void* context) {
    if (instance_) {
      ++instance_->frees_intercepted_by_addr[Hash(address)];
    }
    self->next->free_function(self->next, address, context);
  }

  static size_t MockGetSizeEstimate(const AllocatorDispatch* self,
                                    void* address,
                                    void* context) {
    // Special testing values for GetSizeEstimate() interception.
    if (address == kTestSizeEstimateAddress)
      return kTestSizeEstimate;
    return self->next->get_size_estimate_function(self->next, address, context);
  }

  static unsigned MockBatchMalloc(const AllocatorDispatch* self,
                                  size_t size,
                                  void** results,
                                  unsigned num_requested,
                                  void* context) {
    if (instance_) {
      instance_->batch_mallocs_intercepted_by_size[size] =
          instance_->batch_mallocs_intercepted_by_size[size] + num_requested;
    }
    return self->next->batch_malloc_function(self->next, size, results,
                                             num_requested, context);
  }

  static void MockBatchFree(const AllocatorDispatch* self,
                            void** to_be_freed,
                            unsigned num_to_be_freed,
                            void* context) {
    if (instance_) {
      for (unsigned i = 0; i < num_to_be_freed; ++i) {
        ++instance_->batch_frees_intercepted_by_addr[Hash(to_be_freed[i])];
      }
    }
    self->next->batch_free_function(self->next, to_be_freed, num_to_be_freed,
                                    context);
  }

  static void MockFreeDefiniteSize(const AllocatorDispatch* self,
                                   void* ptr,
                                   size_t size,
                                   void* context) {
    if (instance_) {
      ++instance_->frees_intercepted_by_addr[Hash(ptr)];
      ++instance_->free_definite_sizes_intercepted_by_size[size];
    }
    self->next->free_definite_size_function(self->next, ptr, size, context);
  }

  static void* MockAlignedMalloc(const AllocatorDispatch* self,
                                 size_t size,
                                 size_t alignment,
                                 void* context) {
    if (instance_ && size < MaxSizeTracked()) {
      ++instance_->aligned_mallocs_intercepted_by_size[size];
    }
    return self->next->aligned_malloc_function(self->next, size, alignment,
                                               context);
  }

  static void* MockAlignedRealloc(const AllocatorDispatch* self,
                                  void* address,
                                  size_t size,
                                  size_t alignment,
                                  void* context) {
    if (instance_) {
      if (size < MaxSizeTracked())
        ++instance_->aligned_reallocs_intercepted_by_size[size];
      ++instance_->aligned_reallocs_intercepted_by_addr[Hash(address)];
    }
    return self->next->aligned_realloc_function(self->next, address, size,
                                                alignment, context);
  }

  static void MockAlignedFree(const AllocatorDispatch* self,
                              void* address,
                              void* context) {
    if (instance_) {
      ++instance_->aligned_frees_intercepted_by_addr[Hash(address)];
    }
    self->next->aligned_free_function(self->next, address, context);
  }

  static void NewHandler() {
    if (!instance_)
      return;
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
    did_fail_realloc_0xfeed_once = std::make_unique<ThreadLocalBoolean>();
    num_new_handler_calls.store(0, std::memory_order_release);
    instance_ = this;

#if defined(OS_APPLE)
    InitializeAllocatorShim();
#endif
  }

  void TearDown() override {
    instance_ = nullptr;
#if defined(OS_APPLE)
    UninterceptMallocZonesForTesting();
#endif
  }

  static size_t MaxSizeTracked() {
#if defined(OS_IOS)
    // TODO(crbug.com/1077271): 64-bit iOS uses a page size that is larger than
    // SystemPageSize(), causing this test to make larger allocations, relative
    // to SystemPageSize().
    return 6 * base::SystemPageSize();
#else
    return 2 * base::SystemPageSize();
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
  std::unique_ptr<ThreadLocalBoolean> did_fail_realloc_0xfeed_once;
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

class ThreadDelegateForNewHandlerTest : public PlatformThread::Delegate {
 public:
  explicit ThreadDelegateForNewHandlerTest(WaitableEvent* event)
      : event_(event) {}

  void ThreadMain() override {
    event_->Wait();
    void* temp = malloc(1);
    void* res = realloc(temp, 0xFEED);
    EXPECT_EQ(temp, res);
  }

 private:
  WaitableEvent* event_;
};

AllocatorShimTest* AllocatorShimTest::instance_ = nullptr;

AllocatorDispatch g_mock_dispatch = {
    &AllocatorShimTest::MockAlloc,          /* alloc_function */
    &AllocatorShimTest::MockAllocUnchecked, /* alloc_unchecked_function */
    &AllocatorShimTest::MockAllocZeroInit, /* alloc_zero_initialized_function */
    &AllocatorShimTest::MockAllocAligned,  /* alloc_aligned_function */
    &AllocatorShimTest::MockRealloc,       /* realloc_function */
    &AllocatorShimTest::MockFree,          /* free_function */
    &AllocatorShimTest::MockGetSizeEstimate,  /* get_size_estimate_function */
    &AllocatorShimTest::MockBatchMalloc,      /* batch_malloc_function */
    &AllocatorShimTest::MockBatchFree,        /* batch_free_function */
    &AllocatorShimTest::MockFreeDefiniteSize, /* free_definite_size_function */
    &AllocatorShimTest::MockAlignedMalloc,    /* aligned_malloc_function */
    &AllocatorShimTest::MockAlignedRealloc,   /* aligned_realloc_function */
    &AllocatorShimTest::MockAlignedFree,      /* aligned_free_function */
    nullptr,                                  /* next */
};

TEST_F(AllocatorShimTest, InterceptLibcSymbols) {
  InsertAllocatorDispatch(&g_mock_dispatch);

  void* alloc_ptr = malloc(19);
  ASSERT_NE(nullptr, alloc_ptr);
  ASSERT_GE(allocs_intercepted_by_size[19], 1u);

  void* zero_alloc_ptr = calloc(2, 23);
  ASSERT_NE(nullptr, zero_alloc_ptr);
  ASSERT_GE(zero_allocs_intercepted_by_size[2 * 23], 1u);

#if !defined(OS_WIN)
  void* posix_memalign_ptr = nullptr;
  int res = posix_memalign(&posix_memalign_ptr, 256, 59);
  ASSERT_EQ(0, res);
  ASSERT_NE(nullptr, posix_memalign_ptr);
  ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(posix_memalign_ptr) % 256);
  ASSERT_GE(aligned_allocs_intercepted_by_alignment[256], 1u);
  ASSERT_GE(aligned_allocs_intercepted_by_size[59], 1u);

  // (p)valloc() are not defined on Android. pvalloc() is a GNU extension,
  // valloc() is not in POSIX.
#if !defined(OS_ANDROID)
  const size_t kPageSize = base::GetPageSize();
  void* valloc_ptr = valloc(61);
  ASSERT_NE(nullptr, valloc_ptr);
  ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(valloc_ptr) % kPageSize);
  ASSERT_GE(aligned_allocs_intercepted_by_alignment[kPageSize], 1u);
  ASSERT_GE(aligned_allocs_intercepted_by_size[61], 1u);
#endif  // !defined(OS_ANDROID)

#endif  // !OS_WIN

#if !defined(OS_WIN) && !defined(OS_APPLE)
  void* memalign_ptr = memalign(128, 53);
  ASSERT_NE(nullptr, memalign_ptr);
  ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(memalign_ptr) % 128);
  ASSERT_GE(aligned_allocs_intercepted_by_alignment[128], 1u);
  ASSERT_GE(aligned_allocs_intercepted_by_size[53], 1u);

#if !defined(OS_ANDROID)
  void* pvalloc_ptr = pvalloc(67);
  ASSERT_NE(nullptr, pvalloc_ptr);
  ASSERT_EQ(0u, reinterpret_cast<uintptr_t>(pvalloc_ptr) % kPageSize);
  ASSERT_GE(aligned_allocs_intercepted_by_alignment[kPageSize], 1u);
  // pvalloc rounds the size up to the next page.
  ASSERT_GE(aligned_allocs_intercepted_by_size[kPageSize], 1u);
#endif  // !defined(OS_ANDROID)

#endif  // !OS_WIN && !OS_APPLE

// See allocator_shim_override_glibc_weak_symbols.h for why we intercept
// internal libc symbols.
#if defined(LIBC_GLIBC) && \
    (BUILDFLAG(USE_TCMALLOC) || BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC))
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

#if !defined(OS_WIN) && !defined(OS_APPLE)
  free(memalign_ptr);
  ASSERT_GE(frees_intercepted_by_addr[Hash(memalign_ptr)], 1u);

#if !defined(OS_ANDROID)
  free(pvalloc_ptr);
  ASSERT_GE(frees_intercepted_by_addr[Hash(pvalloc_ptr)], 1u);
#endif  // !defined(OS_ANDROID)

#endif  // !OS_WIN && !OS_APPLE

#if !defined(OS_WIN)
  free(posix_memalign_ptr);
  ASSERT_GE(frees_intercepted_by_addr[Hash(posix_memalign_ptr)], 1u);

#if !defined(OS_ANDROID)
  free(valloc_ptr);
  ASSERT_GE(frees_intercepted_by_addr[Hash(valloc_ptr)], 1u);
#endif  // !defined(OS_ANDROID)

#endif  // !OS_WIN

#if defined(LIBC_GLIBC) && \
    (BUILDFLAG(USE_TCMALLOC) || BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC))
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
#if defined(OS_APPLE) && !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
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
#endif  // defined(OS_APPLE) && !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#if defined(OS_WIN)
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
  CHECK(alloc_ptr);
  alloc_ptr = _aligned_realloc(alloc_ptr, 0, 16);
  CHECK(!alloc_ptr);
}
#endif  // defined(OS_WIN)

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
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
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
  PlatformThreadHandle threads[kNumThreads];

  // The WaitableEvent here is used to attempt to trigger all the threads at
  // the same time, after they have been initialized.
  WaitableEvent event(WaitableEvent::ResetPolicy::MANUAL,
                      WaitableEvent::InitialState::NOT_SIGNALED);

  ThreadDelegateForNewHandlerTest mock_thread_main(&event);

  for (int i = 0; i < kNumThreads; ++i)
    PlatformThread::Create(0, &mock_thread_main, &threads[i]);

  std::set_new_handler(&AllocatorShimTest::NewHandler);
  SetCallNewHandlerOnMallocFailure(true);  // It's going to fail on realloc().
  InsertAllocatorDispatch(&g_mock_dispatch);
  event.Signal();
  for (int i = 0; i < kNumThreads; ++i)
    PlatformThread::Join(threads[i]);
  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
  ASSERT_EQ(kNumThreads, GetNumberOfNewHandlerCalls());
}

#if defined(OS_WIN)
TEST_F(AllocatorShimTest, ShimReplacesCRTHeapWhenEnabled) {
  ASSERT_EQ(::GetProcessHeap(), reinterpret_cast<HANDLE>(_get_heap_handle()));
}
#endif  // defined(OS_WIN)

#if defined(OS_WIN)
static size_t GetUsableSize(void* ptr) {
  return _msize(ptr);
}
#elif defined(OS_APPLE)
static size_t GetUsableSize(void* ptr) {
  return malloc_size(ptr);
}
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
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

#if defined(OS_ANDROID)
TEST_F(AllocatorShimTest, InterceptCLibraryFunctions) {
  auto total_counts = [](const std::vector<size_t>& counts) {
    size_t total = 0;
    for (const auto count : counts)
      total += count;
    return total;
  };
  size_t counts_before;
  size_t counts_after = total_counts(allocs_intercepted_by_size);
  void* ptr;

  InsertAllocatorDispatch(&g_mock_dispatch);

  // <stdlib.h>
  counts_before = counts_after;
  ptr = realpath(".", nullptr);
  EXPECT_NE(nullptr, ptr);
  free(ptr);
  counts_after = total_counts(allocs_intercepted_by_size);
  EXPECT_GT(counts_after, counts_before);

  // <string.h>
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

  // Calls vasprintf() indirectly, see below.
  counts_before = counts_after;
  std::stringstream stream;
  stream << std::setprecision(1) << std::showpoint << std::fixed << 1.e38;
  EXPECT_GT(stream.str().size(), 30u);
  counts_after = total_counts(allocs_intercepted_by_size);
  EXPECT_GT(counts_after, counts_before);

  RemoveAllocatorDispatchForTesting(&g_mock_dispatch);
}

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// Non-regression test for crbug.com/1166558.
TEST_F(AllocatorShimTest, InterceptVasprintf) {
  // Printing a float which expands to >=30 characters calls vasprintf() in
  // libc, which we should intercept.
  std::stringstream stream;
  stream << std::setprecision(1) << std::showpoint << std::fixed << 1.e38;
  EXPECT_GT(stream.str().size(), 30u);
  // Should not crash.
}
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#endif  // defined(OS_ANDROID)

}  // namespace
}  // namespace allocator
}  // namespace base
