// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/activity_tracker.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/pending_task.h"
#include "base/rand_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/test/spin_wait.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace debug {

namespace {

class TestActivityTracker : public ThreadActivityTracker {
 public:
  TestActivityTracker(std::unique_ptr<char[]> memory, size_t mem_size)
      : ThreadActivityTracker(memset(memory.get(), 0, mem_size), mem_size),
        mem_segment_(std::move(memory)) {}

  ~TestActivityTracker() override = default;

 private:
  std::unique_ptr<char[]> mem_segment_;
};

}  // namespace


class ActivityTrackerTest : public testing::Test {
 public:
  const int kMemorySize = 1 << 20;  // 1MiB
  const int kStackSize  = 1 << 10;  // 1KiB

  using ActivityId = ThreadActivityTracker::ActivityId;

  ActivityTrackerTest() = default;

  ~ActivityTrackerTest() override {
    GlobalActivityTracker* global_tracker = GlobalActivityTracker::Get();
    if (global_tracker) {
      global_tracker->ReleaseTrackerForCurrentThreadForTesting();
      delete global_tracker;
    }
  }

  std::unique_ptr<ThreadActivityTracker> CreateActivityTracker() {
    std::unique_ptr<char[]> memory(new char[kStackSize]);
    return std::make_unique<TestActivityTracker>(std::move(memory), kStackSize);
  }

  size_t GetGlobalActiveTrackerCount() {
    GlobalActivityTracker* global_tracker = GlobalActivityTracker::Get();
    if (!global_tracker)
      return 0;
    return global_tracker->thread_tracker_count_.load(
        std::memory_order_relaxed);
  }

  size_t GetGlobalInactiveTrackerCount() {
    GlobalActivityTracker* global_tracker = GlobalActivityTracker::Get();
    if (!global_tracker)
      return 0;
    AutoLock autolock(global_tracker->thread_tracker_allocator_lock_);
    return global_tracker->thread_tracker_allocator_.cache_used();
  }

  size_t GetGlobalUserDataMemoryCacheUsed() {
    AutoLock autolock(GlobalActivityTracker::Get()->user_data_allocator_lock_);
    return GlobalActivityTracker::Get()->user_data_allocator_.cache_used();
  }

  void HandleProcessExit(int64_t id,
                         int64_t stamp,
                         int code,
                         GlobalActivityTracker::ProcessPhase phase,
                         std::string&& command,
                         ActivityUserData::Snapshot&& data) {
    exit_id_ = id;
    exit_stamp_ = stamp;
    exit_code_ = code;
    exit_phase_ = phase;
    exit_command_ = std::move(command);
    exit_data_ = std::move(data);
  }

  int64_t exit_id_ = 0;
  int64_t exit_stamp_;
  int exit_code_;
  GlobalActivityTracker::ProcessPhase exit_phase_;
  std::string exit_command_;
  ActivityUserData::Snapshot exit_data_;
};

TEST_F(ActivityTrackerTest, UserDataTest) {
  char buffer[256];
  memset(buffer, 0, sizeof(buffer));
  ActivityUserData data(buffer, sizeof(buffer));
  size_t space = sizeof(buffer) - sizeof(ActivityUserData::MemoryHeader);
  ASSERT_EQ(space, data.available_);

  data.SetInt("foo", 1);
  space -= 24;
  ASSERT_EQ(space, data.available_);

  data.SetUint("b", 1U);  // Small names fit beside header in a word.
  space -= 16;
  ASSERT_EQ(space, data.available_);

  data.Set("c", buffer, 10);
  space -= 24;
  ASSERT_EQ(space, data.available_);

  data.SetString("dear john", "it's been fun");
  space -= 32;
  ASSERT_EQ(space, data.available_);

  data.Set("c", buffer, 20);
  ASSERT_EQ(space, data.available_);

  data.SetString("dear john", "but we're done together");
  ASSERT_EQ(space, data.available_);

  data.SetString("dear john", "bye");
  ASSERT_EQ(space, data.available_);

  data.SetChar("d", 'x');
  space -= 8;
  ASSERT_EQ(space, data.available_);

  data.SetBool("ee", true);
  space -= 16;
  ASSERT_EQ(space, data.available_);

  data.SetString("f", "");
  space -= 8;
  ASSERT_EQ(space, data.available_);
}

