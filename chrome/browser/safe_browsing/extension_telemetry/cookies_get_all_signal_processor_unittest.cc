// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_all_signal_processor.h"

#include <array>

#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_all_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

using SignalInfo = ExtensionTelemetryReportRequest_SignalInfo;
using CookiesGetAllInfo =
    ExtensionTelemetryReportRequest_SignalInfo_CookiesGetAllInfo;
using GetAllArgsInfo =
    ExtensionTelemetryReportRequest_SignalInfo_CookiesGetAllInfo_GetAllArgsInfo;

constexpr auto kExtensionId = std::to_array(
    {"aaaaaaaabbbbbbbbccccccccdddddddd", "eeeeeeeeffffffffgggggggghhhhhhhh"});
constexpr auto domains = std::to_array({"domain1", "domain2"});
constexpr auto names = std::to_array({"cookie-1", "cookie-2"});
constexpr auto paths = std::to_array({"/path1", "/path2"});
constexpr auto is_secure_cookie_values = std::to_array({true, false});
constexpr auto store_ids = std::to_array({"store-1", "store-2"});
constexpr auto urls =
    std::to_array({"http://www.example1.com/", "https://www.example2.com/"});
constexpr auto is_session_cookie_values = std::to_array({false, true});

class CookiesGetAllSignalProcessorTest : public ::testing::Test {
 protected:
  CookiesGetAllSignalProcessorTest() = default;

  CookiesGetAllSignalProcessor processor_;
};

TEST_F(CookiesGetAllSignalProcessorTest, EmptyProcessorWithNoData) {
  EXPECT_FALSE(processor_.HasDataToReportForTest());
}

TEST_F(CookiesGetAllSignalProcessorTest, StoresDataAfterProcessingSignal) {
  auto signal =
      CookiesGetAllSignal(kExtensionId[0], domains[0], names[0], paths[0],
                          is_secure_cookie_values[0], store_ids[0], urls[0],
                          is_session_cookie_values[0]);
  processor_.ProcessSignal(signal);

  // Verify that the processor now has some data to report.
  EXPECT_TRUE(processor_.HasDataToReportForTest());

  // Verify that there is signal info only for the correct
  // extension id.
  EXPECT_TRUE(processor_.GetSignalInfoForReport(kExtensionId[0]));
  EXPECT_FALSE(processor_.GetSignalInfoForReport(kExtensionId[1]));
}

TEST_F(CookiesGetAllSignalProcessorTest,
       ReportsSignalInfoCorrectlyWithMultipleUniqueArgSets) {
  // Process 3 signals for extension 1, each corresponding to the first set of
  // arguments.
  for (int i = 0; i < 3; i++) {
    auto signal =
        CookiesGetAllSignal(kExtensionId[0], domains[0], names[0], paths[0],
                            is_secure_cookie_values[0], store_ids[0], urls[0],
                            is_session_cookie_values[0]);
    processor_.ProcessSignal(signal);
  }

  // Process 3 signals for extension 2. Two signals contain the first argument
  // set, and the third contains the second argument set.
  for (int i = 0; i < 2; i++) {
    auto signal =
        CookiesGetAllSignal(kExtensionId[1], domains[0], names[0], paths[0],
                            is_secure_cookie_values[0], store_ids[0], urls[0],
                            is_session_cookie_values[0]);
    processor_.ProcessSignal(signal);
  }
  {
    auto signal =
        CookiesGetAllSignal(kExtensionId[1], domains[1], names[1], paths[1],
                            is_secure_cookie_values[1], store_ids[1], urls[1],
                            is_session_cookie_values[1]);
    processor_.ProcessSignal(signal);
  }

  // Retrieve signal info for first extension.
  std::unique_ptr<SignalInfo> ext_0_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  ASSERT_NE(ext_0_signal_info, nullptr);

  // Verify that the processor still has some data to report (for the second
  // extension).
  EXPECT_TRUE(processor_.HasDataToReportForTest());

  // Retrieve signal info for the second extension.
  std::unique_ptr<SignalInfo> ext_1_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[1]);
  ASSERT_NE(ext_1_signal_info, nullptr);

  // Verify that the processor no longer has data to report.
  EXPECT_FALSE(processor_.HasDataToReportForTest());

  // Verify signal info contents for first extension.
  {
    const CookiesGetAllInfo& cookies_get_all_info =
        ext_0_signal_info->cookies_get_all_info();

    // Verify data stored: only 1 set of args (3 executions).
    ASSERT_EQ(cookies_get_all_info.get_all_args_info_size(), 1);
    const GetAllArgsInfo& get_all_args_info =
        cookies_get_all_info.get_all_args_info(0);
    EXPECT_EQ(get_all_args_info.domain(), domains[0]);
    EXPECT_EQ(get_all_args_info.name(), names[0]);
    EXPECT_EQ(get_all_args_info.path(), paths[0]);
    EXPECT_EQ(get_all_args_info.secure(), is_secure_cookie_values[0]);
    EXPECT_EQ(get_all_args_info.store_id(), store_ids[0]);
    EXPECT_EQ(get_all_args_info.url(), urls[0]);
    EXPECT_EQ(get_all_args_info.is_session(), is_session_cookie_values[0]);
    EXPECT_EQ(get_all_args_info.count(), static_cast<uint32_t>(3));
  }

  // Verify signal info contents for second extension.
  {
    const CookiesGetAllInfo& cookies_get_all_info =
        ext_1_signal_info->cookies_get_all_info();

    // Verify data stored: 2 sets of args (2 executions for 1st, 1 for the
    // 2nd).
    ASSERT_EQ(cookies_get_all_info.get_all_args_info_size(), 2);
    {
      const GetAllArgsInfo& get_all_args_info =
          cookies_get_all_info.get_all_args_info(0);
      EXPECT_EQ(get_all_args_info.domain(), domains[0]);
      EXPECT_EQ(get_all_args_info.name(), names[0]);
      EXPECT_EQ(get_all_args_info.path(), paths[0]);
      EXPECT_EQ(get_all_args_info.secure(), is_secure_cookie_values[0]);
      EXPECT_EQ(get_all_args_info.store_id(), store_ids[0]);
      EXPECT_EQ(get_all_args_info.url(), urls[0]);
      EXPECT_EQ(get_all_args_info.is_session(), is_session_cookie_values[0]);
      EXPECT_EQ(get_all_args_info.count(), static_cast<uint32_t>(2));
    }
    {
      const GetAllArgsInfo& get_all_args_info =
          cookies_get_all_info.get_all_args_info(1);
      EXPECT_EQ(get_all_args_info.domain(), domains[1]);
      EXPECT_EQ(get_all_args_info.name(), names[1]);
      EXPECT_EQ(get_all_args_info.path(), paths[1]);
      EXPECT_EQ(get_all_args_info.secure(), is_secure_cookie_values[1]);
      EXPECT_EQ(get_all_args_info.store_id(), store_ids[1]);
      EXPECT_EQ(get_all_args_info.url(), urls[1]);
      EXPECT_EQ(get_all_args_info.is_session(), is_session_cookie_values[1]);
      EXPECT_EQ(get_all_args_info.count(), static_cast<uint32_t>(1));
    }
  }
}

