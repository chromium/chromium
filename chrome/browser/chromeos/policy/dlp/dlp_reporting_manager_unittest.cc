// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "components/reporting/util/status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Mock;

namespace policy {

namespace {
const char kCompanyPattern[] = "company.com";
}  // namespace

class DlpReportingManagerTest : public testing::Test {
 protected:
  DlpReportingManagerTest() = default;

  void SetUp() override {
    testing::Test::SetUp();
    SetReportQueueForReportingManager(&manager_, events_);
  }

 protected:
  DlpReportingManager manager_;
  std::vector<DlpPolicyEvent> events_;
};

TEST_F(DlpReportingManagerTest, ReportEvent) {
  manager_.ReportEvent(kCompanyPattern, DlpRulesManager::Restriction::kPrinting,
                       DlpRulesManager::Level::kBlock);

  EXPECT_EQ(events_.size(), 1);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  kCompanyPattern, DlpRulesManager::Restriction::kPrinting,
                  DlpRulesManager::Level::kBlock)));
}

TEST_F(DlpReportingManagerTest, ReportEventWithUrlDst) {
  const std::string dst_pattern = "*";
  manager_.ReportEvent(kCompanyPattern, dst_pattern,
                       DlpRulesManager::Restriction::kClipboard,
                       DlpRulesManager::Level::kBlock);

  EXPECT_EQ(events_.size(), 1);
  EXPECT_THAT(events_[0], IsDlpPolicyEvent(CreateDlpPolicyEvent(
                              kCompanyPattern, dst_pattern,
                              DlpRulesManager::Restriction::kClipboard,
                              DlpRulesManager::Level::kBlock)));
}

TEST_F(DlpReportingManagerTest, ReportEventWithComponentDst) {
  manager_.ReportEvent(kCompanyPattern, DlpRulesManager::Component::kArc,
                       DlpRulesManager::Restriction::kClipboard,
                       DlpRulesManager::Level::kBlock);

  EXPECT_EQ(events_.size(), 1);
  EXPECT_THAT(events_[0], IsDlpPolicyEvent(CreateDlpPolicyEvent(
                              kCompanyPattern, DlpRulesManager::Component::kArc,
                              DlpRulesManager::Restriction::kClipboard,
                              DlpRulesManager::Level::kBlock)));
}

TEST_F(DlpReportingManagerTest, MetricsReported) {
  base::HistogramTester histogram_tester;
  manager_.ReportEvent(kCompanyPattern, DlpRulesManager::Restriction::kPrinting,
                       DlpRulesManager::Level::kBlock);
  manager_.ReportEvent(kCompanyPattern,
                       DlpRulesManager::Restriction::kScreenshot,
                       DlpRulesManager::Level::kReport);

  EXPECT_EQ(events_.size(), 2);
  histogram_tester.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kReportedEventStatus,
      reporting::error::Code::OK, 2);
  histogram_tester.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kReportedBlockLevelRestriction,
      DlpRulesManager::Restriction::kPrinting, 1);
  histogram_tester.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kReportedReportLevelRestriction,
      DlpRulesManager::Restriction::kScreenshot, 1);
}

}  // namespace policy
