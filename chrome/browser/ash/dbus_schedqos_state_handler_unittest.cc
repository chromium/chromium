// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus_schedqos_state_handler.h"

#include <memory>

#include "base/process/process.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/resourced/fake_resourced_client.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/resource_manager/dbus-constants.h"

using ::testing::ElementsAre;
using ::testing::Pair;

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

TEST_F(DBusSchedQOSStateHandlerTest, SetProcessPriority) {
  ASSERT_EQ(resourced_client_->GetProcessStateHistory().size(), 0ul);

  base::Process dummy_process = base::Process::Open(1);
  process_.SetPriority(base::Process::Priority::kUserBlocking);
  dummy_process.SetPriority(base::Process::Priority::kBestEffort);
  dummy_process.SetPriority(base::Process::Priority::kUserVisible);
  task_environment_.RunUntilIdle();

  EXPECT_THAT(
      resourced_client_->GetProcessStateHistory(),
      ElementsAre(
          Pair(process_.Pid(), resource_manager::ProcessState::kNormal),
          Pair(dummy_process.Pid(),
               resource_manager::ProcessState::kBackground),
          Pair(dummy_process.Pid(), resource_manager::ProcessState::kNormal)));
}

}  // namespace ash