TEST_F(CookiesGetAllSignalProcessorTest,
       MaxExceededArgSetsCountNotIncremented) {
  // Set max args limit to 1 for testing.
  processor_.SetMaxArgSetsForTest(1);

  // Process 2 signals with the same args for extension 1.
  auto signal =
      CookiesGetAllSignal(kExtensionId[0], domains[0], names[0], paths[0],
                          is_secure_cookie_values[0], store_ids[0], urls[0],
                          is_session_cookie_values[0]);
  processor_.ProcessSignal(signal);
  processor_.ProcessSignal(signal);

  // Retrieve signal info.
  std::unique_ptr<SignalInfo> extension_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  const CookiesGetAllInfo& cookies_get_all_info =
      extension_signal_info->cookies_get_all_info();

  // Verify 1 args set with a count of 2.
  ASSERT_EQ(cookies_get_all_info.get_all_args_info_size(), 1);
  const GetAllArgsInfo& get_all_args_info =
      cookies_get_all_info.get_all_args_info(0);
  EXPECT_EQ(get_all_args_info.count(), static_cast<uint32_t>(2));

  // Verify max exceeded args is 0.
  EXPECT_EQ(cookies_get_all_info.max_exceeded_args_count(),
            static_cast<size_t>(0));
}

TEST_F(CookiesGetAllSignalProcessorTest, MaxExceededArgSetsCountIncremented) {
  // Set max args limit to 1 for testing.
  processor_.SetMaxArgSetsForTest(1);

  // Process 3 signals for extension 1:
  // - signals 1,2 have the same args.
  // - signals 3 has different args.
  for (int i = 0; i < 2; i++) {
    auto signal =
        CookiesGetAllSignal(kExtensionId[0], domains[0], names[0], paths[0],
                            is_secure_cookie_values[0], store_ids[0], urls[0],
                            is_session_cookie_values[0]);
    processor_.ProcessSignal(signal);
  }
  {
    auto signal =
        CookiesGetAllSignal(kExtensionId[0], domains[1], names[1], paths[1],
                            is_secure_cookie_values[1], store_ids[1], urls[1],
                            is_session_cookie_values[1]);
    processor_.ProcessSignal(signal);
  }

  // Retrieve signal info.
  std::unique_ptr<SignalInfo> extension_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  const CookiesGetAllInfo& cookies_get_all_info =
      extension_signal_info->cookies_get_all_info();

  // Verify only 1 args with execution count of 2
  ASSERT_EQ(cookies_get_all_info.get_all_args_info_size(), 1);
  const GetAllArgsInfo& get_all_args_info =
      cookies_get_all_info.get_all_args_info(0);
  EXPECT_EQ(get_all_args_info.count(), static_cast<uint32_t>(2));

  // Verify the max exceeded count is 1. signal3 is not processed because of max
  // args limit of 1.
  EXPECT_EQ(cookies_get_all_info.max_exceeded_args_count(),
            static_cast<size_t>(1));
}

