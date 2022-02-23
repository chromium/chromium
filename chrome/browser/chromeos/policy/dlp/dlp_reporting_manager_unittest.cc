// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "components/account_id/account_id.h"
#include "components/reporting/util/status.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
    SetReportQueueForReportingManager(
        &manager_, events_, base::ThreadPool::CreateSequencedTaskRunner({}));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  DlpReportingManager manager_;
  std::vector<DlpPolicyEvent> events_;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService lacros_service_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

TEST_F(DlpReportingManagerTest, ReportEvent) {
  manager_.ReportEvent(kCompanyPattern, DlpRulesManager::Restriction::kPrinting,
                       DlpRulesManager::Level::kBlock);

  EXPECT_EQ(events_.size(), 1u);
  EXPECT_EQ(events_[0].source().url(), kCompanyPattern);
  EXPECT_FALSE(events_[0].has_destination());
  EXPECT_EQ(events_[0].restriction(), DlpPolicyEvent_Restriction_PRINTING);
  EXPECT_EQ(events_[0].mode(), DlpPolicyEvent_Mode_BLOCK);
}

TEST_F(DlpReportingManagerTest, ReportEventWithUrlDst) {
  const std::string dst_pattern = "*";
  manager_.ReportEvent(kCompanyPattern, dst_pattern,
                       DlpRulesManager::Restriction::kClipboard,
                       DlpRulesManager::Level::kBlock);

  EXPECT_EQ(events_.size(), 1u);
  EXPECT_EQ(events_[0].source().url(), kCompanyPattern);
  EXPECT_EQ(events_[0].destination().url(), dst_pattern);
  EXPECT_FALSE(events_[0].destination().has_component());
  EXPECT_EQ(events_[0].restriction(), DlpPolicyEvent_Restriction_CLIPBOARD);
  EXPECT_EQ(events_[0].mode(), DlpPolicyEvent_Mode_BLOCK);
}

TEST_F(DlpReportingManagerTest, ReportEventWithComponentDst) {
  manager_.ReportEvent(kCompanyPattern, DlpRulesManager::Component::kArc,
                       DlpRulesManager::Restriction::kClipboard,
                       DlpRulesManager::Level::kBlock);

  EXPECT_EQ(events_.size(), 1u);
  EXPECT_EQ(events_[0].source().url(), kCompanyPattern);
  EXPECT_FALSE(events_[0].destination().has_url());
  EXPECT_EQ(events_[0].destination().component(),
            DlpPolicyEventDestination_Component_ARC);
  EXPECT_EQ(events_[0].restriction(), DlpPolicyEvent_Restriction_CLIPBOARD);
  EXPECT_EQ(events_[0].mode(), DlpPolicyEvent_Mode_BLOCK);
}

TEST_F(DlpReportingManagerTest, MetricsReported) {
  base::HistogramTester histogram_tester;
  manager_.ReportEvent(kCompanyPattern, DlpRulesManager::Restriction::kPrinting,
                       DlpRulesManager::Level::kBlock);
  manager_.ReportEvent(kCompanyPattern,
                       DlpRulesManager::Restriction::kScreenshot,
                       DlpRulesManager::Level::kReport);

  EXPECT_EQ(events_.size(), 2u);
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

// TODO(crbug.com/1262948): Enable and modify for lacros.
#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(DlpReportingManagerTest, UserType) {
  auto* user_manager = new ash::FakeChromeUserManager();
  user_manager::ScopedUserManager enabler(base::WrapUnique(user_manager));

  AccountId regular_account_id = AccountId::FromUserEmail("user@example.com");
  const auto* regular_user = user_manager->AddUser(regular_account_id);
  AccountId mgs_account_id =
      AccountId::FromUserEmail("managed-guest-session@example.com");
  const auto* mgs_user = user_manager->AddPublicAccountUser(mgs_account_id);
  AccountId kiosk_account_id = AccountId::FromUserEmail("kiosk@example.com");
  const auto* kiosk_user = user_manager->AddKioskAppUser(kiosk_account_id);

  user_manager->UserLoggedIn(regular_account_id, regular_user->username_hash(),
                             /*browser_restart=*/false, /*is_child=*/false);
  manager_.ReportEvent(kCompanyPattern, DlpRulesManager::Restriction::kPrinting,
                       DlpRulesManager::Level::kBlock);
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_EQ(events_[0].user_type(), DlpPolicyEvent_UserType_REGULAR);
  user_manager->RemoveUserFromList(regular_account_id);

  user_manager->UserLoggedIn(mgs_account_id, mgs_user->username_hash(),
                             /*browser_restart=*/false, /*is_child=*/false);
  manager_.ReportEvent(kCompanyPattern, DlpRulesManager::Restriction::kPrinting,
                       DlpRulesManager::Level::kBlock);
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_EQ(events_[1].user_type(), DlpPolicyEvent_UserType_MANAGED_GUEST);
  user_manager->RemoveUserFromList(mgs_account_id);

  user_manager->UserLoggedIn(kiosk_account_id, kiosk_user->username_hash(),
                             /*browser_restart=*/false, /*is_child=*/false);
  manager_.ReportEvent(kCompanyPattern, DlpRulesManager::Restriction::kPrinting,
                       DlpRulesManager::Level::kBlock);
  EXPECT_EQ(events_.size(), 3u);
  EXPECT_EQ(events_[2].user_type(), DlpPolicyEvent_UserType_KIOSK);
  user_manager->RemoveUserFromList(kiosk_account_id);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace policy
