// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/persistent_memory_allocator.h"

#include <memory>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/simple_thread.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {

namespace {

const uint32_t TEST_MEMORY_SIZE = 1 << 20;   // 1 MiB
const uint32_t TEST_MEMORY_PAGE = 64 << 10;  // 64 KiB
const uint32_t TEST_ID = 12345;
const char TEST_NAME[] = "TestAllocator";

void SetFileLength(const base::FilePath& path, size_t length) {
  {
    File file(path, File::FLAG_OPEN | File::FLAG_READ | File::FLAG_WRITE);
    DCHECK(file.IsValid());
    ASSERT_TRUE(file.SetLength(static_cast<int64_t>(length)));
  }

  int64_t actual_length;
  DCHECK(GetFileSize(path, &actual_length));
  DCHECK_EQ(length, static_cast<size_t>(actual_length));
}

}  // namespace

typedef PersistentMemoryAllocator::Reference Reference;

class PersistentMemoryAllocatorTest : public testing::Test {
 public:
  // This can't be statically initialized because it's value isn't defined
  // in the PersistentMemoryAllocator header file. Instead, it's simply set
  // in the constructor.
  uint32_t kAllocAlignment;

  struct TestObject1 {
    static constexpr uint32_t kPersistentTypeId = 1;
    static constexpr size_t kExpectedInstanceSize = 4 + 1 + 3;
    int32_t onething;
    char oranother;
  };

  struct TestObject2 {
    static constexpr uint32_t kPersistentTypeId = 2;
    static constexpr size_t kExpectedInstanceSize = 8 + 4 + 4 + 8 + 8;
    int64_t thiis;
    int32_t that;
    float andthe;
    double other;
    char thing[8];
  };

  PersistentMemoryAllocatorTest() {
    kAllocAlignment = GetAllocAlignment();
    mem_segment_.reset(new char[TEST_MEMORY_SIZE]);
  }

  void SetUp() override {
    allocator_.reset();
    ::memset(mem_segment_.get(), 0, TEST_MEMORY_SIZE);
    allocator_ = std::make_unique<PersistentMemoryAllocator>(
        mem_segment_.get(), TEST_MEMORY_SIZE, TEST_MEMORY_PAGE, TEST_ID,
        TEST_NAME, false);
  }

  void TearDown() override {
    allocator_.reset();
  }

  unsigned CountIterables() {
    PersistentMemoryAllocator::Iterator iter(allocator_.get());
    uint32_t type;
    unsigned count = 0;
    while (iter.GetNext(&type) != 0) {
      ++count;
    }
    return count;
  }

  static uint32_t GetAllocAlignment() {
    return PersistentMemoryAllocator::kAllocAlignment;
  }

 protected:
  std::unique_ptr<char[]> mem_segment_;
  std::unique_ptr<PersistentMemoryAllocator> allocator_;
};

