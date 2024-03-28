// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus_schedqos_state_handler.h"

#include <memory>

#include "base/process/process.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "chromeos/ash/components/dbus/resourced/fake_resourced_client.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"
#include "dbus/dbus_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/resource_manager/dbus-constants.h"

using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace ash {

class DBusSchedQOSStateHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    resourced_client_ = ResourcedClient::InitializeFake();
    handler_.reset(DBusSchedQOSStateHandler::Create(
        base::SequencedTaskRunner::GetCurrentDefault()));
    process_ = base::Process::Current();
  }

  void TearDown() override {
    handler_.reset();
    resourced_client_ = nullptr;
    ResourcedClient::Shutdown();
  }

  // The handler under test.
  base::Process process_;
  std::unique_ptr<DBusSchedQOSStateHandler> handler_;
  raw_ptr<FakeResourcedClient> resourced_client_ = nullptr;

  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(DBusSchedQOSStateHandlerTest, CanSetProcessPriority) {
  EXPECT_TRUE(handler_->CanSetProcessPriority());
  EXPECT_TRUE(process_.CanSetPriority());
}

TEST_F(DBusSchedQOSStateHandlerTest, InitializeProcessPriority) {
  ASSERT_TRUE(resourced_client_->TriggerServiceAvailable(true));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(resourced_client_->GetProcessStateHistory().size(), 0ul);

  process_.InitializePriority();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(resourced_client_->GetProcessStateHistory(),
              ElementsAre(Pair(process_.Pid(),
                               resource_manager::ProcessState::kNormal)));
}

TEST_F(DBusSchedQOSStateHandlerTest, SetProcessPriority) {
  ASSERT_TRUE(resourced_client_->TriggerServiceAvailable(true));
  task_environment_.RunUntilIdle();
  process_.InitializePriority();
  base::Process dummy_process = base::Process::Open(1);
  dummy_process.InitializePriority();
  task_environment_.RunUntilIdle();

  ASSERT_EQ(resourced_client_->GetProcessStateHistory().size(), 2ul);

  process_.SetPriority(base::Process::Priority::kUserBlocking);
  dummy_process.SetPriority(base::Process::Priority::kBestEffort);
  dummy_process.SetPriority(base::Process::Priority::kUserVisible);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(resourced_client_->GetProcessStateHistory().size(), 5ul);

  EXPECT_THAT(
      absl::MakeSpan(resourced_client_->GetProcessStateHistory()).last(3),
      ElementsAre(
          Pair(process_.Pid(), resource_manager::ProcessState::kNormal),
          Pair(dummy_process.Pid(),
               resource_manager::ProcessState::kBackground),
          Pair(dummy_process.Pid(), resource_manager::ProcessState::kNormal)));
}

TEST_F(DBusSchedQOSStateHandlerTest,
       SetProcessPriorityBeforeResourcedAvailable) {
  process_.InitializePriority();
  ASSERT_TRUE(process_.SetPriority(base::Process::Priority::kUserBlocking));
  base::Process dummy_process1 = base::Process::Open(1);
  dummy_process1.InitializePriority();
  ASSERT_TRUE(dummy_process1.SetPriority(base::Process::Priority::kBestEffort));
  // A process with InitializePriority() without SetPriority().
  base::Process dummy_process2 = base::Process::Open(2);
  dummy_process2.InitializePriority();
  task_environment_.RunUntilIdle();

  ASSERT_EQ(resourced_client_->GetProcessStateHistory().size(), 0ul);

  ASSERT_TRUE(resourced_client_->TriggerServiceAvailable(true));
  task_environment_.RunUntilIdle();

  EXPECT_THAT(
      resourced_client_->GetProcessStateHistory(),
      UnorderedElementsAre(
          Pair(process_.Pid(), resource_manager::ProcessState::kNormal),
          Pair(dummy_process1.Pid(),
               resource_manager::ProcessState::kBackground),
          Pair(dummy_process2.Pid(), resource_manager::ProcessState::kNormal)));

  ASSERT_TRUE(process_.SetPriority(base::Process::Priority::kBestEffort));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(resourced_client_->GetProcessStateHistory().size(), 4ul);
  EXPECT_THAT(
      resourced_client_->GetProcessStateHistory()[3],
      Pair(process_.Pid(), resource_manager::ProcessState::kBackground));
}

