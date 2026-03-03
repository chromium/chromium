// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/script_injection_signal_processor.h"

#include <memory>
#include <vector>

#include "chrome/browser/safe_browsing/extension_telemetry/script_injection_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

using SignalInfo = ExtensionTelemetryReportRequest_SignalInfo;
using ScriptInjection = ExtensionTelemetryReportRequest::SignalInfo::
    ScriptInjectionInfo::ScriptInjection;

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Property;
using ::testing::UnorderedElementsAre;

const char* kExtensionId[] = {"extension_0", "extension_1"};

class ScriptInjectionSignalProcessorTest : public ::testing::Test {
 protected:
  ScriptInjectionSignalProcessorTest() = default;

  ScriptInjectionSignalProcessor processor_;
};

TEST_F(ScriptInjectionSignalProcessorTest, EmptyProcessorWithNoData) {
  EXPECT_FALSE(processor_.HasDataToReportForTest());
}

TEST_F(ScriptInjectionSignalProcessorTest, StoresDataAfterProcessingSignal) {
  ScriptInjectionSignal signal(kExtensionId[0], "blinkAddElement",
                               "http://example.com", {"script"}, "",
                               base::Time::Now());
  processor_.ProcessSignal(signal);

  EXPECT_TRUE(processor_.HasDataToReportForTest());
  EXPECT_NE(processor_.GetSignalInfoForReport(kExtensionId[0]), nullptr);
}

TEST_F(ScriptInjectionSignalProcessorTest, AggregatesSignalsCorrectly) {
  base::Time now = base::Time::Now();
  base::Time later = now + base::Seconds(10);

  // Identical signals (different timestamps).
  processor_.ProcessSignal(ScriptInjectionSignal(
      kExtensionId[0], "blinkSetAttribute", "http://example.com",
      {"script", "src", "<arg_url>"}, "http://evil.com/js", now));
  processor_.ProcessSignal(ScriptInjectionSignal(
      kExtensionId[0], "blinkSetAttribute", "http://example.com",
      {"script", "src", "<arg_url>"}, "http://evil.com/js", later));

  // Different signal (different arg_url).
  processor_.ProcessSignal(ScriptInjectionSignal(
      kExtensionId[0], "blinkSetAttribute", "http://example.com",
      {"script", "src", "<arg_url>"}, "http://other.com/js", now));

  std::unique_ptr<SignalInfo> signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  ASSERT_NE(signal_info, nullptr);
  const auto& injection_info = signal_info->script_injection_info();

  ASSERT_EQ(injection_info.script_injections_size(), 2);

  // Check aggregated signal using gMock.
  EXPECT_THAT(injection_info.script_injections(),
              Contains(AllOf(
                  Property(&ScriptInjection::arg_url, Eq("http://evil.com/js")),
                  Property(&ScriptInjection::count, Eq(2u)),
                  Property(&ScriptInjection::timestamp_ms,
                           Eq(later.InMillisecondsSinceUnixEpoch())),
                  Property(&ScriptInjection::args_list_size, Eq(3)))));
}

TEST_F(ScriptInjectionSignalProcessorTest, EnforcesMaxAggregatedSignals) {
  processor_.SetMaxAggregatedSignalsForTest(1);

  processor_.ProcessSignal(ScriptInjectionSignal(
      kExtensionId[0], "api1", "url1", {"arg1"}, "", base::Time::Now()));
  // This one should be ignored.
  processor_.ProcessSignal(ScriptInjectionSignal(
      kExtensionId[0], "api2", "url1", {"arg1"}, "", base::Time::Now()));

  std::unique_ptr<SignalInfo> signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  ASSERT_NE(signal_info, nullptr);
  EXPECT_EQ(signal_info->script_injection_info().script_injections_size(), 1);
}

}  // namespace

}  // namespace safe_browsing
