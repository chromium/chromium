// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <string>
#include <tuple>
#include <vector>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/net/storage_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/tpcd/support/top_level_trial_service.h"
#include "chrome/browser/tpcd/support/top_level_trial_service_factory.h"
#include "chrome/browser/tpcd/support/validity_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_waiter.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/tpcd/metadata/browser/parser.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/features.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/site_for_cookies.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

namespace tpcd::metadata {
namespace {
const base::FilePath::CharType kComponentFileName[] =
    FILE_PATH_LITERAL("metadata.pb");

const char* kFirstPartyHost = "a.test";
const char* kThirdPartyHost1 = "b.test";
const char* kThirdPartyHost1Sub = "sub.b.test";
const char* kThirdPartyHost2 = "c.test";
const char* kThirdPartyHost3 = "d.test";

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Creates a Original Guest Profile (not OTR Profile) for testing.
Profile& CreateOriginalGuestProfile() {
  Profile& original_guest_profile = profiles::testing::CreateProfileSync(
      g_browser_process->profile_manager(),
      ProfileManager::GetGuestProfilePath());

  return original_guest_profile;
}

// Creates a new regular profile for testing.
Profile* CreateRegularProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_path = profile_manager->GenerateNextProfileDirectoryPath();

  profiles::testing::CreateProfileSync(profile_manager, new_path);

  return profile_manager->GetProfile(new_path);
}
#endif

const char kThirdPartyCookieAllowMechanismHistogram[] =
    "PageLoad.Clients.TPCD.CookieAccess.ThirdPartyCookieAllowMechanism3";
using ThirdPartyCookieAllowMechanism =
    content_settings::CookieSettingsBase::ThirdPartyCookieAllowMechanism;
}  // namespace

class ManagerBrowserTest : public InProcessBrowserTest {
 public:
  ManagerBrowserTest() : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    CHECK(fake_install_dir_.CreateUniqueTempDir());
    CHECK(fake_install_dir_.IsValid());
    scoped_feature_list_.InitWithFeatures(
        {content_settings::features::kTrackingProtection3pcd,
         net::features::kTpcdMetadataGrants,
         net::features::kTpcdMetadataStageControl,
         net::features::kTopLevelTpcdTrialSettings},
        {});

    // Disable the validity service so it doesn't remove manually created
    // trial settings.
    tpcd::trial::ValidityService::DisableForTesting();
  }
  ~ManagerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromDirectory(path);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    EXPECT_TRUE(https_server_.Start());
  }

  net::test_server::EmbeddedTestServer* https_server() {
    return &https_server_;
  }

  Parser* parser() { return tpcd::metadata::Parser::GetInstance(); }

  void MockComponentInstallation(Metadata metadata) {
    base::FilePath path =
        fake_install_dir_.GetPath().Append(kComponentFileName);
    CHECK(base::WriteFile(path, metadata.SerializeAsString()));

    CHECK(base::PathExists(path));
    std::string raw_metadata;
    CHECK(base::ReadFileToString(path, &raw_metadata));

    parser()->ParseMetadata(raw_metadata);
  }

  PrefService* GetPrefs(Profile* profile = nullptr) {
    return (profile ? profile : browser()->profile())->GetPrefs();
  }

  content_settings::CookieSettings* GetCookieSettings(
      Profile* profile = nullptr) {
    return CookieSettingsFactory::GetForProfile(profile ? profile
                                                        : browser()->profile())
        .get();
  }

  void NavigateToPageWithFrame(const std::string& host,
                               Browser* alt_browser = nullptr) {
    GURL url(https_server()->GetURL(host, "/iframe.html"));
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        alt_browser ? alt_browser : browser(), url));
  }

  content::WebContents* GetWebContents(Browser* alt_browser = nullptr) {
    return (alt_browser ? alt_browser : browser())
        ->tab_strip_model()
        ->GetActiveWebContents();
  }

  void NavigateFrameTo(const std::string& host,
                       const std::string& path,
                       Browser* alt_browser = nullptr) {
    GURL url = https_server()->GetURL(host, path);
    EXPECT_TRUE(NavigateIframeToURL(GetWebContents(alt_browser), "test", url));
  }

  content::RenderFrameHost* GetFrame(Browser* alt_browser = nullptr) {
    return ChildFrameAt((alt_browser ? alt_browser : browser())
                            ->tab_strip_model()
                            ->GetActiveWebContents()
                            ->GetPrimaryMainFrame(),
                        0);
  }

  std::vector<std::string> ContentSettingsToString(ContentSettingsType type,
                                                   Profile* profile = nullptr) {
    std::vector<std::string> settings;
    for (const auto& setting :
         GetCookieSettings(profile)->GetTpcdMetadataGrants()) {
      settings.emplace_back(base::StringPrintf(
          "[%s,%s]:%d", setting.primary_pattern.ToString().c_str(),
          setting.secondary_pattern.ToString().c_str(),
          content_settings::ValueToContentSetting(setting.setting_value)));
    }
    return settings;
  }

  void ExpectCookie(content::RenderFrameHost* frame, bool expected) {
    // navigator.cookieEnabled only checks the browser setting, not if
    // unpartitioned cookies are enabled.
    EXPECT_TRUE(
        content::EvalJs(frame, "navigator.cookieEnabled").ExtractBool());
    // This returns true even if the cookie fails to be set because
    // unpartitioned cookies are blocked.
    EXPECT_TRUE(content::EvalJs(frame, "setCookie()").ExtractBool());
    // This is the only step that actually verifies if unpartitioned cookies
    // are accessible.
    EXPECT_EQ(content::EvalJs(frame, "hasCookie()").ExtractBool(), expected);
  }

  // third-party storage should only be accessible when:
  // - Third-party cookies are allowed by default,
  // - Access grants are given via third-party cookies deprecation metadata, or
  // - Third-party storage partitioning is enabled,
  // But will never be accessible if blocked by specific user cookie setting
  // preferences (`setting_source_user`).
  void ExpectStorage(content::RenderFrameHost* frame,
                     const bool expected,
                     const bool setting_source_user,
                     const bool is_3psp_enabled) {
    if (expected || (setting_source_user ? false : is_3psp_enabled)) {
      storage::test::SetStorageForFrame(frame, /*include_cookies=*/false, true);
      storage::test::ExpectStorageForFrame(frame, true);
    } else {
      storage::test::SetStorageForFrame(frame, /*include_cookies=*/false,
                                        false);
      storage::test::ExpectStorageForFrame(frame, false);
    }
  }

 private:
  base::ScopedTempDir fake_install_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
  net::test_server::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(ManagerBrowserTest, GetTpcdMetadataGrants_Empty) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrants().size(), 0u);

  Metadata metadata;
  ASSERT_EQ(metadata.metadata_entries_size(), 0);

  MockComponentInstallation(metadata);
  ASSERT_TRUE(GetCookieSettings()->GetTpcdMetadataGrants().empty());
}

