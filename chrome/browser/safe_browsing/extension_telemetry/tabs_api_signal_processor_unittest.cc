// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/tabs_api_signal_processor.h"

#include <array>

#include "chrome/browser/safe_browsing/extension_telemetry/extension_js_callstacks.h"
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_api_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

using SignalInfo = ExtensionTelemetryReportRequest_SignalInfo;
using CallDetails =
    ExtensionTelemetryReportRequest_SignalInfo_TabsApiInfo_CallDetails;

constexpr const char* kExtensionIds[] = {"aaaaaaaabbbbbbbbccccccccdddddddd",
                                         "eeeeeeeeffffffffgggggggghhhhhhhh",
                                         "aaaaeeeebbbbffffccccggggddddhhhh"};
constexpr auto kApiMethods = std::to_array(
    {TabsApiInfo::CREATE, TabsApiInfo::UPDATE, TabsApiInfo::REMOVE});
constexpr const char* kUrls[] = {"http://www.example1.com/",
                                 "https://www.example2.com/"};

class TabsApiSignalProcessorTest : public ::testing::Test {
 protected:
  TabsApiSignalProcessorTest() = default;

  TabsApiSignalProcessor processor_;
};

TEST_F(TabsApiSignalProcessorTest, EmptyProcessorWithNoData) {
  EXPECT_FALSE(processor_.HasDataToReportForTest());
}

TEST_F(TabsApiSignalProcessorTest, IgnoresInvalidSignal) {
  auto invalid_signal = TabsApiSignal(kExtensionIds[0], kApiMethods[0],
                                      /*current_url=*/"", /*new_url=*/"");
  processor_.ProcessSignal(invalid_signal);
  // Verify that processor ignores the signal, i.e., it does not have any data
  // to report.
  EXPECT_FALSE(processor_.HasDataToReportForTest());
}

TEST_F(TabsApiSignalProcessorTest, StoresDataAfterProcessingSignal) {
  auto signal = TabsApiSignal(kExtensionIds[0], kApiMethods[0],
                              /*current_url=*/"", kUrls[0]);
  processor_.ProcessSignal(signal);

  // Verify that processor now has some data to report.
  EXPECT_TRUE(processor_.HasDataToReportForTest());

  // Verify that there is signal info only for the correct
  // extension id.
  EXPECT_TRUE(processor_.GetSignalInfoForReport(kExtensionIds[0]));
  EXPECT_FALSE(processor_.GetSignalInfoForReport(kExtensionIds[1]));
}

TEST_F(TabsApiSignalProcessorTest,
       ReportsSignalInfoCorrectlyWithMultipleUniqueCallDetails) {
  // Process 3 signals for extension 0, each containing the same call details.
  for (int i = 0; i < 3; i++) {
    auto signal = TabsApiSignal(kExtensionIds[0], kApiMethods[0],
                                /*current_url=*/"", kUrls[0]);
    processor_.ProcessSignal(signal);
  }

  // Process 3 signals for extension 1. Two signals contain the same
  // call details as above, and the third contains a second (different) call
  // details.
  for (int i = 0; i < 2; i++) {
    auto signal = TabsApiSignal(kExtensionIds[1], kApiMethods[0],
                                /*current_url=*/"", kUrls[0]);
    processor_.ProcessSignal(signal);
  }
  {
    auto signal =
        TabsApiSignal(kExtensionIds[1], kApiMethods[1], kUrls[0], kUrls[1]);
    processor_.ProcessSignal(signal);
  }

  // Process 1 signal for extension 2. This signal contains different call
  // details from the above.
  {
    auto signal = TabsApiSignal(kExtensionIds[2], kApiMethods[2], kUrls[0],
                                /*new_url=*/"");
    processor_.ProcessSignal(signal);
  }

  // Retrieve signal info for extension 0.
  std::unique_ptr<SignalInfo> extension_0_signal_info =
      processor_.GetSignalInfoForReport(kExtensionIds[0]);
  ASSERT_NE(extension_0_signal_info, nullptr);

  // Verify that processor still has some data to report (for the other two
  // extensions).
  EXPECT_TRUE(processor_.HasDataToReportForTest());

  // Retrieve signal info for extension 1.
  std::unique_ptr<SignalInfo> extension_1_signal_info =
      processor_.GetSignalInfoForReport(kExtensionIds[1]);
  ASSERT_NE(extension_1_signal_info, nullptr);

  // Verify that processor still has some data to report (for the third
  // extension).
  EXPECT_TRUE(processor_.HasDataToReportForTest());

  // Retrieve signal info for extension 2.
  std::unique_ptr<SignalInfo> extension_2_signal_info =
      processor_.GetSignalInfoForReport(kExtensionIds[2]);
  ASSERT_NE(extension_2_signal_info, nullptr);

  // Verify that processor no longer has data to report.
  EXPECT_FALSE(processor_.HasDataToReportForTest());

  // Verify signal info contents for extension 0.
  {
    const TabsApiInfo& tabs_api_info = extension_0_signal_info->tabs_api_info();

    // Verify data stored: only 1 unique call detail (3 API invocations).
    ASSERT_EQ(tabs_api_info.call_details_size(), 1);
    const CallDetails& call_details = tabs_api_info.call_details(0);
    EXPECT_EQ(call_details.method(), kApiMethods[0]);
    EXPECT_EQ(call_details.current_url(), "");
    EXPECT_EQ(call_details.new_url(), kUrls[0]);
    EXPECT_EQ(call_details.count(), static_cast<uint32_t>(3));
  }

  // Verify signal info contents for extension 1.
  {
    const TabsApiInfo& tabs_api_info = extension_1_signal_info->tabs_api_info();

    // Verify data stored: 2 unique call details (2 API invocations for the 1st,
    // 1 for the 2nd).
    ASSERT_EQ(tabs_api_info.call_details_size(), 2);
    {
      const CallDetails& call_details = tabs_api_info.call_details(0);
      EXPECT_EQ(call_details.method(), kApiMethods[0]);
      EXPECT_EQ(call_details.current_url(), "");
      EXPECT_EQ(call_details.new_url(), kUrls[0]);
      EXPECT_EQ(call_details.count(), static_cast<uint32_t>(2));
    }
    {
      const CallDetails& call_details = tabs_api_info.call_details(1);
      EXPECT_EQ(call_details.method(), kApiMethods[1]);
      EXPECT_EQ(call_details.current_url(), kUrls[0]);
      EXPECT_EQ(call_details.new_url(), kUrls[1]);
      EXPECT_EQ(call_details.count(), static_cast<uint32_t>(1));
    }
  }

  // Verify signal info contents for extension 2.
  {
    const TabsApiInfo& tabs_api_info = extension_2_signal_info->tabs_api_info();

    // Verify data stored: only 1 unique call detail (1 API invocation).
    ASSERT_EQ(tabs_api_info.call_details_size(), 1);
    const CallDetails& call_details = tabs_api_info.call_details(0);
    EXPECT_EQ(call_details.method(), kApiMethods[2]);
    EXPECT_EQ(call_details.current_url(), kUrls[0]);
    EXPECT_EQ(call_details.new_url(), "");
    EXPECT_EQ(call_details.count(), static_cast<uint32_t>(1));
  }
}

