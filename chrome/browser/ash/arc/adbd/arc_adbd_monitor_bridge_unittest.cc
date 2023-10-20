// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/adbd/arc_adbd_monitor_bridge.h"

#include <memory>

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_adbd_monitor_instance.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

constexpr const char kArcVmAdbdJobName[] = "arcvm_2dadbd";

class ArcAdbdMonitorBridgeTest : public testing::Test {
 public:
  ArcAdbdMonitorBridgeTest() = default;
  ~ArcAdbdMonitorBridgeTest() override = default;

  ArcAdbdMonitorBridgeTest(const ArcAdbdMonitorBridgeTest& other) = delete;
  ArcAdbdMonitorBridgeTest& operator=(const ArcAdbdMonitorBridgeTest& other) =
      delete;

  void SetUp() override {
    ash::UpstartClient::InitializeFake();
    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    profile_ = std::make_unique<TestingProfile>();
    instance_ = std::make_unique<FakeAdbdMonitorInstance>();
    bridge_ =
        ArcAdbdMonitorBridge::GetForBrowserContextForTesting(profile_.get());
    ArcServiceManager::Get()->arc_bridge_service()->adbd_monitor()->SetInstance(
        instance_.get());
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->adbd_monitor());
  }

  void TearDown() override {
    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->adbd_monitor()
        ->CloseInstance(instance_.get());
    instance_.reset();
    profile_.reset();
    arc_service_manager_.reset();
    ash::UpstartClient::Shutdown();
  }

 protected:
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  ArcAdbdMonitorBridge* arc_adbd_monitor_bridge() const { return bridge_; }

  void InjectUpstartStopJobFailure(const std::string& job_name_to_fail) {
    auto* upstart_client = ash::FakeUpstartClient::Get();
    upstart_client->set_stop_job_cb(base::BindLambdaForTesting(
        [job_name_to_fail](const std::string& job_name,
                           const std::vector<std::string>& env) {
          // Return success unless |job_name| is |job_name_to_fail|.
          return job_name != job_name_to_fail;
        }));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<FakeAdbdMonitorInstance> instance_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  raw_ptr<ArcAdbdMonitorBridge, DanglingUntriaged | ExperimentalAsh> bridge_;
};

// Testing bridge constructor/destructor in setup/teardown.
TEST_F(ArcAdbdMonitorBridgeTest, TestConstructDestruct) {}

// Testing bridge start arcvm-adbd successfully
TEST_F(ArcAdbdMonitorBridgeTest, TestStartArcVmAdbdSuccess) {
  ash::FakeUpstartClient::Get()->StartRecordingUpstartOperations();
  arc_adbd_monitor_bridge()->EnableAdbOverUsbForTesting();
  base::RunLoop run_loop;
  arc_adbd_monitor_bridge()->OnStartArcVmAdbdTesting(base::BindOnce(
      [](base::RunLoop* loop, bool result) {
        EXPECT_EQ(result, true);
        loop->Quit();
      },
      &run_loop));
  run_loop.Run();

  const auto& ops =
      ash::FakeUpstartClient::Get()->GetRecordedUpstartOperationsForJob(
          kArcVmAdbdJobName);
  ASSERT_EQ(ops.size(), 2u);
  EXPECT_EQ(ops[0].type, ash::FakeUpstartClient::UpstartOperationType::STOP);
  EXPECT_EQ(ops[1].type, ash::FakeUpstartClient::UpstartOperationType::START);
}

// Testing bridge start arcvm-adbd regardless stop failure.
TEST_F(ArcAdbdMonitorBridgeTest, TestStartArcVmAdbdFailure) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStopJobFailure(kArcVmAdbdJobName);
  ash::FakeUpstartClient::Get()->StartRecordingUpstartOperations();
  arc_adbd_monitor_bridge()->EnableAdbOverUsbForTesting();
  base::RunLoop run_loop;
  arc_adbd_monitor_bridge()->OnStartArcVmAdbdTesting(base::BindOnce(
      [](base::RunLoop* loop, bool result) {
        EXPECT_EQ(result, true);
        loop->Quit();
      },
      &run_loop));
  run_loop.Run();

  const auto& ops =
      ash::FakeUpstartClient::Get()->GetRecordedUpstartOperationsForJob(
          kArcVmAdbdJobName);
  ASSERT_EQ(ops.size(), 2u);
  EXPECT_EQ(ops[0].type, ash::FakeUpstartClient::UpstartOperationType::STOP);
  EXPECT_EQ(ops[1].type, ash::FakeUpstartClient::UpstartOperationType::START);
}

// Testing bridge handle stop arcvm-adbd job failure well
TEST_F(ArcAdbdMonitorBridgeTest, TestStopArcVmAdbdSuccess) {
  ash::FakeUpstartClient::Get()->StartRecordingUpstartOperations();
  arc_adbd_monitor_bridge()->EnableAdbOverUsbForTesting();
  base::RunLoop run_loop;
  arc_adbd_monitor_bridge()->OnStopArcVmAdbdTesting(base::BindOnce(
      [](base::RunLoop* loop, bool result) {
        EXPECT_EQ(result, true);
        loop->Quit();
      },
      &run_loop));
  run_loop.Run();

  const auto& ops =
      ash::FakeUpstartClient::Get()->GetRecordedUpstartOperationsForJob(
          kArcVmAdbdJobName);
  ASSERT_EQ(ops.size(), 1u);
  EXPECT_EQ(ops[0].type, ash::FakeUpstartClient::UpstartOperationType::STOP);
}

}  // namespace

}  // namespace arc