IN_PROC_BROWSER_TEST_F(ManagerBrowserTest, GetTpcdMetadataGrants) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  const GURL kEmbedded = GURL("http://www.bar.com");
  const url::Origin kEmbedder = url::Origin::Create(GURL("http://www.foo.com"));
  ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrants().size(), 0u);
  EXPECT_FALSE(GetCookieSettings()->IsFullCookieAccessAllowed(
      kEmbedded, net::SiteForCookies(), kEmbedder, {}));

  Metadata metadata;
  tpcd::metadata::helpers::AddEntryToMetadata(metadata, "[*.]bar.com",
                                              "[*.]foo.com");
  ASSERT_EQ(metadata.metadata_entries_size(), 1);

  MockComponentInstallation(metadata);

  ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrants().size(), 1u);
  ASSERT_EQ(GetCookieSettings()
                ->GetTpcdMetadataGrants()
                .front()
                .metadata.tpcd_metadata_rule_source(),
            content_settings::mojom::TpcdMetadataRuleSource::SOURCE_TEST);
  EXPECT_TRUE(GetCookieSettings()->IsFullCookieAccessAllowed(
      kEmbedded, net::SiteForCookies(), kEmbedder, {}));
}

IN_PROC_BROWSER_TEST_F(ManagerBrowserTest, SuccessfullyUpdated) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  const GURL kEmbedded1 = GURL("http://www.bar.com");
  const url::Origin kEmbedder1 =
      url::Origin::Create(GURL("http://www.foo.com"));
  const GURL kEmbedded2 = GURL("http://www.baz.com");
  const url::Origin kEmbedder2 =
      url::Origin::Create(GURL("http://www.daz.com"));

  {
    Metadata metadata;
    tpcd::metadata::helpers::AddEntryToMetadata(metadata, "[*.]bar.com",
                                                "[*.]foo.com");
    ASSERT_EQ(metadata.metadata_entries_size(), 1);

    ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrants().size(), 0u);
    EXPECT_FALSE(GetCookieSettings()->IsFullCookieAccessAllowed(
        kEmbedded1, net::SiteForCookies(), kEmbedder1, {}));

    MockComponentInstallation(metadata);

    ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrants().size(), 1u);
    EXPECT_TRUE(GetCookieSettings()->IsFullCookieAccessAllowed(
        kEmbedded1, net::SiteForCookies(), kEmbedder1, {}));
  }

  {
    Metadata metadata;
    tpcd::metadata::helpers::AddEntryToMetadata(metadata, "[*.]baz.com",
                                                "[*.]daz.com");
    ASSERT_EQ(metadata.metadata_entries_size(), 1);

    ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrants().size(), 1u);
    EXPECT_TRUE(GetCookieSettings()->IsFullCookieAccessAllowed(
        kEmbedded1, net::SiteForCookies(), kEmbedder1, {}));
    EXPECT_FALSE(GetCookieSettings()->IsFullCookieAccessAllowed(
        kEmbedded2, net::SiteForCookies(), kEmbedder2, {}));

    MockComponentInstallation(metadata);

    ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrants().size(), 1u);
    EXPECT_FALSE(GetCookieSettings()->IsFullCookieAccessAllowed(
        kEmbedded1, net::SiteForCookies(), kEmbedder1, {}));
    EXPECT_TRUE(GetCookieSettings()->IsFullCookieAccessAllowed(
        kEmbedded2, net::SiteForCookies(), kEmbedder2, {}));
  }
}

