// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <set>
#include <tuple>
#include <vector>

#include "partition_alloc/address_space_randomization.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/dangling_raw_ptr_checks.h"
#include "partition_alloc/freeslot_bitmap.h"
#include "partition_alloc/in_slot_metadata.h"
#include "partition_alloc/lightweight_quarantine.h"
#include "partition_alloc/memory_reclaimer.h"
#include "partition_alloc/page_allocator_constants.h"
#include "partition_alloc/partition_address_space.h"
#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/cpu.h"
#include "partition_alloc/partition_alloc_base/logging.h"
#include "partition_alloc/partition_alloc_base/numerics/checked_math.h"
#include "partition_alloc/partition_alloc_base/rand_util.h"
#include "partition_alloc/partition_alloc_base/system/sys_info.h"
#include "partition_alloc/partition_alloc_base/test/gtest_util.h"
#include "partition_alloc/partition_alloc_base/thread_annotations.h"
#include "partition_alloc/partition_alloc_base/threading/platform_thread_for_testing.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_alloc_for_testing.h"
#include "partition_alloc/partition_alloc_forward.h"
#include "partition_alloc/partition_bucket.h"
#include "partition_alloc/partition_cookie.h"
#include "partition_alloc/partition_freelist_entry.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/partition_stats.h"
#include "partition_alloc/reservation_offset_table.h"
#include "partition_alloc/tagging.h"
#include "partition_alloc/thread_isolation/thread_isolation.h"
#include "partition_alloc/use_death_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(__ARM_FEATURE_MEMORY_TAGGING)
#include <arm_acle.h>
#endif

#if PA_BUILDFLAG(IS_POSIX)
#if PA_BUILDFLAG(IS_LINUX)
// We need PKEY_DISABLE_WRITE in this file; glibc defines it in sys/mman.h but
// it's actually Linux-specific and other Linux libcs define it in linux/mman.h.
// We have to include both to be sure we get the definition.
#include <linux/mman.h>
#endif  // PA_BUILDFLAG(IS_LINUX)
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#endif  // PA_BUILDFLAG(IS_POSIX)

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && PA_BUILDFLAG(IS_MAC)
#include <OpenCL/opencl.h>
#endif

#if PA_BUILDFLAG(IS_MAC)
#include "partition_alloc/partition_alloc_base/mac/mac_util.h"
#endif

#if PA_BUILDFLAG(ENABLE_PKEYS)
#include <sys/syscall.h>
#endif

// Headers for the AmountOfPhysicalMemory() function.
#if PA_BUILDFLAG(IS_FUCHSIA)
#include <zircon/syscalls.h>
#elif PA_BUILDFLAG(IS_WIN)
#include <windows.h>
#elif PA_BUILDFLAG(IS_APPLE)
#include <mach/host_info.h>
#include <mach/mach.h>
#elif PA_BUILDFLAG(IS_POSIX)
#include <unistd.h>
#endif

// In the MTE world, the upper bits of a pointer can be decorated with a tag,
// thus allowing many versions of the same pointer to exist. These macros take
// that into account when comparing.
#define PA_EXPECT_PTR_EQ(ptr1, ptr2) \
  { EXPECT_EQ(UntagPtr(ptr1), UntagPtr(ptr2)); }
#define PA_EXPECT_PTR_NE(ptr1, ptr2) \
  { EXPECT_NE(UntagPtr(ptr1), UntagPtr(ptr2)); }

namespace {

// Best effort to get the amount of physical memory available to the system.
// Returns 0 on failure.
uint64_t AmountOfPhysicalMemory() {
#if PA_BUILDFLAG(IS_FUCHSIA)
  return zx_system_get_physmem();
#elif PA_BUILDFLAG(IS_WIN)
  MEMORYSTATUSEX mem_status = {.dwLength = sizeof(mem_status)};
  if (GlobalMemoryStatusEx(&mem_status)) {
    return mem_status.ullTotalPhys;
  }
  return 0;
#elif PA_BUILDFLAG(IS_APPLE)
  struct host_basic_info host_basic_info;
  mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
  if (host_info(mach_host_self(), HOST_BASIC_INFO,
                reinterpret_cast<host_info_t>(&host_basic_info),
                &count) == KERN_SUCCESS) {
    return host_basic_info.max_mem;
  }
  return 0;
#elif PA_BUILDFLAG(IS_POSIX)
  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGESIZE);
  if (pages >= 0 && page_size >= 0) {
    return static_cast<uint64_t>(pages) * static_cast<uint64_t>(page_size);
  }
  return 0;
#else
  return 0;
#endif
}

bool IsLargeMemoryDevice() {
  // Treat any device with 4GiB or more of physical memory as a "large memory
  // device". We check for slightly less than GiB so that devices with a small
  // amount of memory not accessible to the OS still count as "large".
  //
  // Set to 4GiB, since we have 2GiB Android devices where tests flakily fail
  // (e.g. Nexus 5X, crbug.com/1191195).
  return AmountOfPhysicalMemory() >= 4000ULL * 1024 * 1024;
}

bool SetAddressSpaceLimit() {
#if !PA_BUILDFLAG(PA_ARCH_CPU_64_BITS) || !PA_BUILDFLAG(IS_POSIX)
  // 32 bits => address space is limited already.
  return true;
#elif PA_BUILDFLAG(IS_POSIX) && !PA_BUILDFLAG(IS_APPLE)
  // macOS will accept, but not enforce, |RLIMIT_AS| changes. See
  // https://crbug.com/435269 and rdar://17576114.
  //
  // Note: This number must be not less than 6 GB, because with
  // sanitizer_coverage_flags=edge, it reserves > 5 GB of address space. See
  // https://crbug.com/674665.
  const size_t kAddressSpaceLimit = static_cast<size_t>(6144) * 1024 * 1024;
  struct rlimit limit;
  if (getrlimit(RLIMIT_DATA, &limit) != 0) {
    return false;
  }
  if (limit.rlim_cur == RLIM_INFINITY || limit.rlim_cur > kAddressSpaceLimit) {
    limit.rlim_cur = kAddressSpaceLimit;
    if (setrlimit(RLIMIT_DATA, &limit) != 0) {
      return false;
    }
  }
  return true;
#else
  return false;
#endif
}

bool ClearAddressSpaceLimit() {
#if !PA_BUILDFLAG(PA_ARCH_CPU_64_BITS) || !PA_BUILDFLAG(IS_POSIX)
  return true;
#elif PA_BUILDFLAG(IS_POSIX)
  struct rlimit limit;
  if (getrlimit(RLIMIT_DATA, &limit) != 0) {
    return false;
  }
  limit.rlim_cur = limit.rlim_max;
  if (setrlimit(RLIMIT_DATA, &limit) != 0) {
    return false;
  }
  return true;
#else
  return false;
#endif
}

const size_t kTestSizes[] = {
    1,
    17,
    100,
    partition_alloc::internal::SystemPageSize(),
    partition_alloc::internal::SystemPageSize() + 1,
    partition_alloc::PartitionRoot::GetDirectMapSlotSize(100),
    1 << 20,
    1 << 21,
};
constexpr size_t kTestSizesCount = std::size(kTestSizes);

template <
    partition_alloc::AllocFlags alloc_flags,
    partition_alloc::FreeFlags free_flags = partition_alloc::FreeFlags::kNone>
void AllocateRandomly(partition_alloc::PartitionRoot* root, size_t count) {
  std::vector<void*> allocations(count, nullptr);
  for (size_t i = 0; i < count; ++i) {
    const size_t size =
        kTestSizes[partition_alloc::internal::base::RandGenerator(
            kTestSizesCount)];
    allocations[i] = root->Alloc<alloc_flags>(size);
    EXPECT_NE(nullptr, allocations[i]) << " size: " << size << " i: " << i;
  }

  for (size_t i = 0; i < count; ++i) {
    if (allocations[i]) {
      root->Free(allocations[i]);
    }
  }
}

void HandleOOM(size_t unused_size) {
  PA_LOG(FATAL) << "Out of memory";
}

int g_dangling_raw_ptr_detected_count = 0;
int g_dangling_raw_ptr_released_count = 0;

class CountDanglingRawPtr {
 public:
  CountDanglingRawPtr() {
    g_dangling_raw_ptr_detected_count = 0;
    g_dangling_raw_ptr_released_count = 0;
    old_detected_fn_ = partition_alloc::GetDanglingRawPtrDetectedFn();
    old_released_fn_ = partition_alloc::GetDanglingRawPtrReleasedFn();

    partition_alloc::SetDanglingRawPtrDetectedFn(
        CountDanglingRawPtr::DanglingRawPtrDetected);
    partition_alloc::SetDanglingRawPtrReleasedFn(
        CountDanglingRawPtr::DanglingRawPtrReleased);
  }
  ~CountDanglingRawPtr() {
    partition_alloc::SetDanglingRawPtrDetectedFn(old_detected_fn_);
    partition_alloc::SetDanglingRawPtrReleasedFn(old_released_fn_);
  }

 private:
  static void DanglingRawPtrDetected(uintptr_t) {
    g_dangling_raw_ptr_detected_count++;
  }
  static void DanglingRawPtrReleased(uintptr_t) {
    g_dangling_raw_ptr_released_count++;
  }

  partition_alloc::DanglingRawPtrDetectedFn* old_detected_fn_;
  partition_alloc::DanglingRawPtrReleasedFn* old_released_fn_;
};

}  // namespace

// Note: This test exercises interfaces inside the `partition_alloc`
// namespace, but inspects objects inside `partition_alloc::internal`.
// For ease of reading, the tests are placed into the latter namespace.
namespace partition_alloc::internal {

using BucketDistribution = PartitionRoot::BucketDistribution;
using SlotSpan = SlotSpanMetadata<MetadataKind::kReadOnly>;

const size_t kTestAllocSize = 16;

constexpr size_t kPointerOffset = 0;
#if !PA_BUILDFLAG(USE_PARTITION_COOKIE)
constexpr size_t kExtraAllocSizeWithoutMetadata = 0ull;
#else
constexpr size_t kExtraAllocSizeWithoutMetadata = kCookieSize;
#endif  // !PA_BUILDFLAG(USE_PARTITION_COOKIE)

const char* type_name = nullptr;

void SetDistributionForPartitionRoot(PartitionRoot* root,
                                     BucketDistribution distribution) {
  switch (distribution) {
    case BucketDistribution::kNeutral:
      root->ResetBucketDistributionForTesting();
      break;
    case BucketDistribution::kDenser:
      root->SwitchToDenserBucketDistribution();
      break;
  }
}

struct PartitionAllocTestParam {
  BucketDistribution bucket_distribution;
  bool use_pkey_pool;
};

const std::vector<PartitionAllocTestParam> GetPartitionAllocTestParams() {
  std::vector<PartitionAllocTestParam> params;
  params.emplace_back(
      PartitionAllocTestParam{BucketDistribution::kNeutral, false});
  params.emplace_back(
      PartitionAllocTestParam{BucketDistribution::kDenser, false});
#if PA_BUILDFLAG(ENABLE_PKEYS)
  if (CPUHasPkeySupport()) {
    params.emplace_back(
        PartitionAllocTestParam{BucketDistribution::kNeutral, true});
    params.emplace_back(
        PartitionAllocTestParam{BucketDistribution::kDenser, true});
  }
#endif
  return params;
}

class PartitionAllocTest
    : public testing::TestWithParam<PartitionAllocTestParam> {
 protected:
  class ScopedPageAllocation {
   public:
    ScopedPageAllocation(PartitionAllocator& allocator,
                         base::CheckedNumeric<size_t> npages)
        : allocator_(allocator),
          npages_(npages),
          ptr_(static_cast<char*>(allocator_.root()->Alloc(
              (npages * SystemPageSize() - ExtraAllocSize(allocator_))
                  .ValueOrDie(),
              type_name))) {}

    ~ScopedPageAllocation() { allocator_.root()->Free(ptr_); }

    void TouchAllPages() {
      memset(ptr_, 'A',
             ((npages_ * SystemPageSize()) - ExtraAllocSize(allocator_))
                 .ValueOrDie());
    }

    void* PageAtIndex(size_t index) {
      return ptr_ - kPointerOffset + (SystemPageSize() * index);
    }

   private:
    PartitionAllocator& allocator_;
    const base::CheckedNumeric<size_t> npages_;
    char* ptr_;
  };

  PartitionAllocTest() = default;

  ~PartitionAllocTest() override = default;

  struct PartitionTestOptions {
    bool use_memory_reclaimer = false;
    bool uncap_empty_slot_span_memory = false;
    bool set_bucket_distribution = false;
  };

  void InitializeTestRoot(PartitionRoot* root,
                          PartitionOptions opts,
                          PartitionTestOptions test_opts) {
    root->Init(opts);
    if (test_opts.use_memory_reclaimer) {
      MemoryReclaimer::Instance()->RegisterPartition(root);
    }
    if (test_opts.uncap_empty_slot_span_memory) {
      root->UncapEmptySlotSpanMemoryForTesting();
    }
    if (test_opts.set_bucket_distribution) {
      SetDistributionForPartitionRoot(root, GetBucketDistribution());
    }
  }

  std::unique_ptr<PartitionRoot> CreateCustomTestRoot(
      PartitionOptions opts,
      PartitionTestOptions test_opts) {
    auto root = std::make_unique<PartitionRoot>();
    InitializeTestRoot(root.get(), opts, test_opts);
    return root;
  }

  PartitionOptions GetCommonPartitionOptions() {
    PartitionOptions opts;
    // Requires explicit `FreeFlag` to activate, no effect otherwise.
    opts.zapping_by_free_flags = PartitionOptions::kEnabled;
    opts.scheduler_loop_quarantine = PartitionOptions::kEnabled;
    opts.scheduler_loop_quarantine_branch_capacity_in_bytes =
        std::numeric_limits<size_t>::max();
    return opts;
  }

  void InitializeMainTestAllocators() {
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    PartitionOptions::EnableToggle enable_backup_ref_ptr =
        PartitionOptions::kEnabled;
#endif
#if PA_BUILDFLAG(ENABLE_PKEYS)
    int pkey = PkeyAlloc(UseThreadIsolatedPool() ? 0 : PKEY_DISABLE_WRITE);
    if (pkey != -1) {
      pkey_ = pkey;
    }

    PartitionOptions pkey_opts = GetCommonPartitionOptions();
    pkey_opts.thread_isolation = ThreadIsolationOption(pkey_);
    // We always want to have a pkey allocator initialized to make sure that the
    // other pools still work. As part of the initializition, we tag some memory
    // with the new pkey, effectively making it read-only. So there's some
    // potential for breakage that this should catch.
    InitializeTestRoot(pkey_allocator.root(), pkey_opts,
                       PartitionTestOptions{.use_memory_reclaimer = true});

    ThreadIsolationOption thread_isolation_opt;
    if (UseThreadIsolatedPool() && pkey_ != kInvalidPkey) {
      thread_isolation_opt = ThreadIsolationOption(pkey_);
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
      // BRP and thread isolated mode use different pools, so they can't be
      // enabled at the same time.
      enable_backup_ref_ptr = PartitionOptions::kDisabled;
#endif
    }
#endif  // PA_BUILDFLAG(ENABLE_PKEYS)

    PartitionOptions opts = GetCommonPartitionOptions();
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    opts.backup_ref_ptr = enable_backup_ref_ptr;
#endif
#if PA_BUILDFLAG(ENABLE_PKEYS)
    opts.thread_isolation = thread_isolation_opt;
#endif
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
    opts.memory_tagging = {
        .enabled =
            partition_alloc::internal::base::CPU::GetInstanceNoAllocation()
                    .has_mte()
                ? PartitionOptions::kEnabled
                : PartitionOptions::kDisabled,
    };
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)
    InitializeTestRoot(
        allocator.root(), opts,
        PartitionTestOptions{.use_memory_reclaimer = true,
                             .uncap_empty_slot_span_memory = true,
                             .set_bucket_distribution = true});
  }

  // Actual slot size used for requests of size kTestAllocSize.
  size_t ActualTestAllocSize() const {
    return SizeToBucketSize(kTestAllocSize + ExtraAllocSize(allocator));
  }

  void SetUp() override {
    PartitionRoot::SetStraightenLargerSlotSpanFreeListsMode(
        StraightenLargerSlotSpanFreeListsMode::kOnlyWhenUnprovisioning);
    PartitionRoot::SetSortSmallerSlotSpanFreeListsEnabled(true);
    PartitionRoot::SetSortActiveSlotSpansEnabled(true);
    PartitionAllocGlobalInit(HandleOOM);
    InitializeMainTestAllocators();
#if PA_CONFIG(ENABLE_SHADOW_METADATA)
    PartitionRoot::EnableShadowMetadata(internal::PoolHandleMask::kRegular |
                                        internal::PoolHandleMask::kBRP);
#endif

    test_bucket_index_ = SizeToIndex(ActualTestAllocSize());
  }

  void TearDown() override {
    allocator.root()->PurgeMemory(PurgeFlags::kDecommitEmptySlotSpans |
                                  PurgeFlags::kDiscardUnusedSystemPages);
    PartitionAllocGlobalUninitForTesting();
#if PA_BUILDFLAG(ENABLE_PKEYS)
    if (pkey_ != kInvalidPkey) {
      PkeyFree(pkey_);
    }
#endif
  }

  size_t SizeToIndex(size_t size) const {
    const auto distribution_to_use = GetBucketDistribution();
    return PartitionRoot::SizeToBucketIndex(size, distribution_to_use);
  }

  size_t SizeToBucketSize(size_t size) const {
    const auto index = SizeToIndex(size);
    return allocator.root()->buckets[index].slot_size;
  }

  static size_t ExtraAllocSize(const PartitionAllocator& allocator) {
    size_t metadata_size = 0;
    // Duplicate the logic from PartitionRoot::Init().
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    if (allocator.root()->brp_enabled()) {
      metadata_size = kInSlotMetadataSizeAdjustment;
    }
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    return kExtraAllocSizeWithoutMetadata + metadata_size;
  }

  size_t GetNumPagesPerSlotSpan(size_t size) {
    size_t real_size = size + ExtraAllocSize(allocator);
    size_t bucket_index = SizeToIndex(real_size);
    PartitionRoot::Bucket* bucket = &allocator.root()->buckets[bucket_index];
    // TODO(tasak): make get_pages_per_slot_span() available at
    // partition_alloc_unittest.cc. Is it allowable to make the code from
    // partition_bucet.cc to partition_bucket.h?
    return (bucket->num_system_pages_per_slot_span +
            (NumSystemPagesPerPartitionPage() - 1)) /
           NumSystemPagesPerPartitionPage();
  }

  const SlotSpan* GetFullSlotSpan(size_t size) {
    size_t real_size = size + ExtraAllocSize(allocator);
    size_t bucket_index = SizeToIndex(real_size);
    PartitionRoot::Bucket* bucket = &allocator.root()->buckets[bucket_index];
    size_t num_slots =
        (bucket->num_system_pages_per_slot_span * SystemPageSize()) /
        bucket->slot_size;
    uintptr_t first = 0;
    uintptr_t last = 0;
    size_t i;
    for (i = 0; i < num_slots; ++i) {
      void* ptr = allocator.root()->Alloc(size, type_name);
      EXPECT_TRUE(ptr);
      if (!i) {
        first = allocator.root()->ObjectToSlotStart(ptr);
      } else if (i == num_slots - 1) {
        last = allocator.root()->ObjectToSlotStart(ptr);
      }
    }
    EXPECT_EQ(SlotSpan::FromSlotStart(first), SlotSpan::FromSlotStart(last));
    if (bucket->num_system_pages_per_slot_span ==
        NumSystemPagesPerPartitionPage()) {
      EXPECT_EQ(first & PartitionPageBaseMask(),
                last & PartitionPageBaseMask());
    }
    EXPECT_EQ(num_slots, bucket->active_slot_spans_head->num_allocated_slots);
    EXPECT_EQ(nullptr, bucket->active_slot_spans_head->get_freelist_head());
    EXPECT_TRUE(bucket->is_valid());
    EXPECT_TRUE(bucket->active_slot_spans_head !=
                SlotSpan::get_sentinel_slot_span());
    EXPECT_TRUE(bucket->active_slot_spans_head->is_full());
    return bucket->active_slot_spans_head;
  }

  void ClearEmptySlotSpanCache() {
    allocator.root()->DecommitEmptySlotSpansForTesting();
  }

  enum ReturnNullTestMode {
    kPartitionAlloc,
    kPartitionRealloc,
  };

  void DoReturnNullTest(size_t alloc_size, ReturnNullTestMode mode) {
    // TODO(crbug.com/41293809): Where necessary and possible, disable the
    // platform's OOM-killing behavior. OOM-killing makes this test flaky on
    // low-memory devices.
    if (!IsLargeMemoryDevice()) {
      PA_LOG(WARNING)
          << "Skipping test on this device because of crbug.com/678782";
      PA_LOG(FATAL) << "Passed DoReturnNullTest";
    }

    ASSERT_TRUE(SetAddressSpaceLimit());

    // Work out the number of allocations for 6 GB of memory.
    const int num_allocations = (6 * 1024 * 1024) / (alloc_size / 1024);

    void** ptrs = static_cast<void**>(
        allocator.root()->Alloc(num_allocations * sizeof(void*), type_name));
    int i;

    for (i = 0; i < num_allocations; ++i) {
      switch (mode) {
        case kPartitionAlloc: {
          ptrs[i] = allocator.root()->Alloc<AllocFlags::kReturnNull>(alloc_size,
                                                                     type_name);
          break;
        }
        case kPartitionRealloc: {
          ptrs[i] =
              allocator.root()->Alloc<AllocFlags::kReturnNull>(1, type_name);
          ptrs[i] = allocator.root()->Realloc<AllocFlags::kReturnNull>(
              ptrs[i], alloc_size, type_name);
          break;
        }
      }

      if (!i) {
        EXPECT_TRUE(ptrs[0]);
      }
      if (!ptrs[i]) {
        ptrs[i] = allocator.root()->Alloc<AllocFlags::kReturnNull>(alloc_size,
                                                                   type_name);
        EXPECT_FALSE(ptrs[i]);
        break;
      }
    }

    // We shouldn't succeed in allocating all 6 GB of memory. If we do, then
    // we're not actually testing anything here.
    EXPECT_LT(i, num_allocations);

    // Free, reallocate and free again each block we allocated. We do this to
    // check that freeing memory also works correctly after a failed allocation.
    for (--i; i >= 0; --i) {
      allocator.root()->Free(ptrs[i]);
      ptrs[i] = allocator.root()->Alloc<AllocFlags::kReturnNull>(alloc_size,
                                                                 type_name);
      EXPECT_TRUE(ptrs[i]);
      allocator.root()->Free(ptrs[i]);
    }

    allocator.root()->Free(ptrs);

    EXPECT_TRUE(ClearAddressSpaceLimit());
    PA_LOG(FATAL) << "Passed DoReturnNullTest";
  }

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  void RunRefCountReallocSubtest(size_t orig_size, size_t new_size);
#endif

  PA_NOINLINE PA_MALLOC_FN void* Alloc(size_t size) {
    return allocator.root()->Alloc(size);
  }

  PA_NOINLINE void Free(void* ptr) { allocator.root()->Free(ptr); }

  BucketDistribution GetBucketDistribution() const {
    return GetParam().bucket_distribution;
  }

  bool UseThreadIsolatedPool() const { return GetParam().use_pkey_pool; }
  bool UseBRPPool() const {
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    return allocator.root()->brp_enabled();
#else
    return false;
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  }

  partition_alloc::PartitionAllocatorForTesting allocator;
#if PA_BUILDFLAG(ENABLE_PKEYS)
  partition_alloc::PartitionAllocatorForTesting pkey_allocator;
#endif
  size_t test_bucket_index_;

#if PA_BUILDFLAG(ENABLE_PKEYS)
  int pkey_ = kInvalidPkey;
#endif
};

#if PA_USE_DEATH_TESTS()

class PartitionAllocDeathTest : public PartitionAllocTest {};

INSTANTIATE_TEST_SUITE_P(AlternateTestParams,
                         PartitionAllocDeathTest,
                         testing::ValuesIn(GetPartitionAllocTestParams()));

#endif

namespace {

void FreeFullSlotSpan(PartitionRoot* root, const SlotSpan* slot_span) {
  EXPECT_TRUE(slot_span->is_full());
  size_t size = slot_span->bucket->slot_size;
  size_t num_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      size;
  EXPECT_EQ(num_slots, slot_span->num_allocated_slots);
  uintptr_t address = SlotSpan::ToSlotSpanStart(slot_span);
  size_t i;
  for (i = 0; i < num_slots; ++i) {
    root->Free(root->SlotStartToObject(address));
    address += size;
  }
  EXPECT_TRUE(slot_span->is_empty());
}

#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
bool CheckPageInCore(void* ptr, bool in_core) {
  unsigned char ret = 0;
  EXPECT_EQ(0, mincore(ptr, SystemPageSize(), &ret));
  return in_core == (ret & 1);
}

#define CHECK_PAGE_IN_CORE(ptr, in_core) \
  EXPECT_TRUE(CheckPageInCore(ptr, in_core))
#else
#define CHECK_PAGE_IN_CORE(ptr, in_core) (void)(0)
#endif  // PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)

class MockPartitionStatsDumper : public PartitionStatsDumper {
 public:
  MockPartitionStatsDumper() = default;

  void PartitionDumpTotals(const char* partition_name,
                           const PartitionMemoryStats* stats) override {
    EXPECT_GE(stats->total_mmapped_bytes, stats->total_resident_bytes);
    EXPECT_EQ(total_resident_bytes, stats->total_resident_bytes);
    EXPECT_EQ(total_active_bytes, stats->total_active_bytes);
    EXPECT_EQ(total_decommittable_bytes, stats->total_decommittable_bytes);
    EXPECT_EQ(total_discardable_bytes, stats->total_discardable_bytes);
  }

  void PartitionsDumpBucketStats(
      [[maybe_unused]] const char* partition_name,
      const PartitionBucketMemoryStats* stats) override {
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->bucket_slot_size & sizeof(void*));
    bucket_stats.push_back(*stats);
    total_resident_bytes += stats->resident_bytes;
    total_active_bytes += stats->active_bytes;
    total_decommittable_bytes += stats->decommittable_bytes;
    total_discardable_bytes += stats->discardable_bytes;
  }

  bool IsMemoryAllocationRecorded() {
    return total_resident_bytes != 0 && total_active_bytes != 0;
  }

  const PartitionBucketMemoryStats* GetBucketStats(size_t bucket_size) {
    for (auto& stat : bucket_stats) {
      if (stat.bucket_slot_size == bucket_size) {
        return &stat;
      }
    }
    return nullptr;
  }

 private:
  size_t total_resident_bytes = 0;
  size_t total_active_bytes = 0;
  size_t total_decommittable_bytes = 0;
  size_t total_discardable_bytes = 0;

  std::vector<PartitionBucketMemoryStats> bucket_stats;
};

#if PA_BUILDFLAG(IS_APPLE)
// After deallocating the memory, another thread may allocate memory whose
// address region overlaps the deallocated memory's. This sometimes happen
// when running the test on apple os with PartitionAlloc-Everywhere.
// So if `IsManagedByNormalBuckets(address_to_check)` returns true, we
// will also check whether `allocator.root()` allocated the memory or not.
// Regarding IsManagedByDirectMap(), this rarely happens because of allocation
// size. But we should also check who allocates the memory.
bool IsNormalBucketsAllocatedByRoot(uintptr_t address, PartitionRoot* root) {
  partition_alloc::internal::PartitionSuperPageExtentEntry<
      partition_alloc::internal::MetadataKind::kReadOnly>* extent =
      root->first_extent;
  while (extent != nullptr) {
    uintptr_t super_page =
        partition_alloc::internal::SuperPagesBeginFromExtent(extent);
    uintptr_t super_page_end =
        partition_alloc::internal::SuperPagesEndFromExtent(extent);
    if (super_page <= address && address < super_page_end) {
      return true;
    }
    extent = extent->next;
  }
  return false;
}

bool IsDirectMapAllocatedByRoot(uintptr_t address, PartitionRoot* root) {
  ::partition_alloc::internal::ScopedGuard locker{
      partition_alloc::internal::PartitionRootLock(root)};

  partition_alloc::internal::PartitionDirectMapExtent<
      partition_alloc::internal::MetadataKind::kReadOnly>* extent =
      root->direct_map_list;
  while (extent != nullptr) {
    uintptr_t super_page =
        reinterpret_cast<uintptr_t>(extent) & kSuperPageBaseMask;
    uintptr_t super_page_end = super_page + extent->reservation_size;
    if (super_page <= address && address < super_page_end) {
      return true;
    }
    extent = extent->next_extent;
  }
  return false;
}
#endif  // PA_BUILDFLAG(IS_APPLE)

bool IsManagedByNormalBucketsForTesting(uintptr_t address,
                                        [[maybe_unused]] PartitionRoot* root) {
  return IsManagedByNormalBuckets(address)
#if PA_BUILDFLAG(IS_APPLE)
         && IsNormalBucketsAllocatedByRoot(address, root)
#endif  // PA_BUILDFLAG(IS_APPLE)
      ;
}

bool IsManagedByDirectMapForTesting(uintptr_t address,
                                    [[maybe_unused]] PartitionRoot* root) {
  return IsManagedByDirectMap(address)
#if PA_BUILDFLAG(IS_APPLE)
         && IsDirectMapAllocatedByRoot(address, root)
#endif  // PA_BUILDFLAG(IS_APPLE)
      ;
}

bool IsManagedByNormalBucketsOrDirectMapForTesting(
    uintptr_t address,
    [[maybe_unused]] PartitionRoot* root) {
  return IsManagedByNormalBucketsOrDirectMap(address)
#if PA_BUILDFLAG(IS_APPLE)
         && (IsManagedByNormalBucketsForTesting(address, root) ||
             IsManagedByDirectMapForTesting(address, root))
#endif  // PA_BUILDFLAG(IS_APPLE)
      ;
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(AlternateTestParams,
                         PartitionAllocTest,
                         testing::ValuesIn(GetPartitionAllocTestParams()));

// Check that the most basic of allocate / free pairs work.
TEST_P(PartitionAllocTest, Basic) {
  PartitionRoot::Bucket* bucket =
      &allocator.root()->buckets[test_bucket_index_];
  auto* seed_slot_span = SlotSpan::get_sentinel_slot_span();

  EXPECT_FALSE(bucket->empty_slot_spans_head);
  EXPECT_FALSE(bucket->decommitted_slot_spans_head);
  EXPECT_EQ(seed_slot_span, bucket->active_slot_spans_head);
  EXPECT_EQ(nullptr, bucket->active_slot_spans_head->next_slot_span);

  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  EXPECT_EQ(kPointerOffset, UntagPtr(ptr) & PartitionPageOffsetMask());
  // Check that the offset appears to include a guard page.
  EXPECT_EQ(PartitionPageSize() +
                partition_alloc::internal::ReservedFreeSlotBitmapSize() +
                kPointerOffset,
            UntagPtr(ptr) & kSuperPageOffsetMask);

  allocator.root()->Free(ptr);
  // Expect that the last active slot span gets noticed as empty but doesn't get
  // decommitted.
  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_FALSE(bucket->decommitted_slot_spans_head);
}

// Test multiple allocations, and freelist handling.
TEST_P(PartitionAllocTest, MultiAlloc) {
  void* ptr1 = allocator.root()->Alloc(kTestAllocSize, type_name);
  void* ptr2 = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr1);
  EXPECT_TRUE(ptr2);
  ptrdiff_t diff = UntagPtr(ptr2) - UntagPtr(ptr1);
  EXPECT_EQ(static_cast<ptrdiff_t>(ActualTestAllocSize()), diff);

  // Check that we re-use the just-freed slot.
  allocator.root()->Free(ptr2);
  ptr2 = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr2);
  diff = UntagPtr(ptr2) - UntagPtr(ptr1);
  EXPECT_EQ(static_cast<ptrdiff_t>(ActualTestAllocSize()), diff);
  allocator.root()->Free(ptr1);
  ptr1 = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr1);
  diff = UntagPtr(ptr2) - UntagPtr(ptr1);
  EXPECT_EQ(static_cast<ptrdiff_t>(ActualTestAllocSize()), diff);

  void* ptr3 = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr3);
  diff = UntagPtr(ptr3) - UntagPtr(ptr1);
  EXPECT_EQ(static_cast<ptrdiff_t>(ActualTestAllocSize() * 2), diff);

  allocator.root()->Free(ptr1);
  allocator.root()->Free(ptr2);
  allocator.root()->Free(ptr3);
}