TEST_F(DBusSchedQOSStateHandlerTest, SetProcessPriorityBeforeInitialize) {
  ASSERT_TRUE(resourced_client_->TriggerServiceAvailable(true));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(resourced_client_->GetProcessStateHistory().size(), 0ul);

  EXPECT_FALSE(process_.SetPriority(base::Process::Priority::kUserBlocking));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(resourced_client_->GetProcessStateHistory().size(), 0ul);

  process_.InitializePriority();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(resourced_client_->GetProcessStateHistory().size(), 1ul);

  ASSERT_TRUE(process_.SetPriority(base::Process::Priority::kBestEffort));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(resourced_client_->GetProcessStateHistory().size(), 2ul);

  EXPECT_THAT(
      resourced_client_->GetProcessStateHistory()[1],
      Pair(process_.Pid(), resource_manager::ProcessState::kBackground));
}

TEST_F(DBusSchedQOSStateHandlerTest, SetProcessPriorityAfterForgetPriority) {
  ASSERT_TRUE(resourced_client_->TriggerServiceAvailable(true));
  task_environment_.RunUntilIdle();
  process_.InitializePriority();
  task_environment_.RunUntilIdle();

  ASSERT_EQ(resourced_client_->GetProcessStateHistory().size(), 1ul);

  ASSERT_TRUE(process_.SetPriority(base::Process::Priority::kUserBlocking));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(resourced_client_->GetProcessStateHistory().size(), 2ul);

  process_.ForgetPriority();

  EXPECT_FALSE(process_.SetPriority(base::Process::Priority::kBestEffort));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(resourced_client_->GetProcessStateHistory().size(), 2ul);
}

TEST_F(DBusSchedQOSStateHandlerTest, SetProcessPriorityRetryOnDisconnect) {
  process_.InitializePriority();
  base::Process dummy_process1 = base::Process::Open(1);
  dummy_process1.InitializePriority();
  base::Process dummy_process2 = base::Process::Open(2);
  dummy_process2.InitializePriority();
  ASSERT_TRUE(resourced_client_->TriggerServiceAvailable(true));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(resourced_client_->GetProcessStateHistory().size(), 3ul);

  resourced_client_->SetProcessStateResult(
      dbus::DBusResult::kErrorServiceUnknown);

  ASSERT_TRUE(process_.SetPriority(base::Process::Priority::kBestEffort));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(resourced_client_->GetProcessStateHistory().size(), 4ul);
  EXPECT_THAT(
      resourced_client_->GetProcessStateHistory()[3],
      Pair(process_.Pid(), resource_manager::ProcessState::kBackground));
  EXPECT_EQ(process_.GetPriority(), base::Process::Priority::kBestEffort);

  EXPECT_TRUE(dummy_process1.SetPriority(base::Process::Priority::kBestEffort));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(dummy_process1.GetPriority(), base::Process::Priority::kBestEffort);

  EXPECT_TRUE(
      dummy_process1.SetPriority(base::Process::Priority::kUserBlocking));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(dummy_process1.GetPriority(),
            base::Process::Priority::kUserBlocking);

  // DBus request is not sent until it reconnects to resourced.
  EXPECT_EQ(resourced_client_->GetProcessStateHistory().size(), 4ul);

  resourced_client_->SetProcessStateResult(dbus::DBusResult::kSuccess);

  // When resourced is reconnected, retry the request.
  EXPECT_TRUE(resourced_client_->TriggerServiceAvailable(true));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(resourced_client_->GetProcessStateHistory().size(), 6ul);
  EXPECT_THAT(
      absl::MakeSpan(resourced_client_->GetProcessStateHistory()).last(2),
      UnorderedElementsAre(
          Pair(process_.Pid(), resource_manager::ProcessState::kBackground),
          Pair(dummy_process1.Pid(), resource_manager::ProcessState::kNormal)));
}

