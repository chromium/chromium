// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_logging.h"

#include <stddef.h>

#include "ash/constants/ash_features.h"
#include "base/no_destructor.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const char kLog1[] = "This string should exist in the log";
const char kLog2[] = "This string should not exist in the log";

// Called for every log message added to the standard logging system. The new
// log is saved in |standard_logs| and consumed so it does not flood stdout.
std::vector<std::string>& GetStandardLogs() {
  static base::NoDestructor<std::vector<std::string>> standard_logs;
  return *standard_logs;
}

bool HandleStandardLogMessage(int severity,
                              const char* file,
                              int line,
                              size_t message_start,
                              const std::string& str) {
  GetStandardLogs().push_back(str);
  return true;
}

}  // namespace

class InputDeviceSettingsLoggingTest : public testing::Test {
 public:
  InputDeviceSettingsLoggingTest() = default;

  void SetUp() override {
    GetStandardLogs().clear();

    previous_handler_ = logging::GetLogMessageHandler();
    logging::SetLogMessageHandler(&HandleStandardLogMessage);
  }

  void TearDown() override { logging::SetLogMessageHandler(previous_handler_); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  logging::LogMessageHandlerFunction previous_handler_{nullptr};
};

TEST_F(InputDeviceSettingsLoggingTest, EnableFeatureFlag) {
  scoped_feature_list_.InitWithFeatures({features::kEnablePeripheralsLogging},
                                        {});
  PR_LOG(INFO, Feature::IDS) << kLog1;

  ASSERT_EQ(1u, GetStandardLogs().size());
  EXPECT_NE(std::string::npos, GetStandardLogs()[0].find(kLog1));
}

TEST_F(InputDeviceSettingsLoggingTest, DisableFeatureFlag) {
  scoped_feature_list_.InitAndDisableFeature(
      {features::kEnablePeripheralsLogging});

  PR_LOG(INFO, Feature::IDS) << kLog2;

  ASSERT_EQ(0u, GetStandardLogs().size());
}

}  // namespace ash