TEST_F(ActivityTrackerTest, PushPopTest) {
  std::unique_ptr<ThreadActivityTracker> tracker = CreateActivityTracker();
  ThreadActivityTracker::Snapshot snapshot;

  ASSERT_TRUE(tracker->CreateSnapshot(&snapshot));
  ASSERT_EQ(0U, snapshot.activity_stack_depth);
  ASSERT_EQ(0U, snapshot.activity_stack.size());

  char origin1;
  ActivityId id1 = tracker->PushActivity(&origin1, Activity::ACT_TASK,
                                         ActivityData::ForTask(11));
  ASSERT_TRUE(tracker->CreateSnapshot(&snapshot));
  ASSERT_EQ(1U, snapshot.activity_stack_depth);
  ASSERT_EQ(1U, snapshot.activity_stack.size());
  EXPECT_NE(0, snapshot.activity_stack[0].time_internal);
  EXPECT_EQ(Activity::ACT_TASK, snapshot.activity_stack[0].activity_type);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(&origin1),
            snapshot.activity_stack[0].origin_address);
  EXPECT_EQ(11U, snapshot.activity_stack[0].data.task.sequence_id);

  char origin2;
  char lock2;
  ActivityId id2 = tracker->PushActivity(&origin2, Activity::ACT_LOCK,
                                         ActivityData::ForLock(&lock2));
  ASSERT_TRUE(tracker->CreateSnapshot(&snapshot));
  ASSERT_EQ(2U, snapshot.activity_stack_depth);
  ASSERT_EQ(2U, snapshot.activity_stack.size());
  EXPECT_LE(snapshot.activity_stack[0].time_internal,
            snapshot.activity_stack[1].time_internal);
  EXPECT_EQ(Activity::ACT_LOCK, snapshot.activity_stack[1].activity_type);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(&origin2),
            snapshot.activity_stack[1].origin_address);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(&lock2),
            snapshot.activity_stack[1].data.lock.lock_address);

  tracker->PopActivity(id2);
  ASSERT_TRUE(tracker->CreateSnapshot(&snapshot));
  ASSERT_EQ(1U, snapshot.activity_stack_depth);
  ASSERT_EQ(1U, snapshot.activity_stack.size());
  EXPECT_EQ(Activity::ACT_TASK, snapshot.activity_stack[0].activity_type);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(&origin1),
            snapshot.activity_stack[0].origin_address);
  EXPECT_EQ(11U, snapshot.activity_stack[0].data.task.sequence_id);

  tracker->PopActivity(id1);
  ASSERT_TRUE(tracker->CreateSnapshot(&snapshot));
  ASSERT_EQ(0U, snapshot.activity_stack_depth);
  ASSERT_EQ(0U, snapshot.activity_stack.size());
}