// Test a bucket with multiple slot spans.
TEST_P(PartitionAllocTest, MultiSlotSpans) {
  PartitionRoot::Bucket* bucket =
      &allocator.root()->buckets[test_bucket_index_];

  auto* slot_span = GetFullSlotSpan(kTestAllocSize);
  FreeFullSlotSpan(allocator.root(), slot_span);
  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_EQ(SlotSpan::get_sentinel_slot_span(), bucket->active_slot_spans_head);
  EXPECT_EQ(nullptr, slot_span->next_slot_span);
  EXPECT_EQ(0u, slot_span->num_allocated_slots);

  slot_span = GetFullSlotSpan(kTestAllocSize);
  auto* slot_span2 = GetFullSlotSpan(kTestAllocSize);

  EXPECT_EQ(slot_span2, bucket->active_slot_spans_head);
  EXPECT_EQ(nullptr, slot_span2->next_slot_span);
  EXPECT_EQ(SlotSpan::ToSlotSpanStart(slot_span) & kSuperPageBaseMask,
            SlotSpan::ToSlotSpanStart(slot_span2) & kSuperPageBaseMask);

  // Fully free the non-current slot span. This will leave us with no current
  // active slot span because one is empty and the other is full.
  FreeFullSlotSpan(allocator.root(), slot_span);
  EXPECT_EQ(0u, slot_span->num_allocated_slots);
  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_EQ(SlotSpan::get_sentinel_slot_span(), bucket->active_slot_spans_head);

  // Allocate a new slot span, it should pull from the freelist.
  slot_span = GetFullSlotSpan(kTestAllocSize);
  EXPECT_FALSE(bucket->empty_slot_spans_head);
  EXPECT_EQ(slot_span, bucket->active_slot_spans_head);

  FreeFullSlotSpan(allocator.root(), slot_span);
  FreeFullSlotSpan(allocator.root(), slot_span2);
  EXPECT_EQ(0u, slot_span->num_allocated_slots);
  EXPECT_EQ(0u, slot_span2->num_allocated_slots);
  EXPECT_EQ(0u, slot_span2->num_unprovisioned_slots);
  EXPECT_TRUE(slot_span2->in_empty_cache());
}

// Test some finer aspects of internal slot span transitions.
TEST_P(PartitionAllocTest, SlotSpanTransitions) {
  PartitionRoot::Bucket* bucket =
      &allocator.root()->buckets[test_bucket_index_];

  auto* slot_span1 = GetFullSlotSpan(kTestAllocSize);
  EXPECT_EQ(slot_span1, bucket->active_slot_spans_head);
  EXPECT_EQ(nullptr, slot_span1->next_slot_span);
  auto* slot_span2 = GetFullSlotSpan(kTestAllocSize);
  EXPECT_EQ(slot_span2, bucket->active_slot_spans_head);
  EXPECT_EQ(nullptr, slot_span2->next_slot_span);

  // Bounce slot_span1 back into the non-full list then fill it up again.
  void* ptr = allocator.root()->SlotStartToObject(
      SlotSpan::ToSlotSpanStart(slot_span1));
  allocator.root()->Free(ptr);
  EXPECT_EQ(slot_span1, bucket->active_slot_spans_head);
  std::ignore = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_EQ(slot_span1, bucket->active_slot_spans_head);
  EXPECT_EQ(slot_span2, bucket->active_slot_spans_head->next_slot_span);

  // Allocating another slot span at this point should cause us to scan over
  // slot_span1 (which is both full and NOT our current slot span), and evict it
  // from the freelist. Older code had a O(n^2) condition due to failure to do
  // this.
  auto* slot_span3 = GetFullSlotSpan(kTestAllocSize);
  EXPECT_EQ(slot_span3, bucket->active_slot_spans_head);
  EXPECT_EQ(nullptr, slot_span3->next_slot_span);

  // Work out a pointer into slot_span2 and free it.
  ptr = allocator.root()->SlotStartToObject(
      SlotSpan::ToSlotSpanStart(slot_span2));
  allocator.root()->Free(ptr);
  // Trying to allocate at this time should cause us to cycle around to
  // slot_span2 and find the recently freed slot.
  void* ptr2 = allocator.root()->Alloc(kTestAllocSize, type_name);
  PA_EXPECT_PTR_EQ(ptr, ptr2);
  EXPECT_EQ(slot_span2, bucket->active_slot_spans_head);
  EXPECT_EQ(slot_span3, slot_span2->next_slot_span);

  // Work out a pointer into slot_span1 and free it. This should pull the slot
  // span back into the list of available slot spans.
  ptr = allocator.root()->SlotStartToObject(
      SlotSpan::ToSlotSpanStart(slot_span1));
  allocator.root()->Free(ptr);
  // This allocation should be satisfied by slot_span1.
  ptr2 = allocator.root()->Alloc(kTestAllocSize, type_name);
  PA_EXPECT_PTR_EQ(ptr, ptr2);
  EXPECT_EQ(slot_span1, bucket->active_slot_spans_head);
  EXPECT_EQ(slot_span2, slot_span1->next_slot_span);

  FreeFullSlotSpan(allocator.root(), slot_span3);
  FreeFullSlotSpan(allocator.root(), slot_span2);
  FreeFullSlotSpan(allocator.root(), slot_span1);

  // Allocating whilst in this state exposed a bug, so keep the test.
  ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  allocator.root()->Free(ptr);
}

// Test that ExtraAllocSize() is exactly what PA takes away from the slot for
// extras.
TEST_P(PartitionAllocTest, ExtraAllocSize) {
  // There is a bucket with a slot size exactly that (asserted below).
  size_t slot_size = 64;
  size_t bucket_index =
      allocator.root()->SizeToBucketIndex(slot_size, GetBucketDistribution());
  PartitionRoot::Bucket* bucket = &allocator.root()->buckets[bucket_index];
  ASSERT_EQ(bucket->slot_size, slot_size);

  // The first allocation is expected to span exactly the capcity of the slot.
  // The second one should overflow into a higher-size slot, and not fill its
  // capacity.
  size_t requested_size1 = slot_size - ExtraAllocSize(allocator);
  size_t requested_size2 = requested_size1 + 1;
  void* ptr1 = allocator.root()->Alloc(requested_size1);
  void* ptr2 = allocator.root()->Alloc(requested_size2);
  size_t capacity1 = allocator.root()->AllocationCapacityFromSlotStart(
      allocator.root()->ObjectToSlotStart(ptr1));
  size_t capacity2 = allocator.root()->AllocationCapacityFromSlotStart(
      allocator.root()->ObjectToSlotStart(ptr2));
  EXPECT_EQ(capacity1, requested_size1);
  EXPECT_LT(capacity1, capacity2);
  EXPECT_LT(requested_size2, capacity2);
  allocator.root()->Free(ptr1);
  allocator.root()->Free(ptr2);
}

TEST_P(PartitionAllocTest, PreferSlotSpansWithProvisionedEntries) {
  size_t size = SystemPageSize() - ExtraAllocSize(allocator);
  size_t real_size = size + ExtraAllocSize(allocator);
  size_t bucket_index =
      allocator.root()->SizeToBucketIndex(real_size, GetBucketDistribution());
  PartitionRoot::Bucket* bucket = &allocator.root()->buckets[bucket_index];
  ASSERT_EQ(bucket->slot_size, real_size);
  size_t slots_per_span = bucket->num_system_pages_per_slot_span;

  // Make 10 full slot spans.
  constexpr int kSpans = 10;
  std::vector<std::vector<void*>> allocated_memory_spans(kSpans);
  for (int span_index = 0; span_index < kSpans; span_index++) {
    for (size_t i = 0; i < slots_per_span; i++) {
      allocated_memory_spans[span_index].push_back(
          allocator.root()->Alloc(size));
    }
  }

  // Reverse ordering, since a newly non-full span is placed at the head of the
  // active list.
  for (int span_index = kSpans - 1; span_index >= 0; span_index--) {
    allocator.root()->Free(allocated_memory_spans[span_index].back());
    allocated_memory_spans[span_index].pop_back();
  }

  // Since slot spans are large enough and we freed memory from the end, the
  // slot spans become partially provisioned after PurgeMemory().
  allocator.root()->PurgeMemory(PurgeFlags::kDecommitEmptySlotSpans |
                                PurgeFlags::kDiscardUnusedSystemPages);
  std::vector<const SlotSpan*> active_slot_spans;
  for (auto* span = bucket->active_slot_spans_head; span;
       span = span->next_slot_span) {
    active_slot_spans.push_back(span);
    ASSERT_EQ(span->num_unprovisioned_slots, 1u);
    // But no freelist entries.
    ASSERT_FALSE(span->get_freelist_head());
  }

  // Free one entry in the middle span, creating a freelist entry.
  constexpr size_t kSpanIndex = 5;
  allocator.root()->Free(allocated_memory_spans[kSpanIndex].back());
  allocated_memory_spans[kSpanIndex].pop_back();

  ASSERT_TRUE(active_slot_spans[kSpanIndex]->get_freelist_head());
  ASSERT_FALSE(bucket->active_slot_spans_head->get_freelist_head());

  // It must come from the middle slot span even though the first one has
  // unprovisioned space.
  void* new_ptr = allocator.root()->Alloc(size);

  // Comes from the middle slot span, since it has a freelist entry.
  auto* new_active_slot_span = active_slot_spans[kSpanIndex];
  ASSERT_FALSE(new_active_slot_span->get_freelist_head());

  // The middle slot span was moved to the front.
  active_slot_spans.erase(active_slot_spans.begin() + kSpanIndex);
  active_slot_spans.insert(active_slot_spans.begin(), new_active_slot_span);

  // Check slot span ordering.
  int index = 0;
  for (auto* span = bucket->active_slot_spans_head; span;
       span = span->next_slot_span) {
    EXPECT_EQ(span, active_slot_spans[index]);
    index++;
  }
  EXPECT_EQ(index, kSpans);

  allocator.root()->Free(new_ptr);
  for (int span_index = 0; span_index < kSpans; span_index++) {
    for (void* ptr : allocated_memory_spans[span_index]) {
      allocator.root()->Free(ptr);
    }
  }
}

// Test some corner cases relating to slot span transitions in the internal
// free slot span list metadata bucket.
TEST_P(PartitionAllocTest, FreeSlotSpanListSlotSpanTransitions) {
  PartitionRoot::Bucket* bucket =
      &allocator.root()->buckets[test_bucket_index_];

  size_t num_to_fill_free_list_slot_span =
      PartitionPageSize() / (sizeof(SlotSpan) + ExtraAllocSize(allocator));
  // The +1 is because we need to account for the fact that the current slot
  // span never gets thrown on the freelist.
  ++num_to_fill_free_list_slot_span;
  auto slot_spans =
      std::make_unique<const SlotSpan*[]>(num_to_fill_free_list_slot_span);

  size_t i;
  for (i = 0; i < num_to_fill_free_list_slot_span; ++i) {
    slot_spans[i] = GetFullSlotSpan(kTestAllocSize);
  }
  EXPECT_EQ(slot_spans[num_to_fill_free_list_slot_span - 1],
            bucket->active_slot_spans_head);
  for (i = 0; i < num_to_fill_free_list_slot_span; ++i) {
    FreeFullSlotSpan(allocator.root(), slot_spans[i]);
  }
  EXPECT_EQ(SlotSpan::get_sentinel_slot_span(), bucket->active_slot_spans_head);
  EXPECT_TRUE(bucket->empty_slot_spans_head);

  // Allocate / free in a different bucket size so we get control of a
  // different free slot span list. We need two slot spans because one will be
  // the last active slot span and not get freed.
  auto* slot_span1 = GetFullSlotSpan(kTestAllocSize * 2);
  auto* slot_span2 = GetFullSlotSpan(kTestAllocSize * 2);
  FreeFullSlotSpan(allocator.root(), slot_span1);
  FreeFullSlotSpan(allocator.root(), slot_span2);

  for (i = 0; i < num_to_fill_free_list_slot_span; ++i) {
    slot_spans[i] = GetFullSlotSpan(kTestAllocSize);
  }
  EXPECT_EQ(slot_spans[num_to_fill_free_list_slot_span - 1],
            bucket->active_slot_spans_head);

  for (i = 0; i < num_to_fill_free_list_slot_span; ++i) {
    FreeFullSlotSpan(allocator.root(), slot_spans[i]);
  }
  EXPECT_EQ(SlotSpan::get_sentinel_slot_span(), bucket->active_slot_spans_head);
  EXPECT_TRUE(bucket->empty_slot_spans_head);
}

// Test a large series of allocations that cross more than one underlying
// super page.
TEST_P(PartitionAllocTest, MultiPageAllocs) {
  size_t num_pages_per_slot_span = GetNumPagesPerSlotSpan(kTestAllocSize);
  // 1 super page has 2 guard partition pages and a tag bitmap.
  size_t num_slot_spans_needed =
      (NumPartitionPagesPerSuperPage() - 2 -
       partition_alloc::internal::NumPartitionPagesPerFreeSlotBitmap()) /
      num_pages_per_slot_span;

  // We need one more slot span in order to cross super page boundary.
  ++num_slot_spans_needed;

  EXPECT_GT(num_slot_spans_needed, 1u);
  auto slot_spans = std::make_unique<const SlotSpan*[]>(num_slot_spans_needed);
  uintptr_t first_super_page_base = 0;
  size_t i;
  for (i = 0; i < num_slot_spans_needed; ++i) {
    slot_spans[i] = GetFullSlotSpan(kTestAllocSize);
    uintptr_t slot_span_start = SlotSpan::ToSlotSpanStart(slot_spans[i]);
    if (!i) {
      first_super_page_base = slot_span_start & kSuperPageBaseMask;
    }
    if (i == num_slot_spans_needed - 1) {
      uintptr_t second_super_page_base = slot_span_start & kSuperPageBaseMask;
      uintptr_t second_super_page_offset =
          slot_span_start & kSuperPageOffsetMask;
      EXPECT_FALSE(second_super_page_base == first_super_page_base);
      // Check that we allocated a guard page and the reserved tag bitmap for
      // the second page.
      EXPECT_EQ(PartitionPageSize() +
                    partition_alloc::internal::ReservedFreeSlotBitmapSize(),
                second_super_page_offset);
    }
  }
  for (i = 0; i < num_slot_spans_needed; ++i) {
    FreeFullSlotSpan(allocator.root(), slot_spans[i]);
  }
}

// Test the generic allocation functions that can handle arbitrary sizes and
// reallocing etc.
TEST_P(PartitionAllocTest, Alloc) {
  void* ptr = allocator.root()->Alloc(1, type_name);
  EXPECT_TRUE(ptr);
  allocator.root()->Free(ptr);
  ptr = allocator.root()->Alloc(kMaxBucketed + 1, type_name);
  EXPECT_TRUE(ptr);
  allocator.root()->Free(ptr);

  // To make both alloc(x + 1) and alloc(x + kSmallestBucket) to allocate from
  // the same bucket, partition_alloc::internal::base::bits::AlignUp(1 + x +
  // ExtraAllocSize(allocator), kAlignment)
  // == partition_alloc::internal::base::bits::AlignUp(kSmallestBucket + x +
  // ExtraAllocSize(allocator), kAlignment), because slot_size is multiples of
  // kAlignment. So (x + ExtraAllocSize(allocator)) must be multiples of
  // kAlignment. x =
  // partition_alloc::internal::base::bits::AlignUp(ExtraAllocSize(allocator),
  // kAlignment) - ExtraAllocSize(allocator);
  size_t base_size = partition_alloc::internal::base::bits::AlignUp(
                         ExtraAllocSize(allocator), kAlignment) -
                     ExtraAllocSize(allocator);
  ptr = allocator.root()->Alloc(base_size + 1, type_name);
  EXPECT_TRUE(ptr);
  void* orig_ptr = ptr;
  char* char_ptr = static_cast<char*>(ptr);
  *char_ptr = 'A';

  // Change the size of the realloc, remaining inside the same bucket.
  void* new_ptr = allocator.root()->Realloc(ptr, base_size + 2, type_name);
  PA_EXPECT_PTR_EQ(ptr, new_ptr);
  new_ptr = allocator.root()->Realloc(ptr, base_size + 1, type_name);
  PA_EXPECT_PTR_EQ(ptr, new_ptr);
  new_ptr =
      allocator.root()->Realloc(ptr, base_size + kSmallestBucket, type_name);
  PA_EXPECT_PTR_EQ(ptr, new_ptr);

  // Change the size of the realloc, switching buckets.
  new_ptr = allocator.root()->Realloc(ptr, base_size + kSmallestBucket + 1,
                                      type_name);
  PA_EXPECT_PTR_NE(new_ptr, ptr);
  // Check that the realloc copied correctly.
  char* new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'A');
#if PA_BUILDFLAG(EXPENSIVE_DCHECKS_ARE_ON)
  // Subtle: this checks for an old bug where we copied too much from the
  // source of the realloc. The condition can be detected by a trashing of
  // the uninitialized value in the space of the upsized allocation.
  EXPECT_EQ(kUninitializedByte,
            static_cast<unsigned char>(*(new_char_ptr + kSmallestBucket)));
#endif
  *new_char_ptr = 'B';
  // The realloc moved. To check that the old allocation was freed, we can
  // do an alloc of the old allocation size and check that the old allocation
  // address is at the head of the freelist and reused.
  void* reused_ptr = allocator.root()->Alloc(base_size + 1, type_name);
  PA_EXPECT_PTR_EQ(reused_ptr, orig_ptr);
  allocator.root()->Free(reused_ptr);

  // Downsize the realloc.
  ptr = new_ptr;
  new_ptr = allocator.root()->Realloc(ptr, base_size + 1, type_name);
  PA_EXPECT_PTR_EQ(new_ptr, orig_ptr);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'B');
  *new_char_ptr = 'C';

  // Upsize the realloc to outside the partition.
  ptr = new_ptr;
  new_ptr = allocator.root()->Realloc(ptr, kMaxBucketed + 1, type_name);
  PA_EXPECT_PTR_NE(new_ptr, ptr);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'C');
  *new_char_ptr = 'D';

  // Upsize and downsize the realloc, remaining outside the partition.
  ptr = new_ptr;
  new_ptr = allocator.root()->Realloc(ptr, kMaxBucketed * 10, type_name);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'D');
  *new_char_ptr = 'E';
  ptr = new_ptr;
  new_ptr = allocator.root()->Realloc(ptr, kMaxBucketed * 2, type_name);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'E');
  *new_char_ptr = 'F';

  // Downsize the realloc to inside the partition.
  ptr = new_ptr;
  new_ptr = allocator.root()->Realloc(ptr, base_size + 1, type_name);
  PA_EXPECT_PTR_NE(new_ptr, ptr);
  PA_EXPECT_PTR_EQ(new_ptr, orig_ptr);
  new_char_ptr = static_cast<char*>(new_ptr);
  EXPECT_EQ(*new_char_ptr, 'F');

  allocator.root()->Free(new_ptr);
}

// Test the generic allocation functions can handle some specific sizes of
// interest.
TEST_P(PartitionAllocTest, AllocSizes) {
  {
    void* ptr = allocator.root()->Alloc(0, type_name);
    EXPECT_TRUE(ptr);
    allocator.root()->Free(ptr);
  }

  {
    // PartitionPageSize() is interesting because it results in just one
    // allocation per page, which tripped up some corner cases.
    const size_t size = PartitionPageSize() - ExtraAllocSize(allocator);
    void* ptr = allocator.root()->Alloc(size, type_name);
    EXPECT_TRUE(ptr);
    void* ptr2 = allocator.root()->Alloc(size, type_name);
    EXPECT_TRUE(ptr2);
    allocator.root()->Free(ptr);
    // Should be freeable at this point.
    auto* slot_span =
        SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr));
    EXPECT_TRUE(slot_span->in_empty_cache());
    allocator.root()->Free(ptr2);
  }

  // TODO(casey.smalley@arm.com): test expects a single slot-span for each
  // allocation, but under 64k pages the requested size is 1MiB which
  // instead is served by a direct map.
#if !(PA_BUILDFLAG(IS_LINUX) && PA_BUILDFLAG(PA_ARCH_CPU_ARM64))
  {
    // Single-slot slot span size.
    const size_t size =
        PartitionPageSize() * kMaxPartitionPagesPerRegularSlotSpan + 1;

    void* ptr = allocator.root()->Alloc(size, type_name);
    EXPECT_TRUE(ptr);
    memset(ptr, 'A', size);
    void* ptr2 = allocator.root()->Alloc(size, type_name);
    EXPECT_TRUE(ptr2);
    void* ptr3 = allocator.root()->Alloc(size, type_name);
    EXPECT_TRUE(ptr3);
    void* ptr4 = allocator.root()->Alloc(size, type_name);
    EXPECT_TRUE(ptr4);

    auto* slot_span =
        SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr));
    auto* slot_span2 =
        SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr3));
    EXPECT_NE(slot_span, slot_span2);

    allocator.root()->Free(ptr);
    allocator.root()->Free(ptr3);
    allocator.root()->Free(ptr2);
    // Should be freeable at this point.
    EXPECT_TRUE(slot_span->in_empty_cache());
    EXPECT_EQ(0u, slot_span->num_allocated_slots);
    EXPECT_EQ(0u, slot_span->num_unprovisioned_slots);
    void* new_ptr_1 = allocator.root()->Alloc(size, type_name);
    PA_EXPECT_PTR_EQ(ptr2, new_ptr_1);
    void* new_ptr_2 = allocator.root()->Alloc(size, type_name);
    PA_EXPECT_PTR_EQ(ptr3, new_ptr_2);

    allocator.root()->Free(new_ptr_1);
    allocator.root()->Free(new_ptr_2);
    allocator.root()->Free(ptr4);

#if PA_BUILDFLAG(EXPENSIVE_DCHECKS_ARE_ON)
    // |SlotSpan::Free| must poison the slot's
    // contents with |kFreedByte|.
    EXPECT_EQ(kFreedByte,
              *(static_cast<unsigned char*>(new_ptr_1) + (size - 1)));
#endif
  }
#endif  // !(PA_BUILDFLAG(IS_LINUX) && PA_BUILDFLAG(PA_ARCH_CPU_ARM64))

  // Can we allocate a massive (128MB) size?
  // Add +1, to test for cookie writing alignment issues.
  // Test this only if the device has enough memory or it might fail due
  // to OOM.
  if (IsLargeMemoryDevice()) {
    void* ptr = allocator.root()->Alloc(128 * 1024 * 1024 + 1, type_name);
    allocator.root()->Free(ptr);
  }

  {
    // Check a more reasonable, but still direct mapped, size.
    // Chop a system page and a byte off to test for rounding errors.
    size_t size = 20 * 1024 * 1024;
    ASSERT_GT(size, kMaxBucketed);
    size -= SystemPageSize();
    size -= 1;
    void* ptr = allocator.root()->Alloc(size, type_name);
    char* char_ptr = static_cast<char*>(ptr);
    *(char_ptr + (size - 1)) = 'A';
    allocator.root()->Free(ptr);

    // Can we free null?
    allocator.root()->Free(nullptr);

    // Do we correctly get a null for a failed allocation?
    EXPECT_EQ(nullptr, allocator.root()->Alloc<AllocFlags::kReturnNull>(
                           3u * 1024 * 1024 * 1024, type_name));
  }
}