IN_PROC_BROWSER_TEST_F(ManagerBrowserTest,
                       TpcdDtGracePeriodEnforced_ValidDtToken) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  const GURL first_party_url = https_server()->GetURL(kFirstPartyHost, "/");
  const GURL third_party_url_1 = https_server()->GetURL(kThirdPartyHost1, "/");
  const GURL third_party_url_2 = https_server()->GetURL(kThirdPartyHost2, "/");
  const GURL third_party_url_3 = https_server()->GetURL(kThirdPartyHost3, "/");

  const std::string secondary_pattern_spec =
      ContentSettingsPattern::FromURLNoWildcard(first_party_url).ToString();
  const std::string primary_pattern_spec_1 =
      ContentSettingsPattern::FromURLNoWildcard(third_party_url_1).ToString();
  const std::string primary_pattern_spec_2 =
      ContentSettingsPattern::FromURLNoWildcard(third_party_url_2).ToString();
  const std::string primary_pattern_spec_3 =
      ContentSettingsPattern::FromURLNoWildcard(third_party_url_3).ToString();

  Metadata metadata;

  const uint32_t dtrp_guarantees_grace_period_forced_on = 0;
  tpcd::metadata::helpers::AddEntryToMetadata(
      metadata, primary_pattern_spec_1, secondary_pattern_spec,
      Parser::kSource1pDt, dtrp_guarantees_grace_period_forced_on);

  const uint32_t dtrp_guarantees_grace_period_forced_off = 100;
  tpcd::metadata::helpers::AddEntryToMetadata(
      metadata, primary_pattern_spec_2, secondary_pattern_spec,
      Parser::kSource1pDt, dtrp_guarantees_grace_period_forced_off);

  tpcd::metadata::helpers::AddEntryToMetadata(metadata, primary_pattern_spec_3,
                                              secondary_pattern_spec,
                                              Parser::kSourceCriticalSector);

  EXPECT_EQ(GetCookieSettings()->GetTpcdMetadataGrants().size(), 0u);
  MockComponentInstallation(metadata);
  EXPECT_EQ(GetCookieSettings()->GetTpcdMetadataGrants().size(), 3u);

  auto* service = tpcd::trial::TopLevelTrialServiceFactory::GetForProfile(
      browser()->profile());
  auto embedder_origin = url::Origin::Create(first_party_url);
  service->UpdateTopLevelTrialSettingsForTesting(
      embedder_origin, /*match_subdomains=*/true, /*enabled=*/true);

  EXPECT_TRUE(GetCookieSettings()->IsFullCookieAccessAllowed(
      third_party_url_1, net::SiteForCookies(), embedder_origin, {}));
  {
    base::HistogramTester histogram_tester;
    content::CookieChangeObserver observer(GetWebContents(),
                                           /*num_expected_calls=*/2);
    NavigateToPageWithFrame(kFirstPartyHost);
    NavigateFrameTo(kThirdPartyHost1, "/browsing_data/site_data.html");
    ExpectCookie(GetFrame(), /*expected=*/true);
    observer.Wait();
    EXPECT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

    histogram_tester.ExpectUniqueSample(
        kThirdPartyCookieAllowMechanismHistogram,
        ThirdPartyCookieAllowMechanism::kAllowBy3PCDMetadataSource1pDt, 2);
  }

  EXPECT_TRUE(GetCookieSettings()->IsFullCookieAccessAllowed(
      third_party_url_2, net::SiteForCookies(), embedder_origin, {}));
  {
    base::HistogramTester histogram_tester;
    content::CookieChangeObserver observer(GetWebContents(),
                                           /*num_expected_calls=*/2);
    NavigateToPageWithFrame(kFirstPartyHost);
    NavigateFrameTo(kThirdPartyHost2, "/browsing_data/site_data.html");
    ExpectCookie(GetFrame(), /*expected=*/true);
    observer.Wait();
    EXPECT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

    histogram_tester.ExpectUniqueSample(
        kThirdPartyCookieAllowMechanismHistogram,
        ThirdPartyCookieAllowMechanism::kAllowByTopLevel3PCD, 2);
  }

  EXPECT_TRUE(GetCookieSettings()->IsFullCookieAccessAllowed(
      third_party_url_3, net::SiteForCookies(), embedder_origin, {}));
  {
    base::HistogramTester histogram_tester;
    content::CookieChangeObserver observer(GetWebContents(),
                                           /*num_expected_calls=*/2);
    NavigateToPageWithFrame(kFirstPartyHost);
    NavigateFrameTo(kThirdPartyHost3, "/browsing_data/site_data.html");
    ExpectCookie(GetFrame(), /*expected=*/true);
    observer.Wait();
    EXPECT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

    histogram_tester.ExpectUniqueSample(
        kThirdPartyCookieAllowMechanismHistogram,
        ThirdPartyCookieAllowMechanism::
            kAllowBy3PCDMetadataSourceCriticalSector,
        2);
  }
}

// This test coverage ensures more specific patterns precede on others.
IN_PROC_BROWSER_TEST_F(ManagerBrowserTest,
                       TpcdDtGracePeriodEnforced_EntryPrecedence_1) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  const GURL first_party_url = https_server()->GetURL(kFirstPartyHost, "/");
  const GURL third_party_url_1 = https_server()->GetURL(kThirdPartyHost1, "/");
  const GURL third_party_url_1_sub =
      https_server()->GetURL(kThirdPartyHost1Sub, "/");

  const std::string secondary_pattern_spec =
      ContentSettingsPattern::FromURLNoWildcard(first_party_url).ToString();

  const std::string wildcard = "[*.]";
  const std::string primary_pattern_spec_1 =
      base::StrCat({wildcard, kThirdPartyHost1});
  const std::string primary_pattern_spec_1_sub =
      base::StrCat({wildcard, kThirdPartyHost1Sub});

  Metadata metadata;

  // This order of insertion of entries should be maintained in order to have
  // the less specific entry first in the list.
  {
    const uint32_t dtrp_guarantees_grace_period_forced_on = 0;
    const uint32_t dtrp_guarantees_grace_period_forced_off = 100;

    tpcd::metadata::helpers::AddEntryToMetadata(
        metadata, primary_pattern_spec_1, secondary_pattern_spec,
        Parser::kSource1pDt, dtrp_guarantees_grace_period_forced_on);

    tpcd::metadata::helpers::AddEntryToMetadata(
        metadata, primary_pattern_spec_1_sub, secondary_pattern_spec,
        Parser::kSource1pDt, dtrp_guarantees_grace_period_forced_off);
  }

  EXPECT_EQ(GetCookieSettings()->GetTpcdMetadataGrants().size(), 0u);
  MockComponentInstallation(metadata);
  EXPECT_EQ(GetCookieSettings()->GetTpcdMetadataGrants().size(), 2u);

  auto embedder_origin = url::Origin::Create(first_party_url);

  EXPECT_TRUE(GetCookieSettings()->IsFullCookieAccessAllowed(
      third_party_url_1, net::SiteForCookies(), embedder_origin, {}));
  {
    base::HistogramTester histogram_tester;

    content::CookieChangeObserver observer(GetWebContents(),
                                           /*num_expected_calls=*/2);
    NavigateToPageWithFrame(kFirstPartyHost);
    NavigateFrameTo(kThirdPartyHost1, "/browsing_data/site_data.html");
    ExpectCookie(GetFrame(), /*expected=*/true);
    observer.Wait();
    EXPECT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

    histogram_tester.ExpectUniqueSample(
        kThirdPartyCookieAllowMechanismHistogram,
        ThirdPartyCookieAllowMechanism::kAllowBy3PCDMetadataSource1pDt, 2);
  }

  EXPECT_FALSE(GetCookieSettings()->IsFullCookieAccessAllowed(
      third_party_url_1_sub, net::SiteForCookies(), embedder_origin, {}));
  {
    base::HistogramTester histogram_tester;

    NavigateToPageWithFrame(kFirstPartyHost);
    NavigateFrameTo(kThirdPartyHost1Sub, "/browsing_data/site_data.html");
    ExpectCookie(GetFrame(), /*expected=*/false);
    EXPECT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

    EXPECT_EQ(
        histogram_tester.GetTotalSum(kThirdPartyCookieAllowMechanismHistogram),
        0);
  }
}