TEST_F(PersistentMemoryAllocatorTest, AllocateAndIterate) {
  allocator_->CreateTrackingHistograms(allocator_->Name());

  std::string base_name(TEST_NAME);
  EXPECT_EQ(TEST_ID, allocator_->Id());
  EXPECT_TRUE(allocator_->used_histogram_);
  EXPECT_EQ("UMA.PersistentAllocator." + base_name + ".UsedPct",
            allocator_->used_histogram_->histogram_name());
  EXPECT_EQ(PersistentMemoryAllocator::MEMORY_INITIALIZED,
            allocator_->GetMemoryState());

  // Get base memory info for later comparison.
  PersistentMemoryAllocator::MemoryInfo meminfo0;
  allocator_->GetMemoryInfo(&meminfo0);
  EXPECT_EQ(TEST_MEMORY_SIZE, meminfo0.total);
  EXPECT_GT(meminfo0.total, meminfo0.free);

  // Validate allocation of test object and make sure it can be referenced
  // and all metadata looks correct.
  TestObject1* obj1 = allocator_->New<TestObject1>();
  ASSERT_TRUE(obj1);
  Reference block1 = allocator_->GetAsReference(obj1);
  ASSERT_NE(0U, block1);
  EXPECT_NE(nullptr, allocator_->GetAsObject<TestObject1>(block1));
  EXPECT_EQ(nullptr, allocator_->GetAsObject<TestObject2>(block1));
  EXPECT_LE(sizeof(TestObject1), allocator_->GetAllocSize(block1));
  EXPECT_GT(sizeof(TestObject1) + kAllocAlignment,
            allocator_->GetAllocSize(block1));
  PersistentMemoryAllocator::MemoryInfo meminfo1;
  allocator_->GetMemoryInfo(&meminfo1);
  EXPECT_EQ(meminfo0.total, meminfo1.total);
  EXPECT_GT(meminfo0.free, meminfo1.free);

  // Verify that pointers can be turned back into references and that invalid
  // addresses return null.
  char* memory1 = allocator_->GetAsArray<char>(block1, 1, 1);
  ASSERT_TRUE(memory1);
  EXPECT_EQ(block1, allocator_->GetAsReference(memory1, 0));
  EXPECT_EQ(block1, allocator_->GetAsReference(memory1, 1));
  EXPECT_EQ(0U, allocator_->GetAsReference(memory1, 2));
  EXPECT_EQ(0U, allocator_->GetAsReference(memory1 + 1, 0));
  EXPECT_EQ(0U, allocator_->GetAsReference(memory1 + 16, 0));
  EXPECT_EQ(0U, allocator_->GetAsReference(nullptr, 0));
  EXPECT_EQ(0U, allocator_->GetAsReference(&base_name, 0));

  // Ensure that the test-object can be made iterable.
  PersistentMemoryAllocator::Iterator iter1a(allocator_.get());
  EXPECT_EQ(0U, iter1a.GetLast());
  uint32_t type;
  EXPECT_EQ(0U, iter1a.GetNext(&type));
  allocator_->MakeIterable(block1);
  EXPECT_EQ(block1, iter1a.GetNext(&type));
  EXPECT_EQ(1U, type);
  EXPECT_EQ(block1, iter1a.GetLast());
  EXPECT_EQ(0U, iter1a.GetNext(&type));
  EXPECT_EQ(block1, iter1a.GetLast());

  // Create second test-object and ensure everything is good and it cannot
  // be confused with test-object of another type.
  TestObject2* obj2 = allocator_->New<TestObject2>();
  ASSERT_TRUE(obj2);
  Reference block2 = allocator_->GetAsReference(obj2);
  ASSERT_NE(0U, block2);
  EXPECT_NE(nullptr, allocator_->GetAsObject<TestObject2>(block2));
  EXPECT_EQ(nullptr, allocator_->GetAsObject<TestObject1>(block2));
  EXPECT_LE(sizeof(TestObject2), allocator_->GetAllocSize(block2));
  EXPECT_GT(sizeof(TestObject2) + kAllocAlignment,
            allocator_->GetAllocSize(block2));
  PersistentMemoryAllocator::MemoryInfo meminfo2;
  allocator_->GetMemoryInfo(&meminfo2);
  EXPECT_EQ(meminfo1.total, meminfo2.total);
  EXPECT_GT(meminfo1.free, meminfo2.free);

  // Ensure that second test-object can also be made iterable.
  allocator_->MakeIterable(obj2);
  EXPECT_EQ(block2, iter1a.GetNext(&type));
  EXPECT_EQ(2U, type);
  EXPECT_EQ(block2, iter1a.GetLast());
  EXPECT_EQ(0U, iter1a.GetNext(&type));
  EXPECT_EQ(block2, iter1a.GetLast());

  // Check that the iterator can be reset to the beginning.
  iter1a.Reset();
  EXPECT_EQ(0U, iter1a.GetLast());
  EXPECT_EQ(block1, iter1a.GetNext(&type));
  EXPECT_EQ(block1, iter1a.GetLast());
  EXPECT_EQ(block2, iter1a.GetNext(&type));
  EXPECT_EQ(block2, iter1a.GetLast());
  EXPECT_EQ(0U, iter1a.GetNext(&type));

  // Check that the iterator can be reset to an arbitrary location.
  iter1a.Reset(block1);
  EXPECT_EQ(block1, iter1a.GetLast());
  EXPECT_EQ(block2, iter1a.GetNext(&type));
  EXPECT_EQ(block2, iter1a.GetLast());
  EXPECT_EQ(0U, iter1a.GetNext(&type));

  // Check that iteration can begin after an arbitrary location.
  PersistentMemoryAllocator::Iterator iter1b(allocator_.get(), block1);
  EXPECT_EQ(block2, iter1b.GetNext(&type));
  EXPECT_EQ(0U, iter1b.GetNext(&type));

  // Ensure nothing has gone noticably wrong.
  EXPECT_FALSE(allocator_->IsFull());
  EXPECT_FALSE(allocator_->IsCorrupt());

  // Check the internal histogram record of used memory.
  allocator_->UpdateTrackingHistograms();
  std::unique_ptr<HistogramSamples> used_samples(
      allocator_->used_histogram_->SnapshotSamples());
  EXPECT_TRUE(used_samples);
  EXPECT_EQ(1, used_samples->TotalCount());

  // Check that an object's type can be changed.
  EXPECT_EQ(2U, allocator_->GetType(block2));
  allocator_->ChangeType(block2, 3, 2, false);
  EXPECT_EQ(3U, allocator_->GetType(block2));
  allocator_->New<TestObject2>(block2, 3, false);
  EXPECT_EQ(2U, allocator_->GetType(block2));

  // Create second allocator (read/write) using the same memory segment.
  std::unique_ptr<PersistentMemoryAllocator> allocator2(
      new PersistentMemoryAllocator(mem_segment_.get(), TEST_MEMORY_SIZE,
                                    TEST_MEMORY_PAGE, 0, "", false));
  EXPECT_EQ(TEST_ID, allocator2->Id());
  EXPECT_FALSE(allocator2->used_histogram_);

  // Ensure that iteration and access through second allocator works.
  PersistentMemoryAllocator::Iterator iter2(allocator2.get());
  EXPECT_EQ(block1, iter2.GetNext(&type));
  EXPECT_EQ(block2, iter2.GetNext(&type));
  EXPECT_EQ(0U, iter2.GetNext(&type));
  EXPECT_NE(nullptr, allocator2->GetAsObject<TestObject1>(block1));
  EXPECT_NE(nullptr, allocator2->GetAsObject<TestObject2>(block2));

  // Create a third allocator (read-only) using the same memory segment.
  std::unique_ptr<const PersistentMemoryAllocator> allocator3(
      new PersistentMemoryAllocator(mem_segment_.get(), TEST_MEMORY_SIZE,
                                    TEST_MEMORY_PAGE, 0, "", true));
  EXPECT_EQ(TEST_ID, allocator3->Id());
  EXPECT_FALSE(allocator3->used_histogram_);

  // Ensure that iteration and access through third allocator works.
  PersistentMemoryAllocator::Iterator iter3(allocator3.get());
  EXPECT_EQ(block1, iter3.GetNext(&type));
  EXPECT_EQ(block2, iter3.GetNext(&type));
  EXPECT_EQ(0U, iter3.GetNext(&type));
  EXPECT_NE(nullptr, allocator3->GetAsObject<TestObject1>(block1));
  EXPECT_NE(nullptr, allocator3->GetAsObject<TestObject2>(block2));

  // Ensure that GetNextOfType works.
  PersistentMemoryAllocator::Iterator iter1c(allocator_.get());
  EXPECT_EQ(block2, iter1c.GetNextOfType<TestObject2>());
  EXPECT_EQ(0U, iter1c.GetNextOfType(2));

  // Ensure that GetNextOfObject works.
  PersistentMemoryAllocator::Iterator iter1d(allocator_.get());
  EXPECT_EQ(obj2, iter1d.GetNextOfObject<TestObject2>());
  EXPECT_EQ(nullptr, iter1d.GetNextOfObject<TestObject2>());

  // Ensure that deleting an object works.
  allocator_->Delete(obj2);
  PersistentMemoryAllocator::Iterator iter1z(allocator_.get());
  EXPECT_EQ(nullptr, iter1z.GetNextOfObject<TestObject2>());

  // Ensure that the memory state can be set.
  allocator_->SetMemoryState(PersistentMemoryAllocator::MEMORY_DELETED);
  EXPECT_EQ(PersistentMemoryAllocator::MEMORY_DELETED,
            allocator_->GetMemoryState());
}

