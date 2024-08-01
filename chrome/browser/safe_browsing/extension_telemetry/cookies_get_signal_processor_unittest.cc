// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_signal_processor.h"

#include <array>

#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

using SignalInfo = ExtensionTelemetryReportRequest_SignalInfo;
using CookiesGetInfo =
    ExtensionTelemetryReportRequest_SignalInfo_CookiesGetInfo;
using GetArgsInfo =
    ExtensionTelemetryReportRequest_SignalInfo_CookiesGetInfo_GetArgsInfo;

constexpr auto kExtensionId = std::to_array(
    {"aaaaaaaabbbbbbbbccccccccdddddddd", "eeeeeeeeffffffffgggggggghhhhhhhh"});
constexpr auto names = std::to_array({"cookie-1", "cookie-2"});
constexpr auto store_ids = std::to_array({"store-1", "store-2"});
constexpr auto urls =
    std::to_array({"http://www.example1.com/", "https://www.example2.com/"});

class CookiesGetSignalProcessorTest : public ::testing::Test {
 protected:
  CookiesGetSignalProcessorTest() = default;

  CookiesGetSignalProcessor processor_;
};

TEST_F(CookiesGetSignalProcessorTest, EmptyProcessorWithNoData) {
  EXPECT_FALSE(processor_.HasDataToReportForTest());
}

TEST_F(CookiesGetSignalProcessorTest, StoresDataAfterProcessingSignal) {
  auto signal =
      CookiesGetSignal(kExtensionId[0], names[0], store_ids[0], urls[0]);
  processor_.ProcessSignal(signal);

  // Verify that processor now has some data to report.
  EXPECT_TRUE(processor_.HasDataToReportForTest());

  // Verify that there is signal info only for the correct
  // extension id.
  EXPECT_TRUE(processor_.GetSignalInfoForReport(kExtensionId[0]));
  EXPECT_FALSE(processor_.GetSignalInfoForReport(kExtensionId[1]));
}

TEST_F(CookiesGetSignalProcessorTest,
       ReportsSignalInfoCorrectlyWithMultipleUniqueArgSets) {
  // Process 3 signals for extension 1, each corresponding to the first set of
  // arguments.
  for (int i = 0; i < 3; i++) {
    auto signal =
        CookiesGetSignal(kExtensionId[0], names[0], store_ids[0], urls[0]);
    processor_.ProcessSignal(signal);
  }

  // Process 3 signals for extension 2. Two signals contain the first argument
  // set, and the third contains the second argument set.
  for (int i = 0; i < 2; i++) {
    auto signal =
        CookiesGetSignal(kExtensionId[1], names[0], store_ids[0], urls[0]);
    processor_.ProcessSignal(signal);
  }
  {
    auto signal =
        CookiesGetSignal(kExtensionId[1], names[1], store_ids[1], urls[1]);
    processor_.ProcessSignal(signal);
  }

  // Retrieve signal info for first extension.
  std::unique_ptr<SignalInfo> ext_0_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  ASSERT_NE(ext_0_signal_info, nullptr);

  // Verify that processor still has some data to report (for the second
  // extension).
  EXPECT_TRUE(processor_.HasDataToReportForTest());

  // Retrieve signal info for the second extension.
  std::unique_ptr<SignalInfo> ext_1_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[1]);
  ASSERT_NE(ext_1_signal_info, nullptr);

  // Verify that processor no longer has data to report.
  EXPECT_FALSE(processor_.HasDataToReportForTest());

  // Verify signal info contents for first extension.
  {
    const CookiesGetInfo& cookies_get_info =
        ext_0_signal_info->cookies_get_info();

    // Verify data stored: only 1 set of args (3 executions).
    ASSERT_EQ(cookies_get_info.get_args_info_size(), 1);
    const GetArgsInfo& get_args_info = cookies_get_info.get_args_info(0);
    EXPECT_EQ(get_args_info.name(), names[0]);
    EXPECT_EQ(get_args_info.store_id(), store_ids[0]);
    EXPECT_EQ(get_args_info.url(), urls[0]);
    EXPECT_EQ(get_args_info.count(), static_cast<uint32_t>(3));
  }

  // Verify signal info contents for second extension.
  {
    const CookiesGetInfo& cookies_get_info =
        ext_1_signal_info->cookies_get_info();

    // Verify data stored: 2 sets of args (2 executions for 1st, 1 for the
    // 2nd).
    ASSERT_EQ(cookies_get_info.get_args_info_size(), 2);
    {
      const GetArgsInfo& get_args_info = cookies_get_info.get_args_info(0);
      EXPECT_EQ(get_args_info.name(), names[0]);
      EXPECT_EQ(get_args_info.store_id(), store_ids[0]);
      EXPECT_EQ(get_args_info.url(), urls[0]);
      EXPECT_EQ(get_args_info.count(), static_cast<uint32_t>(2));
    }
    {
      const GetArgsInfo& get_args_info = cookies_get_info.get_args_info(1);
      EXPECT_EQ(get_args_info.name(), names[1]);
      EXPECT_EQ(get_args_info.store_id(), store_ids[1]);
      EXPECT_EQ(get_args_info.url(), urls[1]);
      EXPECT_EQ(get_args_info.count(), static_cast<uint32_t>(1));
    }
  }
}

