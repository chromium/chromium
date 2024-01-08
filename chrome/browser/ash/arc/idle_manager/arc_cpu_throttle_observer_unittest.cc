// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/idle_manager/arc_cpu_throttle_observer.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/metrics/stability_metrics_manager.h"
#include "ash/components/arc/power/arc_power_bridge.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/arc/idle_manager/arc_throttle_test_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_instance_throttle.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/test_screen.h"

namespace arc {

class ArcCpuThrottleObserverTest : public testing::Test {
 public:
  ArcCpuThrottleObserverTest() = default;

  ArcCpuThrottleObserverTest(const ArcCpuThrottleObserverTest&) = delete;
  ArcCpuThrottleObserverTest& operator=(const ArcCpuThrottleObserverTest&) =
      delete;

  ~ArcCpuThrottleObserverTest() override = default;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    service_manager_ = std::make_unique<ArcServiceManager>();
    session_manager_ =
        CreateTestArcSessionManager(std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
    testing_profile_ = std::make_unique<TestingProfile>();

    StabilityMetricsManager::Initialize(&local_state_);
    prefs::RegisterLocalStatePrefs(local_state_.registry());
    prefs::RegisterProfilePrefs(local_state_.registry());

    arc_metrics_service_ = ArcMetricsService::GetForBrowserContextForTesting(
        testing_profile_.get());
    arc_metrics_service_->SetHistogramNamerCallback(base::BindLambdaForTesting(
        [](const std::string&) -> std::string { return ""; }));

    test_instance_throttle_ =
        ArcInstanceThrottle::GetForBrowserContextForTesting(
            testing_profile_.get());
    DCHECK(test_instance_throttle_);
  }

  void TearDown() override {
    arc::StabilityMetricsManager::Shutdown();
    testing_profile_.reset();
    session_manager_.reset();
    service_manager_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  ArcCpuThrottleObserver* observer() { return &cpu_throttle_observer_; }
  ArcInstanceThrottle* throttle() { return test_instance_throttle_; }
  TestingProfile* profile() { return testing_profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  display::test::TestScreen test_screen_{/*create_display=*/true,
                                         /*register_screen=*/true};
  TestingPrefServiceSimple local_state_;
  raw_ptr<ArcMetricsService, DanglingUntriaged> arc_metrics_service_ = nullptr;
  ArcCpuThrottleObserver cpu_throttle_observer_;
  std::unique_ptr<ArcServiceManager> service_manager_;
  std::unique_ptr<ArcSessionManager> session_manager_;
  std::unique_ptr<TestingProfile> testing_profile_;
  raw_ptr<ArcInstanceThrottle, DanglingUntriaged> test_instance_throttle_;
};

TEST_F(ArcCpuThrottleObserverTest, TestConstructDestruct) {}

TEST_F(ArcCpuThrottleObserverTest, TestStatusChanges) {
  unittest::ThrottleTestObserver test_observer;
  EXPECT_EQ(0, test_observer.count());
  EXPECT_FALSE(throttle()->HasServiceObserverForTesting(observer()));
  // base::Unretained below: safe because all involved objects share scope.
  observer()->StartObserving(
      profile(), base::BindRepeating(&unittest::ThrottleTestObserver::Monitor,
                                     base::Unretained(&test_observer)));
  EXPECT_TRUE(throttle()->HasServiceObserverForTesting(observer()));

  EXPECT_EQ(1, test_observer.count());
  EXPECT_EQ(0, test_observer.active_count());
  EXPECT_EQ(0, test_observer.enforced_count());

  EXPECT_FALSE(observer()->active());
  EXPECT_FALSE(observer()->enforced());

  observer()->OnThrottle(false);  // Not throttled.
  EXPECT_EQ(2, test_observer.count());
  EXPECT_EQ(1, test_observer.active_count());
  EXPECT_EQ(0, test_observer.enforced_count());
  EXPECT_TRUE(observer()->active());
  EXPECT_FALSE(observer()->enforced());

  observer()->OnThrottle(true);  // Yes, throttled.
  EXPECT_EQ(3, test_observer.count());
  EXPECT_EQ(1, test_observer.active_count());
  EXPECT_EQ(0, test_observer.enforced_count());
  EXPECT_FALSE(observer()->active());
  EXPECT_FALSE(observer()->enforced());

  EXPECT_TRUE(throttle()->HasServiceObserverForTesting(observer()));
  observer()->StopObserving();
  EXPECT_FALSE(throttle()->HasServiceObserverForTesting(observer()));
}

}  // namespace arc
