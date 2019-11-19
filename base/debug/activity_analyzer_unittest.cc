// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/activity_analyzer.h"

#include <atomic>
#include <memory>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/debug/activity_tracker.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/pending_task.h"
#include "base/process/process.h"
#include "base/stl_util.h"
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


class ActivityAnalyzerTest : public testing::Test {
 public:
  const int kMemorySize = 1 << 20;  // 1MiB
  const int kStackSize  = 1 << 10;  // 1KiB

  ActivityAnalyzerTest() = default;

  ~ActivityAnalyzerTest() override {
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

  template <typename Function>
  void AsOtherProcess(int64_t pid, Function function) {
    std::unique_ptr<GlobalActivityTracker> old_global =
        GlobalActivityTracker::ReleaseForTesting();
    ASSERT_TRUE(old_global);

    PersistentMemoryAllocator* old_allocator = old_global->allocator();
    std::unique_ptr<PersistentMemoryAllocator> new_allocator(
        std::make_unique<PersistentMemoryAllocator>(
            const_cast<void*>(old_allocator->data()), old_allocator->size(), 0,
            0, "", false));
    GlobalActivityTracker::CreateWithAllocator(std::move(new_allocator), 3,
                                               pid);

    function();

    GlobalActivityTracker::ReleaseForTesting();
    GlobalActivityTracker::SetForTesting(std::move(old_global));
  }

  static void DoNothing() {}
};

TEST_F(ActivityAnalyzerTest, ThreadAnalyzerConstruction) {
  std::unique_ptr<ThreadActivityTracker> tracker = CreateActivityTracker();
  {
    ThreadActivityAnalyzer analyzer(*tracker);
    EXPECT_TRUE(analyzer.IsValid());
    EXPECT_EQ(PlatformThread::GetName(), analyzer.GetThreadName());
  }

  // TODO(bcwhite): More tests once Analyzer does more.
}


// GlobalActivityAnalyzer tests below.

namespace {

class SimpleActivityThread : public SimpleThread {
 public:
  SimpleActivityThread(const std::string& name,
                       const void* source,
                       Activity::Type activity,
                       const ActivityData& data)
      : SimpleThread(name, Options()),
        source_(source),
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
            ->PushActivity(source_, activity_, data_);

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
  const void* source_;
  Activity::Type activity_;
  ActivityData data_;

  std::atomic<bool> ready_;
  std::atomic<bool> exit_;
  Lock lock_;
  ConditionVariable exit_condition_;

