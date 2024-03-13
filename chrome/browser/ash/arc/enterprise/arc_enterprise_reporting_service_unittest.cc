// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/enterprise/arc_enterprise_reporting_service.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

constexpr char kTestProfileName[] = "user@gmail.com";
constexpr char kTestGaiaId[] = "1234567890";
constexpr int64_t kTestTimeMs = 100;

class ArcEnterpriseReportingServiceTest : public testing::Test {
 protected:
  ArcEnterpriseReportingServiceTest() = default;
  ArcEnterpriseReportingServiceTest(const ArcEnterpriseReportingServiceTest&) =
      delete;
  ArcEnterpriseReportingServiceTest& operator=(
      const ArcEnterpriseReportingServiceTest&) = delete;
  ~ArcEnterpriseReportingServiceTest() override = default;

  void SetUp() override {
    // Set up user manager and profile and for ReportCloudDpcOperationTime tests
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    profile_ = profile_manager_->CreateTestingProfile(kTestProfileName);

    const auto account_id = AccountId::FromUserEmailGaiaId(
        profile_->GetProfileUserName(), kTestGaiaId);
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);

    // Set up ArcSessionManager for ReportManagementState tests
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    ArcSessionManager::SetUiEnabledForTesting(false);
    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(arc::FakeArcSession::Create)));
    arc_session_manager_->SetProfile(profile_);

    service_ =
        ArcEnterpriseReportingService::GetForBrowserContextForTesting(profile_);
  }

  void TearDown() override {
    arc_session_manager_->Shutdown();
    profile_manager_->DeleteTestingProfile(kTestProfileName);
    profile_ = nullptr;
    profile_manager_.reset();
    fake_user_manager_.Reset();
    arc_session_manager_.reset();
    arc_service_manager_.reset();
  }

  ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }

  TestingProfile* profile() { return profile_; }

  ArcEnterpriseReportingService* service() { return service_; }

 private:
  raw_ptr<ArcEnterpriseReportingService, DanglingUntriaged> service_ = nullptr;
  content::BrowserTaskEnvironment task_environment_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  session_manager::SessionManager session_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
};

TEST_F(ArcEnterpriseReportingServiceTest, ReportCloudDpcOperationTime_Success) {
  base::HistogramTester tester;
  service()->ReportCloudDpcOperationTime(
      kTestTimeMs, mojom::TimedCloudDpcOp::DEVICE_SETUP, true);

  tester.ExpectUniqueTimeSample(
      "Arc.CloudDpc.DeviceSetup.TimeDelta.Success.Unmanaged",
      base::Milliseconds(kTestTimeMs), 1);
}

TEST_F(ArcEnterpriseReportingServiceTest, ReportCloudDpcOperationTime_Failure) {
  base::HistogramTester tester;
  service()->ReportCloudDpcOperationTime(
      kTestTimeMs, mojom::TimedCloudDpcOp::DEVICE_SETUP, false);

  tester.ExpectUniqueTimeSample(
      "Arc.CloudDpc.DeviceSetup.TimeDelta.Failure.Unmanaged",
      base::Milliseconds(kTestTimeMs), 1);
}

TEST_F(ArcEnterpriseReportingServiceTest,
       ReportCloudDpcOperationTime_UnknownOp) {
  base::HistogramTester tester;
  service()->ReportCloudDpcOperationTime(
      kTestTimeMs, mojom::TimedCloudDpcOp::UNKNOWN_OP, true);

  tester.ExpectUniqueTimeSample("Arc.CloudDpc..TimeDelta.Success.Unmanaged",
                                base::Milliseconds(kTestTimeMs), 0);
}

}  // namespace
}  // namespace arc