// Test that we can fetch the real allocated size after an allocation.
TEST_P(PartitionAllocTest, AllocGetSizeAndStart) {
  void* ptr;
  size_t requested_size, actual_capacity, predicted_capacity;

  // Allocate something small.
  requested_size = 511 - ExtraAllocSize(allocator);
  predicted_capacity =
      allocator.root()->AllocationCapacityFromRequestedSize(requested_size);
  ptr = allocator.root()->Alloc(requested_size, type_name);
  EXPECT_TRUE(ptr);
  uintptr_t slot_start = allocator.root()->ObjectToSlotStart(ptr);
  actual_capacity =
      allocator.root()->AllocationCapacityFromSlotStart(slot_start);
  EXPECT_EQ(predicted_capacity, actual_capacity);
  EXPECT_LT(requested_size, actual_capacity);
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  if (UseBRPPool()) {
    uintptr_t address = UntagPtr(ptr);
    for (size_t offset = 0; offset < requested_size; ++offset) {
      EXPECT_EQ(PartitionAllocGetSlotStartAndSizeInBRPPool(address + offset)
                    .slot_start,
                slot_start);
    }
  }
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  allocator.root()->Free(ptr);

  // Allocate a size that should be a perfect match for a bucket, because it
  // is an exact power of 2.
  requested_size = (256 * 1024) - ExtraAllocSize(allocator);
  predicted_capacity =
      allocator.root()->AllocationCapacityFromRequestedSize(requested_size);
  ptr = allocator.root()->Alloc(requested_size, type_name);
  EXPECT_TRUE(ptr);
  slot_start = allocator.root()->ObjectToSlotStart(ptr);
  actual_capacity =
      allocator.root()->AllocationCapacityFromSlotStart(slot_start);
  EXPECT_EQ(predicted_capacity, actual_capacity);
  EXPECT_EQ(requested_size, actual_capacity);
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  if (UseBRPPool()) {
    uintptr_t address = UntagPtr(ptr);
    for (size_t offset = 0; offset < requested_size; offset += 877) {
      EXPECT_EQ(PartitionAllocGetSlotStartAndSizeInBRPPool(address + offset)
                    .slot_start,
                slot_start);
    }
  }
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  allocator.root()->Free(ptr);

  // Allocate a size that is a system page smaller than a bucket.
  // AllocationCapacityFromSlotStart() should return a larger size than we asked
  // for now.
  size_t num = 64;
  while (num * SystemPageSize() >= 1024 * 1024) {
    num /= 2;
  }
  requested_size =
      num * SystemPageSize() - SystemPageSize() - ExtraAllocSize(allocator);
  predicted_capacity =
      allocator.root()->AllocationCapacityFromRequestedSize(requested_size);
  ptr = allocator.root()->Alloc(requested_size, type_name);
  EXPECT_TRUE(ptr);
  slot_start = allocator.root()->ObjectToSlotStart(ptr);
  actual_capacity =
      allocator.root()->AllocationCapacityFromSlotStart(slot_start);
  EXPECT_EQ(predicted_capacity, actual_capacity);
  EXPECT_EQ(requested_size + SystemPageSize(), actual_capacity);
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  if (UseBRPPool()) {
    uintptr_t address = UntagPtr(ptr);
    for (size_t offset = 0; offset < requested_size; offset += 4999) {
      EXPECT_EQ(PartitionAllocGetSlotStartAndSizeInBRPPool(address + offset)
                    .slot_start,
                slot_start);
    }
  }
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  allocator.root()->Free(ptr);

  // Allocate the maximum allowed bucketed size.
  requested_size = kMaxBucketed - ExtraAllocSize(allocator);
  predicted_capacity =
      allocator.root()->AllocationCapacityFromRequestedSize(requested_size);
  ptr = allocator.root()->Alloc(requested_size, type_name);
  EXPECT_TRUE(ptr);
  slot_start = allocator.root()->ObjectToSlotStart(ptr);
  actual_capacity =
      allocator.root()->AllocationCapacityFromSlotStart(slot_start);
  EXPECT_EQ(predicted_capacity, actual_capacity);
  EXPECT_EQ(requested_size, actual_capacity);
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  if (UseBRPPool()) {
    uintptr_t address = UntagPtr(ptr);
    for (size_t offset = 0; offset < requested_size; offset += 4999) {
      EXPECT_EQ(PartitionAllocGetSlotStartAndSizeInBRPPool(address + offset)
                    .slot_start,
                slot_start);
    }
  }
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

  // Check that we can write at the end of the reported size too.
  char* char_ptr = static_cast<char*>(ptr);
  *(char_ptr + (actual_capacity - 1)) = 'A';
  allocator.root()->Free(ptr);

  // Allocate something very large, and uneven.
  if (IsLargeMemoryDevice()) {
    requested_size = 128 * 1024 * 1024 - 33;
    predicted_capacity =
        allocator.root()->AllocationCapacityFromRequestedSize(requested_size);
    ptr = allocator.root()->Alloc(requested_size, type_name);
    EXPECT_TRUE(ptr);
    slot_start = allocator.root()->ObjectToSlotStart(ptr);
    actual_capacity =
        allocator.root()->AllocationCapacityFromSlotStart(slot_start);
    EXPECT_EQ(predicted_capacity, actual_capacity);

    EXPECT_LT(requested_size, actual_capacity);

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    if (UseBRPPool()) {
      uintptr_t address = UntagPtr(ptr);
      for (size_t offset = 0; offset < requested_size; offset += 16111) {
        EXPECT_EQ(PartitionAllocGetSlotStartAndSizeInBRPPool(address + offset)
                      .slot_start,
                  slot_start);
      }
    }
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    allocator.root()->Free(ptr);
  }

  // Too large allocation.
  requested_size = MaxDirectMapped() + 1;
  predicted_capacity =
      allocator.root()->AllocationCapacityFromRequestedSize(requested_size);
  EXPECT_EQ(requested_size, predicted_capacity);
}

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
TEST_P(PartitionAllocTest, MTEProtectsFreedPtr) {
  // This test checks that Arm's memory tagging extension (MTE) is correctly
  // protecting freed pointers.
  base::CPU cpu;
  if (!cpu.has_mte()) {
    // This test won't pass without MTE support.
    GTEST_SKIP();
  }

  // Create an arbitrarily-sized small allocation.
  size_t alloc_size = 64 - ExtraAllocSize(allocator);
  uint64_t* ptr1 =
      static_cast<uint64_t*>(allocator.root()->Alloc(alloc_size, type_name));
  EXPECT_TRUE(ptr1);

  // Invalidate the pointer by freeing it.
  allocator.root()->Free(ptr1);

  // When we immediately reallocate a pointer, we should see the same allocation
  // slot but with a different tag (PA_EXPECT_PTR_EQ ignores the MTE tag).
  uint64_t* ptr2 =
      static_cast<uint64_t*>(allocator.root()->Alloc(alloc_size, type_name));
  PA_EXPECT_PTR_EQ(ptr1, ptr2);
  // The different tag bits mean that ptr1 is not the same as ptr2.
  EXPECT_NE(ptr1, ptr2);

  // When we free again, we expect a new tag for that area that's different from
  // ptr1 and ptr2.
  allocator.root()->Free(ptr2);
  uint64_t* ptr3 =
      static_cast<uint64_t*>(allocator.root()->Alloc(alloc_size, type_name));
  PA_EXPECT_PTR_EQ(ptr2, ptr3);
  EXPECT_NE(ptr1, ptr3);
  EXPECT_NE(ptr2, ptr3);

  // We don't check anything about ptr3, but we do clean it up to avoid DCHECKs.
  allocator.root()->Free(ptr3);
}
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
TEST_P(PartitionAllocTest, IsPtrWithinSameAlloc) {
  if (!UseBRPPool()) {
    return;
  }

  const size_t kMinReasonableTestSize =
      partition_alloc::internal::base::bits::AlignUp(
          ExtraAllocSize(allocator) + 1, kAlignment);
  ASSERT_GT(kMinReasonableTestSize, ExtraAllocSize(allocator));
  const size_t kSizes[] = {kMinReasonableTestSize,
                           256,
                           SystemPageSize(),
                           PartitionPageSize(),
                           MaxRegularSlotSpanSize(),
                           MaxRegularSlotSpanSize() + 1,
                           MaxRegularSlotSpanSize() + SystemPageSize(),
                           MaxRegularSlotSpanSize() + PartitionPageSize(),
                           kMaxBucketed,
                           kMaxBucketed + 1,
                           kMaxBucketed + SystemPageSize(),
                           kMaxBucketed + PartitionPageSize(),
                           kSuperPageSize};
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  constexpr size_t kFarFarAwayDelta = 512 * kGiB;
#else
  constexpr size_t kFarFarAwayDelta = kGiB;
#endif
  for (size_t size : kSizes) {
    size_t requested_size = size - ExtraAllocSize(allocator);
    // For regular slot-span allocations, confirm the size fills the entire
    // slot. Otherwise the test would be ineffective, as PartitionAlloc has no
    // ability to check against the actual allocated size.
    // Single-slot slot-spans and direct map don't have that problem.
    if (size <= MaxRegularSlotSpanSize()) {
      ASSERT_EQ(requested_size,
                allocator.root()->AllocationCapacityFromRequestedSize(
                    requested_size));
    }

    constexpr size_t kNumRepeats = 3;
    void* ptrs[kNumRepeats];
    for (void*& ptr : ptrs) {
      ptr = allocator.root()->Alloc(requested_size, type_name);
      // Double check.
      if (size <= MaxRegularSlotSpanSize()) {
        uintptr_t slot_start = allocator.root()->ObjectToSlotStart(ptr);
        EXPECT_EQ(
            requested_size,
            allocator.root()->AllocationCapacityFromSlotStart(slot_start));
      }

      uintptr_t address = UntagPtr(ptr);
      EXPECT_EQ(IsPtrWithinSameAlloc(address, address - kFarFarAwayDelta, 0u),
                PtrPosWithinAlloc::kFarOOB);
      EXPECT_EQ(IsPtrWithinSameAlloc(address, address - kSuperPageSize, 0u),
                PtrPosWithinAlloc::kFarOOB);
      EXPECT_EQ(IsPtrWithinSameAlloc(address, address - 1, 0u),
                PtrPosWithinAlloc::kFarOOB);
      EXPECT_EQ(IsPtrWithinSameAlloc(address, address, 0u),
                PtrPosWithinAlloc::kInBounds);
      EXPECT_EQ(IsPtrWithinSameAlloc(address, address + requested_size / 2, 0u),
                PtrPosWithinAlloc::kInBounds);
#if PA_BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
      EXPECT_EQ(IsPtrWithinSameAlloc(address, address + requested_size - 1, 1u),
                PtrPosWithinAlloc::kInBounds);
      EXPECT_EQ(IsPtrWithinSameAlloc(address, address + requested_size, 1u),
                PtrPosWithinAlloc::kAllocEnd);
      EXPECT_EQ(IsPtrWithinSameAlloc(address, address + requested_size - 4, 4u),
                PtrPosWithinAlloc::kInBounds);
      for (size_t subtrahend = 0; subtrahend < 4; subtrahend++) {
        EXPECT_EQ(IsPtrWithinSameAlloc(
                      address, address + requested_size - subtrahend, 4u),
                  PtrPosWithinAlloc::kAllocEnd);
      }
#else  // PA_BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
      EXPECT_EQ(IsPtrWithinSameAlloc(address, address + requested_size, 0u),
                PtrPosWithinAlloc::kInBounds);
#endif
      EXPECT_EQ(IsPtrWithinSameAlloc(address, address + requested_size + 1, 0u),
                PtrPosWithinAlloc::kFarOOB);
      EXPECT_EQ(IsPtrWithinSameAlloc(
                    address, address + requested_size + kSuperPageSize, 0u),
                PtrPosWithinAlloc::kFarOOB);
      EXPECT_EQ(IsPtrWithinSameAlloc(
                    address, address + requested_size + kFarFarAwayDelta, 0u),
                PtrPosWithinAlloc::kFarOOB);
      EXPECT_EQ(
          IsPtrWithinSameAlloc(address + requested_size,
                               address + requested_size + kFarFarAwayDelta, 0u),
          PtrPosWithinAlloc::kFarOOB);
      EXPECT_EQ(
          IsPtrWithinSameAlloc(address + requested_size,
                               address + requested_size + kSuperPageSize, 0u),
          PtrPosWithinAlloc::kFarOOB);
      EXPECT_EQ(IsPtrWithinSameAlloc(address + requested_size,
                                     address + requested_size + 1, 0u),
                PtrPosWithinAlloc::kFarOOB);
#if PA_BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
      EXPECT_EQ(IsPtrWithinSameAlloc(address + requested_size - 1,
                                     address + requested_size - 1, 1u),
                PtrPosWithinAlloc::kInBounds);
      EXPECT_EQ(IsPtrWithinSameAlloc(address + requested_size - 1,
                                     address + requested_size, 1u),
                PtrPosWithinAlloc::kAllocEnd);
      EXPECT_EQ(IsPtrWithinSameAlloc(address + requested_size,
                                     address + requested_size, 1u),
                PtrPosWithinAlloc::kAllocEnd);
      EXPECT_EQ(IsPtrWithinSameAlloc(address + requested_size - 4,
                                     address + requested_size - 4, 4u),
                PtrPosWithinAlloc::kInBounds);
      for (size_t addend = 1; addend < 4; addend++) {
        EXPECT_EQ(
            IsPtrWithinSameAlloc(address + requested_size - 4,
                                 address + requested_size - 4 + addend, 4u),
            PtrPosWithinAlloc::kAllocEnd);
      }
#else  // PA_BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
      EXPECT_EQ(IsPtrWithinSameAlloc(address + requested_size,
                                     address + requested_size, 0u),
                PtrPosWithinAlloc::kInBounds);
#endif
      EXPECT_EQ(IsPtrWithinSameAlloc(
                    address + requested_size,
                    address + requested_size - (requested_size / 2), 0u),
                PtrPosWithinAlloc::kInBounds);
      EXPECT_EQ(IsPtrWithinSameAlloc(address + requested_size, address, 0u),
                PtrPosWithinAlloc::kInBounds);
      EXPECT_EQ(IsPtrWithinSameAlloc(address + requested_size, address - 1, 0u),
                PtrPosWithinAlloc::kFarOOB);
      EXPECT_EQ(IsPtrWithinSameAlloc(address + requested_size,
                                     address - kSuperPageSize, 0u),
                PtrPosWithinAlloc::kFarOOB);
      EXPECT_EQ(IsPtrWithinSameAlloc(address + requested_size,
                                     address - kFarFarAwayDelta, 0u),
                PtrPosWithinAlloc::kFarOOB);
    }

    for (void* ptr : ptrs) {
      allocator.root()->Free(ptr);
    }
  }
}

TEST_P(PartitionAllocTest, GetSlotStartMultiplePages) {
  if (!UseBRPPool()) {
    return;
  }

  auto* root = allocator.root();
  // Find the smallest bucket with multiple PartitionPages. When searching for
  // a bucket here, we need to check two conditions:
  // (1) The bucket is used in our current bucket distribution.
  // (2) The bucket is large enough that our requested size (see below) will be
  // non-zero.
  size_t real_size = 0;
  for (const auto& bucket : root->buckets) {
    if ((root->buckets + SizeToIndex(bucket.slot_size))->slot_size !=
        bucket.slot_size) {
      continue;
    }
    if (bucket.slot_size <= ExtraAllocSize(allocator)) {
      continue;
    }
    if (bucket.num_system_pages_per_slot_span >
        NumSystemPagesPerPartitionPage()) {
      real_size = bucket.slot_size;
      break;
    }
  }

  // Make sure that we've managed to find an appropriate bucket.
  ASSERT_GT(real_size, 0u);

  const size_t requested_size = real_size - ExtraAllocSize(allocator);
  // Double check we don't end up with 0 or negative size.
  EXPECT_GT(requested_size, 0u);
  EXPECT_LE(requested_size, real_size);
  const auto* bucket = allocator.root()->buckets + SizeToIndex(real_size);
  EXPECT_EQ(bucket->slot_size, real_size);
  // Make sure the test is testing multiple partition pages case.
  EXPECT_GT(bucket->num_system_pages_per_slot_span,
            PartitionPageSize() / SystemPageSize());
  size_t num_slots =
      (bucket->num_system_pages_per_slot_span * SystemPageSize()) / real_size;
  std::vector<void*> ptrs;
  for (size_t i = 0; i < num_slots; ++i) {
    ptrs.push_back(allocator.root()->Alloc(requested_size, type_name));
  }
  for (void* ptr : ptrs) {
    uintptr_t address = UntagPtr(ptr);
    uintptr_t slot_start = allocator.root()->ObjectToSlotStart(ptr);
    EXPECT_EQ(allocator.root()->AllocationCapacityFromSlotStart(slot_start),
              requested_size);
    for (size_t offset = 0; offset < requested_size; offset += 13) {
      EXPECT_EQ(PartitionAllocGetSlotStartAndSizeInBRPPool(address + offset)
                    .slot_start,
                slot_start);
    }
    allocator.root()->Free(ptr);
  }
}
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

// Test the realloc() contract.
TEST_P(PartitionAllocTest, Realloc) {
  // realloc(0, size) should be equivalent to malloc().
  void* ptr = allocator.root()->Realloc(nullptr, kTestAllocSize, type_name);
  memset(ptr, 'A', kTestAllocSize);
  auto* slot_span =
      SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr));
  // realloc(ptr, 0) should be equivalent to free().
  void* ptr2 = allocator.root()->Realloc(ptr, 0, type_name);
  EXPECT_EQ(nullptr, ptr2);
  EXPECT_EQ(allocator.root()->ObjectToSlotStart(ptr),
            UntagPtr(slot_span->get_freelist_head()));

  // Test that growing an allocation with realloc() copies everything from the
  // old allocation.
  size_t size = SystemPageSize() - ExtraAllocSize(allocator);
  // Confirm size fills the entire slot.
  ASSERT_EQ(size, allocator.root()->AllocationCapacityFromRequestedSize(size));
  ptr = allocator.root()->Alloc(size, type_name);
  memset(ptr, 'A', size);
  ptr2 = allocator.root()->Realloc(ptr, size + 1, type_name);
  PA_EXPECT_PTR_NE(ptr, ptr2);
  char* char_ptr2 = static_cast<char*>(ptr2);
  EXPECT_EQ('A', char_ptr2[0]);
  EXPECT_EQ('A', char_ptr2[size - 1]);
#if PA_BUILDFLAG(EXPENSIVE_DCHECKS_ARE_ON)
  EXPECT_EQ(kUninitializedByte, static_cast<unsigned char>(char_ptr2[size]));
#endif

  // Test that shrinking an allocation with realloc() also copies everything
  // from the old allocation. Use |size - 1| to test what happens to the extra
  // space before the cookie.
  ptr = allocator.root()->Realloc(ptr2, size - 1, type_name);
  PA_EXPECT_PTR_NE(ptr2, ptr);
  char* char_ptr = static_cast<char*>(ptr);
  EXPECT_EQ('A', char_ptr[0]);
  EXPECT_EQ('A', char_ptr[size - 2]);
#if PA_BUILDFLAG(EXPENSIVE_DCHECKS_ARE_ON)
  EXPECT_EQ(kUninitializedByte, static_cast<unsigned char>(char_ptr[size - 1]));
#endif

  allocator.root()->Free(ptr);

  // Single-slot slot spans...
  // Test that growing an allocation with realloc() copies everything from the
  // old allocation.
  size = MaxRegularSlotSpanSize() + 1;
  ASSERT_LE(2 * size, kMaxBucketed);  // should be in single-slot span range
  // Confirm size doesn't fill the entire slot.
  ASSERT_LT(size, allocator.root()->AllocationCapacityFromRequestedSize(size));
  ptr = allocator.root()->Alloc(size, type_name);
  memset(ptr, 'A', size);
  ptr2 = allocator.root()->Realloc(ptr, size * 2, type_name);
  PA_EXPECT_PTR_NE(ptr, ptr2);
  char_ptr2 = static_cast<char*>(ptr2);
  EXPECT_EQ('A', char_ptr2[0]);
  EXPECT_EQ('A', char_ptr2[size - 1]);
#if PA_BUILDFLAG(EXPENSIVE_DCHECKS_ARE_ON)
  EXPECT_EQ(kUninitializedByte, static_cast<unsigned char>(char_ptr2[size]));
#endif
  allocator.root()->Free(ptr2);

  // Test that shrinking an allocation with realloc() also copies everything
  // from the old allocation.
  size = 2 * (MaxRegularSlotSpanSize() + 1);
  ASSERT_GT(size / 2, MaxRegularSlotSpanSize());  // in single-slot span range
  ptr = allocator.root()->Alloc(size, type_name);
  memset(ptr, 'A', size);
  ptr2 = allocator.root()->Realloc(ptr2, size / 2, type_name);
  PA_EXPECT_PTR_NE(ptr, ptr2);
  char_ptr2 = static_cast<char*>(ptr2);
  EXPECT_EQ('A', char_ptr2[0]);
  EXPECT_EQ('A', char_ptr2[size / 2 - 1]);
#if PA_BUILDFLAG(USE_PARTITION_COOKIE)
  // For single-slot slot spans, the cookie is always placed immediately after
  // the allocation.
  EXPECT_EQ(kCookieValue[0], static_cast<unsigned char>(char_ptr2[size / 2]));
#endif
  allocator.root()->Free(ptr2);

  // Test that shrinking a direct mapped allocation happens in-place.
  // Pick a large size so that Realloc doesn't think it's worthwhile to
  // downsize even if one less super page is used (due to high granularity on
  // 64-bit systems).
  size = 10 * kSuperPageSize + SystemPageSize() - 42;
  ASSERT_GT(size - 32 * SystemPageSize(), kMaxBucketed);
  ptr = allocator.root()->Alloc(size, type_name);
  uintptr_t slot_start = allocator.root()->ObjectToSlotStart(ptr);
  size_t actual_capacity =
      allocator.root()->AllocationCapacityFromSlotStart(slot_start);
  ptr2 = allocator.root()->Realloc(ptr, size - SystemPageSize(), type_name);
  uintptr_t slot_start2 = allocator.root()->ObjectToSlotStart(ptr2);
  EXPECT_EQ(slot_start, slot_start2);
  EXPECT_EQ(actual_capacity - SystemPageSize(),
            allocator.root()->AllocationCapacityFromSlotStart(slot_start2));
  void* ptr3 =
      allocator.root()->Realloc(ptr2, size - 32 * SystemPageSize(), type_name);
  uintptr_t slot_start3 = allocator.root()->ObjectToSlotStart(ptr3);
  EXPECT_EQ(slot_start2, slot_start3);
  EXPECT_EQ(actual_capacity - 32 * SystemPageSize(),
            allocator.root()->AllocationCapacityFromSlotStart(slot_start3));

  // Test that a previously in-place shrunk direct mapped allocation can be
  // expanded up again up to its original size.
  ptr = allocator.root()->Realloc(ptr3, size, type_name);
  slot_start = allocator.root()->ObjectToSlotStart(ptr);
  EXPECT_EQ(slot_start3, slot_start);
  EXPECT_EQ(actual_capacity,
            allocator.root()->AllocationCapacityFromSlotStart(slot_start));

  // Test that the allocation can be expanded in place up to its capacity.
  ptr2 = allocator.root()->Realloc(ptr, actual_capacity, type_name);
  slot_start2 = allocator.root()->ObjectToSlotStart(ptr2);
  EXPECT_EQ(slot_start, slot_start2);
  EXPECT_EQ(actual_capacity,
            allocator.root()->AllocationCapacityFromSlotStart(slot_start2));

  // Test that a direct mapped allocation is performed not in-place when the
  // new size is small enough.
  ptr3 = allocator.root()->Realloc(ptr2, SystemPageSize(), type_name);
  slot_start3 = allocator.root()->ObjectToSlotStart(ptr3);
  EXPECT_NE(slot_start, slot_start3);

  allocator.root()->Free(ptr3);
}

TEST_P(PartitionAllocTest, ReallocDirectMapAligned) {
  size_t alignments[] = {
      PartitionPageSize(),
      2 * PartitionPageSize(),
      kMaxSupportedAlignment / 2,
      kMaxSupportedAlignment,
  };

  for (size_t alignment : alignments) {
    // Test that shrinking a direct mapped allocation happens in-place.
    // Pick a large size so that Realloc doesn't think it's worthwhile to
    // downsize even if one less super page is used (due to high granularity on
    // 64-bit systems), even if the alignment padding is taken out.
    size_t size = 10 * kSuperPageSize + SystemPageSize() - 42;
    ASSERT_GT(size, kMaxBucketed);
    void* ptr =
        allocator.root()->AllocInternalForTesting(size, alignment, type_name);
    uintptr_t slot_start = allocator.root()->ObjectToSlotStart(ptr);
    size_t actual_capacity =
        allocator.root()->AllocationCapacityFromSlotStart(slot_start);
    void* ptr2 =
        allocator.root()->Realloc(ptr, size - SystemPageSize(), type_name);
    uintptr_t slot_start2 = allocator.root()->ObjectToSlotStart(ptr2);
    EXPECT_EQ(slot_start, slot_start2);
    EXPECT_EQ(actual_capacity - SystemPageSize(),
              allocator.root()->AllocationCapacityFromSlotStart(slot_start2));
    void* ptr3 = allocator.root()->Realloc(ptr2, size - 32 * SystemPageSize(),
                                           type_name);
    uintptr_t slot_start3 = allocator.root()->ObjectToSlotStart(ptr3);
    EXPECT_EQ(slot_start2, slot_start3);
    EXPECT_EQ(actual_capacity - 32 * SystemPageSize(),
              allocator.root()->AllocationCapacityFromSlotStart(slot_start3));

    // Test that a previously in-place shrunk direct mapped allocation can be
    // expanded up again up to its original size.
    ptr = allocator.root()->Realloc(ptr3, size, type_name);
    slot_start = allocator.root()->ObjectToSlotStart(ptr);
    EXPECT_EQ(slot_start3, slot_start);
    EXPECT_EQ(actual_capacity,
              allocator.root()->AllocationCapacityFromSlotStart(slot_start));

    // Test that the allocation can be expanded in place up to its capacity.
    ptr2 = allocator.root()->Realloc(ptr, actual_capacity, type_name);
    slot_start2 = allocator.root()->ObjectToSlotStart(ptr2);
    EXPECT_EQ(slot_start, slot_start2);
    EXPECT_EQ(actual_capacity,
              allocator.root()->AllocationCapacityFromSlotStart(slot_start2));

    // Test that a direct mapped allocation is performed not in-place when the
    // new size is small enough.
    ptr3 = allocator.root()->Realloc(ptr2, SystemPageSize(), type_name);
    slot_start3 = allocator.root()->ObjectToSlotStart(ptr3);
    EXPECT_NE(slot_start2, slot_start3);

    allocator.root()->Free(ptr3);
  }
}

TEST_P(PartitionAllocTest, ReallocDirectMapAlignedRelocate) {
  // Pick size such that the alignment will put it cross the super page
  // boundary.
  size_t size = 2 * kSuperPageSize - kMaxSupportedAlignment + SystemPageSize();
  ASSERT_GT(size, kMaxBucketed);
  void* ptr = allocator.root()->AllocInternalForTesting(
      size, kMaxSupportedAlignment, type_name);
  // Reallocating with the same size will actually relocate, because without a
  // need for alignment we can downsize the reservation significantly.
  void* ptr2 = allocator.root()->Realloc(ptr, size, type_name);
  PA_EXPECT_PTR_NE(ptr, ptr2);
  allocator.root()->Free(ptr2);

  // Again pick size such that the alignment will put it cross the super page
  // boundary, but this time make it so large that Realloc doesn't fing it worth
  // shrinking.
  size = 10 * kSuperPageSize - kMaxSupportedAlignment + SystemPageSize();
  ASSERT_GT(size, kMaxBucketed);
  ptr = allocator.root()->AllocInternalForTesting(size, kMaxSupportedAlignment,
                                                  type_name);
  ptr2 = allocator.root()->Realloc(ptr, size, type_name);
  EXPECT_EQ(ptr, ptr2);
  allocator.root()->Free(ptr2);
}

// Tests the handing out of freelists for partial slot spans.
TEST_P(PartitionAllocTest, PartialPageFreelists) {
  size_t big_size = SystemPageSize() - ExtraAllocSize(allocator);
  size_t bucket_index = SizeToIndex(big_size + ExtraAllocSize(allocator));
  PartitionRoot::Bucket* bucket = &allocator.root()->buckets[bucket_index];
  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);

  void* ptr = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr);

  auto* slot_span =
      SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr));
  size_t total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      (big_size + ExtraAllocSize(allocator));
  EXPECT_EQ(4u, total_slots);
  // The freelist should have one entry, because we were able to exactly fit
  // one object slot and one freelist pointer (the null that the head points
  // to) into a system page.
  EXPECT_FALSE(slot_span->get_freelist_head());
  EXPECT_EQ(1u, slot_span->num_allocated_slots);
  EXPECT_EQ(3u, slot_span->num_unprovisioned_slots);

  void* ptr2 = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr2);
  EXPECT_FALSE(slot_span->get_freelist_head());
  EXPECT_EQ(2u, slot_span->num_allocated_slots);
  EXPECT_EQ(2u, slot_span->num_unprovisioned_slots);

  void* ptr3 = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr3);
  EXPECT_FALSE(slot_span->get_freelist_head());
  EXPECT_EQ(3u, slot_span->num_allocated_slots);
  EXPECT_EQ(1u, slot_span->num_unprovisioned_slots);

  void* ptr4 = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr4);
  EXPECT_FALSE(slot_span->get_freelist_head());
  EXPECT_EQ(4u, slot_span->num_allocated_slots);
  EXPECT_EQ(0u, slot_span->num_unprovisioned_slots);

  void* ptr5 = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr5);

  auto* slot_span2 =
      SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr5));
  EXPECT_EQ(1u, slot_span2->num_allocated_slots);

  // Churn things a little whilst there's a partial slot span freelist.
  allocator.root()->Free(ptr);
  ptr = allocator.root()->Alloc(big_size, type_name);
  void* ptr6 = allocator.root()->Alloc(big_size, type_name);

  allocator.root()->Free(ptr);
  allocator.root()->Free(ptr2);
  allocator.root()->Free(ptr3);
  allocator.root()->Free(ptr4);
  allocator.root()->Free(ptr5);
  allocator.root()->Free(ptr6);
  EXPECT_TRUE(slot_span->in_empty_cache());
  EXPECT_TRUE(slot_span2->in_empty_cache());
  EXPECT_TRUE(slot_span2->get_freelist_head());
  EXPECT_EQ(0u, slot_span2->num_allocated_slots);

  // Size that's just above half a page.
  size_t non_dividing_size =
      SystemPageSize() / 2 + 1 - ExtraAllocSize(allocator);
  bucket_index = SizeToIndex(non_dividing_size + ExtraAllocSize(allocator));
  bucket = &allocator.root()->buckets[bucket_index];
  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);

  ptr = allocator.root()->Alloc(non_dividing_size, type_name);
  EXPECT_TRUE(ptr);

  slot_span = SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr));
  total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      bucket->slot_size;

  EXPECT_FALSE(slot_span->get_freelist_head());
  EXPECT_EQ(1u, slot_span->num_allocated_slots);
  EXPECT_EQ(total_slots - 1, slot_span->num_unprovisioned_slots);

  ptr2 = allocator.root()->Alloc(non_dividing_size, type_name);
  EXPECT_TRUE(ptr2);
  EXPECT_TRUE(slot_span->get_freelist_head());
  EXPECT_EQ(2u, slot_span->num_allocated_slots);
  // 2 slots got provisioned: the first one fills the rest of the first (already
  // provision page) and exceeds it by just a tad, thus leading to provisioning
  // a new page, and the second one fully fits within that new page.
  EXPECT_EQ(total_slots - 3, slot_span->num_unprovisioned_slots);

  ptr3 = allocator.root()->Alloc(non_dividing_size, type_name);
  EXPECT_TRUE(ptr3);
  EXPECT_FALSE(slot_span->get_freelist_head());
  EXPECT_EQ(3u, slot_span->num_allocated_slots);
  EXPECT_EQ(total_slots - 3, slot_span->num_unprovisioned_slots);

  allocator.root()->Free(ptr);
  allocator.root()->Free(ptr2);
  allocator.root()->Free(ptr3);
  EXPECT_TRUE(slot_span->in_empty_cache());
  EXPECT_TRUE(slot_span2->get_freelist_head());
  EXPECT_EQ(0u, slot_span2->num_allocated_slots);

  // And test a couple of sizes that do not cross SystemPageSize() with a
  // single allocation.
  size_t medium_size = (SystemPageSize() / 2) - ExtraAllocSize(allocator);
  bucket_index = SizeToIndex(medium_size + ExtraAllocSize(allocator));
  bucket = &allocator.root()->buckets[bucket_index];
  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);

  ptr = allocator.root()->Alloc(medium_size, type_name);
  EXPECT_TRUE(ptr);
  slot_span = SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr));
  EXPECT_EQ(1u, slot_span->num_allocated_slots);
  total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      (medium_size + ExtraAllocSize(allocator));
  size_t first_slot_span_slots =
      SystemPageSize() / (medium_size + ExtraAllocSize(allocator));
  EXPECT_EQ(2u, first_slot_span_slots);
  EXPECT_EQ(total_slots - first_slot_span_slots,
            slot_span->num_unprovisioned_slots);

  allocator.root()->Free(ptr);

  size_t small_size = (SystemPageSize() / 4) - ExtraAllocSize(allocator);
  bucket_index = SizeToIndex(small_size + ExtraAllocSize(allocator));
  bucket = &allocator.root()->buckets[bucket_index];
  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);

  ptr = allocator.root()->Alloc(small_size, type_name);
  EXPECT_TRUE(ptr);
  slot_span = SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr));
  EXPECT_EQ(1u, slot_span->num_allocated_slots);
  total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      (small_size + ExtraAllocSize(allocator));
  first_slot_span_slots =
      SystemPageSize() / (small_size + ExtraAllocSize(allocator));
  EXPECT_EQ(total_slots - first_slot_span_slots,
            slot_span->num_unprovisioned_slots);

  allocator.root()->Free(ptr);
  EXPECT_TRUE(slot_span->get_freelist_head());
  EXPECT_EQ(0u, slot_span->num_allocated_slots);

  ASSERT_LT(ExtraAllocSize(allocator), 64u);
  size_t very_small_size = (ExtraAllocSize(allocator) <= 32)
                               ? (32 - ExtraAllocSize(allocator))
                               : (64 - ExtraAllocSize(allocator));
  size_t very_small_adjusted_size =
      allocator.root()->AdjustSize0IfNeeded(very_small_size);
  bucket_index =
      SizeToIndex(very_small_adjusted_size + ExtraAllocSize(allocator));
  bucket = &allocator.root()->buckets[bucket_index];
  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);

  ptr = allocator.root()->Alloc(very_small_size, type_name);
  EXPECT_TRUE(ptr);
  slot_span = SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr));
  EXPECT_EQ(1u, slot_span->num_allocated_slots);
  size_t very_small_actual_size = allocator.root()->GetUsableSize(ptr);
  total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      (very_small_actual_size + ExtraAllocSize(allocator));
  first_slot_span_slots =
      SystemPageSize() / (very_small_actual_size + ExtraAllocSize(allocator));
  EXPECT_EQ(total_slots - first_slot_span_slots,
            slot_span->num_unprovisioned_slots);

  allocator.root()->Free(ptr);
  EXPECT_TRUE(slot_span->get_freelist_head());
  EXPECT_EQ(0u, slot_span->num_allocated_slots);

  // And try an allocation size (against the generic allocator) that is
  // larger than a system page.
  size_t page_and_a_half_size =
      (SystemPageSize() + (SystemPageSize() / 2)) - ExtraAllocSize(allocator);
  ptr = allocator.root()->Alloc(page_and_a_half_size, type_name);
  EXPECT_TRUE(ptr);
  slot_span = SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr));
  EXPECT_EQ(1u, slot_span->num_allocated_slots);
  // Only the first slot was provisioned, and that's the one that was just
  // allocated so the free list is empty.
  EXPECT_TRUE(!slot_span->get_freelist_head());
  total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      (page_and_a_half_size + ExtraAllocSize(allocator));
  EXPECT_EQ(total_slots - 1, slot_span->num_unprovisioned_slots);
  ptr2 = allocator.root()->Alloc(page_and_a_half_size, type_name);
  EXPECT_TRUE(ptr);
  slot_span = SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr));
  EXPECT_EQ(2u, slot_span->num_allocated_slots);
  // As above, only one slot was provisioned.
  EXPECT_TRUE(!slot_span->get_freelist_head());
  EXPECT_EQ(total_slots - 2, slot_span->num_unprovisioned_slots);
  allocator.root()->Free(ptr);
  allocator.root()->Free(ptr2);

  // And then make sure than exactly the page size only faults one page.
  size_t page_size = SystemPageSize() - ExtraAllocSize(allocator);
  ptr = allocator.root()->Alloc(page_size, type_name);
  EXPECT_TRUE(ptr);
  slot_span = SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr));
  EXPECT_EQ(1u, slot_span->num_allocated_slots);
  EXPECT_TRUE(slot_span->get_freelist_head());
  total_slots =
      (slot_span->bucket->num_system_pages_per_slot_span * SystemPageSize()) /
      (page_size + ExtraAllocSize(allocator));
  EXPECT_EQ(total_slots - 2, slot_span->num_unprovisioned_slots);
  allocator.root()->Free(ptr);
}