// This test coverage ensures more specific patterns precede on others.
IN_PROC_BROWSER_TEST_F(ManagerBrowserTest,
                       TpcdDtGracePeriodEnforced_EntryPrecedence_2) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  const GURL first_party_url = https_server()->GetURL(kFirstPartyHost, "/");
  const GURL third_party_url_1 = https_server()->GetURL(kThirdPartyHost1, "/");
  const GURL third_party_url_1_sub =
      https_server()->GetURL(kThirdPartyHost1Sub, "/");

  const std::string secondary_pattern_spec =
      ContentSettingsPattern::FromURLNoWildcard(first_party_url).ToString();

  const std::string wildcard = "[*.]";
  const std::string primary_pattern_spec_1 =
      base::StrCat({wildcard, kThirdPartyHost1});
  const std::string primary_pattern_spec_1_sub =
      base::StrCat({wildcard, kThirdPartyHost1Sub});

  Metadata metadata;

  // This order of insertion of entries should be maintained in order to have
  // the less specific entry first in the list.
  {
    const uint32_t dtrp_guarantees_grace_period_forced_on = 0;
    const uint32_t dtrp_guarantees_grace_period_forced_off = 100;

    tpcd::metadata::helpers::AddEntryToMetadata(
        metadata, primary_pattern_spec_1, secondary_pattern_spec,
        Parser::kSource1pDt, dtrp_guarantees_grace_period_forced_off);

    tpcd::metadata::helpers::AddEntryToMetadata(
        metadata, primary_pattern_spec_1_sub, secondary_pattern_spec,
        Parser::kSource1pDt, dtrp_guarantees_grace_period_forced_on);
  }

  EXPECT_EQ(GetCookieSettings()->GetTpcdMetadataGrants().size(), 0u);
  MockComponentInstallation(metadata);
  EXPECT_EQ(GetCookieSettings()->GetTpcdMetadataGrants().size(), 2u);

  auto embedder_origin = url::Origin::Create(first_party_url);

  EXPECT_FALSE(GetCookieSettings()->IsFullCookieAccessAllowed(
      third_party_url_1, net::SiteForCookies(), embedder_origin, {}));
  {
    base::HistogramTester histogram_tester;

    NavigateToPageWithFrame(kFirstPartyHost);
    NavigateFrameTo(kThirdPartyHost1, "/browsing_data/site_data.html");
    ExpectCookie(GetFrame(), /*expected=*/false);
    EXPECT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

    EXPECT_EQ(
        histogram_tester.GetTotalSum(kThirdPartyCookieAllowMechanismHistogram),
        0);
  }

  EXPECT_TRUE(GetCookieSettings()->IsFullCookieAccessAllowed(
      third_party_url_1_sub, net::SiteForCookies(), embedder_origin, {}));
  {
    base::HistogramTester histogram_tester;

    content::CookieChangeObserver observer(GetWebContents(),
                                           /*num_expected_calls=*/2);
    NavigateToPageWithFrame(kFirstPartyHost);
    NavigateFrameTo(kThirdPartyHost1Sub, "/browsing_data/site_data.html");
    ExpectCookie(GetFrame(), /*expected=*/true);
    observer.Wait();
    EXPECT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

    histogram_tester.ExpectUniqueSample(
        kThirdPartyCookieAllowMechanismHistogram,
        ThirdPartyCookieAllowMechanism::kAllowBy3PCDMetadataSource1pDt, 2);
  }
}

IN_PROC_BROWSER_TEST_F(ManagerBrowserTest,
                       TpcdDtGracePeriodEnforced_InValidDtToken) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  const GURL first_party_url = https_server()->GetURL(kFirstPartyHost, "/");
  const GURL third_party_url_1 = https_server()->GetURL(kThirdPartyHost1, "/");
  const GURL third_party_url_2 = https_server()->GetURL(kThirdPartyHost2, "/");
  const GURL third_party_url_3 = https_server()->GetURL(kThirdPartyHost3, "/");

  const std::string secondary_pattern_spec =
      ContentSettingsPattern::FromURLNoWildcard(first_party_url).ToString();
  const std::string primary_pattern_spec_1 =
      ContentSettingsPattern::FromURLNoWildcard(third_party_url_1).ToString();
  const std::string primary_pattern_spec_2 =
      ContentSettingsPattern::FromURLNoWildcard(third_party_url_2).ToString();
  const std::string primary_pattern_spec_3 =
      ContentSettingsPattern::FromURLNoWildcard(third_party_url_3).ToString();

  Metadata metadata;

  const uint32_t dtrp_guarantees_grace_period_forced_on = 0;
  tpcd::metadata::helpers::AddEntryToMetadata(
      metadata, primary_pattern_spec_1, secondary_pattern_spec,
      Parser::kSource1pDt, dtrp_guarantees_grace_period_forced_on);

  const uint32_t dtrp_guarantees_grace_period_forced_off = 100;
  tpcd::metadata::helpers::AddEntryToMetadata(
      metadata, primary_pattern_spec_2, secondary_pattern_spec,
      Parser::kSource1pDt, dtrp_guarantees_grace_period_forced_off);

  tpcd::metadata::helpers::AddEntryToMetadata(metadata, primary_pattern_spec_3,
                                              secondary_pattern_spec,
                                              Parser::kSourceCriticalSector);

  EXPECT_EQ(GetCookieSettings()->GetTpcdMetadataGrants().size(), 0u);
  MockComponentInstallation(metadata);
  EXPECT_EQ(GetCookieSettings()->GetTpcdMetadataGrants().size(), 3u);

  auto embedder_origin = url::Origin::Create(first_party_url);

  EXPECT_TRUE(GetCookieSettings()->IsFullCookieAccessAllowed(
      third_party_url_1, net::SiteForCookies(), embedder_origin, {}));
  {
    base::HistogramTester histogram_tester;
    content::CookieChangeObserver observer(GetWebContents(),
                                           /*num_expected_calls=*/2);
    NavigateToPageWithFrame(kFirstPartyHost);
    NavigateFrameTo(kThirdPartyHost1, "/browsing_data/site_data.html");
    ExpectCookie(GetFrame(), /*expected=*/true);
    observer.Wait();
    EXPECT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

    histogram_tester.ExpectUniqueSample(
        kThirdPartyCookieAllowMechanismHistogram,
        ThirdPartyCookieAllowMechanism::kAllowBy3PCDMetadataSource1pDt, 2);
  }

  EXPECT_FALSE(GetCookieSettings()->IsFullCookieAccessAllowed(
      third_party_url_2, net::SiteForCookies(), embedder_origin, {}));
  {
    base::HistogramTester histogram_tester;
    NavigateToPageWithFrame(kFirstPartyHost);
    NavigateFrameTo(kThirdPartyHost2, "/browsing_data/site_data.html");
    ExpectCookie(GetFrame(), /*expected=*/false);
    EXPECT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

    EXPECT_EQ(
        histogram_tester.GetTotalSum(kThirdPartyCookieAllowMechanismHistogram),
        0);
  }

  EXPECT_TRUE(GetCookieSettings()->IsFullCookieAccessAllowed(
      third_party_url_3, net::SiteForCookies(), embedder_origin, {}));
  {
    base::HistogramTester histogram_tester;
    content::CookieChangeObserver observer(GetWebContents(),
                                           /*num_expected_calls=*/2);
    NavigateToPageWithFrame(kFirstPartyHost);
    NavigateFrameTo(kThirdPartyHost3, "/browsing_data/site_data.html");
    ExpectCookie(GetFrame(), /*expected=*/true);
    observer.Wait();
    EXPECT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

    histogram_tester.ExpectUniqueSample(
        kThirdPartyCookieAllowMechanismHistogram,
        ThirdPartyCookieAllowMechanism::
            kAllowBy3PCDMetadataSourceCriticalSector,
        2);
  }
}

