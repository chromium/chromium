// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_impl.h"
#include "chrome/browser/chromeos/policy/dlp/test/dlp_rules_manager_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/enterprise/data_controls/core/browser/component.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"

namespace policy {

namespace {

class FakeDlpRulesManager : public DlpRulesManagerImpl {
 public:
  explicit FakeDlpRulesManager(PrefService* local_state, Profile* profile)
      : DlpRulesManagerImpl(local_state, profile) {}
  ~FakeDlpRulesManager() override = default;
};

}  // namespace

const DlpContentRestrictionSet kScreenshotRestricted(
    DlpContentRestriction::kScreenshot,
    DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kPrivacyScreenEnforced(
    DlpContentRestriction::kPrivacyScreen,
    DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kPrintRestricted(DlpContentRestriction::kPrint,
                                                DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kScreenShareRestricted(
    DlpContentRestriction::kScreenShare,
    DlpRulesManager::Level::kBlock);

constexpr char kExampleUrl[] = "https://example.com";
constexpr char kUrl1[] = "https://example1.com";
constexpr char kUrl2[] = "https://example2.com";
constexpr char kUrl3[] = "https://example3.com";
constexpr char kUrl4[] = "https://example4.com";

class DlpContentRestrictionSetBrowserTest : public InProcessBrowserTest {
 public:
  DlpContentRestrictionSetBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(
            &DlpContentRestrictionSetBrowserTest::SetDlpRulesManager,
            base::Unretained(this)));
    ASSERT_TRUE(DlpRulesManagerFactory::GetForPrimaryProfile());
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    return std::make_unique<FakeDlpRulesManager>(
        g_browser_process->local_state(), Profile::FromBrowserContext(context));
  }
};

IN_PROC_BROWSER_TEST_F(DlpContentRestrictionSetBrowserTest,
                       GetRestrictionSetForURL) {
  {  // Do not remove the brackets, policy update is triggered on
     // ScopedListPrefUpdate destructor.
    ScopedListPrefUpdate update(g_browser_process->local_state(),
                                policy_prefs::kDlpRulesList);

    dlp_test_util::DlpRule rule1("rule #1", "Block", "testid1");
    rule1.AddSrcUrl(kUrl1).AddRestriction(data_controls::kRestrictionScreenshot,
                                          data_controls::kLevelBlock);
    update->Append(rule1.Create());

    dlp_test_util::DlpRule rule2("rule #2", "Block", "testid2");
    rule2.AddSrcUrl(kUrl2).AddRestriction(
        data_controls::kRestrictionPrivacyScreen, data_controls::kLevelBlock);
    update->Append(rule2.Create());

    dlp_test_util::DlpRule rule3("rule #3", "Block", "testid3");
    rule3.AddSrcUrl(kUrl3).AddRestriction(data_controls::kRestrictionPrinting,
                                          data_controls::kLevelBlock);
    update->Append(rule3.Create());

    dlp_test_util::DlpRule rule4("rule #4", "Block", "testid4");
    rule4.AddSrcUrl(kUrl4).AddRestriction(
        data_controls::kRestrictionScreenShare, data_controls::kLevelBlock);
    update->Append(rule4.Create());
  }

  EXPECT_EQ(kScreenshotRestricted,
            DlpContentRestrictionSet::GetForURL(GURL(kUrl1)));
  EXPECT_EQ(kPrivacyScreenEnforced,
            DlpContentRestrictionSet::GetForURL(GURL(kUrl2)));
  EXPECT_EQ(kPrintRestricted, DlpContentRestrictionSet::GetForURL(GURL(kUrl3)));
  EXPECT_EQ(kScreenShareRestricted,
            DlpContentRestrictionSet::GetForURL(GURL(kUrl4)));
  EXPECT_EQ(DlpContentRestrictionSet(),
            DlpContentRestrictionSet::GetForURL(GURL(kExampleUrl)));
}

}  // namespace policy
