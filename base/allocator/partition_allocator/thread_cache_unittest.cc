// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/thread_cache.h"

#include <vector>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "base/test/bind_test_util.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

// Only a single partition can have a thread cache at a time. When
// PartitionAlloc is malloc(), it is already in use.
//
// With *SAN, PartitionAlloc is replaced in partition_alloc.h by ASAN, so we
// cannot test the thread cache.
//
// Finally, the thread cache currently uses `thread_local`, which causes issues
// on Windows 7 (at least). As long as it doesn't use something else on Windows,
// disable the cache (and tests)
#if !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    !defined(MEMORY_TOOL_REPLACES_ALLOCATOR) && defined(OS_LINUX)

namespace base {
namespace internal {

namespace {

class LambdaThreadDelegate : public PlatformThread::Delegate {
 public:
  explicit LambdaThreadDelegate(OnceClosure f) : f_(std::move(f)) {}
  void ThreadMain() override { std::move(f_).Run(); }

 private:
  OnceClosure f_;
};

// Need to be a global object without a destructor, because the cache is a
// global object with a destructor (to handle thread destruction), and the
// PartitionRoot has to outlive it.
//
// Forbid extras, since they make finding out which bucket is used harder.
NoDestructor<ThreadSafePartitionRoot> g_root{true, true};

size_t BucketIndexForSize(size_t size) {
  auto* bucket = g_root->SizeToBucket(size);
  return bucket - g_root->buckets;
}

size_t FillThreadCacheAndReturnIndex(size_t size, size_t count = 1) {
  size_t bucket_index = BucketIndexForSize(size);
  std::vector<void*> allocated_data;

  for (size_t i = 0; i < count; ++i) {
    allocated_data.push_back(g_root->Alloc(size, ""));
  }
  for (void* ptr : allocated_data) {
    g_root->Free(ptr);
  }

  return bucket_index;
}

}  // namespace

class ThreadCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto* tcache = g_root->thread_cache_for_testing();
    if (tcache)
      tcache->Purge();
  }
  void TearDown() override {}
};

TEST_F(ThreadCacheTest, Simple) {
  const size_t kTestSize = 12;
  void* ptr = g_root->Alloc(kTestSize, "");
  ASSERT_TRUE(ptr);

  // There is a cache.
  auto* tcache = g_root->thread_cache_for_testing();
  EXPECT_TRUE(tcache);

  size_t index = BucketIndexForSize(kTestSize);
  EXPECT_EQ(0u, tcache->bucket_count_for_testing(index));

  g_root->Free(ptr);
  // Freeing fills the thread cache.
  EXPECT_EQ(1u, tcache->bucket_count_for_testing(index));

  void* ptr2 = g_root->Alloc(kTestSize, "");
  EXPECT_EQ(ptr, ptr2);
  // Allocated from the thread cache.
  EXPECT_EQ(0u, tcache->bucket_count_for_testing(index));
}

TEST_F(ThreadCacheTest, InexactSizeMatch) {
  const size_t kTestSize = 12;
  void* ptr = g_root->Alloc(kTestSize, "");
  ASSERT_TRUE(ptr);

  // There is a cache.
  auto* tcache = g_root->thread_cache_for_testing();
  EXPECT_TRUE(tcache);

  size_t index = BucketIndexForSize(kTestSize);
  EXPECT_EQ(0u, tcache->bucket_count_for_testing(index));

  g_root->Free(ptr);
  // Freeing fills the thread cache.
  EXPECT_EQ(1u, tcache->bucket_count_for_testing(index));

  void* ptr2 = g_root->Alloc(kTestSize + 1, "");
  EXPECT_EQ(ptr, ptr2);
  // Allocated from the thread cache.
  EXPECT_EQ(0u, tcache->bucket_count_for_testing(index));
}

TEST_F(ThreadCacheTest, MultipleObjectsCachedPerBucket) {
  size_t bucket_index = FillThreadCacheAndReturnIndex(100, 10);
  auto* tcache = g_root->thread_cache_for_testing();
  EXPECT_EQ(10u, tcache->bucket_count_for_testing(bucket_index));
}