TEST_F(PersistentMemoryAllocatorTest, PageTest) {
  // This allocation will go into the first memory page.
  Reference block1 = allocator_->Allocate(TEST_MEMORY_PAGE / 2, 1);
  EXPECT_LT(0U, block1);
  EXPECT_GT(TEST_MEMORY_PAGE, block1);

  // This allocation won't fit in same page as previous block.
  Reference block2 =
      allocator_->Allocate(TEST_MEMORY_PAGE - 2 * kAllocAlignment, 2);
  EXPECT_EQ(TEST_MEMORY_PAGE, block2);

  // This allocation will also require a new page.
  Reference block3 = allocator_->Allocate(2 * kAllocAlignment + 99, 3);
  EXPECT_EQ(2U * TEST_MEMORY_PAGE, block3);
}

// A simple thread that takes an allocator and repeatedly allocates random-
// sized chunks from it until no more can be done.
class AllocatorThread : public SimpleThread {
 public:
  AllocatorThread(const std::string& name,
                  void* base,
                  uint32_t size,
                  uint32_t page_size)
      : SimpleThread(name, Options()),
        count_(0),
        iterable_(0),
        allocator_(base, size, page_size, 0, std::string(), false) {}

  void Run() override {
    for (;;) {
      uint32_t size = RandInt(1, 99);
      uint32_t type = RandInt(100, 999);
      Reference block = allocator_.Allocate(size, type);
      if (!block)
        break;

      count_++;
      if (RandInt(0, 1)) {
        allocator_.MakeIterable(block);
        iterable_++;
      }
    }
  }

  unsigned iterable() { return iterable_; }
  unsigned count() { return count_; }

 private:
  unsigned count_;
  unsigned iterable_;
  PersistentMemoryAllocator allocator_;
};

// Test parallel allocation/iteration and ensure consistency across all
// instances.
TEST_F(PersistentMemoryAllocatorTest, ParallelismTest) {
  void* memory = mem_segment_.get();
  AllocatorThread t1("t1", memory, TEST_MEMORY_SIZE, TEST_MEMORY_PAGE);
  AllocatorThread t2("t2", memory, TEST_MEMORY_SIZE, TEST_MEMORY_PAGE);
  AllocatorThread t3("t3", memory, TEST_MEMORY_SIZE, TEST_MEMORY_PAGE);
  AllocatorThread t4("t4", memory, TEST_MEMORY_SIZE, TEST_MEMORY_PAGE);
  AllocatorThread t5("t5", memory, TEST_MEMORY_SIZE, TEST_MEMORY_PAGE);

  t1.Start();
  t2.Start();
  t3.Start();
  t4.Start();
  t5.Start();

  unsigned last_count = 0;
  do {
    unsigned count = CountIterables();
    EXPECT_LE(last_count, count);
  } while (!allocator_->IsCorrupt() && !allocator_->IsFull());

  t1.Join();
  t2.Join();
  t3.Join();
  t4.Join();
  t5.Join();

  EXPECT_FALSE(allocator_->IsCorrupt());
  EXPECT_TRUE(allocator_->IsFull());
  EXPECT_EQ(CountIterables(),
            t1.iterable() + t2.iterable() + t3.iterable() + t4.iterable() +
            t5.iterable());
}

// A simple thread that counts objects by iterating through an allocator.
class CounterThread : public SimpleThread {
 public:
  CounterThread(const std::string& name,
                PersistentMemoryAllocator::Iterator* iterator,
                Lock* lock,
                ConditionVariable* condition,
                bool* wake_up)
      : SimpleThread(name, Options()),
        iterator_(iterator),
        lock_(lock),
        condition_(condition),
        count_(0),
        wake_up_(wake_up) {}

