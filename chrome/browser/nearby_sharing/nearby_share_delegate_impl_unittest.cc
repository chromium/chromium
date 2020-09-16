// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_delegate_impl.h"

#include "ash/public/cpp/nearby_share_controller.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/time/clock.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/mock_nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/nearby_share_settings.h"
#include "chrome/browser/ui/ash/test_session_controller.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockSettingsOpener : public NearbyShareDelegateImpl::SettingsOpener {
 public:
  MOCK_METHOD(void, ShowSettingsPage, (const std::string&), (override));
};

class MockNearbyShareController : public ash::NearbyShareController {
 public:
  MOCK_METHOD(void,
              HighVisibilityCountdownUpdate,
              (base::TimeDelta),
              (override));
  MOCK_METHOD(void, HighVisibilityEnabledChanged, (bool), (override));
};

// TODO(crbug.com/1127940): Refactor these tests to avoid use of GMock.
class NearbyShareDelegateImplTest : public ::testing::Test {
 public:
  NearbyShareDelegateImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~NearbyShareDelegateImplTest() override = default;

  base::Time Now() const { return task_environment_.GetMockClock()->Now(); }

  // Fast-forwards mock time by |delta| and fires relevant timers.
  void FastForward(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  MockNearbySharingService nearby_share_service_;
  MockSettingsOpener* settings_opener_;
  MockNearbyShareController controller_;
  NearbyShareDelegateImpl delegate_;
};

TEST_F(NearbyShareDelegateImplTest, StartHighVisibilityAndTimeout) {
  // TODO(cclem)
}

TEST_F(NearbyShareDelegateImplTest, StartStopHighVisibility) {
  // TODO(cclem)
}

TEST_F(NearbyShareDelegateImplTest, ShowOnboardingAndTurnOnHighVisibility) {
  // TODO(cclem)
}

TEST_F(NearbyShareDelegateImplTest, ShowOnboardingAndTimeout) {
  // TODO(cclem)
}

TEST_F(NearbyShareDelegateImplTest, StopHighVisibilityOnScreenLock) {
  // TODO(cclem)
}

TEST_F(NearbyShareDelegateImplTest, ShowNearbyShareSettings) {
  // TODO(cclem)
}
