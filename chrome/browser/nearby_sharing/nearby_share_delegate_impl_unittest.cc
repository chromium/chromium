// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_delegate_impl.h"

#include "ash/public/cpp/nearby_share_controller.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/time/clock.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/local_device_data/fake_nearby_share_local_device_data_manager.h"
#include "chrome/browser/nearby_sharing/mock_nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/nearby_share_settings.h"
#include "chrome/browser/ui/ash/test_session_controller.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kDefaultDeviceName[] = "Josh's Chromebook";

}  // namespace

using ::testing::_;
using ::testing::Assign;
using ::testing::AtLeast;
using ::testing::Return;
using ::testing::ReturnPointee;

class MockSettingsOpener : public NearbyShareDelegateImpl::SettingsOpener {
 public:
  MOCK_METHOD(void, ShowSettingsPage, (const std::string&), (override));
};

class MockNearbyShareController : public ash::NearbyShareController {
 public:
  MOCK_METHOD(void, HighVisibilityEnabledChanged, (bool), (override));
};

// TODO(crbug.com/1127940): Refactor these tests to avoid use of GMock.
class NearbyShareDelegateImplTest : public ::testing::Test {
 public:
  NearbyShareDelegateImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        test_local_device_data_(kDefaultDeviceName),
        settings_(&test_pref_service_, &test_local_device_data_),
        delegate_(&controller_) {
    RegisterNearbySharingPrefs(test_pref_service_.registry());
  }

  ~NearbyShareDelegateImplTest() override = default;

  base::Time Now() const { return task_environment_.GetMockClock()->Now(); }

  // Fast-forwards mock time by |delta| and fires relevant timers.
  void FastForward(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void SetHighVisibilityOn(bool high_visibility_on) {
    if (high_visibility_on_ != high_visibility_on) {
      high_visibility_on_ = high_visibility_on;
      delegate_.OnHighVisibilityChanged(high_visibility_on);
    }
  }

  void SetUp() override {
    settings_.SetEnabled(false);

    EXPECT_CALL(nearby_share_service_, GetSettings())
        .WillRepeatedly(Return(&settings_));
    EXPECT_CALL(nearby_share_service_, IsInHighVisibility())
        .WillRepeatedly(ReturnPointee(&high_visibility_on_));
    EXPECT_CALL(nearby_share_service_, AddObserver(_))
        .WillRepeatedly(Assign(&service_observer_bound_, true));
    EXPECT_CALL(nearby_share_service_, RemoveObserver(_))
        .WillRepeatedly(Assign(&service_observer_bound_, false));
    EXPECT_CALL(nearby_share_service_, HasObserver(_))
        .WillRepeatedly(ReturnPointee(&service_observer_bound_));

    delegate_.SetNearbyShareServiceForTest(&nearby_share_service_);

    std::unique_ptr<MockSettingsOpener> settings_opener =
        std::make_unique<MockSettingsOpener>();
    settings_opener_ = settings_opener.get();
    delegate_.set_settings_opener_for_test(std::move(settings_opener));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  MockNearbySharingService nearby_share_service_;
  TestSessionController session_controller_;
  sync_preferences::TestingPrefServiceSyncable test_pref_service_;
  FakeNearbyShareLocalDeviceDataManager test_local_device_data_;
  NearbyShareSettings settings_;
  MockSettingsOpener* settings_opener_;
  MockNearbyShareController controller_;
  NearbyShareDelegateImpl delegate_;
  bool high_visibility_on_ = false;
  bool service_observer_bound_ = false;
};

TEST_F(NearbyShareDelegateImplTest, StartHighVisibilityAndTimeout) {
  settings_.SetEnabled(true);

  EXPECT_CALL(*settings_opener_, ShowSettingsPage(_));
  EXPECT_CALL(controller_, HighVisibilityEnabledChanged(true));

  delegate_.EnableHighVisibility();
  SetHighVisibilityOn(true);

  EXPECT_CALL(*settings_opener_, ShowSettingsPage(_));
  EXPECT_CALL(controller_, HighVisibilityEnabledChanged(false));

  // DisableHighVisibility will be called automatically after the timer fires.
  FastForward(base::TimeDelta::FromMinutes(10));
  SetHighVisibilityOn(false);
}

TEST_F(NearbyShareDelegateImplTest, StartStopHighVisibility) {
  settings_.SetEnabled(true);

  EXPECT_CALL(*settings_opener_, ShowSettingsPage(_));
  EXPECT_CALL(controller_, HighVisibilityEnabledChanged(true));

  delegate_.EnableHighVisibility();
  SetHighVisibilityOn(true);

  EXPECT_CALL(*settings_opener_, ShowSettingsPage(_));
  EXPECT_CALL(controller_, HighVisibilityEnabledChanged(false));

  delegate_.DisableHighVisibility();
  SetHighVisibilityOn(false);
}

TEST_F(NearbyShareDelegateImplTest, ShowOnboardingAndTurnOnHighVisibility) {
  settings_.SetEnabled(false);

  // Called once to start onboarding, once to enter high visibility, and once to
  // exit high visibility.
  EXPECT_CALL(*settings_opener_, ShowSettingsPage(_)).Times(3);

  delegate_.EnableHighVisibility();

  EXPECT_CALL(controller_, HighVisibilityEnabledChanged(true));

  // Delegate will observe Nearby Share enabled within onboarding wait period
  // and will turn on high visibility.
  settings_.SetEnabled(true);
  SetHighVisibilityOn(true);

  EXPECT_CALL(controller_, HighVisibilityEnabledChanged(false));

  // DisableHighVisibility will be called automatically after the timer fires.
  FastForward(base::TimeDelta::FromMinutes(10));
  SetHighVisibilityOn(false);
}

TEST_F(NearbyShareDelegateImplTest, ShowOnboardingAndTimeout) {
  settings_.SetEnabled(false);

  EXPECT_CALL(nearby_share_service_, GetSettings())
      .WillRepeatedly(Return(&settings_));
  EXPECT_CALL(*settings_opener_, ShowSettingsPage(_));

  delegate_.EnableHighVisibility();

  EXPECT_CALL(controller_, HighVisibilityEnabledChanged(_)).Times(0);

  // Wait for longer than the onboarding wait period.
  FastForward(base::TimeDelta::FromMinutes(10));

  // Delegate will observe Nearby Share enabled outside of onboarding wait
  // period and will not turn on high visibility.
  settings_.SetEnabled(true);
}

TEST_F(NearbyShareDelegateImplTest, StopHighVisibilityOnScreenLock) {
  settings_.SetEnabled(true);

  EXPECT_CALL(controller_, HighVisibilityEnabledChanged(true));
  EXPECT_CALL(*settings_opener_, ShowSettingsPage(_));

  delegate_.EnableHighVisibility();
  SetHighVisibilityOn(true);

  EXPECT_CALL(controller_, HighVisibilityEnabledChanged(false));
  EXPECT_CALL(*settings_opener_, ShowSettingsPage(_));

  // DisableHighVisibility will be called when the screen locks.
  delegate_.OnLockStateChanged(/*locked=*/true);
  SetHighVisibilityOn(false);
}

TEST_F(NearbyShareDelegateImplTest, ShowNearbyShareSettings) {
  EXPECT_CALL(*settings_opener_, ShowSettingsPage(_));

  delegate_.ShowNearbyShareSettings();
}