// Test some of the fragmentation-resistant properties of the allocator.
TEST_P(PartitionAllocTest, SlotSpanRefilling) {
  PartitionRoot::Bucket* bucket =
      &allocator.root()->buckets[test_bucket_index_];

  // Grab two full slot spans and a non-full slot span.
  auto* slot_span1 = GetFullSlotSpan(kTestAllocSize);
  auto* slot_span2 = GetFullSlotSpan(kTestAllocSize);
  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  EXPECT_NE(slot_span1, bucket->active_slot_spans_head);
  EXPECT_NE(slot_span2, bucket->active_slot_spans_head);
  auto* slot_span =
      SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr));
  EXPECT_EQ(1u, slot_span->num_allocated_slots);

  // Work out a pointer into slot_span2 and free it; and then slot_span1 and
  // free it.
  void* ptr2 = allocator.root()->SlotStartToObject(
      SlotSpan::ToSlotSpanStart(slot_span1));
  allocator.root()->Free(ptr2);
  ptr2 = allocator.root()->SlotStartToObject(
      SlotSpan::ToSlotSpanStart(slot_span2));
  allocator.root()->Free(ptr2);

  // If we perform two allocations from the same bucket now, we expect to
  // refill both the nearly full slot spans.
  std::ignore = allocator.root()->Alloc(kTestAllocSize, type_name);
  std::ignore = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_EQ(1u, slot_span->num_allocated_slots);

  FreeFullSlotSpan(allocator.root(), slot_span2);
  FreeFullSlotSpan(allocator.root(), slot_span1);
  allocator.root()->Free(ptr);
}

// Basic tests to ensure that allocations work for partial page buckets.
TEST_P(PartitionAllocTest, PartialPages) {
  // Find a size that is backed by a partial partition page.
  size_t size = sizeof(void*);
  size_t bucket_index;

  PartitionRoot::Bucket* bucket = nullptr;
  constexpr size_t kMaxSize = 4000u;
  while (size < kMaxSize) {
    bucket_index = SizeToIndex(size + ExtraAllocSize(allocator));
    bucket = &allocator.root()->buckets[bucket_index];
    if (bucket->num_system_pages_per_slot_span %
        NumSystemPagesPerPartitionPage()) {
      break;
    }
    size += sizeof(void*);
  }
  EXPECT_LT(size, kMaxSize);

  auto* slot_span1 = GetFullSlotSpan(size);
  auto* slot_span2 = GetFullSlotSpan(size);
  FreeFullSlotSpan(allocator.root(), slot_span2);
  FreeFullSlotSpan(allocator.root(), slot_span1);
}

// Test correct handling if our mapping collides with another.
TEST_P(PartitionAllocTest, MappingCollision) {
  size_t num_pages_per_slot_span = GetNumPagesPerSlotSpan(kTestAllocSize);
  // The -2 is because the first and last partition pages in a super page are
  // guard pages. We also discount the partition pages used for the tag bitmap.
  size_t num_slot_span_needed =
      (NumPartitionPagesPerSuperPage() - 2 -
       partition_alloc::internal::NumPartitionPagesPerFreeSlotBitmap()) /
      num_pages_per_slot_span;
  size_t num_partition_pages_needed =
      num_slot_span_needed * num_pages_per_slot_span;

  auto first_super_page_pages =
      std::make_unique<const SlotSpan*[]>(num_partition_pages_needed);
  auto second_super_page_pages =
      std::make_unique<const SlotSpan*[]>(num_partition_pages_needed);

  size_t i;
  for (i = 0; i < num_partition_pages_needed; ++i) {
    first_super_page_pages[i] = GetFullSlotSpan(kTestAllocSize);
  }

  uintptr_t slot_span_start =
      SlotSpan::ToSlotSpanStart(first_super_page_pages[0]);
  EXPECT_EQ(PartitionPageSize() +
                partition_alloc::internal::ReservedFreeSlotBitmapSize(),
            slot_span_start & kSuperPageOffsetMask);
  uintptr_t super_page =
      slot_span_start - PartitionPageSize() -
      partition_alloc::internal::ReservedFreeSlotBitmapSize();
  // Map a single system page either side of the mapping for our allocations,
  // with the goal of tripping up alignment of the next mapping.
  uintptr_t map1 =
      AllocPages(super_page - PageAllocationGranularity(),
                 PageAllocationGranularity(), PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kInaccessible),
                 PageTag::kPartitionAlloc);
  EXPECT_TRUE(map1);
  uintptr_t map2 =
      AllocPages(super_page + kSuperPageSize, PageAllocationGranularity(),
                 PageAllocationGranularity(),
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kInaccessible),
                 PageTag::kPartitionAlloc);
  EXPECT_TRUE(map2);

  for (i = 0; i < num_partition_pages_needed; ++i) {
    second_super_page_pages[i] = GetFullSlotSpan(kTestAllocSize);
  }

  FreePages(map1, PageAllocationGranularity());
  FreePages(map2, PageAllocationGranularity());

  super_page = SlotSpan::ToSlotSpanStart(second_super_page_pages[0]);
  EXPECT_EQ(PartitionPageSize() +
                partition_alloc::internal::ReservedFreeSlotBitmapSize(),
            super_page & kSuperPageOffsetMask);
  super_page -= PartitionPageSize() +
                partition_alloc::internal::ReservedFreeSlotBitmapSize();
  // Map a single system page either side of the mapping for our allocations,
  // with the goal of tripping up alignment of the next mapping.
  map1 = AllocPages(super_page - PageAllocationGranularity(),
                    PageAllocationGranularity(), PageAllocationGranularity(),
                    PageAccessibilityConfiguration(
                        PageAccessibilityConfiguration::kReadWriteTagged),
                    PageTag::kPartitionAlloc);
  EXPECT_TRUE(map1);
  map2 = AllocPages(super_page + kSuperPageSize, PageAllocationGranularity(),
                    PageAllocationGranularity(),
                    PageAccessibilityConfiguration(
                        PageAccessibilityConfiguration::kReadWriteTagged),
                    PageTag::kPartitionAlloc);
  EXPECT_TRUE(map2);
  EXPECT_TRUE(TrySetSystemPagesAccess(
      map1, PageAllocationGranularity(),
      PageAccessibilityConfiguration(
          PageAccessibilityConfiguration::kInaccessible)));
  EXPECT_TRUE(TrySetSystemPagesAccess(
      map2, PageAllocationGranularity(),
      PageAccessibilityConfiguration(
          PageAccessibilityConfiguration::kInaccessible)));

  auto* slot_span_in_third_super_page = GetFullSlotSpan(kTestAllocSize);
  FreePages(map1, PageAllocationGranularity());
  FreePages(map2, PageAllocationGranularity());

  EXPECT_EQ(0u, SlotSpan::ToSlotSpanStart(slot_span_in_third_super_page) &
                    PartitionPageOffsetMask());

  // And make sure we really did get a page in a new superpage.
  EXPECT_NE(
      SlotSpan::ToSlotSpanStart(first_super_page_pages[0]) & kSuperPageBaseMask,
      SlotSpan::ToSlotSpanStart(slot_span_in_third_super_page) &
          kSuperPageBaseMask);
  EXPECT_NE(SlotSpan::ToSlotSpanStart(second_super_page_pages[0]) &
                kSuperPageBaseMask,
            SlotSpan::ToSlotSpanStart(slot_span_in_third_super_page) &
                kSuperPageBaseMask);

  FreeFullSlotSpan(allocator.root(), slot_span_in_third_super_page);
  for (i = 0; i < num_partition_pages_needed; ++i) {
    FreeFullSlotSpan(allocator.root(), first_super_page_pages[i]);
    FreeFullSlotSpan(allocator.root(), second_super_page_pages[i]);
  }
}

// Tests that slot spans in the free slot span cache do get freed as
// appropriate.
TEST_P(PartitionAllocTest, FreeCache) {
  EXPECT_EQ(0U, allocator.root()->get_total_size_of_committed_pages());

  size_t big_size = 1000 - ExtraAllocSize(allocator);
  size_t bucket_index = SizeToIndex(big_size + ExtraAllocSize(allocator));
  PartitionBucket* bucket = &allocator.root()->buckets[bucket_index];

  void* ptr = allocator.root()->Alloc(big_size, type_name);
  EXPECT_TRUE(ptr);
  auto* slot_span =
      SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr));
  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);
  EXPECT_EQ(1u, slot_span->num_allocated_slots);
  // Lazy commit commits only needed pages.
  size_t expected_committed_size =
      kUseLazyCommit ? SystemPageSize() : PartitionPageSize();
  EXPECT_EQ(expected_committed_size,
            allocator.root()->get_total_size_of_committed_pages());
  allocator.root()->Free(ptr);
  EXPECT_EQ(0u, slot_span->num_allocated_slots);
  EXPECT_TRUE(slot_span->in_empty_cache());
  EXPECT_TRUE(slot_span->get_freelist_head());

  ClearEmptySlotSpanCache();

  // Flushing the cache should have really freed the unused slot spans.
  EXPECT_FALSE(slot_span->get_freelist_head());
  EXPECT_FALSE(slot_span->in_empty_cache());
  EXPECT_EQ(0u, slot_span->num_allocated_slots);
  EXPECT_EQ(0u, allocator.root()->get_total_size_of_committed_pages());

  // Check that an allocation works ok whilst in this state (a free'd slot span
  // as the active slot spans head).
  ptr = allocator.root()->Alloc(big_size, type_name);
  EXPECT_FALSE(bucket->empty_slot_spans_head);
  allocator.root()->Free(ptr);

  // Also check that a slot span that is bouncing immediately between empty and
  // used does not get freed.
  for (size_t i = 0; i < kMaxEmptySlotSpanRingSize * 2; ++i) {
    ptr = allocator.root()->Alloc(big_size, type_name);
    EXPECT_TRUE(slot_span->get_freelist_head());
    allocator.root()->Free(ptr);
    EXPECT_TRUE(slot_span->get_freelist_head());
  }
  EXPECT_EQ(expected_committed_size,
            allocator.root()->get_total_size_of_committed_pages());
}

// Tests for a bug we had with losing references to free slot spans.
TEST_P(PartitionAllocTest, LostFreeSlotSpansBug) {
  size_t size = PartitionPageSize() - ExtraAllocSize(allocator);

  void* ptr = allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr);
  void* ptr2 = allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr2);

  const SlotSpan* slot_span =
      SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr));
  const SlotSpan* slot_span2 =
      SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr2));
  PartitionBucket* bucket = slot_span->bucket;

  EXPECT_EQ(nullptr, bucket->empty_slot_spans_head);
  EXPECT_EQ(1u, slot_span->num_allocated_slots);
  EXPECT_EQ(1u, slot_span2->num_allocated_slots);
  EXPECT_TRUE(slot_span->is_full());
  EXPECT_TRUE(slot_span2->is_full());
  // The first span was kicked out from the active list, but the second one
  // wasn't.
  EXPECT_TRUE(slot_span->marked_full);
  EXPECT_FALSE(slot_span2->marked_full);

  allocator.root()->Free(ptr);
  allocator.root()->Free(ptr2);

  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_TRUE(bucket->empty_slot_spans_head->next_slot_span);
  EXPECT_EQ(0u, slot_span->num_allocated_slots);
  EXPECT_EQ(0u, slot_span2->num_allocated_slots);
  EXPECT_FALSE(slot_span->is_full());
  EXPECT_FALSE(slot_span->is_full());
  EXPECT_FALSE(slot_span->marked_full);
  EXPECT_FALSE(slot_span2->marked_full);
  EXPECT_TRUE(slot_span->get_freelist_head());
  EXPECT_TRUE(slot_span2->get_freelist_head());

  ClearEmptySlotSpanCache();

  EXPECT_FALSE(slot_span->get_freelist_head());
  EXPECT_FALSE(slot_span2->get_freelist_head());

  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_TRUE(bucket->empty_slot_spans_head->next_slot_span);
  EXPECT_EQ(SlotSpan::get_sentinel_slot_span(), bucket->active_slot_spans_head);

  // At this moment, we have two decommitted slot spans, on the empty list.
  ptr = allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr);
  allocator.root()->Free(ptr);

  EXPECT_EQ(SlotSpan::get_sentinel_slot_span(), bucket->active_slot_spans_head);
  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_TRUE(bucket->decommitted_slot_spans_head);

  ClearEmptySlotSpanCache();

  // We're now set up to trigger a historical bug by scanning over the active
  // slot spans list. The current code gets into a different state, but we'll
  // keep the test as being an interesting corner case.
  ptr = allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr);
  allocator.root()->Free(ptr);

  EXPECT_TRUE(bucket->is_valid());
  EXPECT_TRUE(bucket->empty_slot_spans_head);
  EXPECT_TRUE(bucket->decommitted_slot_spans_head);
}

#if PA_USE_DEATH_TESTS()

// Unit tests that check if an allocation fails in "return null" mode,
// repeating it doesn't crash, and still returns null. The tests need to
// stress memory subsystem limits to do so, hence they try to allocate
// 6 GB of memory, each with a different per-allocation block sizes.
//
// On 64-bit systems we need to restrict the address space to force allocation
// failure, so these tests run only on POSIX systems that provide setrlimit(),
// and use it to limit address space to 6GB.
//
// Disable these tests on Android because, due to the allocation-heavy behavior,
// they tend to get OOM-killed rather than pass.
//
// Disable these test on Windows, since they run slower, so tend to timout and
// cause flake.
#if !PA_BUILDFLAG(IS_WIN) &&                                         \
        (!PA_BUILDFLAG(PA_ARCH_CPU_64_BITS) ||                       \
         (PA_BUILDFLAG(IS_POSIX) &&                                  \
          !(PA_BUILDFLAG(IS_APPLE) || PA_BUILDFLAG(IS_ANDROID)))) || \
    PA_BUILDFLAG(IS_FUCHSIA)
#define MAYBE_RepeatedAllocReturnNullDirect RepeatedAllocReturnNullDirect
#define MAYBE_RepeatedReallocReturnNullDirect RepeatedReallocReturnNullDirect
#else
#define MAYBE_RepeatedAllocReturnNullDirect \
  DISABLED_RepeatedAllocReturnNullDirect
#define MAYBE_RepeatedReallocReturnNullDirect \
  DISABLED_RepeatedReallocReturnNullDirect
#endif

// The following four tests wrap a called function in an expect death statement
// to perform their test, because they are non-hermetic. Specifically they are
// going to attempt to exhaust the allocatable memory, which leaves the
// allocator in a bad global state.
// Performing them as death tests causes them to be forked into their own
// process, so they won't pollute other tests.
//
// These tests are *very* slow when PA_BUILDFLAG(DCHECKS_ARE_ON), because they
// memset() many GiB of data (see crbug.com/1168168).
// TODO(lizeb): make these tests faster.
TEST_P(PartitionAllocDeathTest, MAYBE_RepeatedAllocReturnNullDirect) {
  // A direct-mapped allocation size.
  size_t direct_map_size = 32 * 1024 * 1024;
  ASSERT_GT(direct_map_size, kMaxBucketed);
  EXPECT_DEATH(DoReturnNullTest(direct_map_size, kPartitionAlloc),
               "Passed DoReturnNullTest");
}

// Repeating above test with Realloc
TEST_P(PartitionAllocDeathTest, MAYBE_RepeatedReallocReturnNullDirect) {
  size_t direct_map_size = 32 * 1024 * 1024;
  ASSERT_GT(direct_map_size, kMaxBucketed);
  EXPECT_DEATH(DoReturnNullTest(direct_map_size, kPartitionRealloc),
               "Passed DoReturnNullTest");
}

// TODO(crbug.com/40855174) re-enable the tests below, once the allocator
// actually returns nullptr for non direct-mapped allocations.
// When doing so, they will need to be made MAYBE_ like those above.
//
// Tests "return null" with a 512 kB block size.
TEST_P(PartitionAllocDeathTest, DISABLED_RepeatedAllocReturnNull) {
  // A single-slot but non-direct-mapped allocation size.
  size_t single_slot_size = 512 * 1024;
  ASSERT_GT(single_slot_size, MaxRegularSlotSpanSize());
  ASSERT_LE(single_slot_size, kMaxBucketed);
  EXPECT_DEATH(DoReturnNullTest(single_slot_size, kPartitionAlloc),
               "Passed DoReturnNullTest");
}

// Repeating above test with Realloc.
TEST_P(PartitionAllocDeathTest, DISABLED_RepeatedReallocReturnNull) {
  size_t single_slot_size = 512 * 1024;
  ASSERT_GT(single_slot_size, MaxRegularSlotSpanSize());
  ASSERT_LE(single_slot_size, kMaxBucketed);
  EXPECT_DEATH(DoReturnNullTest(single_slot_size, kPartitionRealloc),
               "Passed DoReturnNullTest");
}

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
// Check that Arm's memory tagging extension (MTE) is correctly protecting
// freed pointers. Writes to a free pointer should result in a crash.
TEST_P(PartitionAllocDeathTest, MTEProtectsFreedPtr) {
  base::CPU cpu;
  if (!cpu.has_mte()) {
    // This test won't pass on systems without MTE.
    GTEST_SKIP();
  }

  constexpr uint64_t kCookie = 0x1234567890ABCDEF;
  constexpr uint64_t kQuarantined = 0xEFEFEFEFEFEFEFEF;

  // Make an arbitrary-sized small allocation.
  size_t alloc_size = 64 - ExtraAllocSize(allocator);
  uint64_t* ptr =
      static_cast<uint64_t*>(allocator.root()->Alloc(alloc_size, type_name));
  EXPECT_TRUE(ptr);

  // Check that the allocation's writable.
  *ptr = kCookie;

  // Invalidate ptr by freeing it.
  allocator.root()->Free(ptr);

  // Writing to ptr after free() should crash
  EXPECT_EXIT(
      {
        // Should be in synchronous MTE mode for running this test.
        *ptr = kQuarantined;
      },
      testing::KilledBySignal(SIGSEGV), "");
}

// Check that accessing freed memory will not trigger a crash in
// SuspendTagCheckingScope.
TEST_P(PartitionAllocDeathTest, SuspendTagCheckingScope) {
  base::CPU cpu;
  if (!cpu.has_mte()) {
    // This test won't pass on systems without MTE.
    GTEST_SKIP();
  }

  constexpr uint64_t kQuarantined = 0xEFEFEFEFEFEFEFEF;

  // Make an arbitrary-sized small allocation.
  size_t alloc_size = 64 - ExtraAllocSize(allocator);
  uint64_t* ptr =
      static_cast<uint64_t*>(allocator.root()->Alloc(alloc_size, type_name));
  EXPECT_TRUE(ptr);

  // Invalidate ptr by freeing it.
  allocator.root()->Free(ptr);

  // Writing to ptr after free() should usually crash but not in
  // |SuspendTagCheckingScope|.
  {
    partition_alloc::SuspendTagCheckingScope scope;
    *ptr = kQuarantined;
  }
  // Check that access after the scope will crash.
  EXPECT_EXIT(
      {
        // Should be in synchronous MTE mode for running this test.
        *ptr = kQuarantined;
      },
      testing::KilledBySignal(SIGSEGV), "");
}
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)

// Make sure that malloc(-1) dies.
// In the past, we had an integer overflow that would alias malloc(-1) to
// malloc(0), which is not good.
TEST_P(PartitionAllocDeathTest, LargeAllocs) {
  // Largest alloc.
  EXPECT_DEATH(allocator.root()->Alloc(static_cast<size_t>(-1), type_name), "");
  // And the smallest allocation we expect to die.
  // TODO(bartekn): Separate into its own test, as it wouldn't run (same below).
  EXPECT_DEATH(allocator.root()->Alloc(MaxDirectMapped() + 1, type_name), "");
}

// These tests don't work deterministically when BRP is enabled on certain
// architectures. On Free(), BRP's ref-count inside in-slot metadata gets
// overwritten by an encoded freelist pointer. On little-endian 64-bit
// architectures, this happens to be always an even number, which will trigger
// BRP's own CHECK (sic!). On other architectures, it's likely to be an odd
// number >1, which will fool BRP into thinking the memory isn't freed and still
// referenced, thus making it quarantine it and return early, before
// PA_CHECK(slot_start != freelist_head) is reached.
#if !PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) || \
    (PA_BUILDFLAG(HAS_64_BIT_POINTERS) &&           \
     PA_BUILDFLAG(PA_ARCH_CPU_LITTLE_ENDIAN))

// Check that our immediate double-free detection works.
TEST_P(PartitionAllocDeathTest, ImmediateDoubleFree) {
  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  allocator.root()->Free(ptr);
  EXPECT_DEATH(allocator.root()->Free(ptr), "");
}

// As above, but when this isn't the only slot in the span.
TEST_P(PartitionAllocDeathTest, ImmediateDoubleFree2ndSlot) {
  void* ptr0 = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr0);
  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  allocator.root()->Free(ptr);
  EXPECT_DEATH(allocator.root()->Free(ptr), "");
  allocator.root()->Free(ptr0);
}

// Check that our double-free detection based on |num_allocated_slots| not going
// below 0 works.
//
// Unlike in ImmediateDoubleFree test, we can't have a 2ndSlot version, as this
// protection wouldn't work when there is another slot present in the span. It
// will prevent |num_allocated_slots| from going below 0.
TEST_P(PartitionAllocDeathTest, NumAllocatedSlotsDoubleFree) {
  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  void* ptr2 = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr2);
  allocator.root()->Free(ptr);
  allocator.root()->Free(ptr2);
  // This is not an immediate double-free so our immediate detection won't
  // fire. However, it does take |num_allocated_slots| to -1, which is illegal
  // and should be trapped.
  EXPECT_DEATH(allocator.root()->Free(ptr), "");
}

#endif  // !PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) || \
        // (PA_BUILDFLAG(HAS_64_BIT_POINTERS) && PA_BUILDFLAG(PA_ARCH_CPU_LITTLE_ENDIAN))

// Check that guard pages are present where expected.
TEST_P(PartitionAllocDeathTest, DirectMapGuardPages) {
  const size_t kSizes[] = {
      kMaxBucketed + ExtraAllocSize(allocator) + 1,
      kMaxBucketed + SystemPageSize(), kMaxBucketed + PartitionPageSize(),
      partition_alloc::internal::base::bits::AlignUp(
          kMaxBucketed + kSuperPageSize, kSuperPageSize) -
          PartitionRoot::GetDirectMapMetadataAndGuardPagesSize()};
  for (size_t size : kSizes) {
    ASSERT_GT(size, kMaxBucketed);
    size -= ExtraAllocSize(allocator);
    EXPECT_GT(size, kMaxBucketed)
        << "allocation not large enough for direct allocation";
    void* ptr = allocator.root()->Alloc(size, type_name);

    EXPECT_TRUE(ptr);
    char* char_ptr = static_cast<char*>(ptr) - kPointerOffset;

    EXPECT_DEATH(*(char_ptr - 1) = 'A', "");
    EXPECT_DEATH(*(char_ptr + partition_alloc::internal::base::bits::AlignUp(
                                  size, SystemPageSize())) = 'A',
                 "");

    allocator.root()->Free(ptr);
  }
}

// These tests rely on precise layout. They handle cookie, not in-slot metadata.
#if !PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) && \
    PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)

TEST_P(PartitionAllocDeathTest, UseAfterFreeDetection) {
  base::CPU cpu;
  void* data = allocator.root()->Alloc(100);
  allocator.root()->Free(data);

  // use after free, not crashing here, but the next allocation should crash,
  // since we corrupted the freelist.
  memset(data, 0x42, 100);
  EXPECT_DEATH(allocator.root()->Alloc(100), "");
}

TEST_P(PartitionAllocDeathTest, FreelistCorruption) {
  base::CPU cpu;
  const size_t alloc_size = 2 * sizeof(void*);
  void** fake_freelist_entry =
      static_cast<void**>(allocator.root()->Alloc(alloc_size));
  fake_freelist_entry[0] = nullptr;
  fake_freelist_entry[1] = nullptr;

  void** uaf_data = static_cast<void**>(allocator.root()->Alloc(alloc_size));
  allocator.root()->Free(uaf_data);
  // Try to confuse the allocator. This is still easy to circumvent willingly,
  // "just" need to set uaf_data[1] to ~uaf_data[0].
  void* previous_uaf_data = uaf_data[0];
  uaf_data[0] = fake_freelist_entry;
  EXPECT_DEATH(allocator.root()->Alloc(alloc_size), "");

  // Restore the freelist entry value, otherwise freelist corruption is detected
  // in TearDown(), crashing this process.
  uaf_data[0] = previous_uaf_data;

  allocator.root()->Free(fake_freelist_entry);
}

// With PA_BUILDFLAG(USE_PARTITION_COOKIE), cookie already handles off-by-one
// detection.
#if !PA_BUILDFLAG(USE_PARTITION_COOKIE)
TEST_P(PartitionAllocDeathTest, OffByOneDetection) {
  base::CPU cpu;
  const size_t alloc_size = 2 * sizeof(void*);
  char* array = static_cast<char*>(allocator.root()->Alloc(alloc_size));
  if (cpu.has_mte()) {
    EXPECT_DEATH(array[alloc_size] = 'A', "");
  } else {
    char previous_value = array[alloc_size];
    // volatile is required to prevent the compiler from getting too clever and
    // eliding the out-of-bounds write. The root cause is that the PA_MALLOC_FN
    // annotation tells the compiler (among other things) that the returned
    // value cannot alias anything.
    *const_cast<volatile char*>(&array[alloc_size]) = 'A';
    // Crash at the next allocation. This assumes that we are touching a new,
    // non-randomized slot span, where the next slot to be handed over to the
    // application directly follows the current one.
    EXPECT_DEATH(allocator.root()->Alloc(alloc_size), "");

    // Restore integrity, otherwise the process will crash in TearDown().
    array[alloc_size] = previous_value;
  }
}

TEST_P(PartitionAllocDeathTest, OffByOneDetectionWithRealisticData) {
  base::CPU cpu;
  const size_t alloc_size = 2 * sizeof(void*);
  void** array = static_cast<void**>(allocator.root()->Alloc(alloc_size));
  char valid;
  if (cpu.has_mte()) {
    EXPECT_DEATH(array[2] = &valid, "");
  } else {
    void* previous_value = array[2];
    // As above, needs volatile to convince the compiler to perform the write.
    *const_cast<void* volatile*>(&array[2]) = &valid;
    // Crash at the next allocation. This assumes that we are touching a new,
    // non-randomized slot span, where the next slot to be handed over to the
    // application directly follows the current one.
    EXPECT_DEATH(allocator.root()->Alloc(alloc_size), "");
    array[2] = previous_value;
  }
}
#endif  // !PA_BUILDFLAG(USE_PARTITION_COOKIE)

#endif  // !PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) &&
        // PA_CONFIG(HAS_FREELIST_SHADOW_ENTRY)

#if PA_BUILDFLAG(USE_PARTITION_COOKIE)
// Similar to PartitionAllocDeathTest/OffByOneDetection, but for cookie.
TEST_P(PartitionAllocDeathTest, OffByOneDetectionByCookie) {
  base::CPU cpu;
  const size_t alloc_size = 2 * sizeof(void*);
  char* array = static_cast<char*>(allocator.root()->Alloc(alloc_size));

  auto* slot_span =
      PartitionRoot::ReadOnlySlotSpanMetadata::FromObjectInnerPtr(array);
  size_t usable_size = allocator.root()->GetSlotUsableSize(slot_span);

  char previous_value = array[usable_size];
  // volatile is required to prevent the compiler from getting too clever and
  // eliding the out-of-bounds write. The root cause is that the PA_MALLOC_FN
  // annotation tells the compiler (among other things) that the returned
  // value cannot alias anything.
  *const_cast<volatile char*>(&array[usable_size]) = 'A';
  // Crash at `free()`, either by cookie check failure or InSlotMetadata
  // corruption.
  EXPECT_DEATH(allocator.root()->Free(array), "");
  // Restore integrity, otherwise the process will crash in TearDown().
  array[usable_size] = previous_value;
  allocator.root()->Free(array);
}

// Similar to PartitionAllocDeathTest/OffByOneDetectionWithRealisticData, but
// for cookie.
TEST_P(PartitionAllocDeathTest, OffByOneDetectionByCookieWithRealisticData) {
  base::CPU cpu;
  const size_t alloc_size = 2 * sizeof(void*);
  void** array = static_cast<void**>(allocator.root()->Alloc(alloc_size));
  char valid;

  auto* slot_span =
      PartitionRoot::ReadOnlySlotSpanMetadata::FromObjectInnerPtr(array);
  size_t usable_size =
      allocator.root()->GetSlotUsableSize(slot_span) / sizeof(void*);

  void* previous_value = array[usable_size];
  // As above, needs volatile to convince the compiler to perform the write.
  *const_cast<void* volatile*>(&array[usable_size]) = &valid;
  // Crash at `free()`, either by cookie check failure or InSlotMetadata
  // corruption.
  EXPECT_DEATH(allocator.root()->Free(array), "");
  // Restore integrity, otherwise the process will crash in TearDown().
  array[usable_size] = previous_value;
  allocator.root()->Free(array);
}
#endif  // PA_BUILDFLAG(USE_PARTITION_COOKIE)

#endif  // PA_USE_DEATH_TESTS()