TEST_F(ActivityTrackerTest, ScopedTaskTest) {
  GlobalActivityTracker::CreateWithLocalMemory(kMemorySize, 0, "", 3, 0);

  ThreadActivityTracker* tracker =
      GlobalActivityTracker::Get()->GetOrCreateTrackerForCurrentThread();
  ThreadActivityTracker::Snapshot snapshot;
  ASSERT_EQ(0U, GetGlobalUserDataMemoryCacheUsed());

  ASSERT_TRUE(tracker->CreateSnapshot(&snapshot));
  ASSERT_EQ(0U, snapshot.activity_stack_depth);
  ASSERT_EQ(0U, snapshot.activity_stack.size());

  {
    PendingTask task1(FROM_HERE, DoNothing());
    ScopedTaskRunActivity activity1(task1);
    ActivityUserData& user_data1 = activity1.user_data();
    (void)user_data1;  // Tell compiler it's been used.
    EXPECT_TRUE(activity1.IsRecorded());

    ASSERT_TRUE(tracker->CreateSnapshot(&snapshot));
    ASSERT_EQ(1U, snapshot.activity_stack_depth);
    ASSERT_EQ(1U, snapshot.activity_stack.size());
    EXPECT_EQ(Activity::ACT_TASK, snapshot.activity_stack[0].activity_type);

    {
      PendingTask task2(FROM_HERE, DoNothing());
      ScopedTaskRunActivity activity2(task2);
      ActivityUserData& user_data2 = activity2.user_data();
      (void)user_data2;  // Tell compiler it's been used.

      ASSERT_TRUE(tracker->CreateSnapshot(&snapshot));
      ASSERT_EQ(2U, snapshot.activity_stack_depth);
      ASSERT_EQ(2U, snapshot.activity_stack.size());
      EXPECT_EQ(Activity::ACT_TASK, snapshot.activity_stack[1].activity_type);
    }

    ASSERT_TRUE(tracker->CreateSnapshot(&snapshot));
    ASSERT_EQ(1U, snapshot.activity_stack_depth);
    ASSERT_EQ(1U, snapshot.activity_stack.size());
    EXPECT_EQ(Activity::ACT_TASK, snapshot.activity_stack[0].activity_type);
  }

  ASSERT_TRUE(tracker->CreateSnapshot(&snapshot));
  ASSERT_EQ(0U, snapshot.activity_stack_depth);
  ASSERT_EQ(0U, snapshot.activity_stack.size());
  ASSERT_EQ(2U, GetGlobalUserDataMemoryCacheUsed());
}

namespace {

class SimpleLockThread : public SimpleThread {
 public:
  SimpleLockThread(const std::string& name, Lock* lock)
      : SimpleThread(name, Options()),
        lock_(lock),
        data_changed_(false),
        is_running_(false) {}

  ~SimpleLockThread() override = default;

  void Run() override {
    ThreadActivityTracker* tracker =
        GlobalActivityTracker::Get()->GetOrCreateTrackerForCurrentThread();
    uint32_t pre_version = tracker->GetDataVersionForTesting();

    is_running_.store(true, std::memory_order_relaxed);
    lock_->Acquire();
    data_changed_ = tracker->GetDataVersionForTesting() != pre_version;
    lock_->Release();
    is_running_.store(false, std::memory_order_relaxed);
  }

  bool IsRunning() { return is_running_.load(std::memory_order_relaxed); }

  bool WasDataChanged() { return data_changed_; }

 private:
  Lock* lock_;
  bool data_changed_;
  std::atomic<bool> is_running_;

  DISALLOW_COPY_AND_ASSIGN(SimpleLockThread);
};

}  // namespace

TEST_F(ActivityTrackerTest, LockTest) {
  GlobalActivityTracker::CreateWithLocalMemory(kMemorySize, 0, "", 3, 0);

  ThreadActivityTracker* tracker =
      GlobalActivityTracker::Get()->GetOrCreateTrackerForCurrentThread();
  ThreadActivityTracker::Snapshot snapshot;
  ASSERT_EQ(0U, GetGlobalUserDataMemoryCacheUsed());

  Lock lock;
  uint32_t pre_version = tracker->GetDataVersionForTesting();

  // Check no activity when only "trying" a lock.
  EXPECT_TRUE(lock.Try());
  EXPECT_EQ(pre_version, tracker->GetDataVersionForTesting());
  lock.AssertAcquired();
  lock.Release();
  EXPECT_EQ(pre_version, tracker->GetDataVersionForTesting());

  // Check no activity when acquiring a free lock.
  SimpleLockThread t1("locker1", &lock);
  t1.Start();
  t1.Join();
  EXPECT_FALSE(t1.WasDataChanged());

  // Check that activity is recorded when acquring a busy lock.
  SimpleLockThread t2("locker2", &lock);
  lock.Acquire();
  t2.Start();
  while (!t2.IsRunning())
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(10));
  // t2 can't join until the lock is released but have to give time for t2 to
  // actually block on the lock before releasing it or the results will not
  // be correct.
  PlatformThread::Sleep(TimeDelta::FromMilliseconds(200));
  lock.Release();
  // Now the results will be valid.
  t2.Join();
  EXPECT_TRUE(t2.WasDataChanged());
}

