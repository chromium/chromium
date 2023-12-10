// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_signal_processor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

using SignalInfo = ExtensionTelemetryReportRequest_SignalInfo;
using DeclarativeNetRequestInfo =
    ExtensionTelemetryReportRequest_SignalInfo_DeclarativeNetRequestInfo;
using TestRule = extensions::declarative_net_request::TestRule;

constexpr const char* kExtensionId[] = {"aaaaaaaabbbbbbbbccccccccdddddddd",
                                        "eeeeeeeeffffffffgggggggghhhhhhhh"};
constexpr const int kRuleId[] = {0, 1, 2, 3, 4, 5};
constexpr const char* kRedirectRuleActionType = "redirect";
constexpr const char* kModifyHeadersRuleActionType = "modifyHeaders";
constexpr const char* kBlockRuleActionType = "block";

extensions::api::declarative_net_request::Rule GetAPIRule(
    const TestRule& rule) {
  auto result =
      extensions::api::declarative_net_request::Rule::FromValue(rule.ToValue());
  if (!result.has_value()) {
    ADD_FAILURE() << "Failed to parse rule value.  error= "
                  << base::UTF16ToUTF8(result.error())
                  << ", contents=" << rule.ToValue();
    return extensions::api::declarative_net_request::Rule();
  }
  return std::move(result).value();
}

TestRule CreateTestRule(int id, const std::string& action_type) {
  TestRule test_rule =
      extensions::declarative_net_request::CreateGenericRule(id);
  test_rule.action->type = action_type;
  return test_rule;
}

class DeclarativeNetRequestSignalProcessorTest : public ::testing::Test {
 protected:
  DeclarativeNetRequestSignalProcessorTest() = default;

  DeclarativeNetRequestSignalProcessor processor_;
};

TEST_F(DeclarativeNetRequestSignalProcessorTest, EmptyProcessorWithNoData) {
  EXPECT_FALSE(processor_.HasDataToReportForTest());
}

TEST_F(DeclarativeNetRequestSignalProcessorTest, ReportsSignalInfo) {
  // Set up 1 redirect rule.
  std::vector<extensions::api::declarative_net_request::Rule> api_rules;
  TestRule redirect_rule_1 =
      CreateTestRule(kRuleId[1], kRedirectRuleActionType);
  TestRule redirect_rule_2 =
      CreateTestRule(kRuleId[2], kRedirectRuleActionType);
  api_rules.push_back(GetAPIRule(redirect_rule_1));
  api_rules.push_back(GetAPIRule(redirect_rule_2));

  auto signal = DeclarativeNetRequestSignal(kExtensionId[0], api_rules);
  processor_.ProcessSignal(signal);

  // Verify that processor now has some data to report.
  EXPECT_TRUE(processor_.HasDataToReportForTest());

  std::unique_ptr<SignalInfo> extension_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  const DeclarativeNetRequestInfo& dnr_info =
      extension_signal_info->declarative_net_request_info();

  EXPECT_EQ(dnr_info.rules_size(), 2);
  EXPECT_EQ(dnr_info.rules(0), redirect_rule_1.ToValue().DebugString());
  EXPECT_EQ(dnr_info.rules(1), redirect_rule_2.ToValue().DebugString());
  EXPECT_EQ(dnr_info.max_exceeded_rules_count(), static_cast<uint32_t>(0));
}

TEST_F(DeclarativeNetRequestSignalProcessorTest,
       ReportsApplicableRuleActionTypesOnly) {
  // Set up redirect, modify header, block rules.
  std::vector<extensions::api::declarative_net_request::Rule> api_rules;
  TestRule redirect_rule = CreateTestRule(kRuleId[1], kRedirectRuleActionType);
  TestRule modify_header_rule =
      CreateTestRule(kRuleId[2], kModifyHeadersRuleActionType);
  TestRule block_rule = CreateTestRule(kRuleId[3], kBlockRuleActionType);
  api_rules.push_back(GetAPIRule(redirect_rule));
  api_rules.push_back(GetAPIRule(modify_header_rule));
  api_rules.push_back(GetAPIRule(block_rule));

  auto signal = DeclarativeNetRequestSignal(kExtensionId[0], api_rules);
  processor_.ProcessSignal(signal);

  std::unique_ptr<SignalInfo> extension_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  const DeclarativeNetRequestInfo& dnr_info =
      extension_signal_info->declarative_net_request_info();

  // Verify block rule is ignored.
  EXPECT_EQ(dnr_info.rules_size(), 2);
  EXPECT_EQ(dnr_info.rules(0), modify_header_rule.ToValue().DebugString());
  EXPECT_EQ(dnr_info.rules(1), redirect_rule.ToValue().DebugString());
  EXPECT_EQ(dnr_info.max_exceeded_rules_count(), static_cast<uint32_t>(0));
}

