// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/site_policy.h"

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_switches.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/lookalikes/lookalike_test_helper.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/core/filters/optimization_hints_component_update_listener.h"
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
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#endif

namespace actor {

namespace {

// Hosts that will trigger lookalike warnings. One causes an interstitial and
// the other only a safety tip.
constexpr char kLookalikeHostInterstitial[] = "google.com.example.com";
constexpr char kLookalikeHostWarning[] = "accounts-google.com";

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)

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

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    SetUpBlocklist(command_line, "bar.com");
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    LookalikeTestHelper::SetUpLookalikeTestParams();
    embedded_https_test_server().SetCertHostnames(
        {"a.com", "b.com", "c.com", "bar.com", "*.bar.com",
         kLookalikeHostInterstitial, kLookalikeHostWarning});
    ASSERT_TRUE(embedded_https_test_server().Start());

    // Optimization guide uses this histogram to signal initialization in tests.
    optimization_guide::RetryForHistogramUntilCountReached(
        &histogram_tester_for_init_,
        "OptimizationGuide.HintsManager.HintCacheInitialized", 1);

    InitActionBlocklist(browser()->profile());

    // Simulate the component loading, as the implementation checks it, but the
    // actual list is set via the command line.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    optimization_guide::OptimizationHintsComponentUpdateListener::GetInstance()
        ->MaybeUpdateHintsComponent(
            {base::Version("123"),
             temp_dir_.GetPath().Append(FILE_PATH_LITERAL("dont_care"))});
  }

  void TearDownOnMainThread() override {
    LookalikeTestHelper::TearDownLookalikeTestParams();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  void CheckUrl(const GURL& url, bool expected_allowed) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    base::test::TestFuture<MayActOnUrlBlockReason> allowed;
    auto* actor_service = ActorKeyedService::Get(browser()->profile());
    MayActOnTab(*browser()->tab_strip_model()->GetActiveTab(),
                actor_service->GetJournal(), TaskId(),
                absl::flat_hash_set<url::Origin>(), allowed.GetCallback());
    // The result should not be provided synchronously.
    EXPECT_FALSE(allowed.IsReady());
    EXPECT_EQ(expected_allowed,
              allowed.Get() == MayActOnUrlBlockReason::kAllowed);
  }

 private:
  base::HistogramTester histogram_tester_for_init_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(ActorSitePolicyBrowserTest, Basic) {
  const GURL allowed_url =
      embedded_https_test_server().GetURL("a.com", "/title1.html");
  CheckUrl(allowed_url, true);
}

IN_PROC_BROWSER_TEST_F(ActorSitePolicyBrowserTest, AllowIfNotInBlocklist) {
  const GURL allowed_url =
      embedded_https_test_server().GetURL("c.com", "/title1.html");
  CheckUrl(allowed_url, true);
}

IN_PROC_BROWSER_TEST_F(ActorSitePolicyBrowserTest, BlockIfInBlocklist) {
  const GURL blocked_url =
      embedded_https_test_server().GetURL("bar.com", "/title1.html");
  CheckUrl(blocked_url, false);
}

IN_PROC_BROWSER_TEST_F(ActorSitePolicyBrowserTest,
                       BlockSubdomainIfInBlocklist) {
  const GURL blocked_url =
      embedded_https_test_server().GetURL("sub.bar.com", "/title1.html");
  CheckUrl(blocked_url, false);
}

IN_PROC_BROWSER_TEST_F(ActorSitePolicyBrowserTest, BlockLookalikes) {
  const GURL lookalike_url = embedded_https_test_server().GetURL(
      kLookalikeHostInterstitial, "/title1.html");
  CheckUrl(lookalike_url, false);
}

IN_PROC_BROWSER_TEST_F(ActorSitePolicyBrowserTest,
                       TreatLookalikeWarningsAsBlocking) {
  const GURL lookalike_url = embedded_https_test_server().GetURL(
      kLookalikeHostWarning, "/title1.html");
  CheckUrl(lookalike_url, false);
}

// This intentionaly does not load a blocklist.
class ActorSitePolicyMissingBlocklistBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_https_test_server().Start());

    // Register the optimization type for the blocklist, but we do not actually
    // load a blocklist.
    InitActionBlocklist(browser()->profile());
  }
};

// If the blocklist doesn't exist, we allow the URL.
IN_PROC_BROWSER_TEST_F(ActorSitePolicyMissingBlocklistBrowserTest, FailOpen) {
  const GURL url =
      embedded_https_test_server().GetURL("bar.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  base::test::TestFuture<MayActOnUrlBlockReason> allowed;
  auto* actor_service = ActorKeyedService::Get(browser()->profile());
  MayActOnTab(*browser()->tab_strip_model()->GetActiveTab(),
              actor_service->GetJournal(), TaskId(),
              absl::flat_hash_set<url::Origin>(), allowed.GetCallback());
  EXPECT_TRUE(allowed.Get() == MayActOnUrlBlockReason::kAllowed);
}

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

class ActorSitePolicyNoSafetyChecksBrowserTest
    : public ActorSitePolicySafeBrowsingBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ActorSitePolicySafeBrowsingBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(actor::switches::kDisableActorSafetyChecks);
  }
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

IN_PROC_BROWSER_TEST_F(ActorSitePolicySafeBrowsingBrowserTest,
                       RequireSafeBrowsing) {
  // Disable SafeBrowsing.
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING);

  // This would otherwise be allowed, but since we don't have SafeBrowsing to
  // check if it's dangerous, we assume it is unsafe.
  const GURL normally_allowed_url =
      embedded_https_test_server().GetURL("a.com", "/title1.html");
  CheckUrl(normally_allowed_url, false);
}

IN_PROC_BROWSER_TEST_F(ActorSitePolicyNoSafetyChecksBrowserTest,
                       DontRequireSafeBrowsing) {
  // Disable SafeBrowsing.
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING);

  // SafeBrowsing is not mandatory in this configuration.
  const GURL url = embedded_https_test_server().GetURL("a.com", "/title1.html");
  CheckUrl(url, true);
}

IN_PROC_BROWSER_TEST_F(ActorSitePolicyNoSafetyChecksBrowserTest,
                       IgnoreBlocklist) {
  // The blocklist is ignored in this configuration.
  const GURL blocked_url =
      embedded_https_test_server().GetURL("bar.com", "/title1.html");
  CheckUrl(blocked_url, true);
}

#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

}  // namespace

}  // namespace actor