  void Run() override {
    // Wait so all threads can start at approximately the same time.
    // Best performance comes from releasing a single worker which then
    // releases the next, etc., etc.
    {
      AutoLock autolock(*lock_);

      // Before calling Wait(), make sure that the wake up condition
      // has not already passed.  Also, since spurious signal events
      // are possible, check the condition in a while loop to make
      // sure that the wake up condition is met when this thread
      // returns from the Wait().
      // See usage comments in src/base/synchronization/condition_variable.h.
      while (!*wake_up_) {
        condition_->Wait();
        condition_->Signal();
      }
    }

    uint32_t type;
    while (iterator_->GetNext(&type) != 0) {
      ++count_;
    }
  }

  unsigned count() { return count_; }

 private:
  PersistentMemoryAllocator::Iterator* iterator_;
  Lock* lock_;
  ConditionVariable* condition_;
  unsigned count_;
  bool* wake_up_;

  DISALLOW_COPY_AND_ASSIGN(CounterThread);
};

// Ensure that parallel iteration returns the same number of objects as
// single-threaded iteration.
TEST_F(PersistentMemoryAllocatorTest, IteratorParallelismTest) {
  // Fill the memory segment with random allocations.
  unsigned iterable_count = 0;
  for (;;) {
    uint32_t size = RandInt(1, 99);
    uint32_t type = RandInt(100, 999);
    Reference block = allocator_->Allocate(size, type);
    if (!block)
      break;
    allocator_->MakeIterable(block);
    ++iterable_count;
  }
  EXPECT_FALSE(allocator_->IsCorrupt());
  EXPECT_TRUE(allocator_->IsFull());
  EXPECT_EQ(iterable_count, CountIterables());

  PersistentMemoryAllocator::Iterator iter(allocator_.get());
  Lock lock;
  ConditionVariable condition(&lock);
  bool wake_up = false;

  CounterThread t1("t1", &iter, &lock, &condition, &wake_up);
  CounterThread t2("t2", &iter, &lock, &condition, &wake_up);
  CounterThread t3("t3", &iter, &lock, &condition, &wake_up);
  CounterThread t4("t4", &iter, &lock, &condition, &wake_up);
  CounterThread t5("t5", &iter, &lock, &condition, &wake_up);

  t1.Start();
  t2.Start();
  t3.Start();
  t4.Start();
  t5.Start();

  // Take the lock and set the wake up condition to true.  This helps to
  // avoid a race condition where the Signal() event is called before
  // all the threads have reached the Wait() and thus never get woken up.
  {
    AutoLock autolock(lock);
    wake_up = true;
  }

  // This will release all the waiting threads.
  condition.Signal();

  t1.Join();
  t2.Join();
  t3.Join();
  t4.Join();
  t5.Join();

  EXPECT_EQ(iterable_count,
            t1.count() + t2.count() + t3.count() + t4.count() + t5.count());

#if 0
  // These ensure that the threads don't run sequentially. It shouldn't be
  // enabled in general because it could lead to a flaky test if it happens
  // simply by chance but it is useful during development to ensure that the
  // test is working correctly.
  EXPECT_NE(iterable_count, t1.count());
  EXPECT_NE(iterable_count, t2.count());
  EXPECT_NE(iterable_count, t3.count());
  EXPECT_NE(iterable_count, t4.count());
  EXPECT_NE(iterable_count, t5.count());
#endif
}

TEST_F(PersistentMemoryAllocatorTest, DelayedAllocationTest) {
  std::atomic<Reference> ref1, ref2;
  ref1.store(0, std::memory_order_relaxed);
  ref2.store(0, std::memory_order_relaxed);
  DelayedPersistentAllocation da1(allocator_.get(), &ref1, 1001, 100, true);
  DelayedPersistentAllocation da2a(allocator_.get(), &ref2, 2002, 200, 0, true);
  DelayedPersistentAllocation da2b(allocator_.get(), &ref2, 2002, 200, 5, true);

  // Nothing should yet have been allocated.
  uint32_t type;
  PersistentMemoryAllocator::Iterator iter(allocator_.get());
  EXPECT_EQ(0U, iter.GetNext(&type));

  // Do first delayed allocation and check that a new persistent object exists.
  EXPECT_EQ(0U, da1.reference());
  void* mem1 = da1.Get();
  ASSERT_TRUE(mem1);
  EXPECT_NE(0U, da1.reference());
  EXPECT_EQ(allocator_->GetAsReference(mem1, 1001),
            ref1.load(std::memory_order_relaxed));
  EXPECT_NE(0U, iter.GetNext(&type));
  EXPECT_EQ(1001U, type);
  EXPECT_EQ(0U, iter.GetNext(&type));

  // Do second delayed allocation and check.
  void* mem2a = da2a.Get();
  ASSERT_TRUE(mem2a);
  EXPECT_EQ(allocator_->GetAsReference(mem2a, 2002),
            ref2.load(std::memory_order_relaxed));
  EXPECT_NE(0U, iter.GetNext(&type));
  EXPECT_EQ(2002U, type);
  EXPECT_EQ(0U, iter.GetNext(&type));

  // Third allocation should just return offset into second allocation.
  void* mem2b = da2b.Get();
  ASSERT_TRUE(mem2b);
  EXPECT_EQ(0U, iter.GetNext(&type));
  EXPECT_EQ(reinterpret_cast<uintptr_t>(mem2a) + 5,
            reinterpret_cast<uintptr_t>(mem2b));
}

