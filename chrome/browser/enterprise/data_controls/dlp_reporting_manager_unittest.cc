// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/policy/messaging_layer/public/report_client.h"
#include "chrome/browser/policy/messaging_layer/public/report_client_test_util.h"
#include "components/account_id/account_id.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "components/enterprise/data_controls/core/browser/dlp_policy_event.pb.h"
#include "components/enterprise/data_controls/core/browser/rule.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/encryption/primitives.h"
#include "components/reporting/storage/test_storage_module.h"
#include "components/reporting/util/status.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_init_params.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

using ::testing::_;
using ::testing::Mock;

namespace data_controls {

namespace {
const char kCompanyUrl[] = "company.com";
const char kFilename[] = "example.txt";
const char kRuleName[] = "ruleName";
const char kRuleId[] = "obfuscatedId";
}  // namespace

class DlpReportingManagerTest : public testing::Test {
 protected:
  DlpReportingManagerTest() = default;

  void SetUp() override {
    testing::Test::SetUp();
    test_reporting_ = ::reporting::ReportingClient::TestEnvironment::
        CreateWithStorageModule();
    // In tests Manager can only be created after TestEnvironment.
    manager_ = std::make_unique<DlpReportingManager>();
    SetReportQueueForReportingManager(
        manager_.get(), events_,
        base::ThreadPool::CreateSequencedTaskRunner({}));
  }