// Tests that |PartitionDumpStats| and |PartitionDumpStats| run without
// crashing and return non-zero values when memory is allocated.
TEST_P(PartitionAllocTest, DumpMemoryStats) {
  {
    void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
    MockPartitionStatsDumper mock_stats_dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &mock_stats_dumper);
    EXPECT_TRUE(mock_stats_dumper.IsMemoryAllocationRecorded());
    allocator.root()->Free(ptr);
  }

  // This series of tests checks the active -> empty -> decommitted states.
  {
    {
      void* ptr =
          allocator.root()->Alloc(2048 - ExtraAllocSize(allocator), type_name);
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(2048u, stats->bucket_slot_size);
      EXPECT_EQ(2048u, stats->active_bytes);
      EXPECT_EQ(1u, stats->active_count);
      EXPECT_EQ(SystemPageSize(), stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(0u, stats->num_full_slot_spans);
      EXPECT_EQ(1u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);
      allocator.root()->Free(ptr);
    }

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(2048u, stats->bucket_slot_size);
      EXPECT_EQ(0u, stats->active_bytes);
      EXPECT_EQ(0u, stats->active_count);
      EXPECT_EQ(SystemPageSize(), stats->resident_bytes);
      EXPECT_EQ(SystemPageSize(), stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(0u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(1u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);
    }

    // TODO(crbug.com/40520833): Commenting this out causes this test to fail
    // when run singly (--gtest_filter=PartitionAllocTest.DumpMemoryStats), but
    // not when run with the others (--gtest_filter=PartitionAllocTest.*).
    ClearEmptySlotSpanCache();

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(2048u, stats->bucket_slot_size);
      EXPECT_EQ(0u, stats->active_bytes);
      EXPECT_EQ(0u, stats->active_count);
      EXPECT_EQ(0u, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(0u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(1u, stats->num_decommitted_slot_spans);
    }
  }

  // This test checks for correct empty slot span list accounting.
  {
    size_t size = PartitionPageSize() - ExtraAllocSize(allocator);
    void* ptr1 = allocator.root()->Alloc(size, type_name);
    void* ptr2 = allocator.root()->Alloc(size, type_name);
    allocator.root()->Free(ptr1);
    allocator.root()->Free(ptr2);

    ClearEmptySlotSpanCache();

    ptr1 = allocator.root()->Alloc(size, type_name);

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(PartitionPageSize());
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_EQ(PartitionPageSize(), stats->bucket_slot_size);
      EXPECT_EQ(PartitionPageSize(), stats->active_bytes);
      EXPECT_EQ(1u, stats->active_count);
      EXPECT_EQ(PartitionPageSize(), stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(1u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(1u, stats->num_decommitted_slot_spans);
    }
    allocator.root()->Free(ptr1);
  }

  // This test checks for correct direct mapped accounting.
  {
    size_t size_smaller = kMaxBucketed + 1;
    size_t size_bigger = (kMaxBucketed * 2) + 1;
    size_t real_size_smaller =
        (size_smaller + SystemPageOffsetMask()) & SystemPageBaseMask();
    size_t real_size_bigger =
        (size_bigger + SystemPageOffsetMask()) & SystemPageBaseMask();
    void* ptr = allocator.root()->Alloc(size_smaller, type_name);
    void* ptr2 = allocator.root()->Alloc(size_bigger, type_name);

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(real_size_smaller);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_TRUE(stats->is_direct_map);
      EXPECT_EQ(real_size_smaller, stats->bucket_slot_size);
      EXPECT_EQ(real_size_smaller, stats->active_bytes);
      EXPECT_EQ(1u, stats->active_count);
      EXPECT_EQ(real_size_smaller, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(1u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);

      stats = dumper.GetBucketStats(real_size_bigger);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_TRUE(stats->is_direct_map);
      EXPECT_EQ(real_size_bigger, stats->bucket_slot_size);
      EXPECT_EQ(real_size_bigger, stats->active_bytes);
      EXPECT_EQ(1u, stats->active_count);
      EXPECT_EQ(real_size_bigger, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->discardable_bytes);
      EXPECT_EQ(1u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);
    }

    allocator.root()->Free(ptr2);
    allocator.root()->Free(ptr);

    // Whilst we're here, allocate again and free with different ordering to
    // give a workout to our linked list code.
    ptr = allocator.root()->Alloc(size_smaller, type_name);
    ptr2 = allocator.root()->Alloc(size_bigger, type_name);
    allocator.root()->Free(ptr);
    allocator.root()->Free(ptr2);
  }

  // This test checks large-but-not-quite-direct allocations.
  {
    size_t requested_size = 16 * SystemPageSize() + 1;
    void* ptr = allocator.root()->Alloc(requested_size, type_name);

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      size_t slot_size = SizeToBucketSize(requested_size);
      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(slot_size);
      ASSERT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_FALSE(stats->is_direct_map);
      EXPECT_EQ(slot_size, stats->bucket_slot_size);
      EXPECT_EQ(requested_size + ExtraAllocSize(allocator),
                stats->active_bytes);
      EXPECT_EQ(1u, stats->active_count);
      EXPECT_EQ(slot_size, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(
          base::bits::AlignDown(slot_size - requested_size, SystemPageSize()),
          stats->discardable_bytes);
      EXPECT_EQ(1u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);
    }

    allocator.root()->Free(ptr);

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

      size_t slot_size = SizeToBucketSize(requested_size);
      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(slot_size);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_FALSE(stats->is_direct_map);
      EXPECT_EQ(slot_size, stats->bucket_slot_size);
      EXPECT_EQ(0u, stats->active_bytes);
      EXPECT_EQ(0u, stats->active_count);
      EXPECT_EQ(slot_size, stats->resident_bytes);
      EXPECT_EQ(slot_size, stats->decommittable_bytes);
      EXPECT_EQ(0u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(1u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);
    }

    requested_size = 17 * SystemPageSize() + 1;
    void* ptr2 = allocator.root()->Alloc(requested_size, type_name);
    EXPECT_EQ(ptr, ptr2);

    {
      MockPartitionStatsDumper dumper;
      allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                  &dumper);
      EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

      size_t slot_size = SizeToBucketSize(requested_size);
      const PartitionBucketMemoryStats* stats =
          dumper.GetBucketStats(slot_size);
      EXPECT_TRUE(stats);
      EXPECT_TRUE(stats->is_valid);
      EXPECT_FALSE(stats->is_direct_map);
      EXPECT_EQ(slot_size, stats->bucket_slot_size);
      EXPECT_EQ(requested_size + ExtraAllocSize(allocator),
                stats->active_bytes);
      EXPECT_EQ(1u, stats->active_count);
      EXPECT_EQ(slot_size, stats->resident_bytes);
      EXPECT_EQ(0u, stats->decommittable_bytes);
      EXPECT_EQ(
          base::bits::AlignDown(slot_size - requested_size, SystemPageSize()),
          stats->discardable_bytes);
      EXPECT_EQ(1u, stats->num_full_slot_spans);
      EXPECT_EQ(0u, stats->num_active_slot_spans);
      EXPECT_EQ(0u, stats->num_empty_slot_spans);
      EXPECT_EQ(0u, stats->num_decommitted_slot_spans);
    }

    allocator.root()->Free(ptr2);
  }
}

// Tests the API to purge freeable memory.
TEST_P(PartitionAllocTest, Purge) {
  char* ptr = static_cast<char*>(
      allocator.root()->Alloc(2048 - ExtraAllocSize(allocator), type_name));
  allocator.root()->Free(ptr);
  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(SystemPageSize(), stats->decommittable_bytes);
    EXPECT_EQ(SystemPageSize(), stats->resident_bytes);
  }
  allocator.root()->PurgeMemory(PurgeFlags::kDecommitEmptySlotSpans);
  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_FALSE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats = dumper.GetBucketStats(2048);
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
    EXPECT_EQ(0u, stats->resident_bytes);
  }
  // Calling purge again here is a good way of testing we didn't mess up the
  // state of the free cache ring.
  allocator.root()->PurgeMemory(PurgeFlags::kDecommitEmptySlotSpans);

  // A single-slot but non-direct-mapped allocation size.
  size_t single_slot_size = 512 * 1024;
  ASSERT_GT(single_slot_size, MaxRegularSlotSpanSize());
  ASSERT_LE(single_slot_size, kMaxBucketed);
  char* big_ptr =
      static_cast<char*>(allocator.root()->Alloc(single_slot_size, type_name));
  allocator.root()->Free(big_ptr);
  allocator.root()->PurgeMemory(PurgeFlags::kDecommitEmptySlotSpans);

  CHECK_PAGE_IN_CORE(ptr - kPointerOffset, false);
  CHECK_PAGE_IN_CORE(big_ptr - kPointerOffset, false);
}

// Tests that we prefer to allocate into a non-empty partition page over an
// empty one. This is an important aspect of minimizing memory usage for some
// allocation sizes, particularly larger ones.
TEST_P(PartitionAllocTest, PreferActiveOverEmpty) {
  size_t size = (SystemPageSize() * 2) - ExtraAllocSize(allocator);
  // Allocate 3 full slot spans worth of 8192-byte allocations.
  // Each slot span for this size is 16384 bytes, or 1 partition page and 2
  // slots.
  void* ptr1 = allocator.root()->Alloc(size, type_name);
  void* ptr2 = allocator.root()->Alloc(size, type_name);
  void* ptr3 = allocator.root()->Alloc(size, type_name);
  void* ptr4 = allocator.root()->Alloc(size, type_name);
  void* ptr5 = allocator.root()->Alloc(size, type_name);
  void* ptr6 = allocator.root()->Alloc(size, type_name);

  const SlotSpan* slot_span1 =
      SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr1));
  const SlotSpan* slot_span2 =
      SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr3));
  const SlotSpan* slot_span3 =
      SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr6));
  EXPECT_NE(slot_span1, slot_span2);
  EXPECT_NE(slot_span2, slot_span3);
  PartitionBucket* bucket = slot_span1->bucket;
  EXPECT_EQ(slot_span3, bucket->active_slot_spans_head);

  // Free up the 2nd slot in each slot span.
  // This leaves the active list containing 3 slot spans, each with 1 used and 1
  // free slot. The active slot span will be the one containing ptr1.
  allocator.root()->Free(ptr6);
  allocator.root()->Free(ptr4);
  allocator.root()->Free(ptr2);
  EXPECT_EQ(slot_span1, bucket->active_slot_spans_head);

  // Empty the middle slot span in the active list.
  allocator.root()->Free(ptr3);
  EXPECT_EQ(slot_span1, bucket->active_slot_spans_head);

  // Empty the first slot span in the active list -- also the current slot span.
  allocator.root()->Free(ptr1);

  // A good choice here is to re-fill the third slot span since the first two
  // are empty. We used to fail that.
  void* ptr7 = allocator.root()->Alloc(size, type_name);
  PA_EXPECT_PTR_EQ(ptr6, ptr7);
  EXPECT_EQ(slot_span3, bucket->active_slot_spans_head);

  allocator.root()->Free(ptr5);
  allocator.root()->Free(ptr7);
}

// Tests the API to purge discardable memory.
TEST_P(PartitionAllocTest, PurgeDiscardableSecondPage) {
  // Free the second of two 4096 byte allocations and then purge.
  void* ptr1 = allocator.root()->Alloc(
      SystemPageSize() - ExtraAllocSize(allocator), type_name);
  char* ptr2 = static_cast<char*>(allocator.root()->Alloc(
      SystemPageSize() - ExtraAllocSize(allocator), type_name));
  allocator.root()->Free(ptr2);
  const SlotSpan* slot_span =
      SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr1));
  EXPECT_EQ(2u, slot_span->num_unprovisioned_slots);
  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats =
        dumper.GetBucketStats(SystemPageSize());
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
    EXPECT_EQ(SystemPageSize(), stats->discardable_bytes);
    EXPECT_EQ(SystemPageSize(), stats->active_bytes);
    EXPECT_EQ(2 * SystemPageSize(), stats->resident_bytes);
  }
  CHECK_PAGE_IN_CORE(ptr2 - kPointerOffset, true);
  allocator.root()->PurgeMemory(PurgeFlags::kDiscardUnusedSystemPages);
  CHECK_PAGE_IN_CORE(ptr2 - kPointerOffset, false);
  EXPECT_EQ(3u, slot_span->num_unprovisioned_slots);

  allocator.root()->Free(ptr1);
}

TEST_P(PartitionAllocTest, PurgeDiscardableFirstPage) {
  // Free the first of two 4096 byte allocations and then purge.
  char* ptr1 = static_cast<char*>(allocator.root()->Alloc(
      SystemPageSize() - ExtraAllocSize(allocator), type_name));
  void* ptr2 = allocator.root()->Alloc(
      SystemPageSize() - ExtraAllocSize(allocator), type_name);
  allocator.root()->Free(ptr1);
  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats =
        dumper.GetBucketStats(SystemPageSize());
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
#if PA_BUILDFLAG(IS_WIN)
    EXPECT_EQ(0u, stats->discardable_bytes);
#else
    EXPECT_EQ(SystemPageSize(), stats->discardable_bytes);
#endif
    EXPECT_EQ(SystemPageSize(), stats->active_bytes);
    EXPECT_EQ(2 * SystemPageSize(), stats->resident_bytes);
  }
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  allocator.root()->PurgeMemory(PurgeFlags::kDiscardUnusedSystemPages);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, false);

  allocator.root()->Free(ptr2);
}

TEST_P(PartitionAllocTest, PurgeDiscardableNonPageSizedAlloc) {
  const size_t requested_size = 2.5 * SystemPageSize();
  char* ptr1 = static_cast<char*>(allocator.root()->Alloc(
      requested_size - ExtraAllocSize(allocator), type_name));
  void* ptr2 = allocator.root()->Alloc(
      requested_size - ExtraAllocSize(allocator), type_name);
  void* ptr3 = allocator.root()->Alloc(
      requested_size - ExtraAllocSize(allocator), type_name);
  void* ptr4 = allocator.root()->Alloc(
      requested_size - ExtraAllocSize(allocator), type_name);
  memset(ptr1, 'A', requested_size - ExtraAllocSize(allocator));
  memset(ptr2, 'A', requested_size - ExtraAllocSize(allocator));
  allocator.root()->Free(ptr1);
  allocator.root()->Free(ptr2);
  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats =
        dumper.GetBucketStats(requested_size);
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
#if PA_BUILDFLAG(IS_WIN)
    EXPECT_EQ(3 * SystemPageSize(), stats->discardable_bytes);
#else
    EXPECT_EQ(4 * SystemPageSize(), stats->discardable_bytes);
#endif
    EXPECT_EQ(requested_size * 2, stats->active_bytes);
    EXPECT_EQ(10 * SystemPageSize(), stats->resident_bytes);
  }
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 4), true);
  allocator.root()->PurgeMemory(PurgeFlags::kDiscardUnusedSystemPages);
  // Except for Windows, the first page is discardable because the freelist
  // pointer on this page is nullptr. Note that CHECK_PAGE_IN_CORE only executes
  // checks for Linux and ChromeOS, not for Windows.
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, false);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), false);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), false);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 4), false);

  allocator.root()->Free(ptr3);
  allocator.root()->Free(ptr4);
}

TEST_P(PartitionAllocTest, PurgeDiscardableNonPageSizedAllocOnSlotBoundary) {
  const size_t requested_size = 2.5 * SystemPageSize();
  char* ptr1 = static_cast<char*>(allocator.root()->Alloc(
      requested_size - ExtraAllocSize(allocator), type_name));
  void* ptr2 = allocator.root()->Alloc(
      requested_size - ExtraAllocSize(allocator), type_name);
  void* ptr3 = allocator.root()->Alloc(
      requested_size - ExtraAllocSize(allocator), type_name);
  void* ptr4 = allocator.root()->Alloc(
      requested_size - ExtraAllocSize(allocator), type_name);
  memset(ptr1, 'A', requested_size - ExtraAllocSize(allocator));
  memset(ptr2, 'A', requested_size - ExtraAllocSize(allocator));
  allocator.root()->Free(ptr2);
  allocator.root()->Free(ptr1);
  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats =
        dumper.GetBucketStats(requested_size);
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
#if PA_BUILDFLAG(IS_WIN)
    EXPECT_EQ(3 * SystemPageSize(), stats->discardable_bytes);
#else
    EXPECT_EQ(4 * SystemPageSize(), stats->discardable_bytes);
#endif
    EXPECT_EQ(requested_size * 2, stats->active_bytes);
    EXPECT_EQ(10 * SystemPageSize(), stats->resident_bytes);
  }
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 4), true);
  allocator.root()->PurgeMemory(PurgeFlags::kDiscardUnusedSystemPages);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), false);
  // Except for Windows, the third page is discardable because the freelist
  // pointer on this page is nullptr. Note that CHECK_PAGE_IN_CORE only executes
  // checks for Linux and ChromeOS, not for Windows.
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), false);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), false);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 4), false);

  allocator.root()->Free(ptr3);
  allocator.root()->Free(ptr4);
}

TEST_P(PartitionAllocTest, PurgeDiscardableManyPages) {
  // On systems with large pages, use less pages because:
  // 1) There must be a bucket for kFirstAllocPages * SystemPageSize(), and
  // 2) On low-end systems, using too many large pages can OOM during the test
  const bool kHasLargePages = SystemPageSize() > 4096;
  const size_t kFirstAllocPages = kHasLargePages ? 32 : 64;
  const size_t kSecondAllocPages = kHasLargePages ? 31 : 61;

  // Detect case (1) from above.
  PA_DCHECK(kFirstAllocPages * SystemPageSize() < (1UL << kMaxBucketedOrder));

  const size_t kDeltaPages = kFirstAllocPages - kSecondAllocPages;

  {
    ScopedPageAllocation p(allocator, kFirstAllocPages);
    p.TouchAllPages();
  }

  ScopedPageAllocation p(allocator, kSecondAllocPages);

  MockPartitionStatsDumper dumper;
  allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                              &dumper);
  EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

  const PartitionBucketMemoryStats* stats =
      dumper.GetBucketStats(kFirstAllocPages * SystemPageSize());
  EXPECT_TRUE(stats);
  EXPECT_TRUE(stats->is_valid);
  EXPECT_EQ(0u, stats->decommittable_bytes);
  EXPECT_EQ(kDeltaPages * SystemPageSize(), stats->discardable_bytes);
  EXPECT_EQ(kSecondAllocPages * SystemPageSize(), stats->active_bytes);
  EXPECT_EQ(kFirstAllocPages * SystemPageSize(), stats->resident_bytes);

  for (size_t i = 0; i < kFirstAllocPages; i++) {
    CHECK_PAGE_IN_CORE(p.PageAtIndex(i), true);
  }

  allocator.root()->PurgeMemory(PurgeFlags::kDiscardUnusedSystemPages);

  for (size_t i = 0; i < kSecondAllocPages; i++) {
    CHECK_PAGE_IN_CORE(p.PageAtIndex(i), true);
  }
  for (size_t i = kSecondAllocPages; i < kFirstAllocPages; i++) {
    CHECK_PAGE_IN_CORE(p.PageAtIndex(i), false);
  }
}

TEST_P(PartitionAllocTest, PurgeDiscardableWithFreeListStraightening) {
  // This sub-test tests truncation of the provisioned slots in a trickier
  // case where the freelist is rewritten.
  allocator.root()->PurgeMemory(PurgeFlags::kDecommitEmptySlotSpans);
  char* ptr1 = static_cast<char*>(allocator.root()->Alloc(
      SystemPageSize() - ExtraAllocSize(allocator), type_name));
  void* ptr2 = allocator.root()->Alloc(
      SystemPageSize() - ExtraAllocSize(allocator), type_name);
  void* ptr3 = allocator.root()->Alloc(
      SystemPageSize() - ExtraAllocSize(allocator), type_name);
  void* ptr4 = allocator.root()->Alloc(
      SystemPageSize() - ExtraAllocSize(allocator), type_name);
  ptr1[0] = 'A';
  ptr1[SystemPageSize()] = 'A';
  ptr1[SystemPageSize() * 2] = 'A';
  ptr1[SystemPageSize() * 3] = 'A';
  const SlotSpan* slot_span =
      SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr1));
  allocator.root()->Free(ptr2);
  allocator.root()->Free(ptr4);
  allocator.root()->Free(ptr1);
  EXPECT_EQ(0u, slot_span->num_unprovisioned_slots);

  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats =
        dumper.GetBucketStats(SystemPageSize());
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
#if PA_BUILDFLAG(IS_WIN)
    EXPECT_EQ(SystemPageSize(), stats->discardable_bytes);
#else
    EXPECT_EQ(2 * SystemPageSize(), stats->discardable_bytes);
#endif
    EXPECT_EQ(SystemPageSize(), stats->active_bytes);
    EXPECT_EQ(4 * SystemPageSize(), stats->resident_bytes);
  }
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), true);
  allocator.root()->PurgeMemory(PurgeFlags::kDiscardUnusedSystemPages);
  EXPECT_EQ(1u, slot_span->num_unprovisioned_slots);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), false);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), false);

  // Let's check we didn't brick the freelist.
  void* ptr1b = allocator.root()->Alloc(
      SystemPageSize() - ExtraAllocSize(allocator), type_name);
  PA_EXPECT_PTR_EQ(ptr1, ptr1b);
  void* ptr2b = allocator.root()->Alloc(
      SystemPageSize() - ExtraAllocSize(allocator), type_name);
  PA_EXPECT_PTR_EQ(ptr2, ptr2b);
  EXPECT_FALSE(slot_span->get_freelist_head());  // ptr4 was unprovisioned
  void* ptr4b = allocator.root()->Alloc(
      SystemPageSize() - ExtraAllocSize(allocator), type_name);
  PA_EXPECT_PTR_EQ(ptr4, ptr4b);
  EXPECT_FALSE(slot_span->get_freelist_head());

  // Free objects such that they're in this order on the list:
  //   head -> ptr2 -> ptr3 -> ptr1
  // However, ptr4 is still unfreed preventing any unprovisioning.
  allocator.root()->Free(ptr1);
  allocator.root()->Free(ptr3);
  allocator.root()->Free(ptr2);
  allocator.root()->PurgeMemory(PurgeFlags::kDiscardUnusedSystemPages);
  // The test by default runs in
  // StraightenLargerSlotSpanFreeListsMode::kOnlyWhenUnprovisioning mode, so the
  // freelist wasn't modified, and the allocations will happen in LIFO order.
  ptr2b = allocator.root()->Alloc(SystemPageSize() - ExtraAllocSize(allocator),
                                  type_name);
  PA_EXPECT_PTR_EQ(ptr2, ptr2b);
  void* ptr3b = allocator.root()->Alloc(
      SystemPageSize() - ExtraAllocSize(allocator), type_name);
  PA_EXPECT_PTR_EQ(ptr3, ptr3b);
  ptr1b = allocator.root()->Alloc(SystemPageSize() - ExtraAllocSize(allocator),
                                  type_name);
  PA_EXPECT_PTR_EQ(ptr1, ptr1b);
  EXPECT_FALSE(slot_span->get_freelist_head());

  // Free objects such that they're in this order on the list:
  //   head -> ptr2 -> ptr3 -> ptr1
  // However, ptr4 is still unfreed preventing any unprovisioning.
  allocator.root()->Free(ptr1);
  allocator.root()->Free(ptr3);
  allocator.root()->Free(ptr2);
  PartitionRoot::SetStraightenLargerSlotSpanFreeListsMode(
      StraightenLargerSlotSpanFreeListsMode::kAlways);
  allocator.root()->PurgeMemory(PurgeFlags::kDiscardUnusedSystemPages);
  // In StraightenLargerSlotSpanFreeListsMode::kAlways mode, the freelist is
  // ordered from left to right.
  ptr1b = allocator.root()->Alloc(SystemPageSize() - ExtraAllocSize(allocator),
                                  type_name);
  PA_EXPECT_PTR_EQ(ptr1, ptr1b);
  ptr2b = allocator.root()->Alloc(SystemPageSize() - ExtraAllocSize(allocator),
                                  type_name);
  PA_EXPECT_PTR_EQ(ptr2, ptr2b);
  ptr3b = allocator.root()->Alloc(SystemPageSize() - ExtraAllocSize(allocator),
                                  type_name);
  PA_EXPECT_PTR_EQ(ptr3, ptr3b);
  EXPECT_FALSE(slot_span->get_freelist_head());

  // Free objects such that they're in this order on the list:
  //   head -> ptr2 -> ptr4 -> ptr1
  // ptr3 is still unfreed preventing unprovisioning of ptr1 and ptr2, but not
  // ptr4.
  allocator.root()->Free(ptr1);
  allocator.root()->Free(ptr4);
  allocator.root()->Free(ptr2);
  PartitionRoot::SetStraightenLargerSlotSpanFreeListsMode(
      StraightenLargerSlotSpanFreeListsMode::kNever);
  allocator.root()->PurgeMemory(PurgeFlags::kDiscardUnusedSystemPages);
  // In StraightenLargerSlotSpanFreeListsMode::kNever mode, unprovisioned
  // entries willbe removed form the freelist but the list won't be reordered.
  ptr2b = allocator.root()->Alloc(SystemPageSize() - ExtraAllocSize(allocator),
                                  type_name);
  PA_EXPECT_PTR_EQ(ptr2, ptr2b);
  ptr1b = allocator.root()->Alloc(SystemPageSize() - ExtraAllocSize(allocator),
                                  type_name);
  PA_EXPECT_PTR_EQ(ptr1, ptr1b);
  EXPECT_FALSE(slot_span->get_freelist_head());
  ptr4b = allocator.root()->Alloc(SystemPageSize() - ExtraAllocSize(allocator),
                                  type_name);
  PA_EXPECT_PTR_EQ(ptr4, ptr4b);
  EXPECT_FALSE(slot_span->get_freelist_head());

  // Clean up.
  allocator.root()->Free(ptr1);
  allocator.root()->Free(ptr2);
  allocator.root()->Free(ptr3);
  allocator.root()->Free(ptr4);
}

TEST_P(PartitionAllocTest, PurgeDiscardableDoubleTruncateFreeList) {
  // This sub-test is similar, but tests a double-truncation.
  allocator.root()->PurgeMemory(PurgeFlags::kDecommitEmptySlotSpans);
  char* ptr1 = static_cast<char*>(allocator.root()->Alloc(
      SystemPageSize() - ExtraAllocSize(allocator), type_name));
  void* ptr2 = allocator.root()->Alloc(
      SystemPageSize() - ExtraAllocSize(allocator), type_name);
  void* ptr3 = allocator.root()->Alloc(
      SystemPageSize() - ExtraAllocSize(allocator), type_name);
  void* ptr4 = allocator.root()->Alloc(
      SystemPageSize() - ExtraAllocSize(allocator), type_name);
  ptr1[0] = 'A';
  ptr1[SystemPageSize()] = 'A';
  ptr1[SystemPageSize() * 2] = 'A';
  ptr1[SystemPageSize() * 3] = 'A';
  const SlotSpan* slot_span =
      SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr1));
  allocator.root()->Free(ptr4);
  allocator.root()->Free(ptr3);
  EXPECT_EQ(0u, slot_span->num_unprovisioned_slots);

  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats =
        dumper.GetBucketStats(SystemPageSize());
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
    EXPECT_EQ(2 * SystemPageSize(), stats->discardable_bytes);
    EXPECT_EQ(2 * SystemPageSize(), stats->active_bytes);
    EXPECT_EQ(4 * SystemPageSize(), stats->resident_bytes);
  }
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), true);
  allocator.root()->PurgeMemory(PurgeFlags::kDiscardUnusedSystemPages);
  EXPECT_EQ(2u, slot_span->num_unprovisioned_slots);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 2), false);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + (SystemPageSize() * 3), false);

  EXPECT_FALSE(slot_span->get_freelist_head());

  allocator.root()->Free(ptr1);
  allocator.root()->Free(ptr2);
}

TEST_P(PartitionAllocTest, PurgeDiscardableSmallSlotsWithTruncate) {
  size_t requested_size = 0.5 * SystemPageSize();
  char* ptr1 = static_cast<char*>(allocator.root()->Alloc(
      requested_size - ExtraAllocSize(allocator), type_name));
  void* ptr2 = allocator.root()->Alloc(
      requested_size - ExtraAllocSize(allocator), type_name);
  void* ptr3 = allocator.root()->Alloc(
      requested_size - ExtraAllocSize(allocator), type_name);
  void* ptr4 = allocator.root()->Alloc(
      requested_size - ExtraAllocSize(allocator), type_name);
  allocator.root()->Free(ptr3);
  allocator.root()->Free(ptr4);
  const SlotSpan* slot_span =
      SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr1));
  EXPECT_EQ(4u, slot_span->num_unprovisioned_slots);
  {
    MockPartitionStatsDumper dumper;
    allocator.root()->DumpStats("mock_allocator", false /* detailed dump */,
                                &dumper);
    EXPECT_TRUE(dumper.IsMemoryAllocationRecorded());

    const PartitionBucketMemoryStats* stats =
        dumper.GetBucketStats(requested_size);
    EXPECT_TRUE(stats);
    EXPECT_TRUE(stats->is_valid);
    EXPECT_EQ(0u, stats->decommittable_bytes);
    EXPECT_EQ(SystemPageSize(), stats->discardable_bytes);
    EXPECT_EQ(requested_size * 2, stats->active_bytes);
    EXPECT_EQ(2 * SystemPageSize(), stats->resident_bytes);
  }
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), true);
  allocator.root()->PurgeMemory(PurgeFlags::kDiscardUnusedSystemPages);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset, true);
  CHECK_PAGE_IN_CORE(ptr1 - kPointerOffset + SystemPageSize(), false);
  EXPECT_EQ(6u, slot_span->num_unprovisioned_slots);

  allocator.root()->Free(ptr1);
  allocator.root()->Free(ptr2);
}

TEST_P(PartitionAllocTest, ActiveListMaintenance) {
  size_t size = SystemPageSize() - ExtraAllocSize(allocator);
  size_t real_size = size + ExtraAllocSize(allocator);
  size_t bucket_index =
      allocator.root()->SizeToBucketIndex(real_size, GetBucketDistribution());
  PartitionRoot::Bucket* bucket = &allocator.root()->buckets[bucket_index];
  ASSERT_EQ(bucket->slot_size, real_size);
  size_t slots_per_span = bucket->num_system_pages_per_slot_span;

  // Make 10 full slot spans.
  constexpr int kSpans = 10;
  std::vector<std::vector<void*>> allocated_memory_spans(kSpans);
  for (int span_index = 0; span_index < kSpans; span_index++) {
    for (size_t i = 0; i < slots_per_span; i++) {
      allocated_memory_spans[span_index].push_back(
          allocator.root()->Alloc(size));
    }
  }

  // Free one entry in the middle span, creating a partial slot span.
  constexpr size_t kSpanIndex = 5;
  allocator.root()->Free(allocated_memory_spans[kSpanIndex].back());
  allocated_memory_spans[kSpanIndex].pop_back();

  // Empty the last slot span.
  for (void* ptr : allocated_memory_spans[kSpans - 1]) {
    allocator.root()->Free(ptr);
  }
  allocated_memory_spans.pop_back();

  // The active list now is:
  // Partial -> Empty -> Full -> Full -> ... -> Full
  bucket->MaintainActiveList(allocator.root());

  // Only one entry in the active list.
  ASSERT_NE(bucket->active_slot_spans_head, SlotSpan::get_sentinel_slot_span());
  EXPECT_FALSE(bucket->active_slot_spans_head->next_slot_span);

  // The empty list has 1 entry.
  ASSERT_NE(bucket->empty_slot_spans_head, SlotSpan::get_sentinel_slot_span());
  EXPECT_FALSE(bucket->empty_slot_spans_head->next_slot_span);

  // The rest are full slot spans.
  EXPECT_EQ(8u, bucket->num_full_slot_spans);

  // Free all memory.
  for (const auto& span : allocated_memory_spans) {
    for (void* ptr : span) {
      allocator.root()->Free(ptr);
    }
  }
}

TEST_P(PartitionAllocTest, ReallocMovesCookie) {
  // Resize so as to be sure to hit a "resize in place" case, and ensure that
  // use of the entire result is compatible with the debug mode's cookie, even
  // when the bucket size is large enough to span more than one partition page
  // and we can track the "raw" size. See https://crbug.com/709271
  static const size_t kSize = MaxRegularSlotSpanSize();
  void* ptr = allocator.root()->Alloc(kSize + 1, type_name);
  EXPECT_TRUE(ptr);

  memset(ptr, 0xbd, kSize + 1);
  ptr = allocator.root()->Realloc(ptr, kSize + 2, type_name);
  EXPECT_TRUE(ptr);

  memset(ptr, 0xbd, kSize + 2);
  allocator.root()->Free(ptr);
}

TEST_P(PartitionAllocTest, SmallReallocDoesNotMoveTrailingCookie) {
  // For crbug.com/781473
  static constexpr size_t kSize = 264;
  void* ptr = allocator.root()->Alloc(kSize, type_name);
  EXPECT_TRUE(ptr);

  ptr = allocator.root()->Realloc(ptr, kSize + 16, type_name);
  EXPECT_TRUE(ptr);

  allocator.root()->Free(ptr);
}