class ManagerPrefsBrowserTest
    : public ManagerBrowserTest,
      public testing::WithParamInterface<
          std::tuple</*net::features::kThirdPartyStoragePartitioning*/ bool,
                     /*prefs::kBlockAll3pcToggleEnabled*/ bool>> {
 public:
  ManagerPrefsBrowserTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{net::features::kForceThirdPartyCookieBlocking, false},
         {net::features::kThirdPartyStoragePartitioning,
          ThirdPartyStoragePartitioningEnabled()},
         {net::features::kThirdPartyPartitionedStorageAllowedByDefault, true},
         {content_settings::features::kTrackingProtection3pcd, true}});
  }

  bool BlockAll3pcToggleEnabled() { return std::get<1>(GetParam()); }

  void SimulateTrackingProtectionSettings(Profile* profile = nullptr) {
    GetPrefs(profile)->SetDefaultPrefValue(
        prefs::kTrackingProtection3pcdEnabled, base::Value(true));
    EXPECT_TRUE(GetCookieSettings()->ShouldBlockThirdPartyCookies());

    GetPrefs(profile)->SetBoolean(prefs::kBlockAll3pcToggleEnabled,
                                  BlockAll3pcToggleEnabled());
  }

  bool GetTrackingProtection3pcdEnabledPref(Profile* profile = nullptr) {
    return GetPrefs(profile)->GetBoolean(prefs::kTrackingProtection3pcdEnabled);
  }

  bool GetBlockAll3pcToggleEnabledPref(Profile* profile = nullptr) {
    return GetPrefs(profile)->GetBoolean(prefs::kBlockAll3pcToggleEnabled);
  }

  bool ThirdPartyStoragePartitioningEnabled() const {
    return std::get<0>(GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The first Bool controls the enablement of
// `net::features::kThirdPartyStoragePartitioning`.
//
// The first Bool controls the
// enablement of `prefs::kBlockAll3pcToggleEnabled`.
INSTANTIATE_TEST_SUITE_P(All,
                         ManagerPrefsBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

IN_PROC_BROWSER_TEST_P(ManagerPrefsBrowserTest,
                       RelevantUserCookieSpecsPrecede) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  SimulateTrackingProtectionSettings();

  GURL first_party_url = https_server()->GetURL(kFirstPartyHost, "/");
  GURL third_party_url = https_server()->GetURL(kThirdPartyHost1, "/");

  // Simulates a user's preference: Blocks all requests to `third_party_url` to
  // access site data in 3P context.
  GetCookieSettings()->SetCookieSetting(third_party_url,
                                        ContentSetting::CONTENT_SETTING_BLOCK);
  EXPECT_EQ(GetCookieSettings()->GetCookieSetting(
                third_party_url, net::SiteForCookies(), GURL(),
                net::CookieSettingOverrides()),
            ContentSetting::CONTENT_SETTING_BLOCK);

  // Simulates a user's preference: Blocks all third parties requests on
  // `first_party_url` to access site data.
  GetCookieSettings()->SetThirdPartyCookieSetting(
      first_party_url, ContentSetting::CONTENT_SETTING_BLOCK);
  EXPECT_EQ(GetCookieSettings()->GetCookieSetting(
                GURL(), net::SiteForCookies(), first_party_url,
                net::CookieSettingOverrides()),
            ContentSetting::CONTENT_SETTING_BLOCK);

  const std::string wildcard_spec = "*";
  Metadata metadata;
  tpcd::metadata::helpers::AddEntryToMetadata(metadata, wildcard_spec,
                                              wildcard_spec);
  EXPECT_EQ(metadata.metadata_entries_size(), 1);
  MockComponentInstallation(metadata);
  EXPECT_THAT(
      ContentSettingsToString(ContentSettingsType::TPCD_METADATA_GRANTS),
      testing::ElementsAre("[*,*]:1"));

  EXPECT_EQ(!BlockAll3pcToggleEnabled(),
            GetCookieSettings()->MitigationsEnabledFor3pcd());
  EXPECT_EQ(GetCookieSettings()->GetCookieSetting(
                third_party_url, net::SiteForCookies(), first_party_url,
                net::CookieSettingOverrides()),
            ContentSetting::CONTENT_SETTING_BLOCK);

  NavigateToPageWithFrame(kFirstPartyHost);
  NavigateFrameTo(kThirdPartyHost1, "/browsing_data/site_data.html");
  ExpectCookie(GetFrame(), /*expected=*/false);
  ExpectStorage(GetFrame(), false, true,
                ThirdPartyStoragePartitioningEnabled());
}

IN_PROC_BROWSER_TEST_P(ManagerPrefsBrowserTest, NoSpecificBlockedCookieSpecs) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  SimulateTrackingProtectionSettings();

  {
    const GURL first_party_url = https_server()->GetURL(kFirstPartyHost, "/");
    const GURL third_party_url = https_server()->GetURL(kThirdPartyHost1, "/");

    const std::string primary_pattern_spec =
        ContentSettingsPattern::FromURLNoWildcard(third_party_url).ToString();
    const std::string secondary_pattern_spec =
        ContentSettingsPattern::FromURLNoWildcard(first_party_url).ToString();
    Metadata metadata;
    tpcd::metadata::helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                                                secondary_pattern_spec);
    EXPECT_EQ(metadata.metadata_entries_size(), 1);
    MockComponentInstallation(metadata);

    EXPECT_THAT(
        ContentSettingsToString(ContentSettingsType::TPCD_METADATA_GRANTS),
        testing::UnorderedElementsAreArray(
            {base::StringPrintf("[%s,%s]:%d", primary_pattern_spec.c_str(),
                                secondary_pattern_spec.c_str(), 1)}));

    bool expected = !BlockAll3pcToggleEnabled();
    EXPECT_EQ(expected, GetCookieSettings()->MitigationsEnabledFor3pcd());
    EXPECT_EQ(GetCookieSettings()->GetCookieSetting(
                  third_party_url, net::SiteForCookies(), first_party_url,
                  net::CookieSettingOverrides()),
              expected ? ContentSetting::CONTENT_SETTING_ALLOW
                       : ContentSetting::CONTENT_SETTING_BLOCK);

    NavigateToPageWithFrame(kFirstPartyHost);
    NavigateFrameTo(kThirdPartyHost1, "/browsing_data/site_data.html");
    ExpectCookie(GetFrame(), expected);
    ExpectStorage(GetFrame(), expected, false,
                  ThirdPartyStoragePartitioningEnabled());
  }

  // Make sure changes get propagated accordingly:
  {
    GetPrefs()->SetBoolean(prefs::kBlockAll3pcToggleEnabled, true);

    const GURL first_party_url = https_server()->GetURL(kFirstPartyHost, "/");
    const GURL third_party_url = https_server()->GetURL(kThirdPartyHost2, "/");

    const std::string primary_pattern_spec =
        ContentSettingsPattern::FromURLNoWildcard(third_party_url).ToString();
    const std::string secondary_pattern_spec =
        ContentSettingsPattern::FromURLNoWildcard(first_party_url).ToString();
    Metadata metadata;
    tpcd::metadata::helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                                                secondary_pattern_spec);
    EXPECT_EQ(metadata.metadata_entries_size(), 1);
    MockComponentInstallation(metadata);

    EXPECT_THAT(
        ContentSettingsToString(ContentSettingsType::TPCD_METADATA_GRANTS),
        testing::UnorderedElementsAreArray(
            {base::StringPrintf("[%s,%s]:%d", primary_pattern_spec.c_str(),
                                secondary_pattern_spec.c_str(), 1)}));

    bool expected = false;
    EXPECT_EQ(GetCookieSettings()->GetCookieSetting(
                  third_party_url, net::SiteForCookies(), first_party_url,
                  net::CookieSettingOverrides()),
              expected ? ContentSetting::CONTENT_SETTING_ALLOW
                       : ContentSetting::CONTENT_SETTING_BLOCK);

    NavigateToPageWithFrame(kFirstPartyHost);
    NavigateFrameTo(kThirdPartyHost2, "/browsing_data/site_data.html");
    ExpectCookie(GetFrame(), expected);
    ExpectStorage(GetFrame(), expected, false,
                  ThirdPartyStoragePartitioningEnabled());
  }
}