 protected:
#if BUILDFLAG(IS_CHROMEOS)
  void ReportEventAndCheckComponent(
      Component rule_component,
      DlpPolicyEventDestination_Component event_component,
      unsigned int event_number) {
    manager_->ReportEvent(kCompanyUrl, rule_component,
                          Rule::Restriction::kClipboard, Rule::Level::kBlock,
                          kRuleName, kRuleId);

    ASSERT_EQ(events_.size(), event_number + 1);
    EXPECT_EQ(events_[event_number].source().url(), kCompanyUrl);
    EXPECT_FALSE(events_[event_number].destination().has_url());
    EXPECT_EQ(events_[event_number].destination().component(), event_component);
    EXPECT_EQ(events_[event_number].restriction(),
              DlpPolicyEvent_Restriction_CLIPBOARD);
    EXPECT_EQ(events_[event_number].mode(), DlpPolicyEvent_Mode_BLOCK);
    EXPECT_EQ(events_[event_number].triggered_rule_name(), kRuleName);
    EXPECT_EQ(events_[event_number].triggered_rule_id(), kRuleId);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void ReportEventAndCheckUser(user_manager::UserManager* user_manager,
                               const AccountId& account_id,
                               const user_manager::User* user,
                               DlpPolicyEvent_UserType DlpUserType,
                               unsigned int event_number,
                               bool is_child = false) {
    user_manager->UserLoggedIn(account_id, user->username_hash(),
                               /*browser_restart=*/false, is_child);
    manager_->ReportEvent(kCompanyUrl, Rule::Restriction::kPrinting,
                          Rule::Level::kBlock, kRuleName, kRuleId);
    ASSERT_EQ(events_.size(), event_number + 1);
    EXPECT_EQ(events_[event_number].user_type(), DlpUserType);
    user_manager->RemoveUserFromList(account_id);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)

  void SetSessionType(crosapi::mojom::SessionType session_type) {
    auto init_params = crosapi::mojom::BrowserInitParams::New();
    init_params->session_type = session_type;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
  }

  void ReportEventAndCheckUser(DlpPolicyEvent_UserType dlp_user_type,
                               unsigned int event_number) {
    manager_->ReportEvent(kCompanyUrl, Rule::Restriction::kPrinting,
                          Rule::Level::kBlock, kRuleName, kRuleId);
    ASSERT_EQ(events_.size(), event_number + 1);
    EXPECT_EQ(events_[event_number].user_type(), dlp_user_type);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<::reporting::ReportingClient::TestEnvironment>
      test_reporting_;
  std::unique_ptr<DlpReportingManager> manager_;
  std::vector<DlpPolicyEvent> events_;
  base::ScopedTempDir location_;
  uint8_t signature_verification_public_key_[reporting::kKeySize];
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService lacros_service_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

TEST_F(DlpReportingManagerTest, ReportEvent) {
  manager_->ReportEvent(kCompanyUrl, Rule::Restriction::kPrinting,
                        Rule::Level::kBlock, kRuleName, kRuleId);

  EXPECT_EQ(manager_->events_reported(), 1u);
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_EQ(events_[0].source().url(), kCompanyUrl);
  EXPECT_FALSE(events_[0].has_destination());
  EXPECT_EQ(events_[0].restriction(), DlpPolicyEvent_Restriction_PRINTING);
  EXPECT_EQ(events_[0].mode(), DlpPolicyEvent_Mode_BLOCK);
  EXPECT_EQ(events_[0].triggered_rule_name(), kRuleName);
  EXPECT_EQ(events_[0].triggered_rule_id(), kRuleId);
}

TEST_F(DlpReportingManagerTest, ReportEventWithUrlDst) {
  const std::string dst_url = "*";
  manager_->ReportEvent(kCompanyUrl, dst_url, Rule::Restriction::kClipboard,
                        Rule::Level::kBlock, kRuleName, kRuleId);

  EXPECT_EQ(manager_->events_reported(), 1u);
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_EQ(events_[0].source().url(), kCompanyUrl);
  EXPECT_EQ(events_[0].destination().url(), dst_url);
  EXPECT_FALSE(events_[0].destination().has_component());
  EXPECT_EQ(events_[0].restriction(), DlpPolicyEvent_Restriction_CLIPBOARD);
  EXPECT_EQ(events_[0].mode(), DlpPolicyEvent_Mode_BLOCK);
  EXPECT_EQ(events_[0].triggered_rule_name(), kRuleName);
  EXPECT_EQ(events_[0].triggered_rule_id(), kRuleId);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(DlpReportingManagerTest, ReportEventWithComponentDst) {
  ReportEventAndCheckComponent(Component::kArc,
                               DlpPolicyEventDestination_Component_ARC, 0u);
  ReportEventAndCheckComponent(Component::kCrostini,
                               DlpPolicyEventDestination_Component_CROSTINI,
                               1u);
  ReportEventAndCheckComponent(Component::kPluginVm,
                               DlpPolicyEventDestination_Component_PLUGIN_VM,
                               2u);
  ReportEventAndCheckComponent(Component::kUsb,
                               DlpPolicyEventDestination_Component_USB, 3u);
  ReportEventAndCheckComponent(Component::kDrive,
                               DlpPolicyEventDestination_Component_DRIVE, 4u);
  ReportEventAndCheckComponent(Component::kOneDrive,
                               DlpPolicyEventDestination_Component_ONEDRIVE,
                               5u);
  ReportEventAndCheckComponent(
      Component::kUnknownComponent,
      DlpPolicyEventDestination_Component_UNDEFINED_COMPONENT, 6u);
  EXPECT_EQ(manager_->events_reported(), 7u);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(DlpReportingManagerTest, ReportEventWithoutNameAndRuleId) {
  manager_->ReportEvent(kCompanyUrl, Rule::Restriction::kPrinting,
                        Rule::Level::kBlock, std::string(), std::string());

  EXPECT_EQ(manager_->events_reported(), 1u);
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_EQ(events_[0].source().url(), kCompanyUrl);
  EXPECT_FALSE(events_[0].has_destination());
  EXPECT_EQ(events_[0].restriction(), DlpPolicyEvent_Restriction_PRINTING);
  EXPECT_EQ(events_[0].mode(), DlpPolicyEvent_Mode_BLOCK);
  EXPECT_FALSE(events_[0].has_triggered_rule_name());
  EXPECT_FALSE(events_[0].has_triggered_rule_id());
}

TEST_F(DlpReportingManagerTest, MetricsReported) {
  base::HistogramTester histogram_tester;
  manager_->ReportEvent(kCompanyUrl, Rule::Restriction::kPrinting,
                        Rule::Level::kBlock, kRuleName, kRuleId);
  manager_->ReportEvent(kCompanyUrl, Rule::Restriction::kScreenshot,
                        Rule::Level::kReport, kRuleName, kRuleId);
  manager_->ReportEvent(kCompanyUrl, Rule::Restriction::kUnknownRestriction,
                        Rule::Level::kWarn, kRuleName, kRuleId);

  EXPECT_EQ(manager_->events_reported(), 3u);
  EXPECT_EQ(events_.size(), 3u);
  histogram_tester.ExpectUniqueSample(
      GetDlpHistogramPrefix() +
          dlp::kReportedEventStatus,
      reporting::error::Code::OK, 3);
  histogram_tester.ExpectUniqueSample(
      GetDlpHistogramPrefix() +
          dlp::kReportedBlockLevelRestriction,
      Rule::Restriction::kPrinting, 1);
  histogram_tester.ExpectUniqueSample(
      GetDlpHistogramPrefix() +
          dlp::kReportedReportLevelRestriction,
      Rule::Restriction::kScreenshot, 1);
  histogram_tester.ExpectUniqueSample(
      GetDlpHistogramPrefix() +
          dlp::kReportedWarnLevelRestriction,
      Rule::Restriction::kUnknownRestriction, 1);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(DlpReportingManagerTest, UserType) {
  ScopedTestingLocalState local_state{TestingBrowserProcess::GetGlobal()};
  auto* user_manager = new ash::FakeChromeUserManager();
  user_manager::ScopedUserManager enabler(base::WrapUnique(user_manager));

  AccountId regular_account_id = AccountId::FromUserEmail("user@example.com");
  const auto* regular_user = user_manager->AddUser(regular_account_id);
  AccountId mgs_account_id =
      AccountId::FromUserEmail("managed-guest-session@example.com");
  const auto* mgs_user = user_manager->AddPublicAccountUser(mgs_account_id);
  AccountId kiosk_account_id = AccountId::FromUserEmail("kiosk@example.com");
  const auto* kiosk_user = user_manager->AddKioskAppUser(kiosk_account_id);
  AccountId web_kiosk_account_id =
      AccountId::FromUserEmail("web-kiosk@example.com");
  const auto* web_kiosk_user =
      user_manager->AddWebKioskAppUser(web_kiosk_account_id);
  AccountId guest_user_id = user_manager::GuestAccountId();
  const auto* guest_user = user_manager->AddGuestUser();
  AccountId child_user_id = AccountId::FromUserEmail("child@example.com");
  const auto* child_user = user_manager->AddChildUser(child_user_id);

  ReportEventAndCheckUser(user_manager, regular_account_id, regular_user,
                          DlpPolicyEvent_UserType_REGULAR, 0u);
  ReportEventAndCheckUser(user_manager, mgs_account_id, mgs_user,
                          DlpPolicyEvent_UserType_MANAGED_GUEST, 1u);
  ReportEventAndCheckUser(user_manager, kiosk_account_id, kiosk_user,
                          DlpPolicyEvent_UserType_KIOSK, 2u);
  ReportEventAndCheckUser(user_manager, web_kiosk_account_id, web_kiosk_user,
                          DlpPolicyEvent_UserType_KIOSK, 3u);
  ReportEventAndCheckUser(user_manager, guest_user_id, guest_user,
                          DlpPolicyEvent_UserType_UNDEFINED_USER_TYPE, 4u);
  ReportEventAndCheckUser(user_manager, child_user_id, child_user,
                          DlpPolicyEvent_UserType_UNDEFINED_USER_TYPE, 5u,
                          true);
  EXPECT_EQ(manager_->events_reported(), 6u);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(DlpReportingManagerTest, UserType) {
  SetSessionType(crosapi::mojom::SessionType::kRegularSession);
  ReportEventAndCheckUser(DlpPolicyEvent_UserType_REGULAR, 0u);

  SetSessionType(crosapi::mojom::SessionType::kPublicSession);
  ReportEventAndCheckUser(DlpPolicyEvent_UserType_MANAGED_GUEST, 1u);

  SetSessionType(crosapi::mojom::SessionType::kAppKioskSession);
  ReportEventAndCheckUser(DlpPolicyEvent_UserType_KIOSK, 2u);

  SetSessionType(crosapi::mojom::SessionType::kWebKioskSession);
  ReportEventAndCheckUser(DlpPolicyEvent_UserType_KIOSK, 3u);

  SetSessionType(crosapi::mojom::SessionType::kGuestSession);
  ReportEventAndCheckUser(DlpPolicyEvent_UserType_UNDEFINED_USER_TYPE, 4u);

  SetSessionType(crosapi::mojom::SessionType::kChildSession);
  ReportEventAndCheckUser(DlpPolicyEvent_UserType_UNDEFINED_USER_TYPE, 5u);

  EXPECT_EQ(manager_->events_reported(), 6u);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_F(DlpReportingManagerTest, CreateEventWithUnknownRestriction) {
  DlpPolicyEvent event =
      CreateDlpPolicyEvent(kCompanyUrl, Rule::Restriction::kUnknownRestriction,
                           kRuleName, kRuleId, Rule::Level::kNotSet);
  EXPECT_EQ(event.source().url(), kCompanyUrl);
  EXPECT_FALSE(event.has_destination());
  EXPECT_EQ(event.restriction(),
            DlpPolicyEvent_Restriction_UNDEFINED_RESTRICTION);
  EXPECT_EQ(event.mode(), DlpPolicyEvent_Mode_UNDEFINED_MODE);
}

TEST_F(DlpReportingManagerTest, CreateEventForFilesRestriction) {
  auto event_builder = DlpPolicyEventBuilder::Event(
      kCompanyUrl, kRuleName, kRuleId, Rule::Restriction::kFiles,
      Rule::Level::kAllow);
  event_builder->SetContentName(kFilename);

  DlpPolicyEvent event = event_builder->Create();

  EXPECT_EQ(event.source().url(), kCompanyUrl);
  EXPECT_FALSE(event.has_destination());
  EXPECT_EQ(event.restriction(), DlpPolicyEvent_Restriction_FILES);
  EXPECT_EQ(event.mode(), DlpPolicyEvent_Mode_UNDEFINED_MODE);
  EXPECT_EQ(event.content_name(), kFilename);
  EXPECT_EQ(event.triggered_rule_name(), kRuleName);
  EXPECT_EQ(event.triggered_rule_id(), kRuleId);
}

TEST_F(DlpReportingManagerTest, CreateEventWithEmptyRuleMetadata) {
  auto event_builder = DlpPolicyEventBuilder::Event(
      kCompanyUrl, std::string(), std::string(), Rule::Restriction::kPrinting,
      Rule::Level::kBlock);

  DlpPolicyEvent event = event_builder->Create();
  EXPECT_EQ(event.source().url(), kCompanyUrl);
  EXPECT_EQ(event.mode(), DlpPolicyEvent_Mode_BLOCK);
  EXPECT_FALSE(event.has_triggered_rule_name());
  EXPECT_FALSE(event.has_triggered_rule_id());
}

TEST_F(DlpReportingManagerTest, Timestamp) {
  const base::Time lower_bound = base::Time::Now();

  DlpPolicyEvent event =
      CreateDlpPolicyEvent(kCompanyUrl, Rule::Restriction::kPrinting, kRuleName,
                           kRuleId, Rule::Level::kBlock);

  ASSERT_TRUE(event.has_timestamp_micro());
  const base::TimeDelta time_since_epoch =
      base::Microseconds(event.timestamp_micro());
  const base::Time upper_bound = base::Time::Now();

  EXPECT_GE(base::Time::UnixEpoch() + time_since_epoch, lower_bound);
  EXPECT_LE(base::Time::UnixEpoch() + time_since_epoch, upper_bound);
}

TEST_F(DlpReportingManagerTest, ReportEventError) {
  auto report_queue =
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr, base::OnTaskRunnerDeleter(
                       base::ThreadPool::CreateSequencedTaskRunner({})));
  manager_->SetReportQueueForTest(std::move(report_queue));

  manager_->ReportEvent(kCompanyUrl, Rule::Restriction::kPrinting,
                        Rule::Level::kBlock, kRuleName, kRuleId);
  EXPECT_EQ(manager_->events_reported(), 0u);
  EXPECT_EQ(events_.size(), 0u);
}

TEST_F(DlpReportingManagerTest, OnEventEnqueuedError) {
  base::HistogramTester histogram_tester;

  auto report_queue =
      std::unique_ptr<::reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
          new ::reporting::MockReportQueue(),
          base::OnTaskRunnerDeleter(
              base::ThreadPool::CreateSequencedTaskRunner({})));

  EXPECT_CALL(*report_queue.get(), AddRecord)
      .WillRepeatedly(testing::WithArgs<2>(
          [](::reporting::ReportQueue::EnqueueCallback callback) {
            std::move(callback).Run(
                ::reporting::Status(::reporting::error::UNKNOWN, "mock"));
          }));

  manager_->SetReportQueueForTest(std::move(report_queue));

  manager_->ReportEvent(kCompanyUrl, Rule::Restriction::kPrinting,
                        Rule::Level::kBlock, kRuleName, kRuleId);

  EXPECT_EQ(manager_->events_reported(), 1u);
  EXPECT_EQ(events_.size(), 0u);
  histogram_tester.ExpectUniqueSample(GetDlpHistogramPrefix() + dlp::kReportedEventStatus,
      reporting::error::UNKNOWN, 1);
}

TEST_F(DlpReportingManagerTest, ReportLongEvent) {
  const std::string rand_source_url = base::RandBytesAsString(64 * 1024);
  const std::string rand_destination_url = base::RandBytesAsString(64 * 1024);

  manager_->ReportEvent(rand_source_url, rand_destination_url,
                        Rule::Restriction::kPrinting, Rule::Level::kBlock,
                        kRuleName, kRuleId);

  EXPECT_EQ(manager_->events_reported(), 1u);
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_NE(events_[0].source().url(), rand_source_url);
  EXPECT_EQ(events_[0].source().url().length(), 32u * 1024u);
  EXPECT_NE(events_[0].destination().url(), rand_destination_url);
  EXPECT_EQ(events_[0].destination().url().length(), 32u * 1024u);
  EXPECT_EQ(events_[0].restriction(), DlpPolicyEvent_Restriction_PRINTING);
  EXPECT_EQ(events_[0].mode(), DlpPolicyEvent_Mode_BLOCK);
  EXPECT_EQ(events_[0].triggered_rule_name(), kRuleName);
  EXPECT_EQ(events_[0].triggered_rule_id(), kRuleId);
}

}  // namespace data_controls