// This test doesn't verify anything other than it doesn't crash. Its goal
// is to find coding errors that aren't otherwise tested for, much like a
// "fuzzer" would.
// This test is suppsoed to fail on TSAN bot (crbug.com/579867).
#if defined(THREAD_SANITIZER)
#define MAYBE_CorruptionTest DISABLED_CorruptionTest
#else
#define MAYBE_CorruptionTest CorruptionTest
#endif
TEST_F(PersistentMemoryAllocatorTest, MAYBE_CorruptionTest) {
  char* memory = mem_segment_.get();
  AllocatorThread t1("t1", memory, TEST_MEMORY_SIZE, TEST_MEMORY_PAGE);
  AllocatorThread t2("t2", memory, TEST_MEMORY_SIZE, TEST_MEMORY_PAGE);
  AllocatorThread t3("t3", memory, TEST_MEMORY_SIZE, TEST_MEMORY_PAGE);
  AllocatorThread t4("t4", memory, TEST_MEMORY_SIZE, TEST_MEMORY_PAGE);
  AllocatorThread t5("t5", memory, TEST_MEMORY_SIZE, TEST_MEMORY_PAGE);

  t1.Start();
  t2.Start();
  t3.Start();
  t4.Start();
  t5.Start();

  do {
    size_t offset = RandInt(0, TEST_MEMORY_SIZE - 1);
    char value = RandInt(0, 255);
    memory[offset] = value;
  } while (!allocator_->IsCorrupt() && !allocator_->IsFull());

  t1.Join();
  t2.Join();
  t3.Join();
  t4.Join();
  t5.Join();

  CountIterables();
}

// Attempt to cause crashes or loops by expressly creating dangerous conditions.
TEST_F(PersistentMemoryAllocatorTest, MaliciousTest) {
  Reference block1 = allocator_->Allocate(sizeof(TestObject1), 1);
  Reference block2 = allocator_->Allocate(sizeof(TestObject1), 2);
  Reference block3 = allocator_->Allocate(sizeof(TestObject1), 3);
  Reference block4 = allocator_->Allocate(sizeof(TestObject1), 3);
  Reference block5 = allocator_->Allocate(sizeof(TestObject1), 3);
  allocator_->MakeIterable(block1);
  allocator_->MakeIterable(block2);
  allocator_->MakeIterable(block3);
  allocator_->MakeIterable(block4);
  allocator_->MakeIterable(block5);
  EXPECT_EQ(5U, CountIterables());
  EXPECT_FALSE(allocator_->IsCorrupt());

  // Create loop in iterable list and ensure it doesn't hang. The return value
  // from CountIterables() in these cases is unpredictable. If there is a
  // failure, the call will hang and the test killed for taking too long.
  uint32_t* header4 = (uint32_t*)(mem_segment_.get() + block4);
  EXPECT_EQ(block5, header4[3]);
  header4[3] = block4;
  CountIterables();  // loop: 1-2-3-4-4
  EXPECT_TRUE(allocator_->IsCorrupt());

  // Test where loop goes back to previous block.
  header4[3] = block3;
  CountIterables();  // loop: 1-2-3-4-3

  // Test where loop goes back to the beginning.
  header4[3] = block1;
  CountIterables();  // loop: 1-2-3-4-1
}


//----- LocalPersistentMemoryAllocator -----------------------------------------

TEST(LocalPersistentMemoryAllocatorTest, CreationTest) {
  LocalPersistentMemoryAllocator allocator(TEST_MEMORY_SIZE, 42, "");
  EXPECT_EQ(42U, allocator.Id());
  EXPECT_NE(0U, allocator.Allocate(24, 1));
  EXPECT_FALSE(allocator.IsFull());
  EXPECT_FALSE(allocator.IsCorrupt());
}

//----- {Writable,ReadOnly}SharedPersistentMemoryAllocator ---------------------

