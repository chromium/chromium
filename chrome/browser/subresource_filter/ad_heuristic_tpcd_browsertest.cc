// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/tpcd/support/tpcd_support_service.h"
#include "chrome/browser/tpcd/support/tpcd_support_service_factory.h"
#include "chrome/browser/tpcd/support/validity_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tpcd_pref_names.h"
#include "components/privacy_sandbox/tpcd_utils.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/tpcd/metadata/browser/parser.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace url {
class Origin;
}  // namespace url

namespace {

// TODO(johnidel): We should instead use the enum from services/network with
// proper layering.
enum class AdsHeuristicCookieOverride {
  kNone = 0,
  kNotAd = 1,
  kAny = 2,
  kSkipHeuristics = 3,
  kSkipMetadata = 4,
  kSkipTrial = 5,
  kSkipTopLevelTrial = 6,
  kMaxValue = kSkipTopLevelTrial
};

const char kAdHeuristicOverrideHistogramName[] =
    "Privacy.3PCD.AdsHeuristicAddedToOverrides";

}  // namespace

class AdHeuristicTPCDBrowserTestBase
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  AdHeuristicTPCDBrowserTestBase()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    CHECK(fake_install_dir_.CreateUniqueTempDir());
    CHECK(fake_install_dir_.IsValid());
  }

  void SetUp() override {
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("isad=1")});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for 127.0.0.1 or localhost, so this
    // is needed to load pages from other hosts (b.com, c.com) without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  PrefService* GetPrefs() {
    return user_prefs::UserPrefs::Get(web_contents()->GetBrowserContext());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void FetchCookies(const std::string& host, const std::string& path) {
    // Fetch a subresrouce.
    std::string fetch_subresource_script = R"(
        const imgElement = document.createElement('img');
        imgElement.src = $1;
        document.body.appendChild(imgElement);
  )";

    std::ignore =
        ExecJs(web_contents(),
               content::JsReplace(fetch_subresource_script,
                                  https_server()->GetURL(host, path).spec()));
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  void NavigateToPageWithFrame(const std::string& host) {
    GURL main_url(https_server()->GetURL(host, "/iframe.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  }

  void NavigateFrameTo(const std::string& host, const std::string& path) {
    GURL page = https_server()->GetURL(host, path);
    NavigateFrameToUrl(page);
  }

  void NavigateFrameToUrl(const GURL& url) {
    EXPECT_TRUE(NavigateIframeToURL(web_contents(), "test", url));
  }

  void MockComponentInstallation(tpcd::metadata::Metadata metadata) {
    base::FilePath path =
        fake_install_dir_.GetPath().Append(FILE_PATH_LITERAL("metadata.pb"));
    CHECK(base::WriteFile(path, metadata.SerializeAsString()));

    CHECK(base::PathExists(path));
    std::string raw_metadata;
    CHECK(base::ReadFileToString(path, &raw_metadata));

    tpcd::metadata::Parser::GetInstance()->ParseMetadata(raw_metadata);
  }

  content::RenderFrameHost* GetChildFrame() {
    return content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  }

  void SetMetadataGrant(const GURL& third_party_url,
                        const GURL& first_party_url) {
    // Set up tpcd metadata, the first_party_url in OnCookieRead and
    // OnCookieChange is empty when Javascript third_party cookies access
    // trigger the calls, it only works for cases when matching the first
    // primary pattern.
    base::ScopedAllowBlockingForTesting allow_blocking;
    tpcd::metadata::Metadata metadata;
    tpcd::metadata::helpers::AddEntryToMetadata(
        metadata, ContentSettingsPattern::FromURL(third_party_url).ToString(),
        "*");
    EXPECT_EQ(metadata.metadata_entries_size(), 1);
    MockComponentInstallation(metadata);
    EXPECT_EQ(
        CookieSettingsFactory::GetForProfile(browser()->profile())
            ->GetCookieSetting(third_party_url, net::SiteForCookies(),
                               first_party_url, net::CookieSettingOverrides()),
        ContentSetting::CONTENT_SETTING_ALLOW);
  }

  void SetHeuristicsGrant(const GURL& third_party_url,
                          const GURL& first_party_url) {
    CookieSettingsFactory::GetForProfile(browser()->profile())
        ->SetTemporaryCookieGrantForHeuristic(third_party_url, first_party_url,
                                              base::Seconds(60));
    EXPECT_EQ(
        CookieSettingsFactory::GetForProfile(browser()->profile())
            ->GetCookieSetting(third_party_url, net::SiteForCookies(),
                               first_party_url, net::CookieSettingOverrides()),
        ContentSetting::CONTENT_SETTING_ALLOW);
  }

  void SetTopLevelTrialGrant(const GURL& third_party_url,
                             const GURL& first_party_url) {
    // Only |first_party_url| is used when creating the top-level tpcd support
    // setting, since the settings are only scoped to the top-level site
    // involved in the cookie access and therefore only use their
    // |primary_site_pattern| field.
    HostContentSettingsMap* settings_map =
        HostContentSettingsMapFactory::GetForProfile(browser()->profile());
    settings_map->SetContentSettingDefaultScope(
        first_party_url, GURL(), ContentSettingsType::TOP_LEVEL_TPCD_TRIAL,
        CONTENT_SETTING_ALLOW);

    browser()
        ->profile()
        ->GetDefaultStoragePartition()
        ->GetCookieManagerForBrowserProcess()
        ->SetContentSettings(ContentSettingsType::TOP_LEVEL_TPCD_TRIAL,
                             settings_map->GetSettingsForOneType(
                                 ContentSettingsType::TOP_LEVEL_TPCD_TRIAL),
                             base::NullCallback());

    EXPECT_EQ(
        CookieSettingsFactory::GetForProfile(browser()->profile())
            ->GetCookieSetting(third_party_url, net::SiteForCookies(),
                               first_party_url, net::CookieSettingOverrides()),
        ContentSetting::CONTENT_SETTING_ALLOW);
  }

  void Verify3PCookieAccessAllowed(
      net::test_server::ControllableHttpResponse* register_response) {
    // 3p cookie read
    content::CookieChangeObserver observer(web_contents());
    FetchCookies("b.test",
                 "/set-cookie?thirdparty=1;SameSite=None;Secure&isad=1");
    observer.Wait();
    FetchCookies("b.test", "/empty.html?isad=1");
    register_response->WaitForRequest();

    // COOKIE SHOULD BE ALLOWED.
    EXPECT_TRUE(
        base::Contains(register_response->http_request()->headers, "Cookie"));

    // Check JS access.
    NavigateFrameTo("b.test", "/empty.html?isad=1");
    EXPECT_EQ("thirdparty=1", EvalJs(GetChildFrame(), "document.cookie"));
  }

  void VerifyAdCookieAccessBlocked(
      net::test_server::ControllableHttpResponse* register_response,
      net::test_server::ControllableHttpResponse* register_response2,
      int metadata_count,
      int heuristics_count,
      int support_count,
      int top_level_support_count) {
    content::CookieChangeObserver observer(web_contents());
    FetchCookies("b.test", "/set-cookie?thirdparty=1;SameSite=None;Secure");
    observer.Wait();

    // Merge before creating the tester so we don't get previous samples from
    // the network service.
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::HistogramTester histogram_tester;

    FetchCookies("b.test", "/empty.html");
    register_response2->WaitForRequest();
    // COOKIE SHOULD NOT BE BLOCKED.
    EXPECT_TRUE(
        base::Contains(register_response2->http_request()->headers, "Cookie"));

    FetchCookies("b.test", "/empty.html?isad=1");
    register_response->WaitForRequest();
    // COOKIE SHOULD BE BLOCKED.
    EXPECT_EQ(
        base::Contains(register_response->http_request()->headers, "Cookie"),
        false);

    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    histogram_tester.ExpectBucketCount(kAdHeuristicOverrideHistogramName,
                                       AdsHeuristicCookieOverride::kAny, 1);
    histogram_tester.ExpectBucketCount(
        kAdHeuristicOverrideHistogramName,
        AdsHeuristicCookieOverride::kSkipMetadata, metadata_count);
    histogram_tester.ExpectBucketCount(
        kAdHeuristicOverrideHistogramName,
        AdsHeuristicCookieOverride::kSkipHeuristics, heuristics_count);
    histogram_tester.ExpectBucketCount(kAdHeuristicOverrideHistogramName,
                                       AdsHeuristicCookieOverride::kSkipTrial,
                                       support_count);
    histogram_tester.ExpectBucketCount(
        kAdHeuristicOverrideHistogramName,
        AdsHeuristicCookieOverride::kSkipTopLevelTrial,
        top_level_support_count);

    // Check JS access.
    NavigateFrameTo("b.test", "/empty.html");
    EXPECT_EQ("thirdparty=1", EvalJs(GetChildFrame(), "document.cookie"));
    NavigateFrameTo("b.test", "/empty.html?isad=1");
    EXPECT_EQ("", EvalJs(GetChildFrame(), "document.cookie"));
  }

 private:
  // This is needed because third party cookies must be marked SameSite=None and
  // Secure, so they must be accessed over HTTPS.
  net::EmbeddedTestServer https_server_;
  base::ScopedTempDir fake_install_dir_;
};

class AdHeuristicTPCDBrowserTestMetadataGrant
    : public AdHeuristicTPCDBrowserTestBase {
 public:
  AdHeuristicTPCDBrowserTestMetadataGrant() {
    // Experiment feature param requests 3PCs blocked.
    feature_list_.InitWithFeaturesAndParameters(
        {{net::features::kTpcdMetadataGrants, {}},
         {content_settings::features::kTrackingProtection3pcd, {}},
         {network::features::kSkipTpcdMitigationsForAds,
          {{"SkipTpcdMitigationsForAdsMetadata", "false"},
           {"SkipTpcdMitigationsForAdsHeuristics", "true"},
           {"SkipTpcdMitigationsForAdsSupport", "true"},
           {"SkipTpcdMitigationsForAdsTopLevelTrial", "true"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AdHeuristicTPCDBrowserTestMetadataGrant, CookieAllowed) {
  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/empty.html?isad=1");
  ASSERT_TRUE(https_server()->Start());
  NavigateToPageWithFrame("a.test");

  GURL third_party_url = https_server()->GetURL("b.test", "/");
  GURL first_party_url = https_server()->GetURL("a.test", "/");
  SetMetadataGrant(third_party_url, first_party_url);

  Verify3PCookieAccessAllowed(register_response.get());
}

class AdHeuristicTPCDBrowserTestSkipMetadata
    : public AdHeuristicTPCDBrowserTestBase {
 public:
  AdHeuristicTPCDBrowserTestSkipMetadata() {
    // Experiment feature param requests 3PCs blocked.
    feature_list_.InitWithFeaturesAndParameters(
        {{net::features::kTpcdMetadataGrants, {}},
         {content_settings::features::kTrackingProtection3pcd, {}},
         {network::features::kSkipTpcdMitigationsForAds,
          {{"SkipTpcdMitigationsForAdsMetadata", "true"},
           {"SkipTpcdMitigationsForAdsHeuristics", "false"},
           {"SkipTpcdMitigationsForAdsSupport", "false"},
           {"SkipTpcdMitigationsForAdsTopLevelTrial", "false"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AdHeuristicTPCDBrowserTestSkipMetadata, CookieBlocked) {
  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/empty.html?isad=1");
  auto register_response2 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/empty.html");

  ASSERT_TRUE(https_server()->Start());

  NavigateToPageWithFrame("a.test");

  GURL third_party_url = https_server()->GetURL("b.test", "/");
  GURL first_party_url = https_server()->GetURL("a.test", "/");
  SetMetadataGrant(third_party_url, first_party_url);

  VerifyAdCookieAccessBlocked(register_response.get(), register_response2.get(),
                              /*metadata_count=*/1, /*heuristics_count=*/0,
                              /*support_count=*/0,
                              /*top_level_support_count=*/0);
}

class AdHeuristicTPCDBrowserTestHeuristicsGrant
    : public AdHeuristicTPCDBrowserTestBase {
 public:
  AdHeuristicTPCDBrowserTestHeuristicsGrant() {
    // Experiment feature param requests 3PCs blocked.
    feature_list_.InitWithFeaturesAndParameters(
        {{content_settings::features::kTpcdHeuristicsGrants,
          {{"TpcdReadHeuristicsGrants", "true"}}},
         {content_settings::features::kTrackingProtection3pcd, {}},
         {network::features::kSkipTpcdMitigationsForAds,
          {{"SkipTpcdMitigationsForAdsHeuristics", "false"},
           {"SkipTpcdMitigationsForAdsMetadata", "true"},
           {"SkipTpcdMitigationsForAdsSupport", "true"},
           {"SkipTpcdMitigationsForAdsTopLevelTrial", "true"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AdHeuristicTPCDBrowserTestHeuristicsGrant,
                       CookieAllowed) {
  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/empty.html?isad=1");

  ASSERT_TRUE(https_server()->Start());

  NavigateToPageWithFrame("a.test");

  GURL third_party_url = https_server()->GetURL("b.test", "/");
  GURL first_party_url = https_server()->GetURL("a.test", "/");
  SetHeuristicsGrant(third_party_url, first_party_url);

  Verify3PCookieAccessAllowed(register_response.get());
}

class AdHeuristicTPCDBrowserTestSkipHeuristicsGrant
    : public AdHeuristicTPCDBrowserTestBase {
 public:
  AdHeuristicTPCDBrowserTestSkipHeuristicsGrant() {
    // Experiment feature param requests 3PCs blocked.
    feature_list_.InitWithFeaturesAndParameters(
        {{content_settings::features::kTpcdHeuristicsGrants,
          {{"TpcdReadHeuristicsGrants", "true"}}},
         {content_settings::features::kTrackingProtection3pcd, {}},
         {network::features::kSkipTpcdMitigationsForAds,
          {{"SkipTpcdMitigationsForAdsHeuristics", "true"},
           {"SkipTpcdMitigationsForAdsMetadata", "false"},
           {"SkipTpcdMitigationsForAdsSupport", "false"},
           {"SkipTpcdMitigationsForAdsTopLevelTrial", "false"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/41481346): Investigate flakiness on Lacros/ChromeOS flakiness.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CookieBlocked DISABLED_CookieBlockedProfile
#else
#define MAYBE_CookieBlocked CookieBlocked
#endif
IN_PROC_BROWSER_TEST_F(AdHeuristicTPCDBrowserTestSkipHeuristicsGrant,
                       MAYBE_CookieBlocked) {
  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/empty.html?isad=1");
  auto register_response2 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/empty.html");

  ASSERT_TRUE(https_server()->Start());

  NavigateToPageWithFrame("a.test");

  GURL third_party_url = https_server()->GetURL("b.test", "/");
  GURL first_party_url = https_server()->GetURL("a.test", "/");
  SetHeuristicsGrant(third_party_url, first_party_url);

  VerifyAdCookieAccessBlocked(register_response.get(), register_response2.get(),
                              /*metadata_count=*/0, /*heuristics_count=*/1,
                              /*support_count=*/0,
                              /*top_level_support_count=*/0);
}

class AdHeuristicTPCDBrowserTestTrialGrant
    : public AdHeuristicTPCDBrowserTestBase {
 public:
  AdHeuristicTPCDBrowserTestTrialGrant() {
    // Experiment feature param requests 3PCs blocked.
    feature_list_.InitWithFeaturesAndParameters(
        {{net::features::kTpcdTrialSettings, {}},
         {content_settings::features::kTrackingProtection3pcd, {}},
         {network::features::kSkipTpcdMitigationsForAds,
          {{"SkipTpcdMitigationsForAdsSupport", "false"},
           {"SkipTpcdMitigationsForAdsTopLevelTrial", "true"},
           {"SkipTpcdMitigationsForAdsMetadata", "true"},
           {"SkipTpcdMitigationsForAdsHeuristics", "true"}}}},
        {});

    // Disable the validity service so it doesn't remove manually created
    // trial settings.
    tpcd::trial::ValidityService::DisableForTesting();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AdHeuristicTPCDBrowserTestTrialGrant, CookieAllowed) {
  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/empty.html?isad=1");

  ASSERT_TRUE(https_server()->Start());

  NavigateToPageWithFrame("a.test");

  GURL third_party_url = https_server()->GetURL("b.test", "/");
  GURL first_party_url = https_server()->GetURL("a.test", "/");
  tpcd::trial::TpcdTrialServiceFactory::GetForProfile(browser()->profile())
      ->Update3pcdTrialSettingsForTesting(OriginTrialStatusChangeDetails(
          url::Origin::Create(third_party_url), first_party_url.spec(),
          /*match_subdomains=*/false,
          /*enabled=*/true, /*source_id=*/std::nullopt));

  Verify3PCookieAccessAllowed(register_response.get());
}

class AdHeuristicTPCDBrowserTestSkipTrialGrant
    : public AdHeuristicTPCDBrowserTestBase {
 public:
  AdHeuristicTPCDBrowserTestSkipTrialGrant() {
    // Experiment feature param requests 3PCs blocked.
    feature_list_.InitWithFeaturesAndParameters(
        {{net::features::kTpcdTrialSettings, {}},
         {content_settings::features::kTrackingProtection3pcd, {}},
         {network::features::kSkipTpcdMitigationsForAds,
          {{"SkipTpcdMitigationsForAdsSupport", "true"},
           {"SkipTpcdMitigationsForAdsTopLevelTrial", "false"},
           {"SkipTpcdMitigationsForAdsMetadata", "false"},
           {"SkipTpcdMitigationsForAdsHeuristics", "false"}}}},
        {});

    // Disable the validity service so it doesn't remove manually created
    // trial settings.
    tpcd::trial::ValidityService::DisableForTesting();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AdHeuristicTPCDBrowserTestSkipTrialGrant,
                       CookieBlocked) {
  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/empty.html?isad=1");
  auto register_response2 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/empty.html");

  ASSERT_TRUE(https_server()->Start());

  NavigateToPageWithFrame("a.test");

  GURL third_party_url = https_server()->GetURL("b.test", "/");
  GURL first_party_url = https_server()->GetURL("a.test", "/");
  tpcd::trial::TpcdTrialServiceFactory::GetForProfile(browser()->profile())
      ->Update3pcdTrialSettingsForTesting(OriginTrialStatusChangeDetails(
          url::Origin::Create(third_party_url), first_party_url.spec(),
          /*match_subdomains=*/false,
          /*enabled=*/true, /*source_id=*/std::nullopt));

  VerifyAdCookieAccessBlocked(register_response.get(), register_response2.get(),
                              /*metadata_count=*/0, /*heuristics_count=*/0,
                              /*support_count=*/1,
                              /*top_level_support_count=*/0);
}

class AdHeuristicTPCDBrowserTestTopLevelTrialGrant
    : public AdHeuristicTPCDBrowserTestBase {
 public:
  AdHeuristicTPCDBrowserTestTopLevelTrialGrant() {
    // Experiment feature param requests 3PCs blocked.
    feature_list_.InitWithFeaturesAndParameters(
        {{net::features::kTopLevelTpcdTrialSettings, {}},
         {content_settings::features::kTrackingProtection3pcd, {}},
         {network::features::kSkipTpcdMitigationsForAds,
          {{"SkipTpcdMitigationsForAdsTopLevelTrial", "false"},
           {"SkipTpcdMitigationsForAdsSupport", "true"},
           {"SkipTpcdMitigationsForAdsMetadata", "true"},
           {"SkipTpcdMitigationsForAdsHeuristics", "true"}}}},
        {});

    // Disable the validity service so it doesn't remove manually created
    // trial settings.
    tpcd::trial::ValidityService::DisableForTesting();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AdHeuristicTPCDBrowserTestTopLevelTrialGrant,
                       CookieAllowed) {
  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/empty.html?isad=1");

  ASSERT_TRUE(https_server()->Start());

  NavigateToPageWithFrame("a.test");

  GURL third_party_url = https_server()->GetURL("b.test", "/");
  GURL first_party_url = https_server()->GetURL("a.test", "/");
  SetTopLevelTrialGrant(third_party_url, first_party_url);

  Verify3PCookieAccessAllowed(register_response.get());
}

class AdHeuristicTPCDBrowserTestSkipTopLevelTrialGrant
    : public AdHeuristicTPCDBrowserTestBase {
 public:
  AdHeuristicTPCDBrowserTestSkipTopLevelTrialGrant() {
    // Experiment feature param requests 3PCs blocked.
    feature_list_.InitWithFeaturesAndParameters(
        {{net::features::kTopLevelTpcdTrialSettings, {}},
         {content_settings::features::kTrackingProtection3pcd, {}},
         {network::features::kSkipTpcdMitigationsForAds,
          {{"SkipTpcdMitigationsForAdsTopLevelTrial", "true"},
           {"SkipTpcdMitigationsForAdsSupport", "false"},
           {"SkipTpcdMitigationsForAdsMetadata", "false"},
           {"SkipTpcdMitigationsForAdsHeuristics", "false"}}}},
        {});

    // Disable the validity service so it doesn't remove manually created
    // trial settings.
    tpcd::trial::ValidityService::DisableForTesting();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AdHeuristicTPCDBrowserTestSkipTopLevelTrialGrant,
                       CookieBlocked) {
  auto register_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/empty.html?isad=1");
  auto register_response2 =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          https_server(), "/empty.html");

  ASSERT_TRUE(https_server()->Start());

  NavigateToPageWithFrame("a.test");

  GURL third_party_url = https_server()->GetURL("b.test", "/");
  GURL first_party_url = https_server()->GetURL("a.test", "/");
  SetTopLevelTrialGrant(third_party_url, first_party_url);

  VerifyAdCookieAccessBlocked(register_response.get(), register_response2.get(),
                              /*metadata_count=*/0, /*heuristics_count=*/0,
                              /*support_count=*/0,
                              /*top_level_support_count=*/1);
}