TEST_F(DBusSchedQOSStateHandlerTest, GetProcessPriority) {
  // Without initializing the priority, the default priority is returned.
  EXPECT_EQ(process_.GetPriority(), base::Process::Priority::kUserBlocking);

  process_.InitializePriority();
  // Default priority is base::Process::Priority::kUserBlocking.
  EXPECT_EQ(process_.GetPriority(), base::Process::Priority::kUserBlocking);

  ASSERT_TRUE(process_.SetPriority(base::Process::Priority::kBestEffort));
  // Even before the D-Bus request is sent, the cached priority is updated.
  EXPECT_EQ(process_.GetPriority(), base::Process::Priority::kBestEffort);
  ASSERT_EQ(resourced_client_->GetProcessStateHistory().size(), 0ul);

  ASSERT_TRUE(resourced_client_->TriggerServiceAvailable(true));
  task_environment_.RunUntilIdle();

  base::Process dummy_process = base::Process::Open(1);
  dummy_process.InitializePriority();
  ASSERT_TRUE(dummy_process.SetPriority(base::Process::Priority::kBestEffort));
  ASSERT_TRUE(process_.SetPriority(base::Process::Priority::kUserBlocking));
  // Caches priorities of multiple processes.
  EXPECT_EQ(dummy_process.GetPriority(), base::Process::Priority::kBestEffort);
  EXPECT_EQ(process_.GetPriority(), base::Process::Priority::kUserBlocking);

  ASSERT_TRUE(process_.SetPriority(base::Process::Priority::kUserVisible));
  // base::Process::Priority::kUserVisible is translated as
  // base::Process::Priority::kUserBlocking.
  EXPECT_EQ(process_.GetPriority(), base::Process::Priority::kUserBlocking);

  process_.ForgetPriority();
  dummy_process.ForgetPriority();
  // On ForgetPriority(), cached priority is cleared.
  EXPECT_EQ(process_.GetPriority(), base::Process::Priority::kUserBlocking);
  EXPECT_EQ(dummy_process.GetPriority(),
            base::Process::Priority::kUserBlocking);
}

TEST_F(DBusSchedQOSStateHandlerTest, SetThreadType) {
  ASSERT_TRUE(resourced_client_->TriggerServiceAvailable(true));
  task_environment_.RunUntilIdle();
  process_.InitializePriority();
  task_environment_.RunUntilIdle();

  ASSERT_EQ(resourced_client_->GetThreadStateHistory().size(), 1ul);

  base::PlatformThread::SetThreadType(process_.Pid(), 100,
                                      base::ThreadType::kBackground,
                                      base::IsViaIPC(false));
  base::PlatformThread::SetThreadType(
      process_.Pid(), 101, base::ThreadType::kUtility, base::IsViaIPC(false));
  base::PlatformThread::SetThreadType(process_.Pid(), 102,
                                      base::ThreadType::kResourceEfficient,
                                      base::IsViaIPC(false));
  base::PlatformThread::SetThreadType(
      process_.Pid(), 103, base::ThreadType::kDefault, base::IsViaIPC(false));
  base::PlatformThread::SetThreadType(process_.Pid(), 104,
                                      base::ThreadType::kCompositing,
                                      base::IsViaIPC(false));
  base::PlatformThread::SetThreadType(process_.Pid(), 105,
                                      base::ThreadType::kDisplayCritical,
                                      base::IsViaIPC(false));
  base::PlatformThread::SetThreadType(process_.Pid(), 106,
                                      base::ThreadType::kRealtimeAudio,
                                      base::IsViaIPC(false));
  task_environment_.RunUntilIdle();

  EXPECT_THAT(
      resourced_client_->GetThreadStateHistory(),
      ElementsAre(
          // InitializePriority() sends request for the main thread.
          FieldsAre(process_.Pid(), process_.Pid(),
                    resource_manager::ThreadState::kBalanced),
          FieldsAre(process_.Pid(), 100,
                    resource_manager::ThreadState::kBackground),
          FieldsAre(process_.Pid(), 101,
                    resource_manager::ThreadState::kUtility),
          FieldsAre(process_.Pid(), 102, resource_manager::ThreadState::kEco),
          FieldsAre(process_.Pid(), 103,
                    resource_manager::ThreadState::kBalanced),
          FieldsAre(process_.Pid(), 104,
                    resource_manager::ThreadState::kUrgent),
          FieldsAre(process_.Pid(), 105,
                    resource_manager::ThreadState::kUrgent),
          FieldsAre(process_.Pid(), 106,
                    resource_manager::ThreadState::kUrgentBursty)));
}