TEST(SharedPersistentMemoryAllocatorTest, CreationTest) {
  base::WritableSharedMemoryRegion rw_region =
      base::WritableSharedMemoryRegion::Create(TEST_MEMORY_SIZE);
  ASSERT_TRUE(rw_region.IsValid());

  PersistentMemoryAllocator::MemoryInfo meminfo1;
  Reference r123, r456, r789;
  {
    base::WritableSharedMemoryMapping mapping = rw_region.Map();
    ASSERT_TRUE(mapping.IsValid());
    WritableSharedPersistentMemoryAllocator local(std::move(mapping), TEST_ID,
                                                  "");
    EXPECT_FALSE(local.IsReadonly());
    r123 = local.Allocate(123, 123);
    r456 = local.Allocate(456, 456);
    r789 = local.Allocate(789, 789);
    local.MakeIterable(r123);
    local.ChangeType(r456, 654, 456, false);
    local.MakeIterable(r789);
    local.GetMemoryInfo(&meminfo1);
    EXPECT_FALSE(local.IsFull());
    EXPECT_FALSE(local.IsCorrupt());
  }

  // Create writable and read-only mappings of the same region.
  base::WritableSharedMemoryMapping rw_mapping = rw_region.Map();
  ASSERT_TRUE(rw_mapping.IsValid());
  base::ReadOnlySharedMemoryRegion ro_region =
      base::WritableSharedMemoryRegion::ConvertToReadOnly(std::move(rw_region));
  ASSERT_TRUE(ro_region.IsValid());
  base::ReadOnlySharedMemoryMapping ro_mapping = ro_region.Map();
  ASSERT_TRUE(ro_mapping.IsValid());

  // Read-only test.
  ReadOnlySharedPersistentMemoryAllocator shalloc2(std::move(ro_mapping), 0,
                                                   "");
  EXPECT_TRUE(shalloc2.IsReadonly());
  EXPECT_EQ(TEST_ID, shalloc2.Id());
  EXPECT_FALSE(shalloc2.IsFull());
  EXPECT_FALSE(shalloc2.IsCorrupt());

  PersistentMemoryAllocator::Iterator iter2(&shalloc2);
  uint32_t type;
  EXPECT_EQ(r123, iter2.GetNext(&type));
  EXPECT_EQ(r789, iter2.GetNext(&type));
  EXPECT_EQ(0U, iter2.GetNext(&type));

  EXPECT_EQ(123U, shalloc2.GetType(r123));
  EXPECT_EQ(654U, shalloc2.GetType(r456));
  EXPECT_EQ(789U, shalloc2.GetType(r789));

  PersistentMemoryAllocator::MemoryInfo meminfo2;
  shalloc2.GetMemoryInfo(&meminfo2);
  EXPECT_EQ(meminfo1.total, meminfo2.total);
  EXPECT_EQ(meminfo1.free, meminfo2.free);

  // Read/write test.
  WritableSharedPersistentMemoryAllocator shalloc3(std::move(rw_mapping), 0,
                                                   "");
  EXPECT_FALSE(shalloc3.IsReadonly());
  EXPECT_EQ(TEST_ID, shalloc3.Id());
  EXPECT_FALSE(shalloc3.IsFull());
  EXPECT_FALSE(shalloc3.IsCorrupt());

  PersistentMemoryAllocator::Iterator iter3(&shalloc3);
  EXPECT_EQ(r123, iter3.GetNext(&type));
  EXPECT_EQ(r789, iter3.GetNext(&type));
  EXPECT_EQ(0U, iter3.GetNext(&type));

  EXPECT_EQ(123U, shalloc3.GetType(r123));
  EXPECT_EQ(654U, shalloc3.GetType(r456));
  EXPECT_EQ(789U, shalloc3.GetType(r789));

  PersistentMemoryAllocator::MemoryInfo meminfo3;
  shalloc3.GetMemoryInfo(&meminfo3);
  EXPECT_EQ(meminfo1.total, meminfo3.total);
  EXPECT_EQ(meminfo1.free, meminfo3.free);

  // Interconnectivity test.
  Reference obj = shalloc3.Allocate(42, 42);
  ASSERT_TRUE(obj);
  shalloc3.MakeIterable(obj);
  EXPECT_EQ(obj, iter2.GetNext(&type));
  EXPECT_EQ(42U, type);

  // Clear-on-change test.
  Reference data_ref = shalloc3.Allocate(sizeof(int) * 4, 911);
  int* data = shalloc3.GetAsArray<int>(data_ref, 911, 4);
  ASSERT_TRUE(data);
  data[0] = 0;
  data[1] = 1;
  data[2] = 2;
  data[3] = 3;
  ASSERT_TRUE(shalloc3.ChangeType(data_ref, 119, 911, false));
  EXPECT_EQ(0, data[0]);
  EXPECT_EQ(1, data[1]);
  EXPECT_EQ(2, data[2]);
  EXPECT_EQ(3, data[3]);
  ASSERT_TRUE(shalloc3.ChangeType(data_ref, 191, 119, true));
  EXPECT_EQ(0, data[0]);
  EXPECT_EQ(0, data[1]);
  EXPECT_EQ(0, data[2]);
  EXPECT_EQ(0, data[3]);
}


#if !defined(OS_NACL)
//----- FilePersistentMemoryAllocator ------------------------------------------

TEST(FilePersistentMemoryAllocatorTest, CreationTest) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath file_path = temp_dir.GetPath().AppendASCII("persistent_memory");

  PersistentMemoryAllocator::MemoryInfo meminfo1;
  Reference r123, r456, r789;
  {
    LocalPersistentMemoryAllocator local(TEST_MEMORY_SIZE, TEST_ID, "");
    EXPECT_FALSE(local.IsReadonly());
    r123 = local.Allocate(123, 123);
    r456 = local.Allocate(456, 456);
    r789 = local.Allocate(789, 789);
    local.MakeIterable(r123);
    local.ChangeType(r456, 654, 456, false);
    local.MakeIterable(r789);
    local.GetMemoryInfo(&meminfo1);
    EXPECT_FALSE(local.IsFull());
    EXPECT_FALSE(local.IsCorrupt());

    File writer(file_path, File::FLAG_CREATE | File::FLAG_WRITE);
    ASSERT_TRUE(writer.IsValid());
    writer.Write(0, (const char*)local.data(), local.used());
  }

  std::unique_ptr<MemoryMappedFile> mmfile(new MemoryMappedFile());
  ASSERT_TRUE(mmfile->Initialize(file_path));
  EXPECT_TRUE(mmfile->IsValid());
  const size_t mmlength = mmfile->length();
  EXPECT_GE(meminfo1.total, mmlength);

  FilePersistentMemoryAllocator file(std::move(mmfile), 0, 0, "", false);
  EXPECT_FALSE(file.IsReadonly());
  EXPECT_EQ(TEST_ID, file.Id());
  EXPECT_FALSE(file.IsFull());
  EXPECT_FALSE(file.IsCorrupt());

  PersistentMemoryAllocator::Iterator iter(&file);
  uint32_t type;
  EXPECT_EQ(r123, iter.GetNext(&type));
  EXPECT_EQ(r789, iter.GetNext(&type));
  EXPECT_EQ(0U, iter.GetNext(&type));

  EXPECT_EQ(123U, file.GetType(r123));
  EXPECT_EQ(654U, file.GetType(r456));
  EXPECT_EQ(789U, file.GetType(r789));

  PersistentMemoryAllocator::MemoryInfo meminfo2;
  file.GetMemoryInfo(&meminfo2);
  EXPECT_GE(meminfo1.total, meminfo2.total);
  EXPECT_GE(meminfo1.free, meminfo2.free);
  EXPECT_EQ(mmlength, meminfo2.total);
  EXPECT_EQ(0U, meminfo2.free);

  // There's no way of knowing if Flush actually does anything but at least
  // verify that it runs without CHECK violations.
  file.Flush(false);
  file.Flush(true);
}

