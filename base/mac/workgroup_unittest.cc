// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/workgroup.h"

#include <os/object.h>
#include <os/workgroup.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::mac {
namespace {

class JoinAndExitThread : public SimpleThread {
 public:
  explicit JoinAndExitThread(Workgroup workgroup)
      : SimpleThread("WorkgroupJoinAndExitThread"),
        workgroup_(std::move(workgroup)) {}

  void Run() override {
    joined_ = Workgroup::JoinCurrentThread(workgroup_);
    // Exit without LeaveCurrentThread() to exercise TLS cleanup.
  }

  bool joined() const { return joined_; }

 private:
  Workgroup workgroup_;
  bool joined_ = false;
};

// Owns a WorkgroupInterval on a dedicated background thread (mirroring
// VizCompositorThread) so the main test thread can act as a client thread.
class RemoteIntervalHost {
 public:
  explicit RemoteIntervalHost(const char* name) : thread_(name) {
    CHECK(thread_.Start());
    thread_.task_runner()->PostTask(FROM_HERE, BindLambdaForTesting([&] {
                                      interval_ =
                                          WorkgroupInterval::Create(name);
                                      if (interval_) {
                                        workgroup_ = interval_->workgroup();
                                      }
                                    }));
    thread_.FlushForTesting();
  }

  ~RemoteIntervalHost() {
    thread_.task_runner()->PostTask(
        FROM_HERE, BindLambdaForTesting([&] { interval_.reset(); }));
    thread_.FlushForTesting();
  }

  const Workgroup& workgroup() const { return workgroup_; }
  apple::ScopedMachSendRight CopyPort() const { return workgroup_.CopyPort(); }

 private:
  Thread thread_;
  std::unique_ptr<WorkgroupInterval> interval_;
  Workgroup workgroup_;
};

class WorkgroupIntervalTest : public testing::Test {
 protected:
  WorkgroupIntervalTest() {
    scoped_feature_list_.InitAndEnableFeature(kMacWorkgroupInterval);
  }

  ~WorkgroupIntervalTest() override {
    ResetProcessForegroundWorkgroupForTesting();
  }