TEST_F(TabsApiSignalProcessorTest, EnforcesMaxUniqueCallDetails) {
  processor_.SetMaxUniqueCallDetailsForTest(1);

  // Process 3 signals for extension 0:
  // - signals 1,2 have the same call details.
  // - signals 3 has different call details.
  for (int i = 0; i < 2; i++) {
    auto signal = TabsApiSignal(kExtensionIds[0], kApiMethods[0],
                                /*current_url=*/"", kUrls[0]);
    processor_.ProcessSignal(signal);
  }
  {
    auto signal =
        TabsApiSignal(kExtensionIds[0], kApiMethods[1], kUrls[0], kUrls[1]);
    processor_.ProcessSignal(signal);
  }

  // Retrieve signal info.
  std::unique_ptr<SignalInfo> extension_signal_info =
      processor_.GetSignalInfoForReport(kExtensionIds[0]);
  const TabsApiInfo& tabs_api_info = extension_signal_info->tabs_api_info();

  // Verify there is only 1 call details present with invocation count of 2.
  // The 2nd call details is ignored because of the limit of 1.
  ASSERT_EQ(tabs_api_info.call_details_size(), 1);
  const CallDetails& call_details = tabs_api_info.call_details(0);
  EXPECT_EQ(call_details.count(), static_cast<uint32_t>(2));
}

TEST_F(TabsApiSignalProcessorTest, IncludesJSCallStacksInSignalInfo) {
  const std::array<extensions::StackTrace, 3> stack_trace = {
      {{{1, 1, u"foo1.js", u"chrome.tabs.create"},
        {2, 2, u"foo2.js", u"Func2"},
        {3, 3, u"foo3.js", u"Func3"},
        {4, 4, u"foo4.js", u"Func4"},
        {5, 5, u"foo5.js", u"Func5"}},
       {{1, 1, u"foo1.js", u"chrome.tabs.update"},
        {2, 2, u"foo2.js", u"Func2"},
        {3, 3, u"foo3.js", u"Func3"},
        {5, 5, u"foo5.js", u"Func4"}},
       {{1, 1, u"foo1.js", u"chrome.tabs.remove"},
        {2, 2, u"foo2.js", u"Func2"},
        {3, 3, u"foo3.js", u"Func3"}}}};

  // Process 3 signals, each corresponding to a different tabs API method.
  {
    auto signal0 = TabsApiSignal(kExtensionIds[0], kApiMethods[0],
                                 /*current_url=*/"", kUrls[0], stack_trace[0]);
    auto signal1 = TabsApiSignal(kExtensionIds[0], kApiMethods[1], kUrls[0],
                                 kUrls[1], stack_trace[1]);
    auto signal2 = TabsApiSignal(kExtensionIds[0], kApiMethods[2], kUrls[0],
                                 /*new_url=*/"", stack_trace[2]);
    processor_.ProcessSignal(signal0);
    processor_.ProcessSignal(signal1);
    processor_.ProcessSignal(signal2);
  }

  // Retrieve signal info.
  std::unique_ptr<SignalInfo> signal_info =
      processor_.GetSignalInfoForReport(kExtensionIds[0]);
  ASSERT_NE(signal_info, nullptr);
  const TabsApiInfo& tabs_api_info = signal_info->tabs_api_info();
  ASSERT_EQ(tabs_api_info.call_details_size(), 3);

  // Verify JS callstack data stored.
  for (int i = 0; i < 3; i++) {
    const CallDetails& call_details = tabs_api_info.call_details(i);
    EXPECT_EQ(call_details.method(), kApiMethods[i]);
    EXPECT_EQ(call_details.js_callstacks_size(), 1);
    const SignalInfoJSCallStack siginfo_callstack =
        call_details.js_callstacks(0);
    extensions::StackTrace trace =
        ExtensionJSCallStacks::ToExtensionsStackTrace(siginfo_callstack);
    EXPECT_EQ(trace, stack_trace[i]);
  }
}

}  // namespace

}  // namespace safe_browsing
