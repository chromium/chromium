// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/install_event_log_util.h"

#include <vector>

#include "base/values.h"
#include "chrome/browser/chromeos/policy/extension_install_event_log.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace em = enterprise_management;

namespace policy {
namespace {

constexpr char kExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

// Common Key names used when building the dictionary to pass to the Chrome
// Reporting API. These must be same as ones mentioned in
// install_event_log_util.cc.
constexpr char kEventType[] = "eventType";
constexpr char kTime[] = "time";
constexpr char kFailureReason[] = "failureReason";
constexpr char kIsMisconfigurationFailure[] = "isMisconfigurationFailure";
constexpr char kExtensionInstallEvent[] = "extensionAppInstallEvent";

void ConvertToValueAndVerify(const em::ExtensionInstallReportLogEvent& event,
                             const std::vector<std::string>& keys) {
  base::Value context = reporting::GetContext(nullptr /*profile*/);
  base::Value wrapper;
  wrapper = ConvertExtensionEventToValue(kExtensionId, event, context);
  ASSERT_TRUE(wrapper.FindKey(kExtensionInstallEvent) != nullptr);
  EXPECT_TRUE(wrapper.FindKey(kTime) != nullptr);
  base::Value* dict = wrapper.FindKey(kExtensionInstallEvent);
  for (const std::string& key : keys) {
    EXPECT_TRUE(dict->FindKey(key) != nullptr);
  }
}

}  // namespace

class InstallEventLogUtilTest : public testing::Test {
 public:
  InstallEventLogUtilTest()
      : scoped_fake_statistics_provider_(
            std::make_unique<
                chromeos::system::ScopedFakeStatisticsProvider>()) {}

 private:
  std::unique_ptr<chromeos::system::ScopedFakeStatisticsProvider>
      scoped_fake_statistics_provider_;
};

// Verifies that an event reporting extension install failure is successfully
// parsed.
TEST_F(InstallEventLogUtilTest, FailureReasonEvent) {
  em::ExtensionInstallReportLogEvent event;
  event.set_timestamp(1000);
  event.set_event_type(em::ExtensionInstallReportLogEvent::INSTALLATION_FAILED);
  event.set_failure_reason(em::ExtensionInstallReportLogEvent::INVALID_ID);
  event.set_is_misconfiguration_failure(false);
  ConvertToValueAndVerify(
      event, {kEventType, kFailureReason, kIsMisconfigurationFailure});
}

}  // namespace policy