TEST_P(PartitionAllocTest, ZeroFill) {
  static constexpr size_t kAllZerosSentinel =
      std::numeric_limits<size_t>::max();
  for (size_t size : kTestSizes) {
    char* p = static_cast<char*>(
        allocator.root()->Alloc<AllocFlags::kZeroFill>(size));
    size_t non_zero_position = kAllZerosSentinel;
    for (size_t i = 0; i < size; ++i) {
      if (0 != p[i]) {
        non_zero_position = i;
        break;
      }
    }
    EXPECT_EQ(kAllZerosSentinel, non_zero_position)
        << "test allocation size: " << size;
    allocator.root()->Free(p);
  }

  for (int i = 0; i < 10; ++i) {
    SCOPED_TRACE(i);
    AllocateRandomly<AllocFlags::kZeroFill>(allocator.root(), 250);
  }
}

TEST_P(PartitionAllocTest, SchedulerLoopQuarantine) {
  LightweightQuarantineBranch& branch =
      allocator.root()->GetSchedulerLoopQuarantineBranchForTesting();

  constexpr size_t kCapacityInBytes = std::numeric_limits<size_t>::max();
  size_t original_capacity_in_bytes = branch.GetCapacityInBytes();
  branch.SetCapacityInBytes(kCapacityInBytes);

  for (size_t size : kTestSizes) {
    SCOPED_TRACE(size);

    void* object = allocator.root()->Alloc(size);
    allocator.root()->Free<FreeFlags::kSchedulerLoopQuarantine>(object);

    ASSERT_TRUE(branch.IsQuarantinedForTesting(object));
  }

  for (int i = 0; i < 10; ++i) {
    SCOPED_TRACE(i);
    AllocateRandomly<AllocFlags::kNone, FreeFlags::kSchedulerLoopQuarantine>(
        allocator.root(), 250);
  }

  branch.Purge();
  branch.SetCapacityInBytes(original_capacity_in_bytes);
}

// Ensures `Free<kSchedulerLoopQuarantine>` works as `Free<kNone>` if disabled.
// See: https://crbug.com/324994233.
TEST_P(PartitionAllocTest, SchedulerLoopQuarantineDisabled) {
  PartitionOptions opts = GetCommonPartitionOptions();
  opts.scheduler_loop_quarantine = PartitionOptions::kDisabled;
  opts.thread_cache = PartitionOptions::kDisabled;
  opts.star_scan_quarantine = PartitionOptions::kDisallowed;
  std::unique_ptr<PartitionRoot> root = CreateCustomTestRoot(opts, {});

  // This allocation is required to prevent slot span from being empty and
  // decomitted.
  void* ptr_to_keep_slot_span = root->Alloc(kTestAllocSize, type_name);
  void* ptr = root->Alloc(kTestAllocSize, type_name);

  auto* slot_span = SlotSpan::FromSlotStart(root->ObjectToSlotStart(ptr));
  root->Free<FreeFlags::kSchedulerLoopQuarantine>(ptr);

  // The object should be freed immediately.
  EXPECT_EQ(root->ObjectToSlotStart(ptr),
            UntagPtr(slot_span->get_freelist_head()));

  root->Free(ptr_to_keep_slot_span);
}

TEST_P(PartitionAllocTest, ZapOnFree) {
  void* ptr = allocator.root()->Alloc(1, type_name);
  EXPECT_TRUE(ptr);
  memset(ptr, 'A', 1);
  allocator.root()->Free<FreeFlags::kZap>(ptr);
  // Accessing memory after free requires a retag.
  ptr = TagPtr(ptr);
  EXPECT_NE('A', *static_cast<unsigned char*>(ptr));

  constexpr size_t size = 1024;
  ptr = allocator.root()->Alloc(size, type_name);
  EXPECT_TRUE(ptr);
  memset(ptr, 'A', size);
  allocator.root()->Free<FreeFlags::kZap>(ptr);
  // Accessing memory after free requires a retag.
  ptr = TagPtr(ptr);
  EXPECT_NE('A', *static_cast<unsigned char*>(ptr));
  EXPECT_EQ(kFreedByte,
            *(static_cast<unsigned char*>(ptr) + 2 * sizeof(void*)));
  EXPECT_EQ(kFreedByte, *(static_cast<unsigned char*>(ptr) + size - 1));
}

TEST_P(PartitionAllocTest, Bug_897585) {
  // Need sizes big enough to be direct mapped and a delta small enough to
  // allow re-use of the slot span when cookied. These numbers fall out of the
  // test case in the indicated bug.
  size_t kInitialSize = 983050;
  size_t kDesiredSize = 983100;
  ASSERT_GT(kInitialSize, kMaxBucketed);
  ASSERT_GT(kDesiredSize, kMaxBucketed);
  void* ptr = allocator.root()->Alloc<AllocFlags::kReturnNull>(kInitialSize);
  ASSERT_NE(nullptr, ptr);
  ptr = allocator.root()->Realloc<AllocFlags::kReturnNull>(ptr, kDesiredSize,
                                                           nullptr);
  ASSERT_NE(nullptr, ptr);
  memset(ptr, 0xbd, kDesiredSize);
  allocator.root()->Free(ptr);
}

TEST_P(PartitionAllocTest, OverrideHooks) {
  constexpr size_t kOverriddenSize = 1234;
  constexpr const char* kOverriddenType = "Overridden type";
  constexpr unsigned char kOverriddenChar = 'A';

  // Marked static so that we can use them in non-capturing lambdas below.
  // (Non-capturing lambdas convert directly to function pointers.)
  static volatile bool free_called = false;
  static void* overridden_allocation = nullptr;
  overridden_allocation = malloc(kOverriddenSize);
  memset(overridden_allocation, kOverriddenChar, kOverriddenSize);

  PartitionAllocHooks::SetOverrideHooks(
      [](void** out, AllocFlags flags, size_t size,
         const char* type_name) -> bool {
        if (size == kOverriddenSize && type_name == kOverriddenType) {
          *out = overridden_allocation;
          return true;
        }
        return false;
      },
      [](void* address) -> bool {
        if (address == overridden_allocation) {
          free_called = true;
          return true;
        }
        return false;
      },
      [](size_t* out, void* address) -> bool {
        if (address == overridden_allocation) {
          *out = kOverriddenSize;
          return true;
        }
        return false;
      });

  void* ptr = allocator.root()->Alloc<AllocFlags::kReturnNull>(kOverriddenSize,
                                                               kOverriddenType);
  ASSERT_EQ(ptr, overridden_allocation);

  allocator.root()->Free(ptr);
  EXPECT_TRUE(free_called);

  // overridden_allocation has not actually been freed so we can now immediately
  // realloc it.
  free_called = false;
  ptr = allocator.root()->Realloc<AllocFlags::kReturnNull>(ptr, 1, nullptr);
  ASSERT_NE(ptr, nullptr);
  EXPECT_NE(ptr, overridden_allocation);
  EXPECT_TRUE(free_called);
  EXPECT_EQ(*(char*)ptr, kOverriddenChar);
  allocator.root()->Free(ptr);

  PartitionAllocHooks::SetOverrideHooks(nullptr, nullptr, nullptr);
  free(overridden_allocation);
}

TEST_P(PartitionAllocTest, Alignment) {
  std::vector<void*> allocated_ptrs;

  for (size_t size = 1; size <= PartitionPageSize(); size <<= 1) {
    if (size <= ExtraAllocSize(allocator)) {
      continue;
    }
    size_t requested_size = size - ExtraAllocSize(allocator);

    // All allocations which are not direct-mapped occupy contiguous slots of a
    // span, starting on a page boundary. This means that allocations are first
    // rounded up to the nearest bucket size, then have an address of the form:
    //   (partition-page-aligned address) + i * bucket_size.
    //
    // All powers of two are bucket sizes, meaning that all power of two
    // allocations smaller than a page will be aligned on the allocation size.
    size_t expected_alignment = size;
    for (int index = 0; index < 3; index++) {
      void* ptr = allocator.root()->Alloc(requested_size);
      allocated_ptrs.push_back(ptr);
      EXPECT_EQ(0u,
                allocator.root()->ObjectToSlotStart(ptr) % expected_alignment)
          << (index + 1) << "-th allocation of size=" << size;
    }
  }

  for (void* ptr : allocated_ptrs) {
    allocator.root()->Free(ptr);
  }
}

TEST_P(PartitionAllocTest, FundamentalAlignment) {
  // See the test above for details. Essentially, checking the bucket size is
  // sufficient to ensure that alignment will always be respected, as long as
  // the fundamental alignment is <= 16 bytes.
  size_t fundamental_alignment = kAlignment;
  for (size_t size = 0; size < SystemPageSize(); size++) {
    // Allocate several pointers, as the first one in use in a size class will
    // be aligned on a page boundary.
    void* ptr = allocator.root()->Alloc(size);
    void* ptr2 = allocator.root()->Alloc(size);
    void* ptr3 = allocator.root()->Alloc(size);

    EXPECT_EQ(UntagPtr(ptr) % fundamental_alignment, 0u);
    EXPECT_EQ(UntagPtr(ptr2) % fundamental_alignment, 0u);
    EXPECT_EQ(UntagPtr(ptr3) % fundamental_alignment, 0u);

    uintptr_t slot_start = allocator.root()->ObjectToSlotStart(ptr);
    // The capacity(C) is slot size - ExtraAllocSize(allocator).
    // Since slot size is multiples of kAlignment,
    // C % kAlignment == (slot_size - ExtraAllocSize(allocator)) % kAlignment.
    // C % kAlignment == (-ExtraAllocSize(allocator)) % kAlignment.
    EXPECT_EQ(allocator.root()->AllocationCapacityFromSlotStart(slot_start) %
                  fundamental_alignment,
              -ExtraAllocSize(allocator) % fundamental_alignment);

    allocator.root()->Free(ptr);
    allocator.root()->Free(ptr2);
    allocator.root()->Free(ptr3);
  }
}

void VerifyAlignment(PartitionRoot* root, size_t size, size_t alignment) {
  std::vector<void*> allocated_ptrs;

  for (int index = 0; index < 3; index++) {
    void* ptr = root->AlignedAlloc(alignment, size);
    ASSERT_TRUE(ptr);
    allocated_ptrs.push_back(ptr);
    EXPECT_EQ(0ull, UntagPtr(ptr) % alignment)
        << (index + 1) << "-th allocation of size=" << size
        << ", alignment=" << alignment;
  }

  for (void* ptr : allocated_ptrs) {
    root->Free(ptr);
  }
}

TEST_P(PartitionAllocTest, AlignedAllocations) {
  size_t alloc_sizes[] = {1,
                          10,
                          100,
                          1000,
                          10000,
                          60000,
                          70000,
                          130000,
                          500000,
                          900000,
                          kMaxBucketed + 1,
                          2 * kMaxBucketed,
                          kSuperPageSize - 2 * PartitionPageSize(),
                          4 * kMaxBucketed};
  for (size_t alloc_size : alloc_sizes) {
    for (size_t alignment = 1; alignment <= kMaxSupportedAlignment;
         alignment <<= 1) {
      VerifyAlignment(allocator.root(), alloc_size, alignment);
    }
  }
}

// Test that the optimized `GetSlotNumber` implementation produces valid
// results.
TEST_P(PartitionAllocTest, OptimizedGetSlotNumber) {
  for (size_t i = 0; i < kNumBuckets; ++i) {
    auto& bucket = allocator.root()->buckets[i];
    if (SizeToIndex(bucket.slot_size) != i) {
      continue;
    }
    for (size_t slot = 0, offset = 0; slot < bucket.get_slots_per_span();
         ++slot, offset += bucket.slot_size) {
      EXPECT_EQ(slot, bucket.GetSlotNumber(offset));
      EXPECT_EQ(slot, bucket.GetSlotNumber(offset + bucket.slot_size / 2));
      EXPECT_EQ(slot, bucket.GetSlotNumber(offset + bucket.slot_size - 1));
    }
  }
}

TEST_P(PartitionAllocTest, GetUsableSizeNull) {
  EXPECT_EQ(0ULL, PartitionRoot::GetUsableSize(nullptr));
}

TEST_P(PartitionAllocTest, GetUsableSize) {
#if PA_CONFIG(MAYBE_ENABLE_MAC11_MALLOC_SIZE_HACK)
  allocator.root()->EnableMac11MallocSizeHackForTesting();
#endif
  size_t delta = 31;
  for (size_t size = 1; size <= kMinDirectMappedDownsize; size += delta) {
    void* ptr = allocator.root()->Alloc(size);
    EXPECT_TRUE(ptr);
    size_t usable_size = PartitionRoot::GetUsableSize(ptr);
    size_t usable_size_with_hack =
        PartitionRoot::GetUsableSizeWithMac11MallocSizeHack(ptr);
#if PA_CONFIG(MAYBE_ENABLE_MAC11_MALLOC_SIZE_HACK)
    if (size != internal::kMac11MallocSizeHackRequestedSize)
#endif
      EXPECT_EQ(usable_size_with_hack, usable_size);
    EXPECT_LE(size, usable_size);
    memset(ptr, 0xDE, usable_size);
    // Should not crash when free the ptr.
    allocator.root()->Free(ptr);
  }
}

#if PA_CONFIG(MAYBE_ENABLE_MAC11_MALLOC_SIZE_HACK)
TEST_P(PartitionAllocTest, GetUsableSizeWithMac11MallocSizeHack) {
  if (internal::base::mac::MacOSMajorVersion() != 11) {
    GTEST_SKIP() << "Skipping because the test is for Mac11.";
  }

  allocator.root()->EnableMac11MallocSizeHackForTesting();
  size_t size = internal::kMac11MallocSizeHackRequestedSize;
  void* ptr = allocator.root()->Alloc(size);
  size_t usable_size = PartitionRoot::GetUsableSize(ptr);
  size_t usable_size_with_hack =
      PartitionRoot::GetUsableSizeWithMac11MallocSizeHack(ptr);
  EXPECT_EQ(usable_size,
            allocator.root()->settings.mac11_malloc_size_hack_usable_size_);
  EXPECT_EQ(usable_size_with_hack, size);

  allocator.root()->Free(ptr);
}
#endif  // PA_CONFIG(MAYBE_ENABLE_MAC11_MALLOC_SIZE_HACK)

TEST_P(PartitionAllocTest, Bookkeeping) {
  auto& root = *allocator.root();

  EXPECT_EQ(0U, root.total_size_of_committed_pages);
  EXPECT_EQ(0U, root.max_size_of_committed_pages);
  EXPECT_EQ(0U, root.get_total_size_of_allocated_bytes());
  EXPECT_EQ(0U, root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(0U, root.total_size_of_super_pages);
  size_t small_size = 1000;

  // A full slot span of size 1 partition page is committed.
  void* ptr = root.Alloc(small_size - ExtraAllocSize(allocator), type_name);
  // Lazy commit commits only needed pages.
  size_t expected_committed_size =
      kUseLazyCommit ? SystemPageSize() : PartitionPageSize();
  size_t expected_super_pages_size = kSuperPageSize;
  size_t expected_max_committed_size = expected_committed_size;
  size_t bucket_index = SizeToIndex(small_size - ExtraAllocSize(allocator));
  PartitionBucket* bucket = &root.buckets[bucket_index];
  size_t expected_total_allocated_size = bucket->slot_size;
  size_t expected_max_allocated_size = expected_total_allocated_size;

  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_total_allocated_size,
            root.get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // Freeing memory doesn't result in decommitting pages right away.
  root.Free(ptr);
  expected_total_allocated_size = 0U;
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_total_allocated_size,
            root.get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // Allocating the same size lands it in the same slot span.
  ptr = root.Alloc(small_size - ExtraAllocSize(allocator), type_name);
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // Freeing memory doesn't result in decommitting pages right away.
  root.Free(ptr);
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // Allocating another size commits another slot span.
  ptr = root.Alloc(2 * small_size - ExtraAllocSize(allocator), type_name);
  expected_committed_size +=
      kUseLazyCommit ? SystemPageSize() : PartitionPageSize();
  expected_max_committed_size =
      std::max(expected_max_committed_size, expected_committed_size);
  expected_max_allocated_size =
      std::max(expected_max_allocated_size, static_cast<size_t>(2048));
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // Freeing memory doesn't result in decommitting pages right away.
  root.Free(ptr);
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // Single-slot slot spans...
  //
  // When the system page size is larger than 4KiB, we don't necessarily have
  // enough space in the superpage to store two of the largest bucketed
  // allocations, particularly when we reserve extra space for e.g. bitmaps.
  // To avoid this, we use something just below kMaxBucketed.
  size_t big_size = kMaxBucketed * 4 / 5 - SystemPageSize();

  ASSERT_GT(big_size, MaxRegularSlotSpanSize());
  ASSERT_LE(big_size, kMaxBucketed);
  bucket_index = SizeToIndex(big_size - ExtraAllocSize(allocator));
  bucket = &root.buckets[bucket_index];
  // Assert the allocation doesn't fill the entire span nor entire partition
  // page, to make the test more interesting.
  ASSERT_LT(big_size, bucket->get_bytes_per_span());
  ASSERT_NE(big_size % PartitionPageSize(), 0U);
  ptr = root.Alloc(big_size - ExtraAllocSize(allocator), type_name);
  expected_committed_size += bucket->get_bytes_per_span();
  expected_max_committed_size =
      std::max(expected_max_committed_size, expected_committed_size);
  expected_total_allocated_size += bucket->get_bytes_per_span();
  expected_max_allocated_size =
      std::max(expected_max_allocated_size, expected_total_allocated_size);
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_total_allocated_size,
            root.get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // Allocating 2nd time doesn't overflow the super page...
  void* ptr2 = root.Alloc(big_size - ExtraAllocSize(allocator), type_name);
  expected_committed_size += bucket->get_bytes_per_span();
  expected_max_committed_size =
      std::max(expected_max_committed_size, expected_committed_size);
  expected_total_allocated_size += bucket->get_bytes_per_span();
  expected_max_allocated_size =
      std::max(expected_max_allocated_size, expected_total_allocated_size);
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_total_allocated_size,
            root.get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // ... but 3rd time does.
  void* ptr3 = root.Alloc(big_size - ExtraAllocSize(allocator), type_name);
  expected_committed_size += bucket->get_bytes_per_span();
  expected_max_committed_size =
      std::max(expected_max_committed_size, expected_committed_size);
  expected_total_allocated_size += bucket->get_bytes_per_span();
  expected_max_allocated_size =
      std::max(expected_max_allocated_size, expected_total_allocated_size);
  expected_super_pages_size += kSuperPageSize;
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_total_allocated_size,
            root.get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // Freeing memory doesn't result in decommitting pages right away.
  root.Free(ptr);
  root.Free(ptr2);
  root.Free(ptr3);
  expected_total_allocated_size -= 3 * bucket->get_bytes_per_span();
  expected_max_allocated_size =
      std::max(expected_max_allocated_size, expected_total_allocated_size);
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_total_allocated_size,
            root.get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // Now everything should be decommitted. The reserved space for super pages
  // stays the same and will never go away (by design).
  root.PurgeMemory(PurgeFlags::kDecommitEmptySlotSpans);
  expected_committed_size = 0;
  EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
  EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
  EXPECT_EQ(expected_total_allocated_size,
            root.get_total_size_of_allocated_bytes());
  EXPECT_EQ(expected_max_allocated_size,
            root.get_max_size_of_allocated_bytes());
  EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);

  // None of the above should affect the direct map space.
  EXPECT_EQ(0U, root.total_size_of_direct_mapped_pages);

  size_t huge_sizes[] = {
      kMaxBucketed + SystemPageSize(),
      kMaxBucketed + SystemPageSize() + 123,
      kSuperPageSize - PageAllocationGranularity(),
      kSuperPageSize - SystemPageSize() - PartitionPageSize(),
      kSuperPageSize - PartitionPageSize(),
      kSuperPageSize - SystemPageSize(),
      kSuperPageSize,
      kSuperPageSize + SystemPageSize(),
      kSuperPageSize + PartitionPageSize(),
      kSuperPageSize + SystemPageSize() + PartitionPageSize(),
      kSuperPageSize + PageAllocationGranularity(),
      kSuperPageSize + DirectMapAllocationGranularity(),
  };
  size_t alignments[] = {
      PartitionPageSize(),
      2 * PartitionPageSize(),
      kMaxSupportedAlignment / 2,
      kMaxSupportedAlignment,
  };
  for (size_t huge_size : huge_sizes) {
    ASSERT_GT(huge_size, kMaxBucketed);
    for (size_t alignment : alignments) {
      // For direct map, we commit only as many pages as needed.
      size_t aligned_size = partition_alloc::internal::base::bits::AlignUp(
          huge_size, SystemPageSize());
      ptr = root.AllocInternalForTesting(huge_size - ExtraAllocSize(allocator),
                                         alignment, type_name);
      expected_committed_size += aligned_size;
      expected_max_committed_size =
          std::max(expected_max_committed_size, expected_committed_size);
      expected_total_allocated_size += aligned_size;
      expected_max_allocated_size =
          std::max(expected_max_allocated_size, expected_total_allocated_size);
      // The total reserved map includes metadata and guard pages at the ends.
      // It also includes alignment. However, these would double count the first
      // partition page, so it needs to be subtracted.
      size_t surrounding_pages_size =
          PartitionRoot::GetDirectMapMetadataAndGuardPagesSize() + alignment -
          PartitionPageSize();
      size_t expected_direct_map_size =
          partition_alloc::internal::base::bits::AlignUp(
              aligned_size + surrounding_pages_size,
              DirectMapAllocationGranularity());
      EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
      EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
      EXPECT_EQ(expected_total_allocated_size,
                root.get_total_size_of_allocated_bytes());
      EXPECT_EQ(expected_max_allocated_size,
                root.get_max_size_of_allocated_bytes());
      EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);
      EXPECT_EQ(expected_direct_map_size,
                root.total_size_of_direct_mapped_pages);

      // Freeing memory in the diret map decommits pages right away. The address
      // space is released for re-use too.
      root.Free(ptr);
      expected_committed_size -= aligned_size;
      expected_direct_map_size = 0;
      expected_max_committed_size =
          std::max(expected_max_committed_size, expected_committed_size);
      expected_total_allocated_size -= aligned_size;
      expected_max_allocated_size =
          std::max(expected_max_allocated_size, expected_total_allocated_size);
      EXPECT_EQ(expected_committed_size, root.total_size_of_committed_pages);
      EXPECT_EQ(expected_max_committed_size, root.max_size_of_committed_pages);
      EXPECT_EQ(expected_total_allocated_size,
                root.get_total_size_of_allocated_bytes());
      EXPECT_EQ(expected_max_allocated_size,
                root.get_max_size_of_allocated_bytes());
      EXPECT_EQ(expected_super_pages_size, root.total_size_of_super_pages);
      EXPECT_EQ(expected_direct_map_size,
                root.total_size_of_direct_mapped_pages);
    }
  }
}

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

TEST_P(PartitionAllocTest, RefCountBasic) {
  if (!UseBRPPool()) {
    return;
  }

  constexpr uint64_t kCookie = 0x1234567890ABCDEF;
  constexpr uint64_t kQuarantined = 0xEFEFEFEFEFEFEFEF;

  size_t alloc_size = 64 - ExtraAllocSize(allocator);
  uint64_t* ptr1 =
      static_cast<uint64_t*>(allocator.root()->Alloc(alloc_size, type_name));
  EXPECT_TRUE(ptr1);

  *ptr1 = kCookie;

  auto* in_slot_metadata =
      allocator.root()->InSlotMetadataPointerFromObjectForTesting(ptr1);
  EXPECT_TRUE(in_slot_metadata->IsAliveWithNoKnownRefs());

  in_slot_metadata->Acquire();
  EXPECT_FALSE(in_slot_metadata->Release());
  EXPECT_TRUE(in_slot_metadata->IsAliveWithNoKnownRefs());
  EXPECT_EQ(*ptr1, kCookie);

  in_slot_metadata->AcquireFromUnprotectedPtr();
  EXPECT_FALSE(in_slot_metadata->IsAliveWithNoKnownRefs());

  allocator.root()->Free(ptr1);
  // The allocation shouldn't be reclaimed, and its contents should be zapped.
  // Retag ptr1 to get its correct MTE tag.
  ptr1 = TagPtr(ptr1);
  EXPECT_NE(*ptr1, kCookie);
  EXPECT_EQ(*ptr1, kQuarantined);

  // The allocator should not reuse the original slot since its reference count
  // doesn't equal zero.
  uint64_t* ptr2 =
      static_cast<uint64_t*>(allocator.root()->Alloc(alloc_size, type_name));
  PA_EXPECT_PTR_NE(ptr1, ptr2);
  allocator.root()->Free(ptr2);

  // When the last reference is released, the slot should become reusable.
  // Retag in_slot_metadata because PartitionAlloc retags ptr to enforce
  // quarantine.
  in_slot_metadata = TagPtr(in_slot_metadata);
  EXPECT_TRUE(in_slot_metadata->ReleaseFromUnprotectedPtr());
  PartitionAllocFreeForRefCounting(allocator.root()->ObjectToSlotStart(ptr1));
  uint64_t* ptr3 =
      static_cast<uint64_t*>(allocator.root()->Alloc(alloc_size, type_name));
  PA_EXPECT_PTR_EQ(ptr1, ptr3);
  allocator.root()->Free(ptr3);
}

void PartitionAllocTest::RunRefCountReallocSubtest(size_t orig_size,
                                                   size_t new_size) {
  void* ptr1 = allocator.root()->Alloc(orig_size, type_name);
  EXPECT_TRUE(ptr1);

  auto* in_slot_metadata1 =
      allocator.root()->InSlotMetadataPointerFromObjectForTesting(ptr1);
  EXPECT_TRUE(in_slot_metadata1->IsAliveWithNoKnownRefs());

  in_slot_metadata1->AcquireFromUnprotectedPtr();
  EXPECT_FALSE(in_slot_metadata1->IsAliveWithNoKnownRefs());

  void* ptr2 = allocator.root()->Realloc(ptr1, new_size, type_name);
  EXPECT_TRUE(ptr2);

  // PartitionAlloc may retag memory areas on realloc (even if they
  // do not move), so recover the true tag here.
  in_slot_metadata1 = TagPtr(in_slot_metadata1);

  // Re-query in-slot metadata. It may have moved if Realloc changed the slot.
  auto* in_slot_metadata2 =
      allocator.root()->InSlotMetadataPointerFromObjectForTesting(ptr2);

  if (UntagPtr(ptr1) == UntagPtr(ptr2)) {
    // If the slot didn't change, in-slot metadata should stay the same.
    EXPECT_EQ(in_slot_metadata1, in_slot_metadata2);
    EXPECT_FALSE(in_slot_metadata2->IsAliveWithNoKnownRefs());

    EXPECT_FALSE(in_slot_metadata2->ReleaseFromUnprotectedPtr());
  } else {
    // If the allocation was moved to another slot, the old in-slot metadata
    // stayed in the same location in memory, is no longer alive, but still has
    // a reference. The new in-slot metadata is alive, but has no references.
    EXPECT_NE(in_slot_metadata1, in_slot_metadata2);
    EXPECT_FALSE(in_slot_metadata1->IsAlive());
    EXPECT_FALSE(in_slot_metadata1->IsAliveWithNoKnownRefs());
    EXPECT_TRUE(in_slot_metadata2->IsAliveWithNoKnownRefs());

    EXPECT_TRUE(in_slot_metadata1->ReleaseFromUnprotectedPtr());
    PartitionAllocFreeForRefCounting(allocator.root()->ObjectToSlotStart(ptr1));
  }

  allocator.root()->Free(ptr2);
}

TEST_P(PartitionAllocTest, RefCountRealloc) {
  if (!UseBRPPool()) {
    return;
  }

  size_t raw_sizes[] = {500, 5000, 50000, 400000, 5000000};

  for (size_t raw_size : raw_sizes) {
    size_t alloc_size = raw_size - ExtraAllocSize(allocator);
    RunRefCountReallocSubtest(alloc_size, alloc_size - 9);
    RunRefCountReallocSubtest(alloc_size, alloc_size + 9);
    RunRefCountReallocSubtest(alloc_size, alloc_size * 2);
    RunRefCountReallocSubtest(alloc_size, alloc_size / 2);
    RunRefCountReallocSubtest(alloc_size, alloc_size / 10 * 11);
    RunRefCountReallocSubtest(alloc_size, alloc_size / 10 * 9);
  }
}

int g_unretained_dangling_raw_ptr_detected_count = 0;

class UnretainedDanglingRawPtrTest : public PartitionAllocTest {
 public:
  void SetUp() override {
    PartitionAllocTest::SetUp();
    g_unretained_dangling_raw_ptr_detected_count = 0;
    old_detected_fn_ = partition_alloc::GetUnretainedDanglingRawPtrDetectedFn();

    partition_alloc::SetUnretainedDanglingRawPtrDetectedFn(
        &UnretainedDanglingRawPtrTest::DanglingRawPtrDetected);
    old_unretained_dangling_ptr_enabled_ =
        partition_alloc::SetUnretainedDanglingRawPtrCheckEnabled(true);
  }
  void TearDown() override {
    partition_alloc::SetUnretainedDanglingRawPtrDetectedFn(old_detected_fn_);
    partition_alloc::SetUnretainedDanglingRawPtrCheckEnabled(
        old_unretained_dangling_ptr_enabled_);
    PartitionAllocTest::TearDown();
  }

 private:
  static void DanglingRawPtrDetected(uintptr_t) {
    g_unretained_dangling_raw_ptr_detected_count++;
  }

  partition_alloc::DanglingRawPtrDetectedFn* old_detected_fn_;
  bool old_unretained_dangling_ptr_enabled_;
};

INSTANTIATE_TEST_SUITE_P(AlternateTestParams,
                         UnretainedDanglingRawPtrTest,
                         testing::ValuesIn(GetPartitionAllocTestParams()));

TEST_P(UnretainedDanglingRawPtrTest, UnretainedDanglingPtrNoReport) {
  if (!UseBRPPool()) {
    return;
  }

  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  auto* in_slot_metadata =
      allocator.root()->InSlotMetadataPointerFromObjectForTesting(ptr);
  in_slot_metadata->Acquire();
  EXPECT_TRUE(in_slot_metadata->IsAlive());
  // Allocation is still live, so calling ReportIfDangling() should not result
  // in any detections.
  in_slot_metadata->ReportIfDangling();
  EXPECT_EQ(g_unretained_dangling_raw_ptr_detected_count, 0);
  EXPECT_FALSE(in_slot_metadata->Release());
  allocator.root()->Free(ptr);
}

TEST_P(UnretainedDanglingRawPtrTest, UnretainedDanglingPtrShouldReport) {
  if (!UseBRPPool()) {
    return;
  }

  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  auto* in_slot_metadata =
      allocator.root()->InSlotMetadataPointerFromObjectForTesting(ptr);
  in_slot_metadata->AcquireFromUnprotectedPtr();
  EXPECT_TRUE(in_slot_metadata->IsAlive());
  allocator.root()->Free(ptr);
  // At this point, memory shouldn't be alive...
  EXPECT_FALSE(in_slot_metadata->IsAlive());
  // ...and we should report the ptr as dangling.
  in_slot_metadata->ReportIfDangling();
  EXPECT_EQ(g_unretained_dangling_raw_ptr_detected_count, 1);
  EXPECT_TRUE(in_slot_metadata->ReleaseFromUnprotectedPtr());

  PartitionAllocFreeForRefCounting(allocator.root()->ObjectToSlotStart(ptr));
}