TEST_F(DeclarativeNetRequestSignalProcessorTest, ReportsDuplicateRulesOnce) {
  // Set up 3 identical redirect rules.
  std::vector<extensions::api::declarative_net_request::Rule> api_rules;
  TestRule redirect_rule = CreateTestRule(kRuleId[1], kRedirectRuleActionType);
  api_rules.push_back(GetAPIRule(redirect_rule));
  api_rules.push_back(GetAPIRule(redirect_rule));
  api_rules.push_back(GetAPIRule(redirect_rule));

  auto signal = DeclarativeNetRequestSignal(kExtensionId[0], api_rules);
  processor_.ProcessSignal(signal);

  std::unique_ptr<SignalInfo> extension_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  const DeclarativeNetRequestInfo& dnr_info =
      extension_signal_info->declarative_net_request_info();

  // Verify identical rules are reported only once.
  EXPECT_EQ(dnr_info.rules_size(), 1);
  EXPECT_EQ(dnr_info.rules(0), redirect_rule.ToValue().DebugString());
  EXPECT_EQ(dnr_info.max_exceeded_rules_count(), static_cast<uint32_t>(0));
}

TEST_F(DeclarativeNetRequestSignalProcessorTest, ReportsMultipleExtensions) {
  TestRule redirect_rule_1 =
      CreateTestRule(kRuleId[1], kRedirectRuleActionType);
  TestRule modify_header_rule_2 =
      CreateTestRule(kRuleId[2], kModifyHeadersRuleActionType);
  TestRule redirect_rule_3 =
      CreateTestRule(kRuleId[3], kRedirectRuleActionType);
  TestRule redirect_rule_4 =
      CreateTestRule(kRuleId[4], kRedirectRuleActionType);
  TestRule modify_header_rule_5 =
      CreateTestRule(kRuleId[5], kModifyHeadersRuleActionType);

  // Process redirect_rule_1 and modify_header_2 for extension_0.
  {
    std::vector<extensions::api::declarative_net_request::Rule> api_rules;
    api_rules.push_back(GetAPIRule(redirect_rule_1));
    api_rules.push_back(GetAPIRule(modify_header_rule_2));

    auto signal = DeclarativeNetRequestSignal(kExtensionId[0], api_rules);
    processor_.ProcessSignal(signal);
  }

  // Process redirect_rule_3, redirect_rule_4, and modify_header_5 for
  // extension_1.
  {
    std::vector<extensions::api::declarative_net_request::Rule> api_rules;
    api_rules.push_back(GetAPIRule(redirect_rule_3));
    api_rules.push_back(GetAPIRule(redirect_rule_4));
    api_rules.push_back(GetAPIRule(modify_header_rule_5));

    auto signal = DeclarativeNetRequestSignal(kExtensionId[1], api_rules);
    processor_.ProcessSignal(signal);
  }

  // Retrieve and verify for extension_0.
  std::unique_ptr<SignalInfo> extension_0_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  {
    const DeclarativeNetRequestInfo& dnr_info =
        extension_0_signal_info->declarative_net_request_info();

    EXPECT_EQ(dnr_info.rules_size(), 2);
    EXPECT_EQ(dnr_info.rules(0), modify_header_rule_2.ToValue().DebugString());
    EXPECT_EQ(dnr_info.rules(1), redirect_rule_1.ToValue().DebugString());
    EXPECT_EQ(dnr_info.max_exceeded_rules_count(), static_cast<uint32_t>(0));
  }

  // Retrieve and verify for extension_1.
  std::unique_ptr<SignalInfo> extension_1_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[1]);
  {
    const DeclarativeNetRequestInfo& dnr_info =
        extension_1_signal_info->declarative_net_request_info();

    EXPECT_EQ(dnr_info.rules_size(), 3);
    EXPECT_EQ(dnr_info.rules(0), modify_header_rule_5.ToValue().DebugString());
    EXPECT_EQ(dnr_info.rules(1), redirect_rule_3.ToValue().DebugString());
    EXPECT_EQ(dnr_info.rules(2), redirect_rule_4.ToValue().DebugString());
    EXPECT_EQ(dnr_info.max_exceeded_rules_count(), static_cast<uint32_t>(0));
  }
}

TEST_F(DeclarativeNetRequestSignalProcessorTest,
       IncrementsMaxExceededRulesCount) {
  // Set max rules limit to 1 for testing.
  processor_.SetMaxRulesForTest(1);

  // Set up redirect and modify header rules.
  std::vector<extensions::api::declarative_net_request::Rule> api_rules;
  TestRule redirect_rule = CreateTestRule(kRuleId[1], kRedirectRuleActionType);
  TestRule modify_header_rule =
      CreateTestRule(kRuleId[2], kModifyHeadersRuleActionType);
  api_rules.push_back(GetAPIRule(redirect_rule));
  api_rules.push_back(GetAPIRule(modify_header_rule));

  auto signal = DeclarativeNetRequestSignal(kExtensionId[0], api_rules);
  processor_.ProcessSignal(signal);

  std::unique_ptr<SignalInfo> extension_signal_info =
      processor_.GetSignalInfoForReport(kExtensionId[0]);
  const DeclarativeNetRequestInfo& dnr_info =
      extension_signal_info->declarative_net_request_info();

  EXPECT_EQ(dnr_info.rules_size(), 1);
  EXPECT_EQ(dnr_info.rules(0), redirect_rule.ToValue().DebugString());
  EXPECT_EQ(dnr_info.max_exceeded_rules_count(), static_cast<uint32_t>(1));
}

}  // namespace

}  // namespace safe_browsing