// ChromeOS doesn't support multiple profiles.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(ManagerPrefsBrowserTest,
                       NoSpecificBlockedCookieSpecs_AltRegularProfile) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  const GURL first_party_url = https_server()->GetURL(kFirstPartyHost, "/");
  const GURL third_party_url = https_server()->GetURL(kThirdPartyHost1, "/");

  const std::string primary_pattern_spec =
      ContentSettingsPattern::FromURLNoWildcard(third_party_url).ToString();
  const std::string secondary_pattern_spec =
      ContentSettingsPattern::FromURLNoWildcard(first_party_url).ToString();
  Metadata metadata;
  tpcd::metadata::helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                                              secondary_pattern_spec);
  EXPECT_EQ(metadata.metadata_entries_size(), 1);
  MockComponentInstallation(metadata);

  // Regular profile 1:
  {
    SimulateTrackingProtectionSettings();

    // Expected to be updated by the TPCD metadata manager instance as soon as
    // the profiles are created.
    EXPECT_THAT(
        ContentSettingsToString(ContentSettingsType::TPCD_METADATA_GRANTS),
        testing::UnorderedElementsAreArray(
            {base::StringPrintf("[%s,%s]:%d", primary_pattern_spec.c_str(),
                                secondary_pattern_spec.c_str(), 1)}));

    bool expected = !BlockAll3pcToggleEnabled();
    EXPECT_EQ(expected, GetCookieSettings()->MitigationsEnabledFor3pcd());
    EXPECT_EQ(GetCookieSettings()->GetCookieSetting(
                  third_party_url, net::SiteForCookies(), first_party_url,
                  net::CookieSettingOverrides()),
              expected ? ContentSetting::CONTENT_SETTING_ALLOW
                       : ContentSetting::CONTENT_SETTING_BLOCK);
  }

  // Regular profile 2:
  {
    Profile* profile = browser()->profile();
    Profile* alt_profile = CreateRegularProfile();
    EXPECT_NE(profile, alt_profile);
    EXPECT_NE(profile->GetProfileKey(), alt_profile->GetProfileKey());
    Browser* browser = CreateBrowser(alt_profile);

    SimulateTrackingProtectionSettings(alt_profile);

    // Expected to be updated by the TPCD metadata manager instance as soon as
    // the profiles are created.
    EXPECT_THAT(ContentSettingsToString(
                    ContentSettingsType::TPCD_METADATA_GRANTS, alt_profile),
                testing::UnorderedElementsAreArray({base::StringPrintf(
                    "[%s,%s]:%d", primary_pattern_spec.c_str(),
                    secondary_pattern_spec.c_str(), 1)}));

    bool expected = !BlockAll3pcToggleEnabled();
    EXPECT_EQ(expected,
              GetCookieSettings(alt_profile)->MitigationsEnabledFor3pcd());
    EXPECT_EQ(
        GetCookieSettings(alt_profile)
            ->GetCookieSetting(third_party_url, net::SiteForCookies(),
                               first_party_url, net::CookieSettingOverrides()),
        expected ? ContentSetting::CONTENT_SETTING_ALLOW
                 : ContentSetting::CONTENT_SETTING_BLOCK);

    NavigateToPageWithFrame(kFirstPartyHost, browser);
    NavigateFrameTo(kThirdPartyHost1, "/browsing_data/site_data.html", browser);
    ExpectCookie(GetFrame(browser), expected);
    ExpectStorage(GetFrame(browser), expected, /*setting_source_user=*/false,
                  ThirdPartyStoragePartitioningEnabled());
  }
}
#endif