TEST(FilePersistentMemoryAllocatorTest, ExtendTest) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath file_path = temp_dir.GetPath().AppendASCII("extend_test");
  MemoryMappedFile::Region region = {0, 16 << 10};  // 16KiB maximum size.

  // Start with a small but valid file of persistent data.
  ASSERT_FALSE(PathExists(file_path));
  {
    LocalPersistentMemoryAllocator local(TEST_MEMORY_SIZE, TEST_ID, "");
    local.Allocate(1, 1);
    local.Allocate(11, 11);

    File writer(file_path, File::FLAG_CREATE | File::FLAG_WRITE);
    ASSERT_TRUE(writer.IsValid());
    writer.Write(0, (const char*)local.data(), local.used());
  }
  ASSERT_TRUE(PathExists(file_path));
  int64_t before_size;
  ASSERT_TRUE(GetFileSize(file_path, &before_size));

  // Map it as an extendable read/write file and append to it.
  {
    std::unique_ptr<MemoryMappedFile> mmfile(new MemoryMappedFile());
    ASSERT_TRUE(mmfile->Initialize(
        File(file_path, File::FLAG_OPEN | File::FLAG_READ | File::FLAG_WRITE),
        region, MemoryMappedFile::READ_WRITE_EXTEND));
    FilePersistentMemoryAllocator allocator(std::move(mmfile), region.size, 0,
                                            "", false);
    EXPECT_EQ(static_cast<size_t>(before_size), allocator.used());

    allocator.Allocate(111, 111);
    EXPECT_LT(static_cast<size_t>(before_size), allocator.used());
  }

  // Validate that append worked.
  int64_t after_size;
  ASSERT_TRUE(GetFileSize(file_path, &after_size));
  EXPECT_LT(before_size, after_size);

  // Verify that it's still an acceptable file.
  {
    std::unique_ptr<MemoryMappedFile> mmfile(new MemoryMappedFile());
    ASSERT_TRUE(mmfile->Initialize(
        File(file_path, File::FLAG_OPEN | File::FLAG_READ | File::FLAG_WRITE),
        region, MemoryMappedFile::READ_WRITE_EXTEND));
    EXPECT_TRUE(FilePersistentMemoryAllocator::IsFileAcceptable(*mmfile, true));
    EXPECT_TRUE(
        FilePersistentMemoryAllocator::IsFileAcceptable(*mmfile, false));
  }
}

