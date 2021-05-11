// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"

#include <memory>

#include "chrome/browser/chromeos/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/reporting/client/mock_report_queue.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
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
    profile_ = std::make_unique<TestingProfile>();
    SetReportQueueForReportingManager(&manager_, events_);
  }

  std::unique_ptr<content::WebContents> CreateWebContents() const {
    return content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                             nullptr);
  }

 protected:
  // BrowserTaskEnvironment needs to be destroyed before TestEnvironment
  // so that tasks on other threads don't run after they are destroyed.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
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

}  // namespace policy