IN_PROC_BROWSER_TEST_P(ManagerPrefsBrowserTest,
                       NoSpecificBlockedCookieSpecs_IncognitoProfile) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  SimulateTrackingProtectionSettings();

  const GURL first_party_url = https_server()->GetURL(kFirstPartyHost, "/");
  const GURL third_party_url = https_server()->GetURL(kThirdPartyHost1, "/");

  const std::string primary_pattern_spec =
      ContentSettingsPattern::FromURLNoWildcard(third_party_url).ToString();
  const std::string secondary_pattern_spec =
      ContentSettingsPattern::FromURLNoWildcard(first_party_url).ToString();
  Metadata metadata;
  tpcd::metadata::helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                                              secondary_pattern_spec);
  EXPECT_EQ(metadata.metadata_entries_size(), 1);
  MockComponentInstallation(metadata);

  // Regular profile:
  {
    EXPECT_THAT(
        ContentSettingsToString(ContentSettingsType::TPCD_METADATA_GRANTS),
        testing::UnorderedElementsAreArray(
            {base::StringPrintf("[%s,%s]:%d", primary_pattern_spec.c_str(),
                                secondary_pattern_spec.c_str(), 1)}));

    bool expected = !BlockAll3pcToggleEnabled();
    EXPECT_EQ(expected, GetCookieSettings()->MitigationsEnabledFor3pcd());
    EXPECT_EQ(GetCookieSettings()->GetCookieSetting(
                  third_party_url, net::SiteForCookies(), first_party_url,
                  net::CookieSettingOverrides()),
              expected ? ContentSetting::CONTENT_SETTING_ALLOW
                       : ContentSetting::CONTENT_SETTING_BLOCK);
  }

  // Incognito profile:
  {
    Profile* incognito_profile =
        browser()->profile()->GetPrimaryOTRProfile(true);
    EXPECT_TRUE(incognito_profile->IsIncognitoProfile());
    EXPECT_TRUE(incognito_profile->IsOffTheRecord());
    EXPECT_EQ(GetTrackingProtection3pcdEnabledPref(incognito_profile), true);
    EXPECT_EQ(GetBlockAll3pcToggleEnabledPref(incognito_profile),
              BlockAll3pcToggleEnabled());
    Browser* browser = CreateBrowser(incognito_profile);

    // Expected to be left unaffected by the TPCD metadata manager instance
    // is spawned for this profile. And, the content setting will only be
    // inherited by incognito if it's less permissive.
    EXPECT_TRUE(
        ContentSettingsToString(ContentSettingsType::TPCD_METADATA_GRANTS,
                                incognito_profile)
            .empty());

    EXPECT_FALSE(
        GetCookieSettings(incognito_profile)->MitigationsEnabledFor3pcd());
    bool expected = false;
    EXPECT_EQ(
        GetCookieSettings(incognito_profile)
            ->GetCookieSetting(third_party_url, net::SiteForCookies(),
                               first_party_url, net::CookieSettingOverrides()),
        expected ? ContentSetting::CONTENT_SETTING_ALLOW
                 : ContentSetting::CONTENT_SETTING_BLOCK);

    NavigateToPageWithFrame(kFirstPartyHost, browser);
    NavigateFrameTo(kThirdPartyHost1, "/browsing_data/site_data.html", browser);
    ExpectCookie(GetFrame(browser), expected);
    ExpectStorage(GetFrame(browser), expected, /*setting_source_user=*/false,
                  ThirdPartyStoragePartitioningEnabled());
  }
}

