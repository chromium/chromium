// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_instance_throttle.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/metrics/stability_metrics_manager.h"
#include "ash/components/arc/mojom/power.mojom.h"
#include "ash/components/arc/power/arc_power_bridge.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_app_host.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/components/arc/test/fake_power_instance.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_active_audio_throttle_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_boot_phase_throttle_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_power_throttle_observer.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/throttle/throttle_observer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/arc/test/fake_intent_helper_host.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/test_screen.h"

namespace arc {

class ArcInstanceThrottleTest : public testing::Test {
 public:
  ArcInstanceThrottleTest() = default;
  ~ArcInstanceThrottleTest() override = default;

  ArcInstanceThrottleTest(const ArcInstanceThrottleTest&) = delete;
  ArcInstanceThrottleTest& operator=(const ArcInstanceThrottleTest&) = delete;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
    testing_profile_ = std::make_unique<TestingProfile>();

    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());

    ArcBootPhaseMonitorBridge::GetForBrowserContextForTesting(
        testing_profile_.get());

    ash::SessionManagerClient::InitializeFakeInMemory();
    ash::FakeSessionManagerClient::Get()->set_arc_available(true);
    StabilityMetricsManager::Initialize(&local_state_);
    prefs::RegisterLocalStatePrefs(local_state_.registry());
    prefs::RegisterProfilePrefs(local_state_.registry());
    arc_metrics_service_ = ArcMetricsService::GetForBrowserContextForTesting(
        testing_profile_.get());
    arc_metrics_service_->SetHistogramNamerCallback(base::BindLambdaForTesting(
        [](const std::string&) -> std::string { return ""; }));

    arc_instance_throttle_ =
        ArcInstanceThrottle::GetForBrowserContextForTesting(
            testing_profile_.get());
    arc_instance_throttle_->set_delegate_for_testing(
        std::make_unique<TestDelegateImpl>(this));

    app_host_ = std::make_unique<FakeAppHost>(
        arc_service_manager_->arc_bridge_service()->app());
    app_instance_ = std::make_unique<FakeAppInstance>(app_host_.get());
    intent_helper_host_ = std::make_unique<FakeIntentHelperHost>(
        arc_service_manager_->arc_bridge_service()->intent_helper());
    intent_helper_instance_ = std::make_unique<FakeIntentHelperInstance>();