  DISALLOW_COPY_AND_ASSIGN(SimpleActivityThread);
};

}  // namespace

TEST_F(ActivityAnalyzerTest, GlobalAnalyzerConstruction) {
  GlobalActivityTracker::CreateWithLocalMemory(kMemorySize, 0, "", 3, 0);
  GlobalActivityTracker::Get()->process_data().SetString("foo", "bar");

  PersistentMemoryAllocator* allocator =
      GlobalActivityTracker::Get()->allocator();
  GlobalActivityAnalyzer analyzer(std::make_unique<PersistentMemoryAllocator>(
      const_cast<void*>(allocator->data()), allocator->size(), 0, 0, "", true));

  // The only thread at this point is the test thread of this process.
  const int64_t pid = analyzer.GetFirstProcess();
  ASSERT_NE(0, pid);
  ThreadActivityAnalyzer* ta1 = analyzer.GetFirstAnalyzer(pid);
  ASSERT_TRUE(ta1);
  EXPECT_FALSE(analyzer.GetNextAnalyzer());
  ThreadActivityAnalyzer::ThreadKey tk1 = ta1->GetThreadKey();
  EXPECT_EQ(ta1, analyzer.GetAnalyzerForThread(tk1));
  EXPECT_EQ(0, analyzer.GetNextProcess());

  // Create a second thread that will do something.
  SimpleActivityThread t2("t2", nullptr, Activity::ACT_TASK,
                          ActivityData::ForTask(11));
  t2.Start();
  t2.WaitReady();

  // Now there should be two. Calling GetFirstProcess invalidates any
  // previously returned analyzer pointers.
  ASSERT_EQ(pid, analyzer.GetFirstProcess());
  EXPECT_TRUE(analyzer.GetFirstAnalyzer(pid));
  EXPECT_TRUE(analyzer.GetNextAnalyzer());
  EXPECT_FALSE(analyzer.GetNextAnalyzer());
  EXPECT_EQ(0, analyzer.GetNextProcess());

  // Let thread exit.
  t2.Exit();
  t2.Join();

  // Now there should be only one again.
  ASSERT_EQ(pid, analyzer.GetFirstProcess());
  ThreadActivityAnalyzer* ta2 = analyzer.GetFirstAnalyzer(pid);
  ASSERT_TRUE(ta2);
  EXPECT_FALSE(analyzer.GetNextAnalyzer());
  ThreadActivityAnalyzer::ThreadKey tk2 = ta2->GetThreadKey();
  EXPECT_EQ(ta2, analyzer.GetAnalyzerForThread(tk2));
  EXPECT_EQ(tk1, tk2);
  EXPECT_EQ(0, analyzer.GetNextProcess());

  // Verify that there is process data.
  const ActivityUserData::Snapshot& data_snapshot =
      analyzer.GetProcessDataSnapshot(pid);
  ASSERT_LE(1U, data_snapshot.size());
  EXPECT_EQ("bar", data_snapshot.at("foo").GetString());
}

TEST_F(ActivityAnalyzerTest, GlobalAnalyzerFromSharedMemory) {
  base::MappedReadOnlyRegion shm =
      base::ReadOnlySharedMemoryRegion::Create(kMemorySize);
  ASSERT_TRUE(shm.IsValid());
  base::WritableSharedMemoryMapping rw_mapping = std::move(shm.mapping);
  base::ReadOnlySharedMemoryMapping ro_mapping = shm.region.Map();
  ASSERT_TRUE(ro_mapping.IsValid());

  GlobalActivityTracker::CreateWithSharedMemory(std::move(rw_mapping), 0, "",
                                                3);
  GlobalActivityTracker::Get()->process_data().SetString("foo", "bar");

  std::unique_ptr<GlobalActivityAnalyzer> analyzer =
      GlobalActivityAnalyzer::CreateWithSharedMemory(std::move(ro_mapping));

  const int64_t pid = analyzer->GetFirstProcess();
  ASSERT_NE(0, pid);
  const ActivityUserData::Snapshot& data_snapshot =
      analyzer->GetProcessDataSnapshot(pid);
  ASSERT_LE(1U, data_snapshot.size());
  EXPECT_EQ("bar", data_snapshot.at("foo").GetString());
}

TEST_F(ActivityAnalyzerTest, UserDataSnapshotTest) {
  GlobalActivityTracker::CreateWithLocalMemory(kMemorySize, 0, "", 3, 0);
  ThreadActivityAnalyzer::Snapshot tracker_snapshot;

  const char string1a[] = "string1a";
  const char string1b[] = "string1b";
  const char string2a[] = "string2a";
  const char string2b[] = "string2b";

  PersistentMemoryAllocator* allocator =
      GlobalActivityTracker::Get()->allocator();
  GlobalActivityAnalyzer global_analyzer(
      std::make_unique<PersistentMemoryAllocator>(
          const_cast<void*>(allocator->data()), allocator->size(), 0, 0, "",
          true));

  ThreadActivityTracker* tracker =
      GlobalActivityTracker::Get()->GetOrCreateTrackerForCurrentThread();

  {
    ScopedActivity activity1(1, 11, 111);
    ActivityUserData& user_data1 = activity1.user_data();
    user_data1.Set("raw1", "foo1", 4);
    user_data1.SetString("string1", "bar1");
    user_data1.SetChar("char1", '1');
    user_data1.SetInt("int1", -1111);
    user_data1.SetUint("uint1", 1111);
    user_data1.SetBool("bool1", true);
    user_data1.SetReference("ref1", string1a, sizeof(string1a));
    user_data1.SetStringReference("sref1", string1b);

    {
      ScopedActivity activity2(2, 22, 222);
      ActivityUserData& user_data2 = activity2.user_data();
      user_data2.Set("raw2", "foo2", 4);
      user_data2.SetString("string2", "bar2");
      user_data2.SetChar("char2", '2');
      user_data2.SetInt("int2", -2222);
      user_data2.SetUint("uint2", 2222);
      user_data2.SetBool("bool2", false);
      user_data2.SetReference("ref2", string2a, sizeof(string2a));
      user_data2.SetStringReference("sref2", string2b);

      ASSERT_TRUE(tracker->CreateSnapshot(&tracker_snapshot));
      ASSERT_EQ(2U, tracker_snapshot.activity_stack.size());

      ThreadActivityAnalyzer analyzer(*tracker);
      analyzer.AddGlobalInformation(&global_analyzer);
      const ThreadActivityAnalyzer::Snapshot& analyzer_snapshot =
          analyzer.activity_snapshot();
      ASSERT_EQ(2U, analyzer_snapshot.user_data_stack.size());
      const ActivityUserData::Snapshot& user_data =
          analyzer_snapshot.user_data_stack.at(1);
      EXPECT_EQ(8U, user_data.size());
      ASSERT_TRUE(Contains(user_data, "raw2"));
      EXPECT_EQ("foo2", user_data.at("raw2").Get().as_string());
      ASSERT_TRUE(Contains(user_data, "string2"));
      EXPECT_EQ("bar2", user_data.at("string2").GetString().as_string());
      ASSERT_TRUE(Contains(user_data, "char2"));
      EXPECT_EQ('2', user_data.at("char2").GetChar());
      ASSERT_TRUE(Contains(user_data, "int2"));
      EXPECT_EQ(-2222, user_data.at("int2").GetInt());
      ASSERT_TRUE(Contains(user_data, "uint2"));
      EXPECT_EQ(2222U, user_data.at("uint2").GetUint());
      ASSERT_TRUE(Contains(user_data, "bool2"));
      EXPECT_FALSE(user_data.at("bool2").GetBool());
      ASSERT_TRUE(Contains(user_data, "ref2"));
      EXPECT_EQ(string2a, user_data.at("ref2").GetReference().data());
      EXPECT_EQ(sizeof(string2a), user_data.at("ref2").GetReference().size());
      ASSERT_TRUE(Contains(user_data, "sref2"));
      EXPECT_EQ(string2b, user_data.at("sref2").GetStringReference().data());
      EXPECT_EQ(strlen(string2b),
                user_data.at("sref2").GetStringReference().size());
    }

    ASSERT_TRUE(tracker->CreateSnapshot(&tracker_snapshot));
    ASSERT_EQ(1U, tracker_snapshot.activity_stack.size());

    ThreadActivityAnalyzer analyzer(*tracker);
    analyzer.AddGlobalInformation(&global_analyzer);
    const ThreadActivityAnalyzer::Snapshot& analyzer_snapshot =
        analyzer.activity_snapshot();
    ASSERT_EQ(1U, analyzer_snapshot.user_data_stack.size());
    const ActivityUserData::Snapshot& user_data =
        analyzer_snapshot.user_data_stack.at(0);
    EXPECT_EQ(8U, user_data.size());
    EXPECT_EQ("foo1", user_data.at("raw1").Get().as_string());
    EXPECT_EQ("bar1", user_data.at("string1").GetString().as_string());
    EXPECT_EQ('1', user_data.at("char1").GetChar());
    EXPECT_EQ(-1111, user_data.at("int1").GetInt());
    EXPECT_EQ(1111U, user_data.at("uint1").GetUint());
    EXPECT_TRUE(user_data.at("bool1").GetBool());
    EXPECT_EQ(string1a, user_data.at("ref1").GetReference().data());
    EXPECT_EQ(sizeof(string1a), user_data.at("ref1").GetReference().size());
    EXPECT_EQ(string1b, user_data.at("sref1").GetStringReference().data());
    EXPECT_EQ(strlen(string1b),
              user_data.at("sref1").GetStringReference().size());
  }

  ASSERT_TRUE(tracker->CreateSnapshot(&tracker_snapshot));
  ASSERT_EQ(0U, tracker_snapshot.activity_stack.size());
}

TEST_F(ActivityAnalyzerTest, GlobalUserDataTest) {
  const int64_t pid = GetCurrentProcId();
  GlobalActivityTracker::CreateWithLocalMemory(kMemorySize, 0, "", 3, 0);

  const char string1[] = "foo";
  const char string2[] = "bar";

  PersistentMemoryAllocator* allocator =
      GlobalActivityTracker::Get()->allocator();
  GlobalActivityAnalyzer global_analyzer(
      std::make_unique<PersistentMemoryAllocator>(
          const_cast<void*>(allocator->data()), allocator->size(), 0, 0, "",
          true));

  ActivityUserData& process_data = GlobalActivityTracker::Get()->process_data();
  ASSERT_NE(0U, process_data.id());
  process_data.Set("raw", "foo", 3);
  process_data.SetString("string", "bar");
  process_data.SetChar("char", '9');
  process_data.SetInt("int", -9999);
  process_data.SetUint("uint", 9999);
  process_data.SetBool("bool", true);
  process_data.SetReference("ref", string1, sizeof(string1));
  process_data.SetStringReference("sref", string2);

  int64_t first_pid = global_analyzer.GetFirstProcess();
  DCHECK_EQ(pid, first_pid);
  const ActivityUserData::Snapshot& snapshot =
      global_analyzer.GetProcessDataSnapshot(pid);
  ASSERT_TRUE(Contains(snapshot, "raw"));
  EXPECT_EQ("foo", snapshot.at("raw").Get().as_string());
  ASSERT_TRUE(Contains(snapshot, "string"));
  EXPECT_EQ("bar", snapshot.at("string").GetString().as_string());
  ASSERT_TRUE(Contains(snapshot, "char"));
  EXPECT_EQ('9', snapshot.at("char").GetChar());
  ASSERT_TRUE(Contains(snapshot, "int"));
  EXPECT_EQ(-9999, snapshot.at("int").GetInt());
  ASSERT_TRUE(Contains(snapshot, "uint"));
  EXPECT_EQ(9999U, snapshot.at("uint").GetUint());
  ASSERT_TRUE(Contains(snapshot, "bool"));
  EXPECT_TRUE(snapshot.at("bool").GetBool());
  ASSERT_TRUE(Contains(snapshot, "ref"));
  EXPECT_EQ(string1, snapshot.at("ref").GetReference().data());
  EXPECT_EQ(sizeof(string1), snapshot.at("ref").GetReference().size());
  ASSERT_TRUE(Contains(snapshot, "sref"));
  EXPECT_EQ(string2, snapshot.at("sref").GetStringReference().data());
  EXPECT_EQ(strlen(string2), snapshot.at("sref").GetStringReference().size());
}

TEST_F(ActivityAnalyzerTest, GlobalModulesTest) {
  GlobalActivityTracker::CreateWithLocalMemory(kMemorySize, 0, "", 3, 0);
  GlobalActivityTracker* global = GlobalActivityTracker::Get();

  PersistentMemoryAllocator* allocator = global->allocator();
  GlobalActivityAnalyzer global_analyzer(
      std::make_unique<PersistentMemoryAllocator>(
          const_cast<void*>(allocator->data()), allocator->size(), 0, 0, "",
          true));

  GlobalActivityTracker::ModuleInfo info1;
  info1.is_loaded = true;
  info1.address = 0x12345678;
  info1.load_time = 1111;
  info1.size = 0xABCDEF;
  info1.timestamp = 111;
  info1.age = 11;
  info1.identifier[0] = 1;
  info1.file = "anything";
  info1.debug_file = "elsewhere";

  global->RecordModuleInfo(info1);
  std::vector<GlobalActivityTracker::ModuleInfo> modules1;
  modules1 = global_analyzer.GetModules(global_analyzer.GetFirstProcess());
  ASSERT_EQ(1U, modules1.size());
  GlobalActivityTracker::ModuleInfo& stored1a = modules1[0];
  EXPECT_EQ(info1.is_loaded, stored1a.is_loaded);
  EXPECT_EQ(info1.address, stored1a.address);
  EXPECT_NE(info1.load_time, stored1a.load_time);
  EXPECT_EQ(info1.size, stored1a.size);
  EXPECT_EQ(info1.timestamp, stored1a.timestamp);
  EXPECT_EQ(info1.age, stored1a.age);
  EXPECT_EQ(info1.identifier[0], stored1a.identifier[0]);
  EXPECT_EQ(info1.file, stored1a.file);
  EXPECT_EQ(info1.debug_file, stored1a.debug_file);

  info1.is_loaded = false;
  global->RecordModuleInfo(info1);
  modules1 = global_analyzer.GetModules(global_analyzer.GetFirstProcess());
  ASSERT_EQ(1U, modules1.size());
  GlobalActivityTracker::ModuleInfo& stored1b = modules1[0];
  EXPECT_EQ(info1.is_loaded, stored1b.is_loaded);
  EXPECT_EQ(info1.address, stored1b.address);
  EXPECT_NE(info1.load_time, stored1b.load_time);
  EXPECT_EQ(info1.size, stored1b.size);
  EXPECT_EQ(info1.timestamp, stored1b.timestamp);
  EXPECT_EQ(info1.age, stored1b.age);
  EXPECT_EQ(info1.identifier[0], stored1b.identifier[0]);
  EXPECT_EQ(info1.file, stored1b.file);
  EXPECT_EQ(info1.debug_file, stored1b.debug_file);

  GlobalActivityTracker::ModuleInfo info2;
  info2.is_loaded = true;
  info2.address = 0x87654321;
  info2.load_time = 2222;
  info2.size = 0xFEDCBA;
  info2.timestamp = 222;
  info2.age = 22;
  info2.identifier[0] = 2;
  info2.file = "nothing";
  info2.debug_file = "farewell";

  global->RecordModuleInfo(info2);
  std::vector<GlobalActivityTracker::ModuleInfo> modules2;
  modules2 = global_analyzer.GetModules(global_analyzer.GetFirstProcess());
  ASSERT_EQ(2U, modules2.size());
  GlobalActivityTracker::ModuleInfo& stored2 = modules2[1];
  EXPECT_EQ(info2.is_loaded, stored2.is_loaded);
  EXPECT_EQ(info2.address, stored2.address);
  EXPECT_NE(info2.load_time, stored2.load_time);
  EXPECT_EQ(info2.size, stored2.size);
  EXPECT_EQ(info2.timestamp, stored2.timestamp);
  EXPECT_EQ(info2.age, stored2.age);
  EXPECT_EQ(info2.identifier[0], stored2.identifier[0]);
  EXPECT_EQ(info2.file, stored2.file);
  EXPECT_EQ(info2.debug_file, stored2.debug_file);
}

TEST_F(ActivityAnalyzerTest, GlobalLogMessages) {
  GlobalActivityTracker::CreateWithLocalMemory(kMemorySize, 0, "", 3, 0);

  PersistentMemoryAllocator* allocator =
      GlobalActivityTracker::Get()->allocator();
  GlobalActivityAnalyzer analyzer(std::make_unique<PersistentMemoryAllocator>(
      const_cast<void*>(allocator->data()), allocator->size(), 0, 0, "", true));

  GlobalActivityTracker::Get()->RecordLogMessage("hello world");
  GlobalActivityTracker::Get()->RecordLogMessage("foo bar");

  std::vector<std::string> messages = analyzer.GetLogMessages();
  ASSERT_EQ(2U, messages.size());
  EXPECT_EQ("hello world", messages[0]);
  EXPECT_EQ("foo bar", messages[1]);
}

TEST_F(ActivityAnalyzerTest, GlobalMultiProcess) {
  GlobalActivityTracker::CreateWithLocalMemory(kMemorySize, 0, "", 3, 1001);
  GlobalActivityTracker* global = GlobalActivityTracker::Get();
  PersistentMemoryAllocator* allocator = global->allocator();
  EXPECT_EQ(1001, global->process_id());

  int64_t process_id;
  int64_t create_stamp;
  ActivityUserData::GetOwningProcessId(
      GlobalActivityTracker::Get()->process_data().GetBaseAddress(),
      &process_id, &create_stamp);
  ASSERT_EQ(1001, process_id);

  GlobalActivityTracker::Get()->process_data().SetInt("pid",
                                                      global->process_id());

  GlobalActivityAnalyzer analyzer(std::make_unique<PersistentMemoryAllocator>(
      const_cast<void*>(allocator->data()), allocator->size(), 0, 0, "", true));

  AsOtherProcess(2002, [&global]() {
    ASSERT_NE(global, GlobalActivityTracker::Get());
    EXPECT_EQ(2002, GlobalActivityTracker::Get()->process_id());

    int64_t process_id;
    int64_t create_stamp;
    ActivityUserData::GetOwningProcessId(
        GlobalActivityTracker::Get()->process_data().GetBaseAddress(),
        &process_id, &create_stamp);
    ASSERT_EQ(2002, process_id);

    GlobalActivityTracker::Get()->process_data().SetInt(
        "pid", GlobalActivityTracker::Get()->process_id());
  });
  ASSERT_EQ(global, GlobalActivityTracker::Get());
  EXPECT_EQ(1001, GlobalActivityTracker::Get()->process_id());

  const int64_t pid1 = analyzer.GetFirstProcess();
  ASSERT_EQ(1001, pid1);
  const int64_t pid2 = analyzer.GetNextProcess();
  ASSERT_EQ(2002, pid2);
  EXPECT_EQ(0, analyzer.GetNextProcess());

  const ActivityUserData::Snapshot& pdata1 =
      analyzer.GetProcessDataSnapshot(pid1);
  const ActivityUserData::Snapshot& pdata2 =
      analyzer.GetProcessDataSnapshot(pid2);
  EXPECT_EQ(1001, pdata1.at("pid").GetInt());
  EXPECT_EQ(2002, pdata2.at("pid").GetInt());
}

}  // namespace debug
}  // namespace base