TEST(FilePersistentMemoryAllocatorTest, AcceptableTest) {
  const uint32_t kAllocAlignment =
      PersistentMemoryAllocatorTest::GetAllocAlignment();
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  LocalPersistentMemoryAllocator local(TEST_MEMORY_SIZE, TEST_ID, "");
  local.MakeIterable(local.Allocate(1, 1));
  local.MakeIterable(local.Allocate(11, 11));
  const size_t minsize = local.used();
  std::unique_ptr<char[]> garbage(new char[minsize]);
  RandBytes(garbage.get(), minsize);

  std::unique_ptr<MemoryMappedFile> mmfile;
  char filename[100];
  for (size_t filesize = minsize; filesize > 0; --filesize) {
    strings::SafeSPrintf(filename, "memory_%d_A", filesize);
    FilePath file_path = temp_dir.GetPath().AppendASCII(filename);
    ASSERT_FALSE(PathExists(file_path));
    {
      File writer(file_path, File::FLAG_CREATE | File::FLAG_WRITE);
      ASSERT_TRUE(writer.IsValid());
      writer.Write(0, (const char*)local.data(), filesize);
    }
    ASSERT_TRUE(PathExists(file_path));

    // Request read/write access for some sizes that are a multple of the
    // allocator's alignment size. The allocator is strict about file size
    // being a multiple of its internal alignment when doing read/write access.
    const bool read_only = (filesize % (2 * kAllocAlignment)) != 0;
    const uint32_t file_flags =
        File::FLAG_OPEN | File::FLAG_READ | (read_only ? 0 : File::FLAG_WRITE);
    const MemoryMappedFile::Access map_access =
        read_only ? MemoryMappedFile::READ_ONLY : MemoryMappedFile::READ_WRITE;

    mmfile = std::make_unique<MemoryMappedFile>();
    ASSERT_TRUE(mmfile->Initialize(File(file_path, file_flags), map_access));
    EXPECT_EQ(filesize, mmfile->length());
    if (FilePersistentMemoryAllocator::IsFileAcceptable(*mmfile, read_only)) {
      // Make sure construction doesn't crash. It will, however, cause
      // error messages warning about about a corrupted memory segment.
      FilePersistentMemoryAllocator allocator(std::move(mmfile), 0, 0, "",
                                              read_only);
      // Also make sure that iteration doesn't crash.
      PersistentMemoryAllocator::Iterator iter(&allocator);
      uint32_t type_id;
      Reference ref;
      while ((ref = iter.GetNext(&type_id)) != 0) {
        const char* data = allocator.GetAsArray<char>(
            ref, 0, PersistentMemoryAllocator::kSizeAny);
        uint32_t type = allocator.GetType(ref);
        size_t size = allocator.GetAllocSize(ref);
        // Ensure compiler can't optimize-out above variables.
        (void)data;
        (void)type;
        (void)size;
      }

      // Ensure that short files are detected as corrupt and full files are not.
      EXPECT_EQ(filesize != minsize, allocator.IsCorrupt());
    } else {
      // For filesize >= minsize, the file must be acceptable. This
      // else clause (file-not-acceptable) should be reached only if
      // filesize < minsize.
      EXPECT_LT(filesize, minsize);
    }

    strings::SafeSPrintf(filename, "memory_%d_B", filesize);
    file_path = temp_dir.GetPath().AppendASCII(filename);
    ASSERT_FALSE(PathExists(file_path));
    {
      File writer(file_path, File::FLAG_CREATE | File::FLAG_WRITE);
      ASSERT_TRUE(writer.IsValid());
      writer.Write(0, (const char*)garbage.get(), filesize);
    }
    ASSERT_TRUE(PathExists(file_path));

    mmfile = std::make_unique<MemoryMappedFile>();
    ASSERT_TRUE(mmfile->Initialize(File(file_path, file_flags), map_access));
    EXPECT_EQ(filesize, mmfile->length());
    if (FilePersistentMemoryAllocator::IsFileAcceptable(*mmfile, read_only)) {
      // Make sure construction doesn't crash. It will, however, cause
      // error messages warning about about a corrupted memory segment.
      FilePersistentMemoryAllocator allocator(std::move(mmfile), 0, 0, "",
                                              read_only);
      EXPECT_TRUE(allocator.IsCorrupt());  // Garbage data so it should be.
    } else {
      // For filesize >= minsize, the file must be acceptable. This
      // else clause (file-not-acceptable) should be reached only if
      // filesize < minsize.
      EXPECT_GT(minsize, filesize);
    }
  }
}

TEST_F(PersistentMemoryAllocatorTest, TruncateTest) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath file_path = temp_dir.GetPath().AppendASCII("truncate_test");

  // Start with a small but valid file of persistent data. Keep the "used"
  // amount for both allocations.
  Reference a1_ref;
  Reference a2_ref;
  size_t a1_used;
  size_t a2_used;
  ASSERT_FALSE(PathExists(file_path));
  {
    LocalPersistentMemoryAllocator allocator(TEST_MEMORY_SIZE, TEST_ID, "");
    a1_ref = allocator.Allocate(100 << 10, 1);
    allocator.MakeIterable(a1_ref);
    a1_used = allocator.used();
    a2_ref = allocator.Allocate(200 << 10, 11);
    allocator.MakeIterable(a2_ref);
    a2_used = allocator.used();

    File writer(file_path, File::FLAG_CREATE | File::FLAG_WRITE);
    ASSERT_TRUE(writer.IsValid());
    writer.Write(0, static_cast<const char*>(allocator.data()),
                 allocator.size());
  }
  ASSERT_TRUE(PathExists(file_path));
  EXPECT_LE(a1_used, a2_ref);

  // Truncate the file to include everything and make sure it can be read, both
  // with read-write and read-only access.
  for (size_t file_length : {a2_used, a1_used, a1_used / 2}) {
    SCOPED_TRACE(StringPrintf("file_length=%zu", file_length));
    SetFileLength(file_path, file_length);

    for (bool read_only : {false, true}) {
      SCOPED_TRACE(StringPrintf("read_only=%s", read_only ? "true" : "false"));

      std::unique_ptr<MemoryMappedFile> mmfile(new MemoryMappedFile());
      ASSERT_TRUE(mmfile->Initialize(
          File(file_path, File::FLAG_OPEN |
                              (read_only ? File::FLAG_READ
                                         : File::FLAG_READ | File::FLAG_WRITE)),
          read_only ? MemoryMappedFile::READ_ONLY
                    : MemoryMappedFile::READ_WRITE));
      ASSERT_TRUE(
          FilePersistentMemoryAllocator::IsFileAcceptable(*mmfile, read_only));

      FilePersistentMemoryAllocator allocator(std::move(mmfile), 0, 0, "",
                                              read_only);

      PersistentMemoryAllocator::Iterator iter(&allocator);
      uint32_t type_id;
      EXPECT_EQ(file_length >= a1_used ? a1_ref : 0U, iter.GetNext(&type_id));
      EXPECT_EQ(file_length >= a2_used ? a2_ref : 0U, iter.GetNext(&type_id));
      EXPECT_EQ(0U, iter.GetNext(&type_id));

      // Ensure that short files are detected as corrupt and full files are not.
      EXPECT_EQ(file_length < a2_used, allocator.IsCorrupt());
    }

    // Ensure that file length was not adjusted.
    int64_t actual_length;
    ASSERT_TRUE(GetFileSize(file_path, &actual_length));
    EXPECT_EQ(file_length, static_cast<size_t>(actual_length));
  }
}

#endif  // !defined(OS_NACL)

}  // namespace base
