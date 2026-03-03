// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/dom_access_signal_processor.h"

#include <memory>

#include "chrome/browser/safe_browsing/extension_telemetry/dom_access_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

using SignalInfo = ExtensionTelemetryReportRequest_SignalInfo;
using DOMAccess =
    ExtensionTelemetryReportRequest::SignalInfo::DOMAccessInfo::DOMAccess;

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Property;
using ::testing::UnorderedElementsAre;

const char* kExtensionId[] = {"extension_0", "extension_1"};

class DOMAccessSignalProcessorTest : public ::testing::Test {
 protected:
  DOMAccessSignalProcessorTest() = default;

  DOMAccessSignalProcessor processor_;
};

TEST_F(DOMAccessSignalProcessorTest, EmptyProcessorWithNoData) {
  EXPECT_FALSE(processor_.HasDataToReportForTest());
}

TEST_F(DOMAccessSignalProcessorTest, StoresDataAfterProcessingSignal) {
  DOMAccessSignal signal(kExtensionId[0], "Document.cookie",
                         "http://example.com", DOMAccessSignal::DOMAccess::READ,
                         base::Time::Now());
  processor_.ProcessSignal(signal);

  EXPECT_TRUE(processor_.HasDataToReportForTest());
  EXPECT_NE(processor_.GetSignalInfoForReport(kExtensionId[0]), nullptr);
}

TEST_F(DOMAccessSignalProcessorTest, AggregatesSignalsCorrectly) {
  base::Time now = base::Time::Now();
  base::Time later = now + base::Seconds(10);

  // 3 identical signals (except timestamp).
  processor_.ProcessSignal(
      DOMAccessSignal(kExtensionId[0], "Document.cookie", "http://example.com",
                      DOMAccessSignal::DOMAccess::READ, now));
  processor_.ProcessSignal(
      DOMAccessSignal(kExtensionId[0], "Document.cookie", "http://example.com",
                      DOMAccessSignal::DOMAccess::READ, later));
  processor_.ProcessSignal(
      DOMAccessSignal(kExtensionId[0], "Document.cookie", "http://example.com",
                      DOMAccessSignal::DOMAccess::READ, now));

  // 1 different signal (different API).
  processor_.ProcessSignal(DOMAccessSignal(
      kExtensionId[0], "HTMLInputElement.value", "http://example.com",
      DOMAccessSignal::DOMAccess::READ, now));

  std::unique_ptr<SignalInfo> signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  ASSERT_NE(signal_info, nullptr);
  const auto& dom_access_info = signal_info->dom_access_info();

  ASSERT_EQ(dom_access_info.dom_accesses_size(), 2);

  EXPECT_THAT(
      dom_access_info.dom_accesses(),
      UnorderedElementsAre(
          AllOf(Property(&DOMAccess::api_name, Eq("Document.cookie")),
                Property(&DOMAccess::count, Eq(3u)),
                Property(&DOMAccess::timestamp_ms,
                         Eq(later.InMillisecondsSinceUnixEpoch()))),
          AllOf(Property(&DOMAccess::api_name, Eq("HTMLInputElement.value")),
                Property(&DOMAccess::count, Eq(1u)))));
}

TEST_F(DOMAccessSignalProcessorTest, EnforcesMaxAggregatedSignals) {
  processor_.SetMaxAggregatedSignalsForTest(1);

  processor_.ProcessSignal(DOMAccessSignal(kExtensionId[0], "api1", "url1",
                                           DOMAccessSignal::DOMAccess::READ,
                                           base::Time::Now()));
  // This one should be ignored.
  processor_.ProcessSignal(DOMAccessSignal(kExtensionId[0], "api2", "url1",
                                           DOMAccessSignal::DOMAccess::READ,
                                           base::Time::Now()));

  std::unique_ptr<SignalInfo> signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  ASSERT_NE(signal_info, nullptr);
  EXPECT_EQ(signal_info->dom_access_info().dom_accesses_size(), 1);
  EXPECT_EQ(signal_info->dom_access_info().dom_accesses(0).api_name(), "api1");
}

}  // namespace

}  // namespace safe_browsing