 private:
  test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(WorkgroupIntervalTest, CreateStartUpdateFinishAndPortImport) {
  if (!WorkgroupInterval::IsSupported()) {
    EXPECT_EQ(WorkgroupInterval::Create("CrUnsupportedTest"), nullptr);
    GTEST_SKIP() << "WorkgroupInterval unsupported on this host";
  }

  auto interval = WorkgroupInterval::Create("CrTestInterval");
  ASSERT_NE(interval, nullptr);
  EXPECT_FALSE(interval->is_started());
  EXPECT_EQ(Workgroup::GetCurrentThreadWorkgroup(), interval->workgroup());

  // Creating a second WorkgroupInterval on the same thread fails cleanly, and
  // LeaveCurrentThread() does not unjoin an active interval owner.
  EXPECT_EQ(WorkgroupInterval::Create("CrDuplicateInterval"), nullptr);
  Workgroup::LeaveCurrentThread();
  EXPECT_EQ(Workgroup::GetCurrentThreadWorkgroup(), interval->workgroup());

  // A start time slightly in the future should be clamped to now.
  const TimeTicks now = TimeTicks::Now();
  EXPECT_TRUE(interval->Start(now + Milliseconds(2), now + Milliseconds(16)));
  EXPECT_TRUE(interval->is_started());
  EXPECT_TRUE(interval->Update(now + Milliseconds(12)));
  EXPECT_TRUE(interval->Finish());
  EXPECT_FALSE(interval->is_started());

  apple::ScopedMachSendRight port = interval->CopyPort();
  ASSERT_TRUE(port.is_valid());
  Workgroup imported = Workgroup::CreateFromPort("CrImportedTest", port.get());
  ASSERT_TRUE(imported);

  Workgroup copy = imported;
  EXPECT_EQ(copy, imported);
  Workgroup moved = std::move(copy);
  EXPECT_FALSE(copy);
  EXPECT_EQ(moved, imported);
  moved = {};
  EXPECT_FALSE(moved);

  JoinAndExitThread helper(imported);
  helper.Start();
  helper.Join();
  EXPECT_TRUE(helper.joined());

  interval.reset();
  EXPECT_FALSE(Workgroup::GetCurrentThreadWorkgroup());
}

TEST_F(WorkgroupIntervalTest, ExternalWorkgroupMembershipIsPreserved) {
  if (!WorkgroupInterval::IsSupported()) {
    GTEST_SKIP() << "WorkgroupInterval unsupported on this host";
  }

  RemoteIntervalHost external_host("CrExternalHost");
  RemoteIntervalHost chrome_host("CrChromeHost");
  ASSERT_TRUE(external_host.workgroup());
  ASSERT_TRUE(chrome_host.workgroup());

  apple::ScopedMachSendRight port = external_host.CopyPort();
  ASSERT_TRUE(port.is_valid());
  os_workgroup_t raw_wg =
      os_workgroup_create_with_port("CrRawExternal", port.get());
  ASSERT_NE(raw_wg, nullptr);
  os_workgroup_join_token_s raw_token{};
  ASSERT_EQ(os_workgroup_join(raw_wg, &raw_token), 0);

  // Both JoinCurrentThread and WorkgroupInterval::Create fail with EALREADY
  // without disturbing `raw_wg`.
  EXPECT_FALSE(Workgroup::JoinCurrentThread(chrome_host.workgroup()));
  EXPECT_EQ(WorkgroupInterval::Create("CrBlockedByExternal"), nullptr);
  EXPECT_FALSE(Workgroup::GetCurrentThreadWorkgroup());

  os_workgroup_leave(raw_wg, &raw_token);
  os_release(raw_wg);
}

TEST_F(WorkgroupIntervalTest, DestructorFinishesStartedIntervalAndLeaves) {
  if (!WorkgroupInterval::IsSupported()) {
    GTEST_SKIP() << "WorkgroupInterval unsupported on this host";
  }

  auto interval = WorkgroupInterval::Create("CrDtorSafetyTest");
  ASSERT_NE(interval, nullptr);
  const TimeTicks now = TimeTicks::Now();
  ASSERT_TRUE(interval->Start(now, now + Milliseconds(16)));

  interval.reset();
  EXPECT_FALSE(Workgroup::GetCurrentThreadWorkgroup());
}

TEST_F(WorkgroupIntervalTest,
       ForegroundSyncDeduplicatesPortsAndPreservesExplicitJoins) {
  if (!WorkgroupInterval::IsSupported()) {
    GTEST_SKIP() << "WorkgroupInterval unsupported on this host";
  }

  EXPECT_TRUE(ShouldJoinProcessForegroundWorkgroup(ThreadType::kDefault));
  EXPECT_TRUE(ShouldJoinProcessForegroundWorkgroup(ThreadType::kPresentation));
  EXPECT_FALSE(ShouldJoinProcessForegroundWorkgroup(ThreadType::kBackground));
  EXPECT_FALSE(ShouldJoinProcessForegroundWorkgroup(ThreadType::kUtility));
  EXPECT_FALSE(
      ShouldJoinProcessForegroundWorkgroup(ThreadType::kAudioProcessing));
  EXPECT_FALSE(
      ShouldJoinProcessForegroundWorkgroup(ThreadType::kRealtimeAudio));

  RemoteIntervalHost display_host("CrForegroundSyncHost");
  RemoteIntervalHost explicit_host("CrExplicitJoinHost");
  ASSERT_TRUE(display_host.workgroup());
  ASSERT_TRUE(explicit_host.workgroup());

  apple::ScopedMachSendRight port1 = display_host.CopyPort();
  ASSERT_TRUE(port1.is_valid());
  Workgroup imported1 = Workgroup::CreateFromPort("CrFrameClient", port1.get());
  ASSERT_TRUE(imported1);

  uint32_t local_gen = 0;
  SetProcessForegroundWorkgroup(imported1);
  MaybeSyncCurrentThreadToForegroundWorkgroup(local_gen);
  EXPECT_NE(local_gen, 0u);
  EXPECT_EQ(Workgroup::GetCurrentThreadWorkgroup(), imported1);

  // Importing another send right to the same port must not bump the generation
  // counter or replace the joined handle.
  const uint32_t gen_before_second_sink =
      internal::g_foreground_workgroup_generation.load();
  apple::ScopedMachSendRight port2 = display_host.CopyPort();
  ASSERT_TRUE(port2.is_valid());
  Workgroup imported2 = Workgroup::CreateFromPort("CrFrameClient", port2.get());
  ASSERT_TRUE(imported2);
  SetProcessForegroundWorkgroup(imported2);
  EXPECT_EQ(internal::g_foreground_workgroup_generation.load(),
            gen_before_second_sink);
  EXPECT_EQ(Workgroup::GetCurrentThreadWorkgroup(), imported1);

  // An explicit JoinCurrentThread() must not be overwritten by foreground sync.
  ASSERT_TRUE(Workgroup::JoinCurrentThread(explicit_host.workgroup()));
  SetProcessForegroundWorkgroupEnabled(false);
  MaybeSyncCurrentThreadToForegroundWorkgroup(local_gen);
  EXPECT_EQ(Workgroup::GetCurrentThreadWorkgroup(), explicit_host.workgroup());
  Workgroup::LeaveCurrentThread();
}

TEST_F(WorkgroupIntervalTest,
       EnableDisableSyncsSequenceManagerAndThreadPoolWorkers) {
  if (!WorkgroupInterval::IsSupported()) {
    GTEST_SKIP() << "WorkgroupInterval unsupported on this host";
  }

  test::TaskEnvironment task_environment;
  Thread helper_thread("CrTestCompositorThread");
  ASSERT_TRUE(helper_thread.Start());
  Thread bg_thread("CrTestBackgroundThread");
  Thread::Options bg_options;
  bg_options.thread_type = ThreadType::kBackground;
  ASSERT_TRUE(bg_thread.StartWithOptions(std::move(bg_options)));

  RemoteIntervalHost display_host("CrEnableDisableHost");
  ASSERT_TRUE(display_host.workgroup());

  // Disabled before configuring: stay unjoined until enabled.
  SetProcessForegroundWorkgroupEnabled(false);
  SetProcessForegroundWorkgroup(display_host.workgroup());
  EXPECT_FALSE(Workgroup::GetCurrentThreadWorkgroup());

  SetProcessForegroundWorkgroupEnabled(true);
  EXPECT_EQ(Workgroup::GetCurrentThreadWorkgroup(), display_host.workgroup());

  bool helper_joined = false;
  helper_thread.task_runner()->PostTask(
      FROM_HERE, BindOnce(
                     [](Workgroup expected, bool* out) {
                       *out =
                           (Workgroup::GetCurrentThreadWorkgroup() == expected);
                     },
                     display_host.workgroup(), Unretained(&helper_joined)));
  helper_thread.FlushForTesting();
  EXPECT_TRUE(helper_joined);

  // kBackground threads are excluded even while foreground sync is enabled.
  bool bg_unjoined = false;
  bg_thread.task_runner()->PostTask(
      FROM_HERE,
      BindOnce(
          [](bool* out) { *out = !Workgroup::GetCurrentThreadWorkgroup(); },
          Unretained(&bg_unjoined)));
  bg_thread.FlushForTesting();
  EXPECT_TRUE(bg_unjoined);

  // Foreground ThreadPool workers join when enabled and leave when disabled.
  {
    RunLoop loop;
    ThreadPool::PostTask(FROM_HERE, {TaskPriority::USER_BLOCKING},
                         BindLambdaForTesting([&] {
                           EXPECT_EQ(Workgroup::GetCurrentThreadWorkgroup(),
                                     display_host.workgroup());
                           loop.Quit();
                         }));
    loop.Run();
  }

  SetProcessForegroundWorkgroupEnabled(false);
  EXPECT_FALSE(Workgroup::GetCurrentThreadWorkgroup());

  {
    RunLoop loop;
    ThreadPool::PostTask(FROM_HERE, {TaskPriority::USER_BLOCKING},
                         BindLambdaForTesting([&] {
                           EXPECT_FALSE(Workgroup::GetCurrentThreadWorkgroup());
                           loop.Quit();
                         }));
    loop.Run();
  }

  bool helper_left = false;
  helper_thread.task_runner()->PostTask(
      FROM_HERE,
      BindOnce(
          [](bool* out) { *out = !Workgroup::GetCurrentThreadWorkgroup(); },
          Unretained(&helper_left)));
  helper_thread.FlushForTesting();
  EXPECT_TRUE(helper_left);

  ResetProcessForegroundWorkgroupForTesting();
  helper_thread.FlushForTesting();
  EXPECT_FALSE(Workgroup::GetCurrentThreadWorkgroup());
}

}  // namespace
}  // namespace base::mac