TEST_F(CookiesGetSignalProcessorTest, MaxExceededArgSetsCountNotIncremented) {
  // Set max args limit to 1 for testing.
  processor_.SetMaxArgSetsForTest(1);

  // Process 2 signals with the same args for extension 1.
  auto signal =
      CookiesGetSignal(kExtensionId[0], names[0], store_ids[0], urls[0]);
  processor_.ProcessSignal(signal);
  processor_.ProcessSignal(signal);

  // Retrieve signal info.
  std::unique_ptr<SignalInfo> extension_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  const CookiesGetInfo& cookies_get_info =
      extension_signal_info->cookies_get_info();

  // Verify 1 args set with a count of 2.
  ASSERT_EQ(cookies_get_info.get_args_info_size(), 1);
  const GetArgsInfo& get_args_info = cookies_get_info.get_args_info(0);
  EXPECT_EQ(get_args_info.count(), static_cast<uint32_t>(2));

  // Verify max exceeded args is 0.
  EXPECT_EQ(cookies_get_info.max_exceeded_args_count(), static_cast<size_t>(0));
}

TEST_F(CookiesGetSignalProcessorTest, MaxExceededArgSetsCountIncremented) {
  // Set max args limit to 1 for testing.
  processor_.SetMaxArgSetsForTest(1);

  // Process 3 signals for extension 1:
  // - signals 1,2 have the same args.
  // - signals 3 has different args.
  for (int i = 0; i < 2; i++) {
    auto signal =
        CookiesGetSignal(kExtensionId[0], names[0], store_ids[0], urls[0]);
    processor_.ProcessSignal(signal);
  }
  {
    auto signal =
        CookiesGetSignal(kExtensionId[0], names[1], store_ids[1], urls[1]);
    processor_.ProcessSignal(signal);
  }

  // Retrieve signal info.
  std::unique_ptr<SignalInfo> extension_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  const CookiesGetInfo& cookies_get_info =
      extension_signal_info->cookies_get_info();

  // Verify only 1 args set with execution count of 2
  ASSERT_EQ(cookies_get_info.get_args_info_size(), 1);
  const GetArgsInfo& get_args_info = cookies_get_info.get_args_info(0);
  EXPECT_EQ(get_args_info.count(), static_cast<uint32_t>(2));

  // Verify the max exceeded count is 1. signal3 is not processed because of max
  // args limit of 1.
  EXPECT_EQ(cookies_get_info.max_exceeded_args_count(), static_cast<size_t>(1));
}

TEST_F(CookiesGetSignalProcessorTest, IncludesJSCallStacksInSignalInfo) {
  const std::array<extensions::StackTrace, 2> stack_trace = {
      {{{1, 1, u"foo1.js", u"cookies.get"},
        {2, 2, u"foo2.js", u"Func2"},
        {3, 3, u"foo3.js", u"Func3"},
        {4, 4, u"foo4.js", u"Func4"},
        {5, 5, u"foo5.js", u"Func5"}},
       {{1, 1, u"foo1.js", u"cookies.get"},
        {2, 2, u"foo2.js", u"Func2"},
        {3, 3, u"foo3.js", u"Func3"},
        {5, 5, u"foo4.js", u"Func4"}}}};

  // Process 2 signals with different argument sets.
  for (int i = 0; i < 2; i++) {
    auto signal = CookiesGetSignal(kExtensionId[0], names[i], store_ids[i],
                                   urls[i], stack_trace[i]);
    processor_.ProcessSignal(signal);
  }

  // Retrieve signal info for the extension.
  std::unique_ptr<SignalInfo> signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  ASSERT_NE(signal_info, nullptr);

  // Verify that processor no longer has data to report.
  EXPECT_FALSE(processor_.HasDataToReportForTest());

  // Verify signal info contents.
  const CookiesGetInfo& cookies_get_info = signal_info->cookies_get_info();

  // Verify data stored: 2 arg sets.
  ASSERT_EQ(cookies_get_info.get_args_info_size(), 2);
  {
    const GetArgsInfo& get_args_info = cookies_get_info.get_args_info(0);
    EXPECT_EQ(get_args_info.name(), names[0]);
    EXPECT_EQ(get_args_info.store_id(), store_ids[0]);
    EXPECT_EQ(get_args_info.url(), urls[0]);
    EXPECT_EQ(get_args_info.count(), static_cast<uint32_t>(1));
    // Verify the callstack stored for this arg set.
    EXPECT_EQ(get_args_info.js_callstacks_size(), 1);
    const SignalInfoJSCallStack& siginfo_callstack =
        get_args_info.js_callstacks(0);
    extensions::StackTrace trace =
        ExtensionJSCallStacks::ToExtensionsStackTrace(siginfo_callstack);
    EXPECT_EQ(trace, stack_trace[0]);
  }
  {
    const GetArgsInfo& get_args_info = cookies_get_info.get_args_info(1);
    EXPECT_EQ(get_args_info.name(), names[1]);
    EXPECT_EQ(get_args_info.store_id(), store_ids[1]);
    EXPECT_EQ(get_args_info.url(), urls[1]);
    EXPECT_EQ(get_args_info.count(), static_cast<uint32_t>(1));
    // Verify the callstack stored for this arg set.
    EXPECT_EQ(get_args_info.js_callstacks_size(), 1);
    const SignalInfoJSCallStack& siginfo_callstack =
        get_args_info.js_callstacks(0);
    extensions::StackTrace trace =
        ExtensionJSCallStacks::ToExtensionsStackTrace(siginfo_callstack);
    EXPECT_EQ(trace, stack_trace[1]);
  }
}

}  // namespace

}  // namespace safe_browsing