TEST_F(ActivityTrackerTest, ExceptionTest) {
  GlobalActivityTracker::CreateWithLocalMemory(kMemorySize, 0, "", 3, 0);
  GlobalActivityTracker* global = GlobalActivityTracker::Get();

  ThreadActivityTracker* tracker =
      GlobalActivityTracker::Get()->GetOrCreateTrackerForCurrentThread();
  ThreadActivityTracker::Snapshot snapshot;
  ASSERT_EQ(0U, GetGlobalUserDataMemoryCacheUsed());

  ASSERT_TRUE(tracker->CreateSnapshot(&snapshot));
  ASSERT_EQ(0U, snapshot.last_exception.activity_type);

  char origin;
  global->RecordException(&origin, 42);

  ASSERT_TRUE(tracker->CreateSnapshot(&snapshot));
  EXPECT_EQ(Activity::ACT_EXCEPTION, snapshot.last_exception.activity_type);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(&origin),
            snapshot.last_exception.origin_address);
  EXPECT_EQ(42U, snapshot.last_exception.data.exception.code);
}

TEST_F(ActivityTrackerTest, CreateWithFileTest) {
  const char temp_name[] = "CreateWithFileTest";
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath temp_file = temp_dir.GetPath().AppendASCII(temp_name);
  const size_t temp_size = 64 << 10;  // 64 KiB

  // Create a global tracker on a new file.
  ASSERT_FALSE(PathExists(temp_file));
  GlobalActivityTracker::CreateWithFile(temp_file, temp_size, 0, "foo", 3);
  GlobalActivityTracker* global = GlobalActivityTracker::Get();
  EXPECT_EQ(std::string("foo"), global->allocator()->Name());
  global->ReleaseTrackerForCurrentThreadForTesting();
  delete global;

  // Create a global tracker over an existing file, replacing it. If the
  // replacement doesn't work, the name will remain as it was first created.
  ASSERT_TRUE(PathExists(temp_file));
  GlobalActivityTracker::CreateWithFile(temp_file, temp_size, 0, "bar", 3);
  global = GlobalActivityTracker::Get();
  EXPECT_EQ(std::string("bar"), global->allocator()->Name());
  global->ReleaseTrackerForCurrentThreadForTesting();
  delete global;
}


// GlobalActivityTracker tests below.

TEST_F(ActivityTrackerTest, BasicTest) {
  GlobalActivityTracker::CreateWithLocalMemory(kMemorySize, 0, "", 3, 0);
  GlobalActivityTracker* global = GlobalActivityTracker::Get();

  // Ensure the data repositories have backing store, indicated by non-zero ID.
  EXPECT_NE(0U, global->process_data().id());
}

namespace {

class SimpleActivityThread : public SimpleThread {
 public:
  SimpleActivityThread(const std::string& name,
                       const void* origin,
                       Activity::Type activity,
                       const ActivityData& data)
      : SimpleThread(name, Options()),
        origin_(origin),
        activity_(activity),
        data_(data),
        ready_(false),
        exit_(false),
        exit_condition_(&lock_) {}

  ~SimpleActivityThread() override = default;

  void Run() override {
    ThreadActivityTracker::ActivityId id =
        GlobalActivityTracker::Get()
            ->GetOrCreateTrackerForCurrentThread()
            ->PushActivity(origin_, activity_, data_);

    {
      AutoLock auto_lock(lock_);
      ready_.store(true, std::memory_order_release);
      while (!exit_.load(std::memory_order_relaxed))
        exit_condition_.Wait();
    }

    GlobalActivityTracker::Get()->GetTrackerForCurrentThread()->PopActivity(id);
  }

  void Exit() {
    AutoLock auto_lock(lock_);
    exit_.store(true, std::memory_order_relaxed);
    exit_condition_.Signal();
  }

