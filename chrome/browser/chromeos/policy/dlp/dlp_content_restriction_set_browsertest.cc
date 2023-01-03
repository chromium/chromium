// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_restriction_set.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_impl.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"

namespace policy {

namespace {

class FakeDlpRulesManager : public DlpRulesManagerImpl {
 public:
  explicit FakeDlpRulesManager(PrefService* local_state)
      : DlpRulesManagerImpl(local_state) {}
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
        g_browser_process->local_state());
  }
};

IN_PROC_BROWSER_TEST_F(DlpContentRestrictionSetBrowserTest,
                       GetRestrictionSetForURL) {
  {
    ScopedListPrefUpdate update(g_browser_process->local_state(),
                                policy_prefs::kDlpRulesList);

    base::Value::List src_urls1;
    src_urls1.Append(kUrl1);
    base::Value::List restrictions1;
    restrictions1.Append(dlp_test_util::CreateRestrictionWithLevel(
        dlp::kScreenshotRestriction, dlp::kBlockLevel));
    update->Append(dlp_test_util::CreateRule(
        "rule #1", "Block", std::move(src_urls1),
        /*dst_urls=*/base::Value::List(),
        /*dst_components=*/base::Value::List(), std::move(restrictions1)));

    base::Value::List src_urls2;
    src_urls2.Append(kUrl2);
    base::Value::List restrictions2;
    restrictions2.Append(dlp_test_util::CreateRestrictionWithLevel(
        dlp::kPrivacyScreenRestriction, dlp::kBlockLevel));
    update->Append(dlp_test_util::CreateRule(
        "rule #2", "Block", std::move(src_urls2),
        /*dst_urls=*/base::Value::List(),
        /*dst_components=*/base::Value::List(), std::move(restrictions2)));

    base::Value::List src_urls3;
    src_urls3.Append(kUrl3);
    base::Value::List restrictions3;
    restrictions3.Append(dlp_test_util::CreateRestrictionWithLevel(
        dlp::kPrintingRestriction, dlp::kBlockLevel));
    update->Append(dlp_test_util::CreateRule(
        "rule #3", "Block", std::move(src_urls3),
        /*dst_urls=*/base::Value::List(),
        /*dst_components=*/base::Value::List(), std::move(restrictions3)));

    base::Value::List src_urls4;
    src_urls4.Append(kUrl4);
    base::Value::List restrictions4;
    restrictions4.Append(dlp_test_util::CreateRestrictionWithLevel(
        dlp::kScreenShareRestriction, dlp::kBlockLevel));
    update->Append(dlp_test_util::CreateRule(
        "rule #4", "Block", std::move(src_urls4),
        /*dst_urls=*/base::Value::List(),
        /*dst_components=*/base::Value::List(), std::move(restrictions4)));
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