TEST_F(ThreadCacheTest, ObjectsCachedCountIsLimited) {
  size_t bucket_index = FillThreadCacheAndReturnIndex(100, 1000);
  auto* tcache = g_root->thread_cache_for_testing();
  EXPECT_LT(tcache->bucket_count_for_testing(bucket_index), 1000u);
}

TEST_F(ThreadCacheTest, Purge) {
  size_t bucket_index = FillThreadCacheAndReturnIndex(100, 10);
  auto* tcache = g_root->thread_cache_for_testing();
  EXPECT_EQ(10u, tcache->bucket_count_for_testing(bucket_index));
  tcache->Purge();
  EXPECT_EQ(0u, tcache->bucket_count_for_testing(bucket_index));
}

TEST_F(ThreadCacheTest, NoCrossPartitionCache) {
  const size_t kTestSize = 12;
  ThreadSafePartitionRoot root{true, false};

  size_t bucket_index = FillThreadCacheAndReturnIndex(kTestSize);
  void* ptr = root.Alloc(kTestSize, "");
  ASSERT_TRUE(ptr);

  auto* tcache = g_root->thread_cache_for_testing();
  EXPECT_EQ(1u, tcache->bucket_count_for_testing(bucket_index));

  ThreadSafePartitionRoot::Free(ptr);
  EXPECT_EQ(1u, tcache->bucket_count_for_testing(bucket_index));
}

#if ENABLE_THREAD_CACHE_STATISTICS  // Required to record hits and misses.
TEST_F(ThreadCacheTest, LargeAllocationsAreNotCached) {
  auto* tcache = g_root->thread_cache_for_testing();
  size_t hits_before = tcache ? tcache->hits_ : 0;

  FillThreadCacheAndReturnIndex(100 * 1024);
  tcache = g_root->thread_cache_for_testing();
  EXPECT_EQ(hits_before, tcache->hits_);
}
#endif

TEST_F(ThreadCacheTest, DirectMappedAllocationsAreNotCached) {
  FillThreadCacheAndReturnIndex(1024 * 1024);
  // The line above would crash due to out of bounds access if this wasn't
  // properly handled.
}

TEST_F(ThreadCacheTest, MultipleThreadCaches) {
  const size_t kTestSize = 100;
  FillThreadCacheAndReturnIndex(kTestSize);
  auto* parent_thread_tcache = g_root->thread_cache_for_testing();
  ASSERT_TRUE(parent_thread_tcache);

  LambdaThreadDelegate delegate{BindLambdaForTesting([&]() {
    EXPECT_FALSE(g_root->thread_cache_for_testing());  // No allocations yet.
    FillThreadCacheAndReturnIndex(kTestSize);
    auto* tcache = g_root->thread_cache_for_testing();
    EXPECT_TRUE(tcache);

    EXPECT_NE(parent_thread_tcache, tcache);
  })};

  PlatformThreadHandle thread_handle;
  PlatformThread::Create(0, &delegate, &thread_handle);
  PlatformThread::Join(thread_handle);
}

TEST_F(ThreadCacheTest, ThreadCacheReclaimedWhenThreadExits) {
  const size_t kTestSize = 100;
  // Make sure that there is always at least one object allocated in the test
  // bucket, so that the PartitionPage is no reclaimed.
  void* tmp = g_root->Alloc(kTestSize, "");
  void* other_thread_ptr;

  LambdaThreadDelegate delegate{BindLambdaForTesting([&]() {
    EXPECT_FALSE(g_root->thread_cache_for_testing());  // No allocations yet.
    other_thread_ptr = g_root->Alloc(kTestSize, "");
    g_root->Free(other_thread_ptr);
    // |other_thread_ptr| is now in the thread cache.
  })};

  PlatformThreadHandle thread_handle;
  PlatformThread::Create(0, &delegate, &thread_handle);
  PlatformThread::Join(thread_handle);

  void* this_thread_ptr = g_root->Alloc(kTestSize, "");
  // |other_thread_ptr| was returned to the central allocator, and is returned
  // |here, as is comes from the freelist.
  EXPECT_EQ(this_thread_ptr, other_thread_ptr);
  g_root->Free(other_thread_ptr);
  g_root->Free(tmp);
}

}  // namespace internal
}  // namespace base

#endif  // !BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR) && defined(OS_LINUX)