TEST_F(CookiesGetAllSignalProcessorTest,
       ReportsSignalInfoCorrectlyWithEmptyBooleans) {
  auto signal =
      CookiesGetAllSignal(kExtensionId[0], domains[0], names[0], paths[0],
                          std::nullopt, store_ids[0], urls[0], std::nullopt);
  processor_.ProcessSignal(signal);

  // Retrieve signal info.
  std::unique_ptr<SignalInfo> extension_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  const CookiesGetAllInfo& cookies_get_all_info =
      extension_signal_info->cookies_get_all_info();
  const GetAllArgsInfo& get_all_args_info =
      cookies_get_all_info.get_all_args_info(0);

  EXPECT_FALSE(get_all_args_info.has_secure());
  EXPECT_FALSE(get_all_args_info.has_is_session());
}

TEST_F(CookiesGetAllSignalProcessorTest, IncludesJSCallStacksInSignalInfo) {
  const std::array<extensions::StackTrace, 2> stack_trace = {
      {{{1, 1, u"foo1.js", u"cookies.getAll"},
        {2, 2, u"foo2.js", u"Func2"},
        {3, 3, u"foo3.js", u"Func3"},
        {4, 4, u"foo4.js", u"Func4"},
        {5, 5, u"foo5.js", u"Func5"}},
       {{1, 1, u"foo1.js", u"cookies.getAll"},
        {2, 2, u"foo2.js", u"Func2"},
        {3, 3, u"foo3.js", u"Func3"},
        {5, 5, u"foo4.js", u"Func4"}}}};

  // Process 2 signals with different argument sets.
  for (int i = 0; i < 2; i++) {
    auto signal =
        CookiesGetAllSignal(kExtensionId[0], domains[i], names[i], paths[i],
                            is_secure_cookie_values[i], store_ids[i], urls[i],
                            is_session_cookie_values[i], stack_trace[i]);
    processor_.ProcessSignal(signal);
  }

  // Retrieve signal info for the extension.
  std::unique_ptr<SignalInfo> signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  ASSERT_NE(signal_info, nullptr);

  // Verify that the processor no longer has data to report.
  EXPECT_FALSE(processor_.HasDataToReportForTest());

  // Verify signal info contents.
  const CookiesGetAllInfo& cookies_get_all_info =
      signal_info->cookies_get_all_info();

  // Verify data stored: 2 arg sets.
  ASSERT_EQ(cookies_get_all_info.get_all_args_info_size(), 2);
  {
    const GetAllArgsInfo& get_all_args_info =
        cookies_get_all_info.get_all_args_info(0);
    EXPECT_EQ(get_all_args_info.domain(), domains[0]);
    EXPECT_EQ(get_all_args_info.name(), names[0]);
    EXPECT_EQ(get_all_args_info.path(), paths[0]);
    EXPECT_EQ(get_all_args_info.secure(), is_secure_cookie_values[0]);
    EXPECT_EQ(get_all_args_info.store_id(), store_ids[0]);
    EXPECT_EQ(get_all_args_info.url(), urls[0]);
    EXPECT_EQ(get_all_args_info.is_session(), is_session_cookie_values[0]);
    EXPECT_EQ(get_all_args_info.count(), 1u);
    // Verify the callstack stored for this arg set.
    ASSERT_EQ(get_all_args_info.js_callstacks_size(), 1);
    const SignalInfoJSCallStack& siginfo_callstack =
        get_all_args_info.js_callstacks(0);
    extensions::StackTrace trace =
        ExtensionJSCallStacks::ToExtensionsStackTrace(siginfo_callstack);
    EXPECT_EQ(trace, stack_trace[0]);
  }
  {
    const GetAllArgsInfo& get_all_args_info =
        cookies_get_all_info.get_all_args_info(1);
    EXPECT_EQ(get_all_args_info.domain(), domains[1]);
    EXPECT_EQ(get_all_args_info.name(), names[1]);
    EXPECT_EQ(get_all_args_info.path(), paths[1]);
    EXPECT_EQ(get_all_args_info.secure(), is_secure_cookie_values[1]);
    EXPECT_EQ(get_all_args_info.store_id(), store_ids[1]);
    EXPECT_EQ(get_all_args_info.url(), urls[1]);
    EXPECT_EQ(get_all_args_info.is_session(), is_session_cookie_values[1]);
    EXPECT_EQ(get_all_args_info.count(), 1u);
    // Verify the callstack stored for this arg set.
    ASSERT_EQ(get_all_args_info.js_callstacks_size(), 1);
    const SignalInfoJSCallStack& siginfo_callstack =
        get_all_args_info.js_callstacks(0);
    extensions::StackTrace trace =
        ExtensionJSCallStacks::ToExtensionsStackTrace(siginfo_callstack);
    EXPECT_EQ(trace, stack_trace[1]);
  }
}

}  // namespace

}  // namespace safe_browsing