#if !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
TEST_P(PartitionAllocTest, BackupRefPtrGuardRegion) {
  if (!UseBRPPool()) {
    return;
  }

  size_t alignment = internal::PageAllocationGranularity();

  uintptr_t requested_address;
  memset(&requested_address, internal::kQuarantinedByte,
         sizeof(requested_address));
  requested_address = RoundDownToPageAllocationGranularity(requested_address);

  uintptr_t allocated_address =
      AllocPages(requested_address, alignment, alignment,
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kReadWrite),
                 PageTag::kPartitionAlloc);
  EXPECT_NE(allocated_address, requested_address);

  if (allocated_address) {
    FreePages(allocated_address, alignment);
  }
}
#endif  // !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

#if PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)

// Allocate memory, and reference it from 3 raw_ptr. Among them 2 will be
// dangling.
TEST_P(PartitionAllocTest, DanglingPtr) {
  if (!UseBRPPool()) {
    return;
  }

  CountDanglingRawPtr dangling_checks;

  // Allocate memory, and reference it from 3 raw_ptr.
  uint64_t* ptr = static_cast<uint64_t*>(
      allocator.root()->Alloc(64 - ExtraAllocSize(allocator), type_name));
  auto* in_slot_metadata =
      allocator.root()->InSlotMetadataPointerFromObjectForTesting(ptr);

  in_slot_metadata->Acquire();
  in_slot_metadata->Acquire();
  in_slot_metadata->Acquire();
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // The first raw_ptr stops referencing it, before the memory has been
  // released.
  EXPECT_FALSE(in_slot_metadata->Release());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // Free it. This creates two dangling pointer.
  allocator.root()->Free(ptr);
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 1);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // The dangling raw_ptr stop referencing it.
  EXPECT_FALSE(in_slot_metadata->Release());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 1);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 1);

  // The dangling raw_ptr stop referencing it again.
  EXPECT_TRUE(in_slot_metadata->Release());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 1);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 2);

  PartitionAllocFreeForRefCounting(allocator.root()->ObjectToSlotStart(ptr));
}

// Allocate memory, and reference it from 3
// raw_ptr<T, DisableDanglingPtrDetection>. Among them 2 will be dangling. This
// doesn't trigger any dangling raw_ptr checks.
TEST_P(PartitionAllocTest, DanglingDanglingPtr) {
  if (!UseBRPPool()) {
    return;
  }

  CountDanglingRawPtr dangling_checks;

  // Allocate memory, and reference it from 3 raw_ptr.
  uint64_t* ptr = static_cast<uint64_t*>(
      allocator.root()->Alloc(64 - ExtraAllocSize(allocator), type_name));
  auto* in_slot_metadata =
      allocator.root()->InSlotMetadataPointerFromObjectForTesting(ptr);
  in_slot_metadata->AcquireFromUnprotectedPtr();
  in_slot_metadata->AcquireFromUnprotectedPtr();
  in_slot_metadata->AcquireFromUnprotectedPtr();
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // The first raw_ptr<T, DisableDanglingPtrDetection> stops referencing it,
  // before the memory has been released.
  EXPECT_FALSE(in_slot_metadata->ReleaseFromUnprotectedPtr());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // Free it. This creates two dangling raw_ptr<T, DisableDanglingPtrDetection>.
  allocator.root()->Free(ptr);
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // The dangling raw_ptr<T, DisableDanglingPtrDetection> stop referencing it.
  EXPECT_FALSE(in_slot_metadata->ReleaseFromUnprotectedPtr());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // The dangling raw_ptr<T, DisableDanglingPtrDetection> stop referencing it
  // again.
  EXPECT_TRUE(in_slot_metadata->ReleaseFromUnprotectedPtr());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  PartitionAllocFreeForRefCounting(allocator.root()->ObjectToSlotStart(ptr));
}

// When 'free' is called, it remain one raw_ptr<> and one
// raw_ptr<T, DisableDanglingPtrDetection>. The raw_ptr<> is released first.
TEST_P(PartitionAllocTest, DanglingMixedReleaseRawPtrFirst) {
  if (!UseBRPPool()) {
    return;
  }

  CountDanglingRawPtr dangling_checks;

  uint64_t* ptr = static_cast<uint64_t*>(
      allocator.root()->Alloc(64 - ExtraAllocSize(allocator), type_name));
  auto* in_slot_metadata =
      allocator.root()->InSlotMetadataPointerFromObjectForTesting(ptr);
  // Acquire a raw_ptr<T, DisableDanglingPtrDetection> and a raw_ptr<>.
  in_slot_metadata->AcquireFromUnprotectedPtr();
  in_slot_metadata->Acquire();
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // Free it.
  allocator.root()->Free(ptr);
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 1);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // The raw_ptr<> stops referencing it.
  EXPECT_FALSE(in_slot_metadata->Release());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 1);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 1);

  // The raw_ptr<T, DisableDanglingPtrDetection> stop referencing it.
  EXPECT_TRUE(in_slot_metadata->ReleaseFromUnprotectedPtr());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 1);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 1);

  PartitionAllocFreeForRefCounting(allocator.root()->ObjectToSlotStart(ptr));
}

// When 'free' is called, it remain one raw_ptr<> and one
// raw_ptr<T, DisableDanglingPtrDetection>.
// The raw_ptr<T, DisableDanglingPtrDetection> is released first. This
// triggers the dangling raw_ptr<> checks.
TEST_P(PartitionAllocTest, DanglingMixedReleaseDanglingPtrFirst) {
  if (!UseBRPPool()) {
    return;
  }

  CountDanglingRawPtr dangling_checks;

  void* ptr =
      allocator.root()->Alloc(64 - ExtraAllocSize(allocator), type_name);
  auto* in_slot_metadata =
      allocator.root()->InSlotMetadataPointerFromObjectForTesting(ptr);
  // Acquire a raw_ptr<T, DisableDanglingPtrDetection> and a raw_ptr<>.
  in_slot_metadata->AcquireFromUnprotectedPtr();
  in_slot_metadata->Acquire();
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // Free it.
  allocator.root()->Free(ptr);
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 1);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // The raw_ptr<> stops referencing it.
  EXPECT_FALSE(in_slot_metadata->ReleaseFromUnprotectedPtr());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 1);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // The raw_ptr<T, DisableDanglingPtrDetection> stop referencing it.
  EXPECT_TRUE(in_slot_metadata->Release());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 1);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 1);

  PartitionAllocFreeForRefCounting(allocator.root()->ObjectToSlotStart(ptr));
}

// When 'free' is called, it remains one
// raw_ptr<T, DisableDanglingPtrDetection>, then it is used to acquire one
// dangling raw_ptr<>. Release the raw_ptr<> first.
TEST_P(PartitionAllocTest, DanglingPtrUsedToAcquireNewRawPtr) {
  if (!UseBRPPool()) {
    return;
  }

  CountDanglingRawPtr dangling_checks;

  void* ptr =
      allocator.root()->Alloc(64 - ExtraAllocSize(allocator), type_name);
  auto* in_slot_metadata =
      allocator.root()->InSlotMetadataPointerFromObjectForTesting(ptr);
  // Acquire a raw_ptr<T, DisableDanglingPtrDetection>.
  in_slot_metadata->AcquireFromUnprotectedPtr();
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // Free it once.
  allocator.root()->Free(ptr);
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // A raw_ptr<> starts referencing it.
  in_slot_metadata->Acquire();
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // The raw_ptr<> stops referencing it.
  EXPECT_FALSE(in_slot_metadata->Release());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // The raw_ptr<T, DisableDanglingPtrDetection> stops referencing it.
  EXPECT_TRUE(in_slot_metadata->ReleaseFromUnprotectedPtr());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  PartitionAllocFreeForRefCounting(allocator.root()->ObjectToSlotStart(ptr));
}

// Same as 'DanglingPtrUsedToAcquireNewRawPtr', but release the
// raw_ptr<T, DisableDanglingPtrDetection> before the raw_ptr<>.
TEST_P(PartitionAllocTest, DanglingPtrUsedToAcquireNewRawPtrVariant) {
  if (!UseBRPPool()) {
    return;
  }

  CountDanglingRawPtr dangling_checks;

  void* ptr =
      allocator.root()->Alloc(64 - ExtraAllocSize(allocator), type_name);
  auto* in_slot_metadata =
      allocator.root()->InSlotMetadataPointerFromObjectForTesting(ptr);
  // Acquire a raw_ptr<T, DisableDanglingPtrDetection>.
  in_slot_metadata->AcquireFromUnprotectedPtr();
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // Free it.
  allocator.root()->Free(ptr);
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // A raw_ptr<> starts referencing it.
  in_slot_metadata->Acquire();
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // The raw_ptr<> stops referencing it.
  EXPECT_FALSE(in_slot_metadata->ReleaseFromUnprotectedPtr());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // The raw_ptr<T, DisableDanglingPtrDetection> stops referencing it.
  EXPECT_TRUE(in_slot_metadata->Release());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  PartitionAllocFreeForRefCounting(allocator.root()->ObjectToSlotStart(ptr));
}

// Acquire a raw_ptr<T>, and release it before freeing memory. In the
// background, there is one raw_ptr<T, DisableDanglingPtrDetection>. This
// doesn't trigger any dangling raw_ptr<T> checks.
TEST_P(PartitionAllocTest, RawPtrReleasedBeforeFree) {
  if (!UseBRPPool()) {
    return;
  }

  CountDanglingRawPtr dangling_checks;

  void* ptr =
      allocator.root()->Alloc(64 - ExtraAllocSize(allocator), type_name);
  auto* in_slot_metadata =
      allocator.root()->InSlotMetadataPointerFromObjectForTesting(ptr);
  // Acquire a raw_ptr<T, DisableDanglingPtrDetection> and a raw_ptr<>.
  in_slot_metadata->Acquire();
  in_slot_metadata->AcquireFromUnprotectedPtr();
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // Release the raw_ptr<>.
  EXPECT_FALSE(in_slot_metadata->Release());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // Free it.
  allocator.root()->Free(ptr);
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // The raw_ptr<T, DisableDanglingPtrDetection> stop referencing it.
  EXPECT_TRUE(in_slot_metadata->ReleaseFromUnprotectedPtr());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  PartitionAllocFreeForRefCounting(allocator.root()->ObjectToSlotStart(ptr));
}

// Similar to `PartitionAllocTest.DanglingPtr`, but using
// `PartitionRoot::Free<FreeFlags::kSchedulerLoopQuarantine>`.
// 1. `PartitionRoot::Free<kSchedulerLoopQuarantine>`
//   - The allocation is owned by Scheduler-Loop Quarantine.
// 2. `InSlotMetadata::Release`
//   - The allocation is still owned by Scheduler-Loop Quarantine.
// 3. The allocation gets purged from Scheduler-Loop Quarantine.
//   - Actual free happens here.
TEST_P(PartitionAllocTest,
       DanglingPtrReleaseBeforeSchedulerLoopQuarantineExit) {
  if (!UseBRPPool()) {
    return;
  }

  CountDanglingRawPtr dangling_checks;

  // Allocate memory, and reference it from 3 raw_ptr.
  uint64_t* ptr = static_cast<uint64_t*>(
      allocator.root()->Alloc(64 - ExtraAllocSize(allocator), type_name));
  auto* ref_count =
      allocator.root()->InSlotMetadataPointerFromObjectForTesting(ptr);

  ref_count->Acquire();
  ref_count->Acquire();
  ref_count->Acquire();
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // The first raw_ptr stops referencing it, before the memory has been
  // released.
  EXPECT_FALSE(ref_count->Release());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // Free it. This creates two dangling pointer.
  allocator.root()->Free<FreeFlags::kSchedulerLoopQuarantine>(ptr);
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 1);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // The dangling raw_ptr stop referencing it.
  EXPECT_FALSE(ref_count->Release());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 1);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 1);

  // The dangling raw_ptr stop referencing it again.
  // Allocation should not be reclaimed because it is still held by the
  // allocator, in the quarantine.
  EXPECT_FALSE(ref_count->Release());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 1);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 2);

  LightweightQuarantineBranch& branch =
      allocator.root()->GetSchedulerLoopQuarantineBranchForTesting();
  branch.Purge();
}

// Similar to `PartitionAllocTest.DanglingPtr`, but using
// `PartitionRoot::Free<FreeFlags::kSchedulerLoopQuarantine>`.
// 1. `PartitionRoot::Free<kSchedulerLoopQuarantine>`
//   - The allocation is owned by Scheduler-Loop Quarantine.
// 2. The allocation gets purged from Scheduler-Loop Quarantine.
//   - The allocation is now moved to BRP-quarantine.
// 3. `InSlotMetadata::Release`
//   - Actual free happens here.
TEST_P(PartitionAllocTest, DanglingPtrReleaseAfterSchedulerLoopQuarantineExit) {
  if (!UseBRPPool()) {
    return;
  }

  CountDanglingRawPtr dangling_checks;

  // Allocate memory, and reference it from 3 raw_ptr.
  uint64_t* ptr = static_cast<uint64_t*>(
      allocator.root()->Alloc(64 - ExtraAllocSize(allocator), type_name));
  auto* ref_count =
      allocator.root()->InSlotMetadataPointerFromObjectForTesting(ptr);

  ref_count->Acquire();
  ref_count->Acquire();
  ref_count->Acquire();
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // The first raw_ptr stops referencing it, before the memory has been
  // released.
  EXPECT_FALSE(ref_count->Release());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 0);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // Free it. This creates two dangling pointer.
  allocator.root()->Free<FreeFlags::kSchedulerLoopQuarantine>(ptr);
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 1);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 0);

  // The dangling raw_ptr stop referencing it.
  EXPECT_FALSE(ref_count->Release());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 1);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 1);

  LightweightQuarantineBranch& branch =
      allocator.root()->GetSchedulerLoopQuarantineBranchForTesting();
  branch.Purge();

  // The dangling raw_ptr stop referencing it again.
  // Allocation should not be reclaimed because it is still held by the
  // allocator, in the quarantine.
  EXPECT_TRUE(ref_count->Release());
  EXPECT_EQ(g_dangling_raw_ptr_detected_count, 1);
  EXPECT_EQ(g_dangling_raw_ptr_released_count, 2);

  PartitionAllocFreeForRefCounting(allocator.root()->ObjectToSlotStart(ptr));
}

#if PA_USE_DEATH_TESTS()
// DCHECK message are stripped in official build. It causes death tests with
// matchers to fail.
#if !defined(OFFICIAL_BUILD) || !defined(NDEBUG)

// Acquire() once, Release() twice => CRASH
TEST_P(PartitionAllocDeathTest, ReleaseUnderflowRawPtr) {
  if (!UseBRPPool()) {
    return;
  }

  void* ptr =
      allocator.root()->Alloc(64 - ExtraAllocSize(allocator), type_name);
  auto* in_slot_metadata =
      allocator.root()->InSlotMetadataPointerFromObjectForTesting(ptr);
  in_slot_metadata->Acquire();
  EXPECT_FALSE(in_slot_metadata->Release());
  PA_EXPECT_DCHECK_DEATH(in_slot_metadata->Release());
  allocator.root()->Free(ptr);
}

// AcquireFromUnprotectedPtr() once, ReleaseFromUnprotectedPtr() twice => CRASH
TEST_P(PartitionAllocDeathTest, ReleaseUnderflowDanglingPtr) {
  if (!UseBRPPool()) {
    return;
  }

  void* ptr =
      allocator.root()->Alloc(64 - ExtraAllocSize(allocator), type_name);
  auto* in_slot_metadata =
      allocator.root()->InSlotMetadataPointerFromObjectForTesting(ptr);
  in_slot_metadata->AcquireFromUnprotectedPtr();
  EXPECT_FALSE(in_slot_metadata->ReleaseFromUnprotectedPtr());
  PA_EXPECT_DCHECK_DEATH(in_slot_metadata->ReleaseFromUnprotectedPtr());
  allocator.root()->Free(ptr);
}

#endif  //! defined(OFFICIAL_BUILD) || !defined(NDEBUG)
#endif  // PA_USE_DEATH_TESTS()
#endif  // PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)

TEST_P(PartitionAllocTest, ReservationOffset) {
  // For normal buckets, offset should be kOffsetTagNormalBuckets.
  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  uintptr_t address = UntagPtr(ptr);
  EXPECT_EQ(kOffsetTagNormalBuckets, *ReservationOffsetPointer(address));
  allocator.root()->Free(ptr);

  // For direct-map,
  size_t large_size = kSuperPageSize * 5 + PartitionPageSize() * .5f;
  ASSERT_GT(large_size, kMaxBucketed);
  ptr = allocator.root()->Alloc(large_size, type_name);
  EXPECT_TRUE(ptr);
  address = UntagPtr(ptr);
  EXPECT_EQ(0U, *ReservationOffsetPointer(address));
  EXPECT_EQ(1U, *ReservationOffsetPointer(address + kSuperPageSize));
  EXPECT_EQ(2U, *ReservationOffsetPointer(address + kSuperPageSize * 2));
  EXPECT_EQ(3U, *ReservationOffsetPointer(address + kSuperPageSize * 3));
  EXPECT_EQ(4U, *ReservationOffsetPointer(address + kSuperPageSize * 4));
  EXPECT_EQ(5U, *ReservationOffsetPointer(address + kSuperPageSize * 5));

  // In-place realloc doesn't affect the offsets.
  void* new_ptr = allocator.root()->Realloc(ptr, large_size * .8, type_name);
  EXPECT_EQ(new_ptr, ptr);
  EXPECT_EQ(0U, *ReservationOffsetPointer(address));
  EXPECT_EQ(1U, *ReservationOffsetPointer(address + kSuperPageSize));
  EXPECT_EQ(2U, *ReservationOffsetPointer(address + kSuperPageSize * 2));
  EXPECT_EQ(3U, *ReservationOffsetPointer(address + kSuperPageSize * 3));
  EXPECT_EQ(4U, *ReservationOffsetPointer(address + kSuperPageSize * 4));
  EXPECT_EQ(5U, *ReservationOffsetPointer(address + kSuperPageSize * 5));

  allocator.root()->Free(ptr);
  // After free, the offsets must be kOffsetTagNotAllocated.
  EXPECT_EQ(kOffsetTagNotAllocated, *ReservationOffsetPointer(address));
  EXPECT_EQ(kOffsetTagNotAllocated,
            *ReservationOffsetPointer(address + kSuperPageSize));
  EXPECT_EQ(kOffsetTagNotAllocated,
            *ReservationOffsetPointer(address + kSuperPageSize * 2));
  EXPECT_EQ(kOffsetTagNotAllocated,
            *ReservationOffsetPointer(address + kSuperPageSize * 3));
  EXPECT_EQ(kOffsetTagNotAllocated,
            *ReservationOffsetPointer(address + kSuperPageSize * 4));
  EXPECT_EQ(kOffsetTagNotAllocated,
            *ReservationOffsetPointer(address + kSuperPageSize * 5));
}

TEST_P(PartitionAllocTest, GetReservationStart) {
  size_t large_size = kSuperPageSize * 3 + PartitionPageSize() * .5f;
  ASSERT_GT(large_size, kMaxBucketed);
  void* ptr = allocator.root()->Alloc(large_size, type_name);
  EXPECT_TRUE(ptr);
  uintptr_t slot_start = allocator.root()->ObjectToSlotStart(ptr);
  uintptr_t reservation_start = slot_start - PartitionPageSize();
  EXPECT_EQ(0U, reservation_start & DirectMapAllocationGranularityOffsetMask());

  uintptr_t address = UntagPtr(ptr);
  for (uintptr_t a = address; a < address + large_size; ++a) {
    uintptr_t address2 = GetDirectMapReservationStart(a) + PartitionPageSize();
    EXPECT_EQ(slot_start, address2);
  }

  EXPECT_EQ(reservation_start, GetDirectMapReservationStart(slot_start));

  allocator.root()->Free(ptr);
}