TEST_F(DBusSchedQOSStateHandlerTest, SetThreadTypeBeforeResourcedAvailable) {
  process_.InitializePriority();
  ASSERT_TRUE(process_.SetPriority(base::Process::Priority::kBestEffort));
  base::PlatformThread::SetThreadType(process_.Pid(), 100,
                                      base::ThreadType::kBackground,
                                      base::IsViaIPC(false));
  base::PlatformThread::SetThreadType(process_.Pid(), 101,
                                      base::ThreadType::kResourceEfficient,
                                      base::IsViaIPC(false));
  base::PlatformThread::SetThreadType(
      process_.Pid(), 101, base::ThreadType::kUtility, base::IsViaIPC(false));
  base::Process dummy_process1 = base::Process::Open(1);
  dummy_process1.InitializePriority();
  base::PlatformThread::SetThreadType(dummy_process1.Pid(), 102,
                                      base::ThreadType::kResourceEfficient,
                                      base::IsViaIPC(false));
  base::PlatformThread::SetThreadType(dummy_process1.Pid(), 103,
                                      base::ThreadType::kDefault,
                                      base::IsViaIPC(false));
  base::Process dummy_process2 = base::Process::Open(2);
  dummy_process2.InitializePriority();
  task_environment_.RunUntilIdle();

  ASSERT_EQ(resourced_client_->GetProcessStateHistory().size(), 0ul);
  ASSERT_EQ(resourced_client_->GetThreadStateHistory().size(), 0ul);

  ASSERT_TRUE(resourced_client_->TriggerServiceAvailable(true));
  task_environment_.RunUntilIdle();

  EXPECT_THAT(
      resourced_client_->GetProcessStateHistory(),
      UnorderedElementsAre(
          Pair(process_.Pid(), resource_manager::ProcessState::kBackground),
          Pair(dummy_process1.Pid(), resource_manager::ProcessState::kNormal),
          Pair(dummy_process2.Pid(), resource_manager::ProcessState::kNormal)));
  EXPECT_THAT(resourced_client_->GetThreadStateHistory(),
              UnorderedElementsAre(
                  FieldsAre(process_.Pid(), process_.Pid(),
                            resource_manager::ThreadState::kBalanced),
                  FieldsAre(process_.Pid(), 100,
                            resource_manager::ThreadState::kBackground),
                  FieldsAre(process_.Pid(), 101,
                            resource_manager::ThreadState::kUtility),
                  FieldsAre(dummy_process1.Pid(), dummy_process1.Pid(),
                            resource_manager::ThreadState::kBalanced),
                  FieldsAre(dummy_process1.Pid(), 102,
                            resource_manager::ThreadState::kEco),
                  FieldsAre(dummy_process1.Pid(), 103,
                            resource_manager::ThreadState::kBalanced),
                  FieldsAre(dummy_process2.Pid(), dummy_process2.Pid(),
                            resource_manager::ThreadState::kBalanced)));

  base::PlatformThread::SetThreadType(process_.Pid(), 101,
                                      base::ThreadType::kResourceEfficient,
                                      base::IsViaIPC(false));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(resourced_client_->GetThreadStateHistory().size(), 8ul);
  EXPECT_THAT(
      resourced_client_->GetThreadStateHistory()[7],
      FieldsAre(process_.Pid(), 101, resource_manager::ThreadState::kEco));
}

TEST_F(DBusSchedQOSStateHandlerTest, SetThreadTypeBeforeInitialize) {
  ASSERT_TRUE(resourced_client_->TriggerServiceAvailable(true));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(resourced_client_->GetThreadStateHistory().size(), 0ul);

  base::PlatformThread::SetThreadType(process_.Pid(), 100,
                                      base::ThreadType::kBackground,
                                      base::IsViaIPC(false));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(resourced_client_->GetThreadStateHistory().size(), 0ul);

  process_.InitializePriority();
  task_environment_.RunUntilIdle();

  base::PlatformThread::SetThreadType(
      process_.Pid(), 101, base::ThreadType::kUtility, base::IsViaIPC(false));
  task_environment_.RunUntilIdle();

  EXPECT_THAT(resourced_client_->GetThreadStateHistory(),
              ElementsAre(FieldsAre(process_.Pid(), process_.Pid(),
                                    resource_manager::ThreadState::kBalanced),
                          FieldsAre(process_.Pid(), 101,
                                    resource_manager::ThreadState::kUtility)));
}

