// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/site_policy.h"

#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "components/safe_browsing/core/browser/db/fake_database_manager.h"
#include "components/safe_browsing/core/common/features.h"
#endif

namespace actor {

namespace {

class ActorSitePolicyBrowserTest : public InProcessBrowserTest {
 public:
  ActorSitePolicyBrowserTest() {
    base::FieldTrialParams params;
    params["allowlist"] = "a.com,b.com";
    params["allowlist_only"] = "false";
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kGlicActionAllowlist, std::move(params));
  }

  ~ActorSitePolicyBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

 protected:
  void CheckUrl(const GURL& url, bool expected_allowed) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    base::test::TestFuture<bool> allowed;
    MayActOnTab(*browser()->tab_strip_model()->GetActiveTab(),
                allowed.GetCallback());
    // The result should not be provided synchronously.
    EXPECT_FALSE(allowed.IsReady());
    EXPECT_EQ(expected_allowed, allowed.Get());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorSitePolicyBrowserTest, Basic) {
  const GURL allowed_url =
      embedded_https_test_server().GetURL("a.com", "/title1.html");
  CheckUrl(allowed_url, true);
}

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)

class ActorSitePolicySafeBrowsingBrowserTest
    : public ActorSitePolicyBrowserTest {
 public:
  ActorSitePolicySafeBrowsingBrowserTest() = default;
  ~ActorSitePolicySafeBrowsingBrowserTest() override = default;

 protected:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    fake_safe_browsing_database_manager_ =
        base::MakeRefCounted<safe_browsing::FakeSafeBrowsingDatabaseManager>(
            content::GetUIThreadTaskRunner({}));
    safe_browsing_factory_.SetTestDatabaseManager(
        fake_safe_browsing_database_manager_.get());
    safe_browsing::SafeBrowsingService::RegisterFactory(
        &safe_browsing_factory_);
    ActorSitePolicyBrowserTest::CreatedBrowserMainParts(browser_main_parts);
  }

  void TearDown() override {
    safe_browsing::SafeBrowsingService::RegisterFactory(nullptr);
    ActorSitePolicyBrowserTest::TearDown();
  }

  void AddDangerousUrl(const GURL& dangerous_url) {
    fake_safe_browsing_database_manager_->AddDangerousUrl(
        dangerous_url, safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_MALWARE);
  }

  void AddPhishingUrl(const GURL& phishing_url) {
    fake_safe_browsing_database_manager_->AddDangerousUrl(
        phishing_url, safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING);
  }

 private:
  scoped_refptr<safe_browsing::FakeSafeBrowsingDatabaseManager>
      fake_safe_browsing_database_manager_;
  safe_browsing::TestSafeBrowsingServiceFactory safe_browsing_factory_;
};

class ActorSitePolicyDelayedWarningBrowserTest
    : public ActorSitePolicySafeBrowsingBrowserTest {
 public:
  ActorSitePolicyDelayedWarningBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(safe_browsing::kDelayedWarnings);
  }
  ~ActorSitePolicyDelayedWarningBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorSitePolicySafeBrowsingBrowserTest,
                       BlockDangerousSite) {
  const GURL dangerous_url =
      embedded_https_test_server().GetURL("c.com", "/title1.html");
  AddDangerousUrl(dangerous_url);
  CheckUrl(dangerous_url, false);
}

IN_PROC_BROWSER_TEST_F(ActorSitePolicyDelayedWarningBrowserTest,
                       BlockPhishingSiteWithDelayedWarning) {
  const GURL phishing_url =
      embedded_https_test_server().GetURL("c.com", "/title1.html");
  AddPhishingUrl(phishing_url);
  CheckUrl(phishing_url, false);
}

#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

}  // namespace

}  // namespace actor
