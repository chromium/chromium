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
#include "ash/components/arc/test/test_browser_context.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
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
    context_ = std::make_unique<TestBrowserContext>();
    instance_ = std::make_unique<FakeAdbdMonitorInstance>();
    bridge_ =
        ArcAdbdMonitorBridge::GetForBrowserContextForTesting(context_.get());
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
    context_.reset();
    arc_service_manager_.reset();
  }

 protected:
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  ArcAdbdMonitorBridge* arc_adbd_monitor_bridge() const { return bridge_; }

  const std::vector<std::pair<std::string, bool>>& upstart_operations() const {
    return upstart_operations_;
  }

  void InjectUpstartStopJobFailure(const std::string& job_name_to_fail) {
    auto* upstart_client = ash::FakeUpstartClient::Get();
    upstart_client->set_stop_job_cb(base::BindLambdaForTesting(
        [job_name_to_fail](const std::string& job_name,
                           const std::vector<std::string>& env) {
          // Return success unless |job_name| is |job_name_to_fail|.
          return job_name != job_name_to_fail;
        }));
  }

  void StartRecordingUpstartOperations() {
    auto* upstart_client = ash::FakeUpstartClient::Get();
    upstart_client->set_start_job_cb(
        base::BindLambdaForTesting([this](const std::string& job_name,
                                          const std::vector<std::string>& env) {
          upstart_operations_.emplace_back(job_name, true);
          return true;
        }));
    upstart_client->set_stop_job_cb(
        base::BindLambdaForTesting([this](const std::string& job_name,
                                          const std::vector<std::string>& env) {
          upstart_operations_.emplace_back(job_name, false);
          return true;
        }));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<FakeAdbdMonitorInstance> instance_;
  std::unique_ptr<TestBrowserContext> context_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  ArcAdbdMonitorBridge* bridge_;

  // List of upstart operations recorded. When it's "start" the boolean is set
  // to true.
  std::vector<std::pair<std::string, bool>> upstart_operations_;
};

// Testing bridge constructor/destructor in setup/teardown.
TEST_F(ArcAdbdMonitorBridgeTest, TestConstructDestruct) {}

// Testing bridge start arcvm-adbd successfully
TEST_F(ArcAdbdMonitorBridgeTest, TestStartArcVmAdbdSuccess) {
  StartRecordingUpstartOperations();
  arc_adbd_monitor_bridge()->EnableAdbOverUsbForTesting();
  base::RunLoop run_loop;
  arc_adbd_monitor_bridge()->OnStartArcVmAdbdTesting(base::BindOnce(
      [](base::RunLoop* loop, bool result) {
        EXPECT_EQ(result, true);
        loop->Quit();
      },
      &run_loop));
  run_loop.Run();

  const auto& ops = upstart_operations();
  // Find the STOP operation for the job.
  auto it = base::ranges::find(
      ops, std::make_pair(std::string(kArcVmAdbdJobName), false));
  ASSERT_NE(ops.end(), it);
  ++it;
  ASSERT_NE(ops.end(), it);
  // The next operation must be START for the job.
  EXPECT_EQ(it->first, kArcVmAdbdJobName);
  EXPECT_TRUE(it->second);  // true means START.
}

// Testing bridge start arcvm-adbd regardless stop failure.
TEST_F(ArcAdbdMonitorBridgeTest, TestStartArcVmAdbdFailure) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStopJobFailure(kArcVmAdbdJobName);
  StartRecordingUpstartOperations();
  arc_adbd_monitor_bridge()->EnableAdbOverUsbForTesting();
  base::RunLoop run_loop;
  arc_adbd_monitor_bridge()->OnStartArcVmAdbdTesting(base::BindOnce(
      [](base::RunLoop* loop, bool result) {
        EXPECT_EQ(result, true);
        loop->Quit();
      },
      &run_loop));
  run_loop.Run();

  const auto& ops = upstart_operations();
  // Find the STOP operation for the job.
  auto it = base::ranges::find(
      ops, std::make_pair(std::string(kArcVmAdbdJobName), false));
  EXPECT_EQ(ops.size(), 2u);
  ASSERT_NE(ops.end(), it);
  ++it;
  ASSERT_NE(ops.end(), it);
  // The next operation must be START for the job.
  EXPECT_EQ(it->first, kArcVmAdbdJobName);
  EXPECT_TRUE(it->second);  // true means START.
}

// Testing bridge handle stop arcvm-adbd job failure well
TEST_F(ArcAdbdMonitorBridgeTest, TestStopArcVmAdbdSuccess) {
  StartRecordingUpstartOperations();
  arc_adbd_monitor_bridge()->EnableAdbOverUsbForTesting();
  base::RunLoop run_loop;
  arc_adbd_monitor_bridge()->OnStopArcVmAdbdTesting(base::BindOnce(
      [](base::RunLoop* loop, bool result) {
        EXPECT_EQ(result, true);
        loop->Quit();
      },
      &run_loop));
  run_loop.Run();

  const auto& ops = upstart_operations();
  // Find the STOP operation for the job.
  auto it = base::ranges::find(
      ops, std::make_pair(std::string(kArcVmAdbdJobName), false));
  EXPECT_EQ(ops.size(), 1u);
  // The next operation must be START for the job.
  EXPECT_EQ(it->first, kArcVmAdbdJobName);
}

}  // namespace

}  // namespace arc