TEST_F(DBusSchedQOSStateHandlerTest, SetThreadTypeAfterForgetPriority) {
  ASSERT_TRUE(resourced_client_->TriggerServiceAvailable(true));
  task_environment_.RunUntilIdle();
  process_.InitializePriority();
  task_environment_.RunUntilIdle();

  ASSERT_EQ(resourced_client_->GetThreadStateHistory().size(), 1ul);

  base::PlatformThread::SetThreadType(process_.Pid(), 100,
                                      base::ThreadType::kBackground,
                                      base::IsViaIPC(false));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(resourced_client_->GetThreadStateHistory().size(), 2ul);

  process_.ForgetPriority();

  base::PlatformThread::SetThreadType(
      process_.Pid(), 100, base::ThreadType::kUtility, base::IsViaIPC(false));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(resourced_client_->GetThreadStateHistory().size(), 2ul);
}

TEST_F(DBusSchedQOSStateHandlerTest, SetThreadTypeRetryOnDisconnect) {
  process_.InitializePriority();
  base::Process dummy_process1 = base::Process::Open(1);
  base::PlatformThread::SetThreadType(process_.Pid(), 100,
                                      base::ThreadType::kBackground,
                                      base::IsViaIPC(false));
  dummy_process1.InitializePriority();
  base::Process dummy_process2 = base::Process::Open(2);
  dummy_process2.InitializePriority();
  base::Process dummy_process3 = base::Process::Open(3);
  dummy_process3.InitializePriority();
  ASSERT_TRUE(resourced_client_->TriggerServiceAvailable(true));
  task_environment_.RunUntilIdle();

  ASSERT_EQ(resourced_client_->GetThreadStateHistory().size(), 5ul);
  ASSERT_EQ(resourced_client_->GetProcessStateHistory().size(), 4ul);

  resourced_client_->SetThreadStateResult(
      dbus::DBusResult::kErrorServiceUnknown);

  base::PlatformThread::SetThreadType(
      process_.Pid(), 101, base::ThreadType::kUtility, base::IsViaIPC(false));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(resourced_client_->GetThreadStateHistory().size(), 6ul);
  EXPECT_THAT(
      resourced_client_->GetThreadStateHistory()[5],
      FieldsAre(process_.Pid(), 101, resource_manager::ThreadState::kUtility));

  base::PlatformThread::SetThreadType(process_.Pid(), 102,
                                      base::ThreadType::kResourceEfficient,
                                      base::IsViaIPC(false));
  base::PlatformThread::SetThreadType(dummy_process1.Pid(), 103,
                                      base::ThreadType::kUtility,
                                      base::IsViaIPC(false));
  base::PlatformThread::SetThreadType(dummy_process1.Pid(), 103,
                                      base::ThreadType::kRealtimeAudio,
                                      base::IsViaIPC(false));
  ASSERT_TRUE(process_.SetPriority(base::Process::Priority::kBestEffort));
  ASSERT_TRUE(
      dummy_process2.SetPriority(base::Process::Priority::kUserBlocking));
  task_environment_.RunUntilIdle();

  // DBus request is not sent until it reconnects to resourced.
  EXPECT_EQ(resourced_client_->GetProcessStateHistory().size(), 4ul);
  EXPECT_EQ(resourced_client_->GetThreadStateHistory().size(), 6ul);

  resourced_client_->SetProcessStateResult(dbus::DBusResult::kSuccess);
  // When resourced is reconnected, retry the request.
  EXPECT_TRUE(resourced_client_->TriggerServiceAvailable(true));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(resourced_client_->GetProcessStateHistory().size(), 6ul);
  EXPECT_EQ(resourced_client_->GetThreadStateHistory().size(), 9ul);
  EXPECT_THAT(
      absl::MakeSpan(resourced_client_->GetProcessStateHistory()).last(2),
      UnorderedElementsAre(
          Pair(process_.Pid(), resource_manager::ProcessState::kBackground),
          Pair(dummy_process2.Pid(), resource_manager::ProcessState::kNormal)));
  EXPECT_THAT(
      absl::MakeSpan(resourced_client_->GetThreadStateHistory()).last(3),
      UnorderedElementsAre(
          FieldsAre(process_.Pid(), 101,
                    resource_manager::ThreadState::kUtility),
          FieldsAre(process_.Pid(), 102, resource_manager::ThreadState::kEco),
          FieldsAre(dummy_process1.Pid(), 103,
                    resource_manager::ThreadState::kUrgentBursty)));
}

}  // namespace ash
