// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_impl.h"

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/test/dlp_rules_manager_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/fake_dlp_client.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"

namespace policy {

namespace {
constexpr char kUrlStr1[] = "https://wwww.example.com";

class FakeDlpRulesManager : public DlpRulesManagerImpl {
 public:
  explicit FakeDlpRulesManager(PrefService* local_state, Profile* profile)
      : DlpRulesManagerImpl(local_state, profile) {}
  ~FakeDlpRulesManager() override = default;
};

class FakeDbusErrorDlpClient : public chromeos::FakeDlpClient {
 public:
  FakeDbusErrorDlpClient() { SetFileAccessAllowed(false); }

  void SetDlpFilesPolicy(const ::dlp::SetDlpFilesPolicyRequest request,
                         SetDlpFilesPolicyCallback callback) override {
    ::dlp::SetDlpFilesPolicyResponse response;
    response.set_error_message(
        "org.freedesktop.DBus.Error.NoReply: Did not receive a reply");
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), response));
  }
};
}  // namespace

class DlpRulesPolicyTest : public InProcessBrowserTest {
 public:
  DlpRulesPolicyTest() = default;

  void InitializeRulesManager() {
    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        browser()->GetProfile(),
        base::BindRepeating(&DlpRulesPolicyTest::SetDlpRulesManager,
                            base::Unretained(this)));
    ASSERT_TRUE(DlpRulesManagerFactory::GetForPrimaryProfile());
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto new_rules_manager = std::make_unique<FakeDlpRulesManager>(
        g_browser_process->local_state(), Profile::FromBrowserContext(context));
    rules_manager_ = new_rules_manager.get();
    return new_rules_manager;
  }

  raw_ptr<DlpRulesManager, DanglingUntriaged> rules_manager_;
};

IN_PROC_BROWSER_TEST_F(DlpRulesPolicyTest, ParsePolicyPref) {
  InitializeRulesManager();

  {  // Do not remove the brackets, policy update is triggered on
     // ScopedListPrefUpdate destructor.
    ScopedListPrefUpdate update(g_browser_process->local_state(),
                                policy_prefs::kDlpRulesList);

    dlp_test_util::DlpRule rule("rule #1", "Block", "testid1");
    rule.AddSrcUrl(kUrlStr1).AddRestriction(
        data_controls::kRestrictionScreenshot, data_controls::kLevelBlock);

    update->Append(rule.Create());
  }

  EXPECT_EQ(DlpRulesManager::Level::kBlock,
            DlpRulesManagerFactory::GetForPrimaryProfile()->IsRestricted(
                GURL(kUrlStr1), DlpRulesManager::Restriction::kScreenshot));
}

IN_PROC_BROWSER_TEST_F(DlpRulesPolicyTest, ReportingEnabled) {
  g_browser_process->local_state()->SetBoolean(
      policy_prefs::kDlpReportingEnabled, true);
  InitializeRulesManager();

  DlpRulesManager* rules_manager =
      DlpRulesManagerFactory::GetForPrimaryProfile();
  EXPECT_TRUE(rules_manager->IsReportingEnabled());
  EXPECT_NE(rules_manager->GetReportingManager(), nullptr);
}

IN_PROC_BROWSER_TEST_F(DlpRulesPolicyTest, ReportingDisabled) {
  g_browser_process->local_state()->SetBoolean(
      policy_prefs::kDlpReportingEnabled, false);
  InitializeRulesManager();

  DlpRulesManager* rules_manager =
      DlpRulesManagerFactory::GetForPrimaryProfile();
  EXPECT_FALSE(rules_manager->IsReportingEnabled());
  EXPECT_EQ(rules_manager->GetReportingManager(), nullptr);
}

IN_PROC_BROWSER_TEST_F(DlpRulesPolicyTest, FilesPolicyErrorDoesNotFailOpen) {
  policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
      browser()->GetProfile(),
      base::BindRepeating(
          [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
            return nullptr;
          }));

  chromeos::DlpClient::Shutdown();
  new FakeDbusErrorDlpClient();

  InitializeRulesManager();

  {
    ScopedListPrefUpdate update(g_browser_process->local_state(),
                                policy_prefs::kDlpRulesList);

    dlp_test_util::DlpRule rule("rule #1", "Block", "testid1");
    rule.AddSrcUrl(kUrlStr1).AddDstUrl("*").AddRestriction(
        data_controls::kRestrictionFiles, data_controls::kLevelBlock);

    update->Append(rule.Create());
  }

  ASSERT_TRUE(base::test::RunUntil(
      []() { return file_access::ScopedFileAccessDelegate::HasInstance(); }));

  base::test::TestFuture<file_access::ScopedFileAccess> future;
  file_access::ScopedFileAccessDelegate::Get()->RequestFilesAccess(
      {base::FilePath("/tmp/test.txt")}, GURL(kUrlStr1), future.GetCallback());
  EXPECT_FALSE(future.Get().is_allowed());
}

}  // namespace policy
