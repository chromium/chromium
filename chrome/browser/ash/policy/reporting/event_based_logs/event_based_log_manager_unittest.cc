// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/event_based_logs/event_based_log_manager.h"

#include <cstddef>
#include <memory>
#include <set>

#include "base/test/task_environment.h"
#include "chrome/browser/ash/policy/reporting/event_based_logs/event_observer_base.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A fake implementation of `EventObserverBase` for testing.
class TestEventObserver : public policy::EventObserverBase {
 public:
  ash::reporting::TriggerEventType GetEventType() const override {
    return ash::reporting::TriggerEventType::TRIGGER_EVENT_TYPE_UNSPECIFIED;
  }

  std::set<support_tool::DataCollectorType> GetDataCollectorTypes()
      const override {
    return {support_tool::DataCollectorType::CHROME_INTERNAL,
            support_tool::DataCollectorType::CHROMEOS_NETWORK_HEALTH};
  }
};

class EventBasedLogManagerTest : public testing::Test {
 public:
  void SetLogUploadEnabled(bool enabled) {
    cros_settings_.device_settings()->SetBoolean(ash::kSystemLogUploadEnabled,
                                                 enabled);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  ash::ScopedTestingCrosSettings cros_settings_;
};

}  // namespace

// TODO: b/332839740 - Add more tests to verify EventObservers are added
// correctly. For now, we only check the removal with a fake EventObserver since
// there's no real one implemented yet.
TEST_F(EventBasedLogManagerTest, RemoveEventObserversWhenPolicyIsDisabled) {
  SetLogUploadEnabled(true);
  policy::EventBasedLogManager log_manager;
  // We need to add a fake event observer manually since there's no real one
  // implemented yet.
  log_manager.AddEventObserverForTesting(
      ash::reporting::TriggerEventType::TRIGGER_EVENT_TYPE_UNSPECIFIED,
      std::make_unique<TestEventObserver>());
  ASSERT_EQ(log_manager.GetEventObserversForTesting().size(), size_t(1));
  SetLogUploadEnabled(false);
  EXPECT_TRUE(log_manager.GetEventObserversForTesting().empty());
}