    // Make sure the next SetActive() call calls into TestDelegateImpl. This
    // is necessary because ArcInstanceThrottle's constructor may initialize the
    // variable (and call the default delegate for production) before doing
    // set_delegate_for_testing(). If that happens, SetActive() might not call
    // the test delegate as expected.
    arc_instance_throttle_->reset_should_throttle_for_testing();
  }

  void TearDown() override {
    DestroyPowerInstance();
    app_host_.reset();
    app_instance_.reset();
    intent_helper_host_.reset();
    intent_helper_instance_.reset();

    arc::StabilityMetricsManager::Shutdown();
    ash::SessionManagerClient::Shutdown();
    testing_profile_.reset();
    arc_session_manager_.reset();
    arc_service_manager_.reset();
    ash::ConciergeClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  void CreatePowerInstance() {
    ArcPowerBridge* const power_bridge =
        ArcPowerBridge::GetForBrowserContextForTesting(testing_profile_.get());
    DCHECK(power_bridge);

    power_instance_ = std::make_unique<FakePowerInstance>();
    arc_bridge_service()->power()->SetInstance(power_instance_.get());
    WaitForInstanceReady(arc_bridge_service()->power());
  }

  void DestroyPowerInstance() {
    if (!power_instance_)
      return;
    arc_bridge_service()->power()->CloseInstance(power_instance_.get());
    power_instance_.reset();
  }

  void ConnectMojo() {
    arc_service_manager_->arc_bridge_service()->app()->SetInstance(
        app_instance_.get());
    WaitForInstanceReady(arc_service_manager_->arc_bridge_service()->app());
    arc_service_manager_->arc_bridge_service()->intent_helper()->SetInstance(
        intent_helper_instance_.get());
    WaitForInstanceReady(
        arc_service_manager_->arc_bridge_service()->intent_helper());
  }

  ArcMetricsService* GetArcMetricsService() { return arc_metrics_service_; }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

  ArcBridgeService* arc_bridge_service() {
    return arc_service_manager_->arc_bridge_service();
  }

  ArcInstanceThrottle* arc_instance_throttle() {
    return arc_instance_throttle_;
  }

  // Returns an observer that will be classified as |kOther| in
  // GetUnthrottlingReason().
  ash::ThrottleObserver* GetThrottleObserver() {
    const auto& observers = arc_instance_throttle()->observers_for_testing();
    DCHECK(!observers.empty());
    for (const auto& observer : observers) {
      // This must be in sync with GetUnthrottlingReason().
      if (observer->name() == kArcBootPhaseThrottleObserverName)
        continue;
      else if (observer->name() == kArcPowerThrottleObserverName)
        continue;
      else
        return observer.get();
    }
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  ash::ThrottleObserver* GetArcBootPhaseThrottleObserver() {
    const auto& observers = arc_instance_throttle()->observers_for_testing();
    DCHECK(!observers.empty());
    for (const auto& observer : observers) {
      if (observer->name() == kArcBootPhaseThrottleObserverName)
        return observer.get();
    }
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  ash::ThrottleObserver* GetArcPowerThrottleObserver() {
    const auto& observers = arc_instance_throttle()->observers_for_testing();
    DCHECK(!observers.empty());
    for (const auto& observer : observers) {
      if (observer->name() == kArcPowerThrottleObserverName)
        return observer.get();
    }
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  ArcInstanceThrottle* CreateArcInstanceThrottle() {
    return ArcInstanceThrottle::GetForBrowserContextForTesting(
        testing_profile_.get());
  }

  FakePowerInstance* power_instance() { return power_instance_.get(); }

  size_t disable_cpu_restriction_counter() const {
    return disable_cpu_restriction_counter_;
  }

  size_t enable_cpu_restriction_counter() const {
    return enable_cpu_restriction_counter_;
  }

  size_t use_quota_counter() const { return use_quota_counter_; }

 private:
  class TestDelegateImpl : public ArcInstanceThrottle::Delegate {
   public:
    explicit TestDelegateImpl(ArcInstanceThrottleTest* test) : test_(test) {}
    ~TestDelegateImpl() override = default;

    TestDelegateImpl(const TestDelegateImpl&) = delete;
    TestDelegateImpl& operator=(const TestDelegateImpl&) = delete;

    void SetCpuRestriction(CpuRestrictionState cpu_restriction_state,
                           bool use_quota) override {
      switch (cpu_restriction_state) {
        case CpuRestrictionState::CPU_RESTRICTION_FOREGROUND:
          ++(test_->disable_cpu_restriction_counter_);
          break;
        case CpuRestrictionState::CPU_RESTRICTION_BACKGROUND:
          ++(test_->enable_cpu_restriction_counter_);
          if (use_quota)
            ++(test_->use_quota_counter_);
          break;
      }
    }

    void RecordCpuRestrictionDisabledUMA(const std::string& observer_name,
                                         base::TimeDelta delta) override {}

    raw_ptr<ArcInstanceThrottleTest> test_;
  };

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  display::test::TestScreen test_screen_{/*create_display=*/true,
                                         /*register_screen=*/true};
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<TestingProfile> testing_profile_;

  std::unique_ptr<FakePowerInstance> power_instance_;
  std::unique_ptr<FakeAppHost> app_host_;
  std::unique_ptr<FakeAppInstance> app_instance_;
  std::unique_ptr<FakeIntentHelperHost> intent_helper_host_;
  std::unique_ptr<FakeIntentHelperInstance> intent_helper_instance_;

  raw_ptr<ArcInstanceThrottle, DanglingUntriaged> arc_instance_throttle_;
  raw_ptr<ArcMetricsService, DanglingUntriaged> arc_metrics_service_ = nullptr;
  size_t disable_cpu_restriction_counter_ = 0;
  size_t enable_cpu_restriction_counter_ = 0;
  size_t use_quota_counter_ = 0;
};

// Tests that ArcInstanceThrottle can be constructed and destructed.

TEST_F(ArcInstanceThrottleTest, TestConstructDestruct) {}

// Tests that ArcInstanceThrottle adjusts ARC CPU restriction
// when ThrottleInstance is called.
TEST_F(ArcInstanceThrottleTest, TestThrottleInstance) {
  GetThrottleObserver()->SetActive(false);
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
  EXPECT_EQ(0U, disable_cpu_restriction_counter());

  // ArcInstanceThrottle is already inactive, expect no change.
  GetThrottleObserver()->SetActive(false);
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
  EXPECT_EQ(0U, disable_cpu_restriction_counter());

  GetThrottleObserver()->SetActive(true);
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
  EXPECT_EQ(1U, disable_cpu_restriction_counter());

  GetThrottleObserver()->SetActive(false);
  EXPECT_EQ(2U, enable_cpu_restriction_counter());
  EXPECT_EQ(1U, disable_cpu_restriction_counter());
}

// Tests that ArcInstanceThrottle enforces quota only until unthrottled via a
// user action.
TEST_F(ArcInstanceThrottleTest, TestThrottleInstanceQuotaEnforcement) {
  // While ARC is booting, quota shouldn't be enforced.
  GetArcBootPhaseThrottleObserver()->SetActive(false);
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
  EXPECT_EQ(0U, use_quota_counter());
  EXPECT_EQ(0U, disable_cpu_restriction_counter());

  // Note: Use ArcBootPhaseThrottleObserver so that quota will still be
  // applicable.
  GetArcBootPhaseThrottleObserver()->SetActive(true);
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
  EXPECT_EQ(0U, use_quota_counter());
  EXPECT_EQ(1U, disable_cpu_restriction_counter());

  // ARC booted, and mojom is connected.
  ConnectMojo();

  // Since 10 seconds haven't passed since the mojom connection, quota is
  // still not enforced.
  GetArcBootPhaseThrottleObserver()->SetActive(false);
  EXPECT_EQ(2U, enable_cpu_restriction_counter());
  EXPECT_EQ(0U, use_quota_counter());
  EXPECT_EQ(1U, disable_cpu_restriction_counter());

  GetArcBootPhaseThrottleObserver()->SetActive(true);
  EXPECT_EQ(2U, enable_cpu_restriction_counter());
  EXPECT_EQ(0U, use_quota_counter());
  EXPECT_EQ(2U, disable_cpu_restriction_counter());

  // 10 seconds passed.
  task_environment()->FastForwardBy(
      ArcBootPhaseThrottleObserver::GetThrottleDelayForTesting());

  // Now quota is enforced,
  GetArcBootPhaseThrottleObserver()->SetActive(false);
  EXPECT_EQ(3U, enable_cpu_restriction_counter());
  EXPECT_EQ(1U, use_quota_counter());
  EXPECT_EQ(2U, disable_cpu_restriction_counter());

  // Unthrottle the instance because of pre-ANR.
  GetArcPowerThrottleObserver()->SetActive(true);
  EXPECT_EQ(3U, enable_cpu_restriction_counter());
  EXPECT_EQ(1U, use_quota_counter());
  EXPECT_EQ(3U, disable_cpu_restriction_counter());

  // Quota is enforced again once pre-ANR is handled.
  GetArcPowerThrottleObserver()->SetActive(false);
  EXPECT_EQ(4U, enable_cpu_restriction_counter());
  EXPECT_EQ(2U, use_quota_counter());
  EXPECT_EQ(3U, disable_cpu_restriction_counter());

  // Quota is not enforced once unthrottled via a user action.
  GetThrottleObserver()->SetActive(true);
  EXPECT_EQ(4U, enable_cpu_restriction_counter());
  EXPECT_EQ(2U, use_quota_counter());
  EXPECT_EQ(4U, disable_cpu_restriction_counter());

  GetThrottleObserver()->SetActive(false);
  EXPECT_EQ(5U, enable_cpu_restriction_counter());
  EXPECT_EQ(2U, use_quota_counter());
  EXPECT_EQ(4U, disable_cpu_restriction_counter());
}

// Tests that ArcInstanceThrottle is not enforced when the boot type is
// 'regular'.
TEST_F(ArcInstanceThrottleTest,
       TestThrottleInstanceNoQuotaEnforcementOnRegularBoot) {
  GetArcBootPhaseThrottleObserver()->SetActive(true);
  EXPECT_EQ(0U, enable_cpu_restriction_counter());
  EXPECT_EQ(0U, use_quota_counter());
  EXPECT_EQ(1U, disable_cpu_restriction_counter());

  // Tell the throttle of the boot type.
  GetArcMetricsService()->ReportBootProgress({}, mojom::BootType::REGULAR_BOOT);

  // ARC booted, and mojom is connected.
  ConnectMojo();

  // 10 seconds passed.
  task_environment()->FastForwardBy(
      ArcBootPhaseThrottleObserver::GetThrottleDelayForTesting());

  // Now CPU restriction is enforced BUT quota is still not enforced.
  GetArcBootPhaseThrottleObserver()->SetActive(false);
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
  EXPECT_EQ(0U, use_quota_counter());
  EXPECT_EQ(1U, disable_cpu_restriction_counter());
}

// Tests the same with non-regular boot.
TEST_F(ArcInstanceThrottleTest,
       TestThrottleInstanceNoQuotaEnforcementOnNonRegularBoot) {
  GetArcBootPhaseThrottleObserver()->SetActive(true);
  EXPECT_EQ(0U, enable_cpu_restriction_counter());
  EXPECT_EQ(0U, use_quota_counter());
  EXPECT_EQ(1U, disable_cpu_restriction_counter());

  // Tell the throttle of the boot type.
  GetArcMetricsService()->ReportBootProgress(
      {}, mojom::BootType::FIRST_BOOT_AFTER_UPDATE);

  // ARC booted, and mojom is connected.
  ConnectMojo();

  // 10 seconds passed.
  task_environment()->FastForwardBy(
      ArcBootPhaseThrottleObserver::GetThrottleDelayForTesting());

  // Now quota _is_ enforced.
  GetArcBootPhaseThrottleObserver()->SetActive(false);
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
  EXPECT_EQ(1U, use_quota_counter());
  EXPECT_EQ(1U, disable_cpu_restriction_counter());
}

// Tests that power instance is correctly notified.
TEST_F(ArcInstanceThrottleTest, TestPowerNotificationEnabledByDefault) {
  // Set power instance and it should be automatically notified once connection
  // is made.
  CreatePowerInstance();
  EXPECT_EQ(1, power_instance()->cpu_restriction_state_count());
  EXPECT_EQ(mojom::CpuRestrictionState::CPU_RESTRICTION_BACKGROUND,
            power_instance()->last_cpu_restriction_state());

  GetThrottleObserver()->SetActive(true);
  EXPECT_EQ(2, power_instance()->cpu_restriction_state_count());
  EXPECT_EQ(mojom::CpuRestrictionState::CPU_RESTRICTION_FOREGROUND,
            power_instance()->last_cpu_restriction_state());

  GetThrottleObserver()->SetActive(false);
  EXPECT_EQ(3, power_instance()->cpu_restriction_state_count());
  EXPECT_EQ(mojom::CpuRestrictionState::CPU_RESTRICTION_BACKGROUND,
            power_instance()->last_cpu_restriction_state());
}

// Tests that power instance notification is off by default.
TEST_F(ArcInstanceThrottleTest, TestPowerNotification) {
  // Set power instance and it should be automatically notified once connection
  // is made.
  CreatePowerInstance();
  EXPECT_EQ(1, power_instance()->cpu_restriction_state_count());
  GetThrottleObserver()->SetActive(true);
  EXPECT_EQ(2, power_instance()->cpu_restriction_state_count());
  GetThrottleObserver()->SetActive(false);
  EXPECT_EQ(3, power_instance()->cpu_restriction_state_count());
}

MATCHER_P(IsObserverNameEquals, name, "") {
  return arg->name() == name;
}

TEST_F(ArcInstanceThrottleTest, UnthrottleOnActiveAudioV2FeatureOff) {
  base::test::ScopedFeatureList feature;
  feature.InitAndDisableFeature(arc::kUnthrottleOnActiveAudioV2);

  auto* arc_instance_throttle = CreateArcInstanceThrottle();
  EXPECT_THAT(arc_instance_throttle->observers_for_testing(),
              testing::Not(testing::Contains(
                  IsObserverNameEquals(kArcActiveAudioThrottleObserverName))));
}

TEST_F(ArcInstanceThrottleTest, UnthrottleOnActiveAudioV2FeatureOn) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(arc::kUnthrottleOnActiveAudioV2);

  auto* arc_instance_throttle = CreateArcInstanceThrottle();
  EXPECT_THAT(arc_instance_throttle->observers_for_testing(),
              testing::Contains(
                  IsObserverNameEquals(kArcActiveAudioThrottleObserverName)));
}

// For testing ARCVM specific part of the class.
class ArcInstanceThrottleVMTest : public testing::Test {
 public:
  ArcInstanceThrottleVMTest() = default;
  ~ArcInstanceThrottleVMTest() override = default;

  explicit ArcInstanceThrottleVMTest(const ArcInstanceThrottleTest&) = delete;
  ArcInstanceThrottleVMTest& operator=(const ArcInstanceThrottleTest&) = delete;

  void SetUp() override {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->InitFromArgv({"", "--enable-arcvm"});

    SetArcAvailableCommandLineForTesting(command_line);

    run_loop_ = std::make_unique<base::RunLoop>();

    ash::ConciergeClient::InitializeFake();
    DCHECK(GetConciergeClient());

    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    arc_session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
    testing_profile_ = std::make_unique<TestingProfile>();

    ash::SessionManagerClient::InitializeFakeInMemory();
    ash::FakeSessionManagerClient::Get()->set_arc_available(true);
    StabilityMetricsManager::Initialize(&local_state_);
    prefs::RegisterLocalStatePrefs(local_state_.registry());
    prefs::RegisterProfilePrefs(local_state_.registry());
    arc_metrics_service_ = ArcMetricsService::GetForBrowserContextForTesting(
        testing_profile_.get());
    arc_metrics_service_->SetHistogramNamerCallback(base::BindLambdaForTesting(
        [](const std::string&) -> std::string { return ""; }));

    arc_instance_throttle_ =
        ArcInstanceThrottle::GetForBrowserContextForTesting(
            testing_profile_.get());

    run_loop()->RunUntilIdle();
  }

  void TearDown() override {
    arc::StabilityMetricsManager::Shutdown();
    ash::SessionManagerClient::Shutdown();
    testing_profile_.reset();
    arc_session_manager_.reset();
    arc_service_manager_.reset();
  }

 protected:
  ash::FakeConciergeClient* GetConciergeClient() {
    return ash::FakeConciergeClient::Get();
  }

  ash::ThrottleObserver* GetThrottleObserver() {
    for (const auto& observer :
         arc_instance_throttle_->observers_for_testing()) {
      if (observer->name() == kArcPowerThrottleObserverName)
        return observer.get();
    }
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  base::RunLoop* run_loop() { return run_loop_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<base::RunLoop> run_loop_;

  display::test::TestScreen test_screen_{/*create_display=*/true,
                                         /*register_screen=*/true};
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<TestingProfile> testing_profile_;

  raw_ptr<ArcInstanceThrottle, DanglingUntriaged> arc_instance_throttle_;
  raw_ptr<ArcMetricsService, DanglingUntriaged> arc_metrics_service_ = nullptr;
};

TEST_F(ArcInstanceThrottleVMTest, Histograms) {
  constexpr char kHistogramName[] = "Arc.CpuRestrictionVmResult";
  base::HistogramTester histogram_tester;

  auto* const client = GetConciergeClient();
  auto* const observer = GetThrottleObserver();

  // No service
  client->set_wait_for_service_to_be_available_response(false);
  observer->SetActive(true);
  run_loop()->RunUntilIdle();
  histogram_tester.ExpectTotalCount(kHistogramName, 1);
  histogram_tester.ExpectBucketCount(kHistogramName,
                                     2 /* kNoConciergeService */, 1);

  // No response
  client->set_wait_for_service_to_be_available_response(true);
  std::optional<vm_tools::concierge::SetVmCpuRestrictionResponse> response;
  client->set_set_vm_cpu_restriction_response(response);
  observer->SetActive(false);
  run_loop()->RunUntilIdle();
  histogram_tester.ExpectTotalCount(kHistogramName, 2);
  histogram_tester.ExpectBucketCount(kHistogramName,
                                     4 /* kConciergeDidNotRespond */, 1);

  // Failure
  response = vm_tools::concierge::SetVmCpuRestrictionResponse();
  response->set_success(false);
  client->set_set_vm_cpu_restriction_response(response);
  observer->SetActive(true);
  run_loop()->RunUntilIdle();
  histogram_tester.ExpectTotalCount(kHistogramName, 3);
  histogram_tester.ExpectBucketCount(kHistogramName, 1 /* kOther */, 1);

  // Success
  response = vm_tools::concierge::SetVmCpuRestrictionResponse();
  response->set_success(true);
  client->set_set_vm_cpu_restriction_response(response);
  observer->SetActive(false);
  run_loop()->RunUntilIdle();
  histogram_tester.ExpectTotalCount(kHistogramName, 4);
  histogram_tester.ExpectBucketCount(kHistogramName, 0 /* kSuccess */, 1);
}

}  // namespace arc