  void WaitReady() {
    SPIN_FOR_1_SECOND_OR_UNTIL_TRUE(ready_.load(std::memory_order_acquire));
  }

 private:
  const void* origin_;
  Activity::Type activity_;
  ActivityData data_;

  std::atomic<bool> ready_;
  std::atomic<bool> exit_;
  Lock lock_;
  ConditionVariable exit_condition_;

  DISALLOW_COPY_AND_ASSIGN(SimpleActivityThread);
};

}  // namespace

TEST_F(ActivityTrackerTest, ThreadDeathTest) {
  GlobalActivityTracker::CreateWithLocalMemory(kMemorySize, 0, "", 3, 0);
  GlobalActivityTracker::Get()->GetOrCreateTrackerForCurrentThread();
  const size_t starting_active = GetGlobalActiveTrackerCount();
  const size_t starting_inactive = GetGlobalInactiveTrackerCount();

  SimpleActivityThread t1("t1", nullptr, Activity::ACT_TASK,
                          ActivityData::ForTask(11));
  t1.Start();
  t1.WaitReady();
  EXPECT_EQ(starting_active + 1, GetGlobalActiveTrackerCount());
  EXPECT_EQ(starting_inactive, GetGlobalInactiveTrackerCount());

  t1.Exit();
  t1.Join();
  EXPECT_EQ(starting_active, GetGlobalActiveTrackerCount());
  EXPECT_EQ(starting_inactive + 1, GetGlobalInactiveTrackerCount());

  // Start another thread and ensure it re-uses the existing memory.

  SimpleActivityThread t2("t2", nullptr, Activity::ACT_TASK,
                          ActivityData::ForTask(22));
  t2.Start();
  t2.WaitReady();
  EXPECT_EQ(starting_active + 1, GetGlobalActiveTrackerCount());
  EXPECT_EQ(starting_inactive, GetGlobalInactiveTrackerCount());

  t2.Exit();
  t2.Join();
  EXPECT_EQ(starting_active, GetGlobalActiveTrackerCount());
  EXPECT_EQ(starting_inactive + 1, GetGlobalInactiveTrackerCount());
}

