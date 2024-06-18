// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/event_based_logs/event_based_log_manager.h"

#include <cstddef>
#include <memory>
#include <set>

#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/reporting/event_based_logs/event_observer_base.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Key;
using ::testing::UnorderedElementsAreArray;

namespace {

class EventBasedLogManagerBrowserTest
    : public policy::DevicePolicyCrosBrowserTest {
 public:
  void SetLogUploadEnabled(bool enabled) {
    cros_settings_.device_settings()->SetBoolean(ash::kSystemLogUploadEnabled,
                                                 enabled);
  }

 private:
  ash::ScopedTestingCrosSettings cros_settings_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(EventBasedLogManagerBrowserTest,
                       AddAllExpectedEventObservers) {
  SetLogUploadEnabled(true);
  policy::EventBasedLogManager log_manager;
  const std::map<ash::reporting::TriggerEventType,
                 std::unique_ptr<policy::EventObserverBase>>&
      event_observers_map = log_manager.GetEventObserversForTesting();
  EXPECT_FALSE(event_observers_map.empty());
  EXPECT_THAT(event_observers_map,
              UnorderedElementsAreArray(
                  {Key(ash::reporting::TriggerEventType::OS_UPDATE_FAILED)}));
}

IN_PROC_BROWSER_TEST_F(EventBasedLogManagerBrowserTest,
                       RemoveEventObserversWhenPolicyIsDisabled) {
  SetLogUploadEnabled(true);
  policy::EventBasedLogManager log_manager;
  // Verify that event observers are added.
  EXPECT_FALSE(log_manager.GetEventObserversForTesting().empty());
  SetLogUploadEnabled(false);
  // All event observers should be deleted when policy is deleted.
  EXPECT_TRUE(log_manager.GetEventObserversForTesting().empty());
}