#if PA_BUILDFLAG(IS_FUCHSIA)
// TODO: https://crbug.com/331366007 - re-enable on Fuchsia once bug is fixed.
TEST_P(PartitionAllocTest, DISABLED_CheckReservationType) {
#else
TEST_P(PartitionAllocTest, CheckReservationType) {
#endif  // PA_BUILDFLAG(IS_FUCHSIA)
  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  EXPECT_TRUE(ptr);
  uintptr_t address = UntagPtr(ptr);
  uintptr_t address_to_check = address;
  EXPECT_FALSE(IsReservationStart(address_to_check));
  EXPECT_TRUE(
      IsManagedByNormalBucketsForTesting(address_to_check, allocator.root()));
  EXPECT_FALSE(
      IsManagedByDirectMapForTesting(address_to_check, allocator.root()));
  EXPECT_TRUE(IsManagedByNormalBucketsOrDirectMapForTesting(address_to_check,
                                                            allocator.root()));
  address_to_check = address + kTestAllocSize - 1;
  EXPECT_FALSE(IsReservationStart(address_to_check));
  EXPECT_TRUE(
      IsManagedByNormalBucketsForTesting(address_to_check, allocator.root()));
  EXPECT_FALSE(
      IsManagedByDirectMapForTesting(address_to_check, allocator.root()));
  EXPECT_TRUE(IsManagedByNormalBucketsOrDirectMapForTesting(address_to_check,
                                                            allocator.root()));
  address_to_check =
      partition_alloc::internal::base::bits::AlignDown(address, kSuperPageSize);
  EXPECT_TRUE(IsReservationStart(address_to_check));
  EXPECT_TRUE(
      IsManagedByNormalBucketsForTesting(address_to_check, allocator.root()));
  EXPECT_FALSE(
      IsManagedByDirectMapForTesting(address_to_check, allocator.root()));
  EXPECT_TRUE(IsManagedByNormalBucketsOrDirectMapForTesting(address_to_check,
                                                            allocator.root()));
  allocator.root()->Free(ptr);
  // Freeing keeps a normal-bucket super page in memory.
  address_to_check =
      partition_alloc::internal::base::bits::AlignDown(address, kSuperPageSize);
  EXPECT_TRUE(IsReservationStart(address_to_check));
  EXPECT_TRUE(
      IsManagedByNormalBucketsForTesting(address_to_check, allocator.root()));
  EXPECT_FALSE(
      IsManagedByDirectMapForTesting(address_to_check, allocator.root()));
  EXPECT_TRUE(IsManagedByNormalBucketsOrDirectMapForTesting(address_to_check,
                                                            allocator.root()));

  size_t large_size = 2 * kSuperPageSize;
  ASSERT_GT(large_size, kMaxBucketed);
  ptr = allocator.root()->Alloc(large_size, type_name);
  EXPECT_TRUE(ptr);
  address = UntagPtr(ptr);
  address_to_check = address;
  EXPECT_FALSE(IsReservationStart(address_to_check));
  EXPECT_FALSE(
      IsManagedByNormalBucketsForTesting(address_to_check, allocator.root()));
  EXPECT_TRUE(
      IsManagedByDirectMapForTesting(address_to_check, allocator.root()));
  EXPECT_TRUE(IsManagedByNormalBucketsOrDirectMapForTesting(address_to_check,
                                                            allocator.root()));
  address_to_check =
      partition_alloc::internal::base::bits::AlignUp(address, kSuperPageSize);
  EXPECT_FALSE(IsReservationStart(address_to_check));
  EXPECT_FALSE(
      IsManagedByNormalBucketsForTesting(address_to_check, allocator.root()));
  EXPECT_TRUE(
      IsManagedByDirectMapForTesting(address_to_check, allocator.root()));
  EXPECT_TRUE(IsManagedByNormalBucketsOrDirectMapForTesting(address_to_check,
                                                            allocator.root()));
  address_to_check = address + large_size - 1;
  EXPECT_FALSE(IsReservationStart(address_to_check));
  EXPECT_FALSE(
      IsManagedByNormalBucketsForTesting(address_to_check, allocator.root()));
  EXPECT_TRUE(
      IsManagedByDirectMapForTesting(address_to_check, allocator.root()));
  EXPECT_TRUE(IsManagedByNormalBucketsOrDirectMapForTesting(address_to_check,
                                                            allocator.root()));
  address_to_check =
      partition_alloc::internal::base::bits::AlignDown(address, kSuperPageSize);
  EXPECT_TRUE(IsReservationStart(address_to_check));
  EXPECT_FALSE(
      IsManagedByNormalBucketsForTesting(address_to_check, allocator.root()));
  EXPECT_TRUE(
      IsManagedByDirectMapForTesting(address_to_check, allocator.root()));
  EXPECT_TRUE(IsManagedByNormalBucketsOrDirectMapForTesting(address_to_check,
                                                            allocator.root()));
  allocator.root()->Free(ptr);
  // Freeing releases direct-map super pages.
  address_to_check =
      partition_alloc::internal::base::bits::AlignDown(address, kSuperPageSize);

  // DCHECKs don't work with EXPECT_DEATH on official builds.
#if PA_BUILDFLAG(DCHECKS_ARE_ON) && \
    (!defined(OFFICIAL_BUILD) || !defined(NDEBUG))
  // Expect to DCHECK on unallocated region.
  EXPECT_DEATH_IF_SUPPORTED(IsReservationStart(address_to_check), "");
#endif  //  PA_BUILDFLAG(DCHECKS_ARE_ON) && (!defined(OFFICIAL_BUILD) ||
        //  !defined(NDEBUG))

  EXPECT_FALSE(
      IsManagedByNormalBucketsForTesting(address_to_check, allocator.root()));
  EXPECT_FALSE(
      IsManagedByDirectMapForTesting(address_to_check, allocator.root()));
  EXPECT_FALSE(IsManagedByNormalBucketsOrDirectMapForTesting(address_to_check,
                                                             allocator.root()));
}

// Test for crash http://crbug.com/1169003.
TEST_P(PartitionAllocTest, CrossPartitionRootRealloc) {
  // Size is large enough to satisfy it from a single-slot slot span
  size_t test_size = MaxRegularSlotSpanSize() - ExtraAllocSize(allocator);
  void* ptr = allocator.root()->Alloc<AllocFlags::kReturnNull>(test_size);
  EXPECT_TRUE(ptr);

  // Create new root and call PurgeMemory to simulate ConfigurePartitions().
  allocator.root()->PurgeMemory(PurgeFlags::kDecommitEmptySlotSpans |
                                PurgeFlags::kDiscardUnusedSystemPages);
  std::unique_ptr<PartitionRoot> new_root = CreateCustomTestRoot(
      GetCommonPartitionOptions(),
      PartitionTestOptions{.set_bucket_distribution = true});

  // Realloc from |allocator.root()| into |new_root|.
  void* ptr2 = new_root->Realloc<AllocFlags::kReturnNull>(ptr, test_size + 1024,
                                                          nullptr);
  EXPECT_TRUE(ptr2);
  PA_EXPECT_PTR_NE(ptr, ptr2);
}

TEST_P(PartitionAllocTest, FastPathOrReturnNull) {
  size_t allocation_size = 64;
  // The very first allocation is never a fast path one, since it needs a new
  // super page and a new partition page.
  EXPECT_FALSE(allocator.root()->Alloc<AllocFlags::kFastPathOrReturnNull>(
      allocation_size, ""));
  void* ptr = allocator.root()->Alloc(allocation_size);
  ASSERT_TRUE(ptr);

  // Next one is, since the partition page has been activated.
  void* ptr2 = allocator.root()->Alloc<AllocFlags::kFastPathOrReturnNull>(
      allocation_size, "");
  EXPECT_TRUE(ptr2);

  // First allocation of a different bucket is slow.
  EXPECT_FALSE(allocator.root()->Alloc<AllocFlags::kFastPathOrReturnNull>(
      2 * allocation_size, ""));

  size_t allocated_size = 2 * allocation_size;
  std::vector<void*> ptrs;
  while (void* new_ptr =
             allocator.root()->Alloc<AllocFlags::kFastPathOrReturnNull>(
                 allocation_size, "")) {
    ptrs.push_back(new_ptr);
    allocated_size += allocation_size;
  }
  EXPECT_LE(allocated_size,
            PartitionPageSize() * kMaxPartitionPagesPerRegularSlotSpan);

  for (void* ptr_to_free : ptrs) {
    allocator.root()->Free<FreeFlags::kNoHooks>(ptr_to_free);
  }

  allocator.root()->Free<FreeFlags::kNoHooks>(ptr);
  allocator.root()->Free<FreeFlags::kNoHooks>(ptr2);
}

#if PA_USE_DEATH_TESTS()
// DCHECK message are stripped in official build. It causes death tests with
// matchers to fail.
#if !defined(OFFICIAL_BUILD) || !defined(NDEBUG)

TEST_P(PartitionAllocDeathTest, CheckTriggered) {
  PA_EXPECT_DCHECK_DEATH_WITH(PA_CHECK(5 == 7), "Check failed.*5 == 7");
  EXPECT_DEATH(PA_CHECK(5 == 7), "Check failed.*5 == 7");
}

#endif  // !defined(OFFICIAL_BUILD) && !defined(NDEBUG)
#endif  // PA_USE_DEATH_TESTS()

// Not on chromecast, since gtest considers extra output from itself as a test
// failure:
// https://ci.chromium.org/ui/p/chromium/builders/ci/Cast%20Audio%20Linux/98492/overview
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && PA_USE_DEATH_TESTS() && \
    !PA_BUILDFLAG(IS_CASTOS)

namespace {

PA_NOINLINE void FreeForTest(void* data) {
  free(data);
}

class ThreadDelegateForPreforkHandler
    : public base::PlatformThreadForTesting::Delegate {
 public:
  ThreadDelegateForPreforkHandler(std::atomic<bool>& please_stop,
                                  std::atomic<int>& started_threads,
                                  const int alloc_size)
      : please_stop_(please_stop),
        started_threads_(started_threads),
        alloc_size_(alloc_size) {}

  void ThreadMain() override {
    started_threads_++;
    while (!please_stop_.load(std::memory_order_relaxed)) {
      void* ptr = malloc(alloc_size_);

      // A simple malloc() / free() pair can be discarded by the compiler (and
      // is), making the test fail. It is sufficient to make |FreeForTest()| a
      // PA_NOINLINE function for the call to not be eliminated, but it is
      // required.
      FreeForTest(ptr);
    }
  }

 private:
  std::atomic<bool>& please_stop_;
  std::atomic<int>& started_threads_;
  const int alloc_size_;
};

}  // namespace

// Disabled because executing it causes Gtest to show a warning in the output,
// which confuses the runner on some platforms, making the test report an
// "UNKNOWN" status even though it succeeded.
TEST_P(PartitionAllocTest, DISABLED_PreforkHandler) {
  std::atomic<bool> please_stop;
  std::atomic<int> started_threads{0};

  // Continuously allocates / frees memory, bypassing the thread cache. This
  // makes it likely that this thread will own the lock, and that the
  // EXPECT_EXIT() part will deadlock.
  constexpr size_t kAllocSize = ThreadCache::kLargeSizeThreshold + 1;
  ThreadDelegateForPreforkHandler delegate(please_stop, started_threads,
                                           kAllocSize);

  constexpr int kThreads = 4;
  base::PlatformThreadHandle thread_handles[kThreads];
  for (auto& thread_handle : thread_handles) {
    base::PlatformThreadForTesting::Create(0, &delegate, &thread_handle);
  }
  // Make sure all threads are actually already running.
  while (started_threads != kThreads) {
  }

  EXPECT_EXIT(
      {
        void* ptr = malloc(kAllocSize);
        FreeForTest(ptr);
        exit(1);
      },
      ::testing::ExitedWithCode(1), "");

  please_stop.store(true);
  for (auto& thread_handle : thread_handles) {
    base::PlatformThreadForTesting::Join(thread_handle);
  }
}

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // PA_USE_DEATH_TESTS() && !PA_BUILDFLAG(IS_CASTOS)

// Checks the bucket index logic.
TEST_P(PartitionAllocTest, GetIndex) {
  BucketIndexLookup lookup{};

  for (size_t size = 0; size < kMaxBucketed; size++) {
    size_t index = BucketIndexLookup::GetIndex(size);
    ASSERT_GE(lookup.bucket_sizes()[index], size);
  }

  // Make sure that power-of-two have exactly matching buckets.
  for (size_t size = (1 << (kMinBucketedOrder - 1)); size < kMaxBucketed;
       size <<= 1) {
    size_t index = BucketIndexLookup::GetIndex(size);
    ASSERT_EQ(lookup.bucket_sizes()[index], size);
  }
}

// Used to check alignment. If the compiler understands the annotations, the
// zeroing in the constructor uses aligned SIMD instructions.
TEST_P(PartitionAllocTest, MallocFunctionAnnotations) {
  struct TestStruct {
    uint64_t a = 0;
    uint64_t b = 0;
  };

  void* buffer = Alloc(sizeof(TestStruct));
  // Should use "mov*a*ps" on x86_64.
  auto* x = new (buffer) TestStruct();

  EXPECT_EQ(x->a, 0u);
  Free(buffer);
}

// Test that the ConfigurablePool works properly.
TEST_P(PartitionAllocTest, ConfigurablePool) {
  EXPECT_FALSE(IsConfigurablePoolAvailable());

  // The rest is only applicable to 64-bit mode
#if PA_BUILDFLAG(PA_ARCH_CPU_64_BITS)
  // Repeat the test for every possible Pool size
  const size_t max_pool_size = PartitionAddressSpace::ConfigurablePoolMaxSize();
  const size_t min_pool_size = PartitionAddressSpace::ConfigurablePoolMinSize();
  for (size_t pool_size = max_pool_size; pool_size >= min_pool_size;
       pool_size /= 2) {
    PA_DCHECK(base::bits::HasSingleBit(pool_size));
    EXPECT_FALSE(IsConfigurablePoolAvailable());
    uintptr_t pool_base =
        AllocPages(pool_size, pool_size,
                   PageAccessibilityConfiguration(
                       PageAccessibilityConfiguration::kInaccessible),
                   PageTag::kPartitionAlloc);
    EXPECT_NE(0u, pool_base);
    PartitionAddressSpace::InitConfigurablePool(pool_base, pool_size);

    EXPECT_TRUE(IsConfigurablePoolAvailable());

    PartitionOptions opts = GetCommonPartitionOptions();
    opts.use_configurable_pool = PartitionOptions::kAllowed;
    std::unique_ptr<PartitionRoot> root = CreateCustomTestRoot(
        opts, PartitionTestOptions{.uncap_empty_slot_span_memory = true,
                                   .set_bucket_distribution = true});

    const size_t count = 250;
    std::vector<void*> allocations(count, nullptr);
    for (size_t i = 0; i < count; ++i) {
      const size_t size = kTestSizes[base::RandGenerator(kTestSizesCount)];
      allocations[i] = root->Alloc(size);
      EXPECT_NE(nullptr, allocations[i]);
      // We don't Untag allocations here because MTE is disabled for
      // configurable pools used by V8.
      // https://bugs.chromium.org/p/v8/issues/detail?id=13117
      uintptr_t allocation_base = reinterpret_cast<uintptr_t>(allocations[i]);
      EXPECT_EQ(allocation_base, UntagPtr(allocations[i]));
      EXPECT_TRUE(allocation_base >= pool_base &&
                  allocation_base < pool_base + pool_size);
    }

    PartitionAddressSpace::UninitConfigurablePoolForTesting();
    FreePages(pool_base, pool_size);
  }

#endif  // PA_BUILDFLAG(PA_ARCH_CPU_64_BITS)
}

TEST_P(PartitionAllocTest, EmptySlotSpanSizeIsCapped) {
  // Use another root, since the ones from the test harness disable the empty
  // slot span size cap.
  std::unique_ptr<PartitionRoot> root = CreateCustomTestRoot(
      GetCommonPartitionOptions(),
      PartitionTestOptions{.set_bucket_distribution = true});

  // Allocate some memory, don't free it to keep committed memory.
  std::vector<void*> allocated_memory;
  const size_t size = SystemPageSize();
  const size_t count = 400;
  for (size_t i = 0; i < count; i++) {
    void* ptr = root->Alloc(size);
    allocated_memory.push_back(ptr);
  }
  ASSERT_GE(root->total_size_of_committed_pages.load(std::memory_order_relaxed),
            size * count);

  // To create empty slot spans, allocate from single-slot slot spans, 128kiB at
  // a time.
  std::vector<void*> single_slot_allocated_memory;
  constexpr size_t single_slot_count = kDefaultEmptySlotSpanRingSize - 1;
  const size_t single_slot_size = MaxRegularSlotSpanSize() + 1;
  // Make sure that even with allocation size rounding up, a single allocation
  // is still below the threshold.
  ASSERT_LT(MaxRegularSlotSpanSize() * 2,
            ((count * size) >> root->max_empty_slot_spans_dirty_bytes_shift));
  for (size_t i = 0; i < single_slot_count; i++) {
    void* ptr = root->Alloc(single_slot_size);
    single_slot_allocated_memory.push_back(ptr);
  }

  // Free everything at once, creating as many empty slot spans as there are
  // allocations (since they are from single-slot slot spans).
  for (void* ptr : single_slot_allocated_memory) {
    root->Free(ptr);
  }

  // Still have some committed empty slot spans.
  // PA_TS_UNCHECKED_READ() is not an issue here, since everything is
  // single-threaded.
  EXPECT_GT(PA_TS_UNCHECKED_READ(root->empty_slot_spans_dirty_bytes), 0u);
  // But not all, as the cap triggered.
  EXPECT_LT(PA_TS_UNCHECKED_READ(root->empty_slot_spans_dirty_bytes),
            single_slot_count * single_slot_size);

  // Nothing left after explicit purge.
  root->PurgeMemory(PurgeFlags::kDecommitEmptySlotSpans);
  EXPECT_EQ(PA_TS_UNCHECKED_READ(root->empty_slot_spans_dirty_bytes), 0u);

  for (void* ptr : allocated_memory) {
    root->Free(ptr);
  }
}

TEST_P(PartitionAllocTest, IncreaseEmptySlotSpanRingSize) {
  std::unique_ptr<PartitionRoot> root = CreateCustomTestRoot(
      GetCommonPartitionOptions(),
      PartitionTestOptions{.uncap_empty_slot_span_memory = true,
                           .set_bucket_distribution = true});

  std::vector<void*> single_slot_allocated_memory;
  constexpr size_t single_slot_count = kDefaultEmptySlotSpanRingSize + 10;
  const size_t single_slot_size = MaxRegularSlotSpanSize() + 1;
  const size_t bucket_size =
      root->buckets[SizeToIndex(single_slot_size)].slot_size;

  for (size_t i = 0; i < single_slot_count; i++) {
    void* ptr = root->Alloc(single_slot_size);
    single_slot_allocated_memory.push_back(ptr);
  }

  // Free everything at once, creating as many empty slot spans as there are
  // allocations (since they are from single-slot slot spans).
  for (void* ptr : single_slot_allocated_memory) {
    root->Free(ptr);
  }
  single_slot_allocated_memory.clear();

  // Some of the free()-s above overflowed the slot span ring.
  EXPECT_EQ(PA_TS_UNCHECKED_READ(root->empty_slot_spans_dirty_bytes),
            kDefaultEmptySlotSpanRingSize * bucket_size);

  // Now can cache more slot spans.
  root->EnableLargeEmptySlotSpanRing();

  constexpr size_t single_slot_large_count = kDefaultEmptySlotSpanRingSize + 10;
  // The assertion following the alloc/free checks that the ring contains the
  // slots spans for the allocations done here. Slot spans that have not yet
  // been added to the ring are added at
  // `PartitionRoot::global_empty_slot_span_ring_index`. By iterating twice,
  // we ensure the ring contains the allocations here. This is because the
  // first time through the empty slot span may decommit one of the allocations
  // done here, the second time through that won't happen (because
  // `global_empty_slot_span_ring_index` will have incremented past
  // `kDefaultEmptySlotSpanRingSize`, and the frees in the second iteration
  // won't decommit one of the allocations here).
  for (int x = 0; x < 2; ++x) {
    for (size_t i = 0; i < single_slot_large_count; i++) {
      void* ptr = root->Alloc(single_slot_size);
      single_slot_allocated_memory.push_back(ptr);
    }

    for (void* ptr : single_slot_allocated_memory) {
      root->Free(ptr);
    }
    single_slot_allocated_memory.clear();
  }

  EXPECT_EQ(PA_TS_UNCHECKED_READ(root->empty_slot_spans_dirty_bytes),
            single_slot_large_count * bucket_size);

  // Constants used here don't work with USE_LARGE_EMPTY_SLOT_SPAN_RING.
#if !PA_BUILDFLAG(USE_LARGE_EMPTY_SLOT_SPAN_RING)
  constexpr size_t single_slot_too_many_count = kMaxEmptySlotSpanRingSize + 10;
  for (size_t i = 0; i < single_slot_too_many_count; i++) {
    void* ptr = root->Alloc(single_slot_size);
    single_slot_allocated_memory.push_back(ptr);
  }

  for (void* ptr : single_slot_allocated_memory) {
    root->Free(ptr);
  }
  single_slot_allocated_memory.clear();

  // Overflow still works.
  EXPECT_EQ(PA_TS_UNCHECKED_READ(root->empty_slot_spans_dirty_bytes),
            kMaxEmptySlotSpanRingSize * bucket_size);
#endif
}

#if PA_BUILDFLAG(IS_CAST_ANDROID) && PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
extern "C" {
void* __real_malloc(size_t);
}  // extern "C"

TEST_P(PartitionAllocTest, HandleMixedAllocations) {
  void* ptr = __real_malloc(12);
  // Should not crash, no test assertion.
  free(ptr);
}
#endif

TEST_P(PartitionAllocTest, SortFreelist) {
  const size_t count = 100;
  const size_t allocation_size = 1;
  void* first_ptr = allocator.root()->Alloc(allocation_size);

  std::vector<void*> allocations;
  for (size_t i = 0; i < count; ++i) {
    allocations.push_back(allocator.root()->Alloc(allocation_size));
  }

  // Shuffle and free memory out of order.
  std::random_device rd;
  std::mt19937 generator(rd());
  std::shuffle(allocations.begin(), allocations.end(), generator);

  // Keep one allocation alive (first_ptr), so that the SlotSpan is not fully
  // empty.
  for (void* ptr : allocations) {
    allocator.root()->Free(ptr);
  }
  allocations.clear();

  allocator.root()->PurgeMemory(PurgeFlags::kDiscardUnusedSystemPages);

  size_t bucket_index =
      SizeToIndex(allocation_size + ExtraAllocSize(allocator));
  auto& bucket = allocator.root()->buckets[bucket_index];
  EXPECT_TRUE(bucket.active_slot_spans_head->freelist_is_sorted());

  // Can sort again.
  allocator.root()->PurgeMemory(PurgeFlags::kDiscardUnusedSystemPages);
  EXPECT_TRUE(bucket.active_slot_spans_head->freelist_is_sorted());

  for (size_t i = 0; i < count; ++i) {
    allocations.push_back(allocator.root()->Alloc(allocation_size));
    // Allocating keeps the freelist sorted.
    EXPECT_TRUE(bucket.active_slot_spans_head->freelist_is_sorted());
  }

  // Check that it is sorted.
  for (size_t i = 1; i < allocations.size(); i++) {
    EXPECT_LT(UntagPtr(allocations[i - 1]), UntagPtr(allocations[i]));
  }

  for (void* ptr : allocations) {
    allocator.root()->Free(ptr);
    // Free()-ing memory destroys order.  Not looking at the head of the active
    // list, as it is not necessarily the one from which |ptr| came from.
    auto* slot_span =
        SlotSpan::FromSlotStart(allocator.root()->ObjectToSlotStart(ptr));
    EXPECT_FALSE(slot_span->freelist_is_sorted());
  }

  allocator.root()->Free(first_ptr);
}

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && PA_BUILDFLAG(IS_LINUX) && \
    PA_BUILDFLAG(PA_ARCH_CPU_64_BITS)
TEST_P(PartitionAllocTest, CrashOnUnknownPointer) {
  int not_a_heap_object = 42;
  EXPECT_DEATH(allocator.root()->Free(&not_a_heap_object), "");
}
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // PA_BUILDFLAG(IS_LINUX) && PA_BUILDFLAG(PA_ARCH_CPU_64_BITS)

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && PA_BUILDFLAG(IS_MAC)

// Adapted from crashpad tests.
class ScopedOpenCLNoOpKernel {
 public:
  ScopedOpenCLNoOpKernel()
      : context_(nullptr),
        program_(nullptr),
        kernel_(nullptr),
        success_(false) {}

  ScopedOpenCLNoOpKernel(const ScopedOpenCLNoOpKernel&) = delete;
  ScopedOpenCLNoOpKernel& operator=(const ScopedOpenCLNoOpKernel&) = delete;

  ~ScopedOpenCLNoOpKernel() {
    if (kernel_) {
      cl_int rv = clReleaseKernel(kernel_);
      EXPECT_EQ(rv, CL_SUCCESS) << "clReleaseKernel";
    }

    if (program_) {
      cl_int rv = clReleaseProgram(program_);
      EXPECT_EQ(rv, CL_SUCCESS) << "clReleaseProgram";
    }

    if (context_) {
      cl_int rv = clReleaseContext(context_);
      EXPECT_EQ(rv, CL_SUCCESS) << "clReleaseContext";
    }
  }

  void SetUp() {
    cl_platform_id platform_id;
    cl_int rv = clGetPlatformIDs(1, &platform_id, nullptr);
    ASSERT_EQ(rv, CL_SUCCESS) << "clGetPlatformIDs";
    cl_device_id device_id;
    rv =
        clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_CPU, 1, &device_id, nullptr);
#if PA_BUILDFLAG(PA_ARCH_CPU_ARM64)
    // CL_DEVICE_TYPE_CPU doesn’t seem to work at all on arm64, meaning that
    // these weird OpenCL modules probably don’t show up there at all. Keep this
    // test even on arm64 in case this ever does start working.
    if (rv == CL_INVALID_VALUE) {
      return;
    }
#endif  // ARCH_CPU_ARM64
    ASSERT_EQ(rv, CL_SUCCESS) << "clGetDeviceIDs";

    context_ = clCreateContext(nullptr, 1, &device_id, nullptr, nullptr, &rv);
    ASSERT_EQ(rv, CL_SUCCESS) << "clCreateContext";

    const char* sources[] = {
        "__kernel void NoOp(void) {barrier(CLK_LOCAL_MEM_FENCE);}",
    };
    const size_t source_lengths[] = {
        strlen(sources[0]),
    };
    static_assert(std::size(sources) == std::size(source_lengths),
                  "arrays must be parallel");

    program_ = clCreateProgramWithSource(context_, std::size(sources), sources,
                                         source_lengths, &rv);
    ASSERT_EQ(rv, CL_SUCCESS) << "clCreateProgramWithSource";

    rv = clBuildProgram(program_, 1, &device_id, "-cl-opt-disable", nullptr,
                        nullptr);
    ASSERT_EQ(rv, CL_SUCCESS) << "clBuildProgram";

    kernel_ = clCreateKernel(program_, "NoOp", &rv);
    ASSERT_EQ(rv, CL_SUCCESS) << "clCreateKernel";

    success_ = true;
  }

  bool success() const { return success_; }

 private:
  cl_context context_;
  cl_program program_;
  cl_kernel kernel_;
  bool success_;
};

// On macOS 10.11, allocations are made with PartitionAlloc, but the pointer
// is incorrectly passed by CoreFoundation to the previous default zone,
// causing crashes. This is intended to detect these issues regressing in future
// versions of macOS.
TEST_P(PartitionAllocTest, OpenCL) {
  ScopedOpenCLNoOpKernel kernel;
  kernel.SetUp();
#if !PA_BUILDFLAG(PA_ARCH_CPU_ARM64)
  ASSERT_TRUE(kernel.success());
#endif
}

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // PA_BUILDFLAG(IS_MAC)

TEST_P(PartitionAllocTest, SmallSlotSpanWaste) {
  for (PartitionRoot::Bucket& bucket : allocator.root()->buckets) {
    const size_t slot_size = bucket.slot_size;
    if (slot_size == kInvalidBucketSize) {
      continue;
    }

    size_t small_system_page_count =
        partition_alloc::internal::ComputeSystemPagesPerSlotSpan(
            bucket.slot_size, true);
    size_t small_waste =
        (small_system_page_count * SystemPageSize()) % slot_size;

    EXPECT_LT(small_waste, .05 * SystemPageSize());
    if (slot_size <= MaxRegularSlotSpanSize()) {
      EXPECT_LE(small_system_page_count, MaxSystemPagesPerRegularSlotSpan());
    }
  }
}

TEST_P(PartitionAllocTest, SortActiveSlotSpans) {
  auto run_test = [this](size_t count) {
    PartitionBucket bucket;
    bucket.Init(16, /*use_small_single_slot_spans=*/false);
    bucket.active_slot_spans_head = nullptr;

#if PA_CONFIG(ENABLE_SHADOW_METADATA)
    // There is 1 metadata area (whose size is SystemPageSize()) per 1
    // SuperPage.
    const size_t num_slotspans_per_superpage =
        SystemPageSize() / sizeof(SlotSpan);
    std::vector<void*> superpages;
    std::vector<const SlotSpan*> slot_spans;

    while (count > 0) {
      // Allocate 2 * kSuperPageSize memory to obtain kSuperPageSize-aligned
      // kSuperPageSize memory.
      void* ptr = allocator.root()->Alloc(kSuperPageSize * 2);
      ASSERT_TRUE(ptr);
      superpages.push_back(ptr);

      uintptr_t super_page = partition_alloc::internal::base::bits::AlignUp(
          reinterpret_cast<uintptr_t>(ptr), kSuperPageSize);
      // Readonly metadata area.
      uintptr_t metadata = super_page + SystemPageSize();

      // Do shared mapping between readonly metadata and writable metadata.
      uintptr_t writable_metadata;
      if (PartitionAddressSpace::IsShadowMetadataEnabled(
              allocator.root()->ChoosePool())) {
        PartitionAddressSpace::MapMetadata(super_page, false);
        writable_metadata = metadata + allocator.root()->ShadowPoolOffset();
      } else {
        writable_metadata = metadata;
      }

      // Initialize the slot spans inside the writable metadata area.
      // But slot_spans must contain the readonly slot spans.
      size_t loop_count = std::min(num_slotspans_per_superpage, count);
      for (size_t j = 0; j < loop_count; ++j) {
        size_t offset = sizeof(SlotSpanMetadata<MetadataKind::kWritable>) * j;
        SlotSpanMetadata<MetadataKind::kWritable>* writable_slot_span =
            new (reinterpret_cast<void*>(writable_metadata + offset))
                SlotSpanMetadata<MetadataKind::kWritable>(&bucket);

        SlotSpan* slot_span = reinterpret_cast<SlotSpan*>(metadata + offset);
        slot_spans.push_back(slot_span);

        writable_slot_span->num_unprovisioned_slots =
            partition_alloc::internal::base::RandGenerator(
                bucket.get_slots_per_span() / 2);
        writable_slot_span->num_allocated_slots =
            partition_alloc::internal::base::RandGenerator(
                bucket.get_slots_per_span() -
                slot_span->num_unprovisioned_slots);
        writable_slot_span->next_slot_span = bucket.active_slot_spans_head;
        PA_DCHECK(slot_span->next_slot_span == bucket.active_slot_spans_head);
        bucket.active_slot_spans_head = slot_span;
      }

      count -= loop_count;
    }
#else
    std::vector<SlotSpan> slot_spans;
    slot_spans.reserve(count);

    // Add slot spans with random freelist length.
    for (size_t i = 0; i < count; i++) {
      slot_spans.emplace_back(&bucket);
      auto& slot_span = slot_spans.back();
      slot_span.ToWritable(nullptr)->num_unprovisioned_slots =
          partition_alloc::internal::base::RandGenerator(
              bucket.get_slots_per_span() / 2);
      slot_span.ToWritable(nullptr)->num_allocated_slots =
          partition_alloc::internal::base::RandGenerator(
              bucket.get_slots_per_span() - slot_span.num_unprovisioned_slots);
      slot_span.ToWritable(nullptr)->next_slot_span =
          bucket.active_slot_spans_head;
      bucket.active_slot_spans_head = &slot_span;
    }
#endif  // PA_CONFIG(ENABLE_SHADOW_METADATA)

    bucket.SortActiveSlotSpans(allocator.root());

    std::set<const SlotSpan*> seen_slot_spans;
    std::vector<const SlotSpan*> sorted_slot_spans;
    for (auto* slot_span = bucket.active_slot_spans_head; slot_span;
         slot_span = slot_span->next_slot_span) {
      sorted_slot_spans.push_back(slot_span);
      seen_slot_spans.insert(slot_span);
    }

    // None repeated, none missing.
    EXPECT_EQ(seen_slot_spans.size(), sorted_slot_spans.size());
    EXPECT_EQ(seen_slot_spans.size(), slot_spans.size());

    // The first slot spans are sorted.
    size_t sorted_spans_count =
        std::min(PartitionBucket::kMaxSlotSpansToSort, count);
    EXPECT_TRUE(std::is_sorted(sorted_slot_spans.begin(),
                               sorted_slot_spans.begin() + sorted_spans_count,
                               partition_alloc::internal::CompareSlotSpans));

    // Slot spans with no freelist entries are at the end of the sorted run.
    auto has_empty_freelist = [](const SlotSpan* a) {
      return a->GetFreelistLength() == 0;
    };
    auto it = std::find_if(sorted_slot_spans.begin(),
                           sorted_slot_spans.begin() + sorted_spans_count,
                           has_empty_freelist);
    if (it != sorted_slot_spans.end()) {
      EXPECT_TRUE(std::all_of(it,
                              sorted_slot_spans.begin() + sorted_spans_count,
                              has_empty_freelist));
    }
#if PA_CONFIG(ENABLE_SHADOW_METADATA)
    for (void* ptr : superpages) {
      allocator.root()->Free(ptr);
    }
#endif  // PA_CONFIG(ENABLE_SHADOW_METADATA)
  };

  // Everything is sorted.
  run_test(PartitionBucket::kMaxSlotSpansToSort / 2);
  // Only the first slot spans are sorted.
  run_test(PartitionBucket::kMaxSlotSpansToSort * 2);

  // Corner cases.
  run_test(0);
  run_test(1);
}

#if PA_BUILDFLAG(USE_FREESLOT_BITMAP)
TEST_P(PartitionAllocTest, FreeSlotBitmapMarkedAsUsedAfterAlloc) {
  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  uintptr_t slot_start = allocator.root()->ObjectToSlotStart(ptr);
  EXPECT_TRUE(FreeSlotBitmapSlotIsUsed(slot_start));

  allocator.root()->Free(ptr);
}

TEST_P(PartitionAllocTest, FreeSlotBitmapMarkedAsFreeAfterFree) {
  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  uintptr_t slot_start = allocator.root()->ObjectToSlotStart(ptr);
  EXPECT_TRUE(FreeSlotBitmapSlotIsUsed(slot_start));

  allocator.root()->Free(ptr);
  EXPECT_FALSE(FreeSlotBitmapSlotIsUsed(slot_start));
}

TEST_P(PartitionAllocTest, FreeSlotBitmapResetAfterDecommit) {
  void* ptr1 = allocator.root()->Alloc(
      SystemPageSize() - ExtraAllocSize(allocator), type_name);
  uintptr_t slot_start = allocator.root()->ObjectToSlotStart(ptr1);
  allocator.root()->Free(ptr1);

  EXPECT_FALSE(FreeSlotBitmapSlotIsUsed(slot_start));
  // Decommit the slot span. Bitmap will be rewritten in Decommit().
  allocator.root()->PurgeMemory(PurgeFlags::kDecommitEmptySlotSpans);
  EXPECT_TRUE(FreeSlotBitmapSlotIsUsed(slot_start));
}

TEST_P(PartitionAllocTest, FreeSlotBitmapResetAfterPurge) {
  void* ptr1 = allocator.root()->Alloc(
      SystemPageSize() - ExtraAllocSize(allocator), type_name);
  char* ptr2 = static_cast<char*>(allocator.root()->Alloc(
      SystemPageSize() - ExtraAllocSize(allocator), type_name));
  uintptr_t slot_start = allocator.root()->ObjectToSlotStart(ptr2);
  allocator.root()->Free(ptr2);

  CHECK_PAGE_IN_CORE(ptr2 - kPointerOffset, true);
  EXPECT_FALSE(FreeSlotBitmapSlotIsUsed(slot_start));
  // Bitmap will be rewritten in PartitionPurgeSlotSpan().
  allocator.root()->PurgeMemory(PurgeFlags::kDiscardUnusedSystemPages);
  CHECK_PAGE_IN_CORE(ptr2 - kPointerOffset, false);
  EXPECT_TRUE(FreeSlotBitmapSlotIsUsed(slot_start));

  allocator.root()->Free(ptr1);
}

#endif  // PA_BUILDFLAG(USE_FREESLOT_BITMAP)

#if PA_BUILDFLAG(USE_LARGE_EMPTY_SLOT_SPAN_RING)
TEST_P(PartitionAllocTest, GlobalEmptySlotSpanRingIndexResets) {
  // Switch to the larger slot span size, and set the
  // global_empty_slot_span_ring_index to one less than max.
  allocator.root()->AdjustForForeground();
  allocator.root()->SetGlobalEmptySlotSpanRingIndexForTesting(
      internal::kMaxEmptySlotSpanRingSize - 1);

  // Switch to the smaller size, allocate, free, and clear the empty cache.
  allocator.root()->AdjustForBackground();
  void* ptr = allocator.root()->Alloc(kTestAllocSize, type_name);
  allocator.root()->Free(ptr);
  ClearEmptySlotSpanCache();

  // This should result in 0 empty_slot_span_dirty_bytes, and more importantly,
  // not crash.
  EXPECT_EQ(
      0u, PA_TS_UNCHECKED_READ(allocator.root()->empty_slot_spans_dirty_bytes));
}
#endif

TEST_P(PartitionAllocTest, FastReclaim) {
  static base::TimeTicks now = base::TimeTicks();
  // Advances times by the same amount every time.
  allocator.root()->now_maybe_overridden_for_testing = [] {
    now += PartitionRoot::kMaxPurgeDuration / 10;
    return now;
  };

  constexpr int kFlags = PurgeFlags::kDecommitEmptySlotSpans |
                         PurgeFlags::kDiscardUnusedSystemPages;
  allocator.root()->PurgeMemory(kFlags);
  ASSERT_GT(now, base::TimeTicks());
  // Here and below, using PA_TS_UNCHECKED_READ since the root is not used
  // conccurently.
  //
  // Went around all buckets.
  EXPECT_EQ(PA_TS_UNCHECKED_READ(allocator.root()->purge_next_bucket_index),
            0u);

  allocator.root()->PurgeMemory(kFlags | PurgeFlags::kLimitDuration);
  // Ran out of time.
  unsigned int next_bucket =
      PA_TS_UNCHECKED_READ(allocator.root()->purge_next_bucket_index);
  EXPECT_NE(next_bucket, 0u);
  allocator.root()->PurgeMemory(kFlags | PurgeFlags::kLimitDuration);
  // Make some progress, but not through all buckets yet.
  EXPECT_GT(PA_TS_UNCHECKED_READ(allocator.root()->purge_next_bucket_index),
            next_bucket);

  allocator.root()->PurgeMemory(kFlags | PurgeFlags::kLimitDuration);
  // Ran out of time.
  EXPECT_NE(PA_TS_UNCHECKED_READ(allocator.root()->purge_next_bucket_index),
            0u);

  // But eventually we make it through all buckets.
  while (PA_TS_UNCHECKED_READ(allocator.root()->purge_next_bucket_index) != 0) {
    allocator.root()->PurgeMemory(kFlags | PurgeFlags::kLimitDuration);
  }
  // No expectation, test will time out if it's incorrect.

  allocator.root()->now_maybe_overridden_for_testing = base::TimeTicks::Now;
}

TEST_P(PartitionAllocTest, FastReclaimEventuallyLooksAtAllBuckets) {
  static base::TimeTicks now = base::TimeTicks();
  // Advances times by the same amount every time.
  allocator.root()->now_maybe_overridden_for_testing = [] {
    now += PartitionRoot::kMaxPurgeDuration / 10;
    return now;
  };

  constexpr int kFlags = PurgeFlags::kDecommitEmptySlotSpans |
                         PurgeFlags::kDiscardUnusedSystemPages;
  allocator.root()->PurgeMemory(kFlags);
  ASSERT_GT(now, base::TimeTicks());
  // Here and below, using PA_TS_UNCHECKED_READ since the root is not used
  // conccurently.
  //
  // Went around all buckets, generation is incremented.
  EXPECT_EQ(PA_TS_UNCHECKED_READ(allocator.root()->purge_next_bucket_index),
            0u);
  EXPECT_EQ(PA_TS_UNCHECKED_READ(allocator.root()->purge_generation), 1u);

  allocator.root()->PurgeMemory(kFlags | PurgeFlags::kLimitDuration);
  // Ran out of time.
  unsigned int next_bucket =
      PA_TS_UNCHECKED_READ(allocator.root()->purge_next_bucket_index);
  EXPECT_NE(next_bucket, 0u);
  allocator.root()->PurgeMemory(kFlags | PurgeFlags::kLimitDuration);
  // Make some progress, but not through all buckets yet.
  EXPECT_GT(PA_TS_UNCHECKED_READ(allocator.root()->purge_next_bucket_index),
            next_bucket);
  EXPECT_EQ(PA_TS_UNCHECKED_READ(allocator.root()->purge_generation), 1u);

  allocator.root()->PurgeMemory(kFlags | PurgeFlags::kLimitDuration);
  // Ran out of time.
  EXPECT_NE(PA_TS_UNCHECKED_READ(allocator.root()->purge_next_bucket_index),
            0u);

  // But eventually we make it through all generations.
  while (PA_TS_UNCHECKED_READ(allocator.root()->purge_generation) != 0) {
    allocator.root()->PurgeMemory(kFlags | PurgeFlags::kLimitDuration);
  }
  EXPECT_EQ(PA_TS_UNCHECKED_READ(allocator.root()->purge_next_bucket_index),
            0u);

  allocator.root()->now_maybe_overridden_for_testing = base::TimeTicks::Now;
}

}  // namespace partition_alloc::internal

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