TEST_F(ActivityTrackerTest, ProcessDeathTest) {
  // This doesn't actually create and destroy a process. Instead, it uses for-
  // testing interfaces to simulate data created by other processes.
  const int64_t other_process_id = GetCurrentProcId() + 1;

  GlobalActivityTracker::CreateWithLocalMemory(kMemorySize, 0, "", 3, 0);
  GlobalActivityTracker* global = GlobalActivityTracker::Get();
  ThreadActivityTracker* thread = global->GetOrCreateTrackerForCurrentThread();

  // Get callbacks for process exit.
  global->SetProcessExitCallback(
      BindRepeating(&ActivityTrackerTest::HandleProcessExit, Unretained(this)));

  // Pretend than another process has started.
  global->RecordProcessLaunch(other_process_id, FILE_PATH_LITERAL("foo --bar"));

  // Do some activities.
  PendingTask task(FROM_HERE, DoNothing());
  ScopedTaskRunActivity activity(task);
  ActivityUserData& user_data = activity.user_data();
  ASSERT_NE(0U, user_data.id());

  // Get the memory-allocator references to that data.
  PersistentMemoryAllocator::Reference proc_data_ref =
      global->allocator()->GetAsReference(
          global->process_data().GetBaseAddress(),
          GlobalActivityTracker::kTypeIdProcessDataRecord);
  ASSERT_TRUE(proc_data_ref);
  PersistentMemoryAllocator::Reference tracker_ref =
      global->allocator()->GetAsReference(
          thread->GetBaseAddress(),
          GlobalActivityTracker::kTypeIdActivityTracker);
  ASSERT_TRUE(tracker_ref);
  PersistentMemoryAllocator::Reference user_data_ref =
      global->allocator()->GetAsReference(
          user_data.GetBaseAddress(),
          GlobalActivityTracker::kTypeIdUserDataRecord);
  ASSERT_TRUE(user_data_ref);

  // Make a copy of the thread-tracker state so it can be restored later.
  const size_t tracker_size = global->allocator()->GetAllocSize(tracker_ref);
  std::unique_ptr<char[]> tracker_copy(new char[tracker_size]);
  memcpy(tracker_copy.get(), thread->GetBaseAddress(), tracker_size);

  // Change the objects to appear to be owned by another process. Use a "past"
  // time so that exit-time is always later than create-time.
  const int64_t past_stamp = Time::Now().ToInternalValue() - 1;
  int64_t owning_id;
  int64_t stamp;
  ASSERT_TRUE(ActivityUserData::GetOwningProcessId(
      global->process_data().GetBaseAddress(), &owning_id, &stamp));
  EXPECT_NE(other_process_id, owning_id);
  ASSERT_TRUE(ThreadActivityTracker::GetOwningProcessId(
      thread->GetBaseAddress(), &owning_id, &stamp));
  EXPECT_NE(other_process_id, owning_id);
  ASSERT_TRUE(ActivityUserData::GetOwningProcessId(user_data.GetBaseAddress(),
                                                   &owning_id, &stamp));
  EXPECT_NE(other_process_id, owning_id);
  global->process_data().SetOwningProcessIdForTesting(other_process_id,
                                                      past_stamp);
  thread->SetOwningProcessIdForTesting(other_process_id, past_stamp);
  user_data.SetOwningProcessIdForTesting(other_process_id, past_stamp);
  ASSERT_TRUE(ActivityUserData::GetOwningProcessId(
      global->process_data().GetBaseAddress(), &owning_id, &stamp));
  EXPECT_EQ(other_process_id, owning_id);
  ASSERT_TRUE(ThreadActivityTracker::GetOwningProcessId(
      thread->GetBaseAddress(), &owning_id, &stamp));
  EXPECT_EQ(other_process_id, owning_id);
  ASSERT_TRUE(ActivityUserData::GetOwningProcessId(user_data.GetBaseAddress(),
                                                   &owning_id, &stamp));
  EXPECT_EQ(other_process_id, owning_id);

  // Check that process exit will perform callback and free the allocations.
  ASSERT_EQ(0, exit_id_);
  ASSERT_EQ(GlobalActivityTracker::kTypeIdProcessDataRecord,
            global->allocator()->GetType(proc_data_ref));
  ASSERT_EQ(GlobalActivityTracker::kTypeIdActivityTracker,
            global->allocator()->GetType(tracker_ref));
  ASSERT_EQ(GlobalActivityTracker::kTypeIdUserDataRecord,
            global->allocator()->GetType(user_data_ref));
  global->RecordProcessExit(other_process_id, 0);
  EXPECT_EQ(other_process_id, exit_id_);
  EXPECT_EQ("foo --bar", exit_command_);
  EXPECT_EQ(GlobalActivityTracker::kTypeIdProcessDataRecordFree,
            global->allocator()->GetType(proc_data_ref));
  EXPECT_EQ(GlobalActivityTracker::kTypeIdActivityTrackerFree,
            global->allocator()->GetType(tracker_ref));
  EXPECT_EQ(GlobalActivityTracker::kTypeIdUserDataRecordFree,
            global->allocator()->GetType(user_data_ref));

  // Restore memory contents and types so things don't crash when doing real
  // process clean-up.
  memcpy(const_cast<void*>(thread->GetBaseAddress()), tracker_copy.get(),
         tracker_size);
  global->allocator()->ChangeType(
      proc_data_ref, GlobalActivityTracker::kTypeIdProcessDataRecord,
      GlobalActivityTracker::kTypeIdUserDataRecordFree, false);
  global->allocator()->ChangeType(
      tracker_ref, GlobalActivityTracker::kTypeIdActivityTracker,
      GlobalActivityTracker::kTypeIdActivityTrackerFree, false);
  global->allocator()->ChangeType(
      user_data_ref, GlobalActivityTracker::kTypeIdUserDataRecord,
      GlobalActivityTracker::kTypeIdUserDataRecordFree, false);
}

}  // namespace debug
}  // namespace base
