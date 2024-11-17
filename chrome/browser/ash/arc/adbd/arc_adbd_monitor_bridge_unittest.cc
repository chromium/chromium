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
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/upstart/fake_upstart_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

constexpr char kProfileName[] = "user@gmail.com";

constexpr const char kArcVmAdbdJobName[] = "arcvm_2dadbd";

const int64_t kArcVmCidForTesting = 32;

class ArcAdbdMonitorBridgeTest : public testing::Test {
 public:
  ArcAdbdMonitorBridgeTest() = default;
  ~ArcAdbdMonitorBridgeTest() override = default;

  ArcAdbdMonitorBridgeTest(const ArcAdbdMonitorBridgeTest& other) = delete;
  ArcAdbdMonitorBridgeTest& operator=(const ArcAdbdMonitorBridgeTest& other) =
      delete;

  void SetUp() override {
    ash::UpstartClient::InitializeFake();
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);

    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->InitFromArgv(
        {"", "--arc-availability=officially-supported", "--enable-arcvm"});

    arc_service_manager_ = std::make_unique<ArcServiceManager>();

    // Make the session manager skip creating UI.
    ArcSessionManager::SetUiEnabledForTesting(/*enabled=*/false);
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(arc::FakeArcSession::Create)));

    // Log in as a primary profile to enable ARCVM.
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    Profile* profile = profile_manager_->CreateTestingProfile(kProfileName);
    const AccountId account_id(
        AccountId::FromUserEmail(profile->GetProfileUserName()));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);

    arc_session_manager_->SetProfile(profile);
    arc_session_manager_->Initialize();
    arc_session_manager_->RequestEnable();

    instance_ = std::make_unique<FakeAdbdMonitorInstance>();
    bridge_ = std::make_unique<ArcAdbdMonitorBridge>(
        profile, arc_service_manager_->arc_bridge_service());
    ArcServiceManager::Get()->arc_bridge_service()->adbd_monitor()->SetInstance(
        instance_.get());
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->adbd_monitor());

    const guest_os::GuestId arcvm_id(guest_os::VmType::ARCVM, kArcVmName, "");
    guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile)
        ->AddGuestForTesting(
            arcvm_id,
            guest_os::GuestInfo{arcvm_id, kArcVmCidForTesting, {}, {}, {}, {}});
  }

  void TearDown() override {
    arc_session_manager_->Shutdown();
    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->adbd_monitor()
        ->CloseInstance(instance_.get());
    bridge_.reset();
    instance_.reset();
    profile_manager_->DeleteTestingProfile(kProfileName);
    arc_session_manager_.reset();
    arc_service_manager_.reset();
    ash::ConciergeClient::Shutdown();
    ash::UpstartClient::Shutdown();
  }

 protected:
  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  ArcAdbdMonitorBridge* arc_adbd_monitor_bridge() const {
    return bridge_.get();
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

 private:
  content::BrowserTaskEnvironment task_environment_;
  session_manager::SessionManager session_manager_;
  std::unique_ptr<FakeAdbdMonitorInstance> instance_;
  std::unique_ptr<ArcAdbdMonitorBridge> bridge_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
};

// Testing bridge constructor/destructor in setup/teardown.
TEST_F(ArcAdbdMonitorBridgeTest, TestConstructDestruct) {}

// Tests that the bridge stops and starts arcvm-adbd when adbd is started.
TEST_F(ArcAdbdMonitorBridgeTest, TestStartArcVmAdbdSuccess) {
  ash::FakeUpstartClient::Get()->StartRecordingUpstartOperations();
  arc_adbd_monitor_bridge()->EnableAdbOverUsbForTesting();
  base::test::TestFuture<bool> future;
  arc_adbd_monitor_bridge()->OnAdbdStartedForTesting(future.GetCallback());
  EXPECT_TRUE(future.Get());

  const auto& ops =
      ash::FakeUpstartClient::Get()->GetRecordedUpstartOperationsForJob(
          kArcVmAdbdJobName);
  ASSERT_EQ(ops.size(), 2u);
  EXPECT_EQ(ops[0].type, ash::FakeUpstartClient::UpstartOperationType::STOP);
  EXPECT_EQ(ops[1].type, ash::FakeUpstartClient::UpstartOperationType::START);

  // Check the environment variables when starting arcvm-adbd Upstart job.
  const auto it_cid = base::ranges::find(
      ops[1].env,
      base::StringPrintf("ARCVM_CID=%" PRId64, kArcVmCidForTesting));
  EXPECT_NE(it_cid, ops[1].env.end());
  const std::string serial_number =
      arc::ArcSessionManager::Get()->GetSerialNumber();
  ASSERT_FALSE(serial_number.empty());
  const auto it_serial =
      base::ranges::find(ops[1].env, "SERIALNUMBER=" + serial_number);
  EXPECT_NE(it_serial, ops[1].env.end());
}

// Tests that the bridge starts arcvm-adbd regardless of stop failure.
TEST_F(ArcAdbdMonitorBridgeTest, TestStartArcVmAdbdFailure) {
  // Inject failure to FakeUpstartClient.
  InjectUpstartStopJobFailure(kArcVmAdbdJobName);
  ash::FakeUpstartClient::Get()->StartRecordingUpstartOperations();
  arc_adbd_monitor_bridge()->EnableAdbOverUsbForTesting();
  base::test::TestFuture<bool> future;
  arc_adbd_monitor_bridge()->OnAdbdStartedForTesting(future.GetCallback());
  EXPECT_TRUE(future.Get());

  const auto& ops =
      ash::FakeUpstartClient::Get()->GetRecordedUpstartOperationsForJob(
          kArcVmAdbdJobName);
  ASSERT_EQ(ops.size(), 2u);
  EXPECT_EQ(ops[0].type, ash::FakeUpstartClient::UpstartOperationType::STOP);
  EXPECT_EQ(ops[1].type, ash::FakeUpstartClient::UpstartOperationType::START);
}

// Tests that bridge stops arcvm-adbd job when adbd is stopped.
TEST_F(ArcAdbdMonitorBridgeTest, TestStopArcVmAdbdSuccess) {
  ash::FakeUpstartClient::Get()->StartRecordingUpstartOperations();
  arc_adbd_monitor_bridge()->EnableAdbOverUsbForTesting();
  base::test::TestFuture<bool> future;
  arc_adbd_monitor_bridge()->OnAdbdStoppedForTesting(future.GetCallback());
  EXPECT_TRUE(future.Get());

  const auto& ops =
      ash::FakeUpstartClient::Get()->GetRecordedUpstartOperationsForJob(
          kArcVmAdbdJobName);
  ASSERT_EQ(ops.size(), 1u);
  EXPECT_EQ(ops[0].type, ash::FakeUpstartClient::UpstartOperationType::STOP);
}

}  // namespace

}  // namespace arc
