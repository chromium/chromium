// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/metrics/structured/test/structured_metrics_mixin.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/login_manager/dbus-constants.h"

namespace metrics::structured {

namespace {

// The name hash of "CrOSEvents".
constexpr uint64_t kCrosEventsHash = 12657197978410187837ULL;

// Login event hash.
constexpr uint64_t kLoginEventHash = 3946957472799472890ULL;

// Logout event hash.
constexpr uint64_t kLogoutEventHash = 15162740773924916380ULL;

}  // namespace

class StructuredMetricsKeyEventsObserverTest : public ash::LoginManagerTest {
 public:
  StructuredMetricsKeyEventsObserverTest() {
    login_mixin_.AppendRegularUsers(1);
  }
  ~StructuredMetricsKeyEventsObserverTest() override = default;

  void SetUpOnMainThread() override {
    ash::LoginManagerTest::SetUpOnMainThread();
    structured_metrics_mixin_.UpdateRecordingState(true);
  }

 protected:
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
  // Allowing the mixin to add a profile because Logging-in doesn't create one
  // for us. Additionally, the profiles created by the browser are created
  // before the mixin has completed its setup.
  StructuredMetricsMixin structured_metrics_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(StructuredMetricsKeyEventsObserverTest,
                       RecordsLoginLogoutEvents) {
  // Simulate login.
  LoginUser(login_mixin_.users()[0].account_id);
  structured_metrics_mixin_.WaitUntilKeysReady();
  structured_metrics_mixin_.WaitUntilEventRecorded(kCrosEventsHash,
                                                   kLoginEventHash);
  ASSERT_TRUE(
      structured_metrics_mixin_.FindEvent(kCrosEventsHash, kLoginEventHash)
          .has_value());

  // Logout.
  ::ash::SessionTerminationManager::Get()->StopSession(
      ::login_manager::SessionStopReason::USER_REQUESTS_SIGNOUT);

  structured_metrics_mixin_.WaitUntilEventRecorded(kCrosEventsHash,
                                                   kLogoutEventHash);
  ASSERT_TRUE(
      structured_metrics_mixin_.FindEvent(kCrosEventsHash, kLogoutEventHash)
          .has_value());
}

}  // namespace metrics::structured