// ChromeOS doesn't support multiple profiles.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(ManagerPrefsBrowserTest,
                       NoSpecificBlockedCookieSpecs_GuestProfile) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  SimulateTrackingProtectionSettings();

  const GURL first_party_url = https_server()->GetURL(kFirstPartyHost, "/");
  const GURL third_party_url = https_server()->GetURL(kThirdPartyHost1, "/");

  const std::string primary_pattern_spec =
      ContentSettingsPattern::FromURLNoWildcard(third_party_url).ToString();
  const std::string secondary_pattern_spec =
      ContentSettingsPattern::FromURLNoWildcard(first_party_url).ToString();
  Metadata metadata;
  tpcd::metadata::helpers::AddEntryToMetadata(metadata, primary_pattern_spec,
                                              secondary_pattern_spec);
  EXPECT_EQ(metadata.metadata_entries_size(), 1);
  MockComponentInstallation(metadata);

  // Regular profile:
  {
    EXPECT_THAT(
        ContentSettingsToString(ContentSettingsType::TPCD_METADATA_GRANTS),
        testing::UnorderedElementsAreArray(
            {base::StringPrintf("[%s,%s]:%d", primary_pattern_spec.c_str(),
                                secondary_pattern_spec.c_str(), 1)}));

    bool expected = !BlockAll3pcToggleEnabled();
    EXPECT_EQ(expected, GetCookieSettings()->MitigationsEnabledFor3pcd());
    EXPECT_EQ(GetCookieSettings()->GetCookieSetting(
                  third_party_url, net::SiteForCookies(), first_party_url,
                  net::CookieSettingOverrides()),
              expected ? ContentSetting::CONTENT_SETTING_ALLOW
                       : ContentSetting::CONTENT_SETTING_BLOCK);
  }

  // Guest profile:
  {
    auto& original_guest_profile = CreateOriginalGuestProfile();
    SimulateTrackingProtectionSettings(&original_guest_profile);

    auto* guest_profile = original_guest_profile.GetPrimaryOTRProfile(
        /*create_if_needed=*/true);
    EXPECT_FALSE(guest_profile->IsRegularProfile());
    EXPECT_FALSE(guest_profile->IsIncognitoProfile());
    EXPECT_TRUE(guest_profile->IsOffTheRecord());
    EXPECT_TRUE(guest_profile->IsGuestSession());
    EXPECT_EQ(GetTrackingProtection3pcdEnabledPref(guest_profile), true);
    EXPECT_EQ(GetBlockAll3pcToggleEnabledPref(guest_profile),
              BlockAll3pcToggleEnabled());
    Browser* browser = CreateBrowser(guest_profile);

    EXPECT_THAT(ContentSettingsToString(
                    ContentSettingsType::TPCD_METADATA_GRANTS, guest_profile),
                testing::UnorderedElementsAreArray({base::StringPrintf(
                    "[%s,%s]:%d", primary_pattern_spec.c_str(),
                    secondary_pattern_spec.c_str(), 1)}));

    bool expected = !BlockAll3pcToggleEnabled();
    EXPECT_EQ(expected,
              GetCookieSettings(guest_profile)->MitigationsEnabledFor3pcd());
    EXPECT_EQ(
        GetCookieSettings(guest_profile)
            ->GetCookieSetting(third_party_url, net::SiteForCookies(),
                               first_party_url, net::CookieSettingOverrides()),
        expected ? ContentSetting::CONTENT_SETTING_ALLOW
                 : ContentSetting::CONTENT_SETTING_BLOCK);

    NavigateToPageWithFrame(kFirstPartyHost, browser);
    NavigateFrameTo(kThirdPartyHost1, "/browsing_data/site_data.html", browser);
    ExpectCookie(GetFrame(browser), expected);
    ExpectStorage(GetFrame(browser), expected, /*setting_source_user=*/false,
                  ThirdPartyStoragePartitioningEnabled());
  }
}

class CookieControlsModePrefManagerBrowserTest : public ManagerBrowserTest {
 public:
  CookieControlsModePrefManagerBrowserTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{privacy_sandbox::kAddLimit3pcsSetting, true},
         {net::features::kForceThirdPartyCookieBlocking, false},
         {net::features::kThirdPartyStoragePartitioning, false},
         {net::features::kThirdPartyPartitionedStorageAllowedByDefault, true},
         {content_settings::features::kTrackingProtection3pcd, false}});
  }

  void AddWildcardMetadataGrant() {
    const std::string wildcard_spec = "*";
    Metadata metadata;
    // Adds a wildcard spec to both primary (third party contexts) and secondary
    // (first party contexts) pattern specs.
    tpcd::metadata::helpers::AddEntryToMetadata(
        metadata, /*primary_pattern_spec=*/wildcard_spec,
        /*secondary_pattern_spec=*/wildcard_spec);
    EXPECT_EQ(metadata.metadata_entries_size(), 1);
    MockComponentInstallation(metadata);
    EXPECT_THAT(
        ContentSettingsToString(ContentSettingsType::TPCD_METADATA_GRANTS),
        testing::ElementsAre("[*,*]:1"));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CookieControlsModePrefManagerBrowserTest,
                       EnablesMitigationsWhenThirdPartyCookiesLimited) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  GURL first_party_url = https_server()->GetURL(kFirstPartyHost, "/");
  GURL third_party_url = https_server()->GetURL(kThirdPartyHost1, "/");

  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kLimited));
  AddWildcardMetadataGrant();

  EXPECT_TRUE(GetCookieSettings()->MitigationsEnabledFor3pcd());
  EXPECT_EQ(GetCookieSettings()->GetCookieSetting(
                third_party_url, net::SiteForCookies(), first_party_url,
                net::CookieSettingOverrides()),
            ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame(kFirstPartyHost);
  NavigateFrameTo(kThirdPartyHost1, "/browsing_data/site_data.html");
  ExpectCookie(GetFrame(), /*expected=*/true);
  ExpectStorage(GetFrame(), /*expected=*/true, /*setting_source_user=*/false,
                /*is_3psp_enabled=*/false);
}

IN_PROC_BROWSER_TEST_F(CookieControlsModePrefManagerBrowserTest,
                       DisablesMitigationsWhenThirdPartyCookiesBlocked) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  GURL first_party_url = https_server()->GetURL(kFirstPartyHost, "/");
  GURL third_party_url = https_server()->GetURL(kThirdPartyHost1, "/");

  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));
  AddWildcardMetadataGrant();

  EXPECT_FALSE(GetCookieSettings()->MitigationsEnabledFor3pcd());
  EXPECT_EQ(GetCookieSettings()->GetCookieSetting(
                third_party_url, net::SiteForCookies(), first_party_url,
                net::CookieSettingOverrides()),
            ContentSetting::CONTENT_SETTING_BLOCK);

  NavigateToPageWithFrame(kFirstPartyHost);
  NavigateFrameTo(kThirdPartyHost1, "/browsing_data/site_data.html");
  ExpectCookie(GetFrame(), /*expected=*/false);
  ExpectStorage(GetFrame(), /*expected=*/false, /*setting_source_user=*/false,
                /*is_3psp_enabled=*/false);
}

#endif

}  // namespace tpcd::metadata
