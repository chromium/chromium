// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/metadata/updater_service.h"

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
#include "chrome/browser/tpcd/metadata/updater_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_waiter.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/tpcd/metadata/parser_test_helper.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace tpcd::metadata {
namespace {
const base::FilePath::CharType kComponentFileName[] =
    FILE_PATH_LITERAL("metadata.pb");

const char* kFirstPartyHost = "a.test";
const char* kThirdPartyHost1 = "b.test";
const char* kThirdPartyHost2 = "c.test";

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
}  // namespace

class UpdaterServiceBrowserTest : public PlatformBrowserTest {
 public:
  ~UpdaterServiceBrowserTest() override = default;

  UpdaterServiceBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    CHECK(fake_install_dir_.CreateUniqueTempDir());
    CHECK(fake_install_dir_.IsValid());
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kTpcdMetadataGrants);
  }

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

  UpdaterService* updater_service() {
    return UpdaterServiceFactory::GetForProfile(browser()->profile());
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

  void NavigateFrameTo(const std::string& host,
                       const std::string& path,
                       Browser* alt_browser = nullptr) {
    GURL url = https_server()->GetURL(host, path);
    content::WebContents* web_contents = (alt_browser ? alt_browser : browser())
                                             ->tab_strip_model()
                                             ->GetActiveWebContents();
    EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", url));
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
         GetCookieSettings(profile)->GetTpcdMetadataGrantsForTesting()) {
      settings.emplace_back(base::StringPrintf(
          "[%s,%s]:%d", setting.primary_pattern.ToString().c_str(),
          setting.secondary_pattern.ToString().c_str(),
          content_settings::ValueToContentSetting(setting.setting_value)));
    }
    return settings;
  }

  void ExpectCookie(content::RenderFrameHost* frame, const bool expected) {
    EXPECT_EQ(content::EvalJs(frame, "navigator.cookieEnabled").ExtractBool(),
              expected);

    if (expected) {
      EXPECT_TRUE(content::EvalJs(frame, "setCookie()").ExtractBool());
      EXPECT_TRUE(content::EvalJs(frame, "hasCookie()").ExtractBool());
    }
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

IN_PROC_BROWSER_TEST_F(UpdaterServiceBrowserTest,
                       ContentSettingsForOneType_Empty) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  const GURL kEmbedded = GURL("http://www.bar.com");
  const GURL kEmbedder = GURL("http://www.foo.com");

  ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrantsForTesting().size(), 0u);
  EXPECT_EQ(
      GetCookieSettings()->GetContentSettingForTesting(
          kEmbedded, kEmbedder, ContentSettingsType::TPCD_METADATA_GRANTS),
      ContentSetting::CONTENT_SETTING_BLOCK);

  std::vector<MetadataPair> metadata_pairs;
  Metadata metadata = MakeMetadataProtoFromVectorOfPair(metadata_pairs);
  ASSERT_EQ(metadata.metadata_entries_size(), 0);

  MockComponentInstallation(metadata);

  ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrantsForTesting().size(), 0u);
  EXPECT_EQ(
      GetCookieSettings()->GetContentSettingForTesting(
          kEmbedded, kEmbedder, ContentSettingsType::TPCD_METADATA_GRANTS),
      ContentSetting::CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(UpdaterServiceBrowserTest,
                       ContentSettingsForOneType_SuccessfullyUpdated) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  const GURL kEmbedded = GURL("http://www.bar.com");
  const GURL kEmbedder = GURL("http://www.foo.com");

  ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrantsForTesting().size(), 0u);
  EXPECT_EQ(
      ContentSetting::CONTENT_SETTING_BLOCK,
      GetCookieSettings()->GetContentSettingForTesting(
          kEmbedded, kEmbedder, ContentSettingsType::TPCD_METADATA_GRANTS));

  const std::string primary_pattern_spec = "[*.]bar.com";
  const std::string secondary_pattern_spec = "[*.]foo.com";

  std::vector<MetadataPair> metadata_pairs;
  metadata_pairs.emplace_back(primary_pattern_spec, secondary_pattern_spec);
  Metadata metadata = MakeMetadataProtoFromVectorOfPair(metadata_pairs);
  ASSERT_EQ(metadata.metadata_entries_size(), 1);

  MockComponentInstallation(metadata);

  ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrantsForTesting().size(), 1u);
  EXPECT_EQ(
      ContentSetting::CONTENT_SETTING_ALLOW,
      GetCookieSettings()->GetContentSettingForTesting(
          kEmbedded, kEmbedder, ContentSettingsType::TPCD_METADATA_GRANTS));
}

IN_PROC_BROWSER_TEST_F(UpdaterServiceBrowserTest,
                       ContentSettingsForOneType_SuccessfullyCleared) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  const GURL kEmbedded1 = GURL("http://www.bar.com");
  const GURL kEmbedder1 = GURL("http://www.foo.com");
  const GURL kEmbedded2 = GURL("http://www.baz.com");
  const GURL kEmbedder2 = GURL("http://www.daz.com");

  {
    const std::string primary_pattern_spec = "[*.]bar.com";
    const std::string secondary_pattern_spec = "[*.]foo.com";

    std::vector<MetadataPair> metadata_pairs;
    metadata_pairs.emplace_back(primary_pattern_spec, secondary_pattern_spec);
    Metadata metadata = MakeMetadataProtoFromVectorOfPair(metadata_pairs);
    ASSERT_EQ(metadata.metadata_entries_size(), 1);

    ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrantsForTesting().size(),
              0u);
    EXPECT_EQ(

        GetCookieSettings()->GetContentSettingForTesting(
            kEmbedded1, kEmbedder1, ContentSettingsType::TPCD_METADATA_GRANTS),
        ContentSetting::CONTENT_SETTING_BLOCK);

    MockComponentInstallation(metadata);

    ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrantsForTesting().size(),
              1u);
    EXPECT_EQ(
        GetCookieSettings()->GetContentSettingForTesting(
            kEmbedded1, kEmbedder1, ContentSettingsType::TPCD_METADATA_GRANTS),
        ContentSetting::CONTENT_SETTING_ALLOW);
  }

  {
    const std::string primary_pattern_spec = "[*.]baz.com";
    const std::string secondary_pattern_spec = "[*.]daz.com";

    std::vector<MetadataPair> metadata_pairs;
    metadata_pairs.emplace_back(primary_pattern_spec, secondary_pattern_spec);
    Metadata metadata = MakeMetadataProtoFromVectorOfPair(metadata_pairs);
    ASSERT_EQ(metadata.metadata_entries_size(), 1);

    ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrantsForTesting().size(),
              1u);
    EXPECT_EQ(
        GetCookieSettings()->GetContentSettingForTesting(
            kEmbedded1, kEmbedder1, ContentSettingsType::TPCD_METADATA_GRANTS),
        ContentSetting::CONTENT_SETTING_ALLOW);
    EXPECT_EQ(
        GetCookieSettings()->GetContentSettingForTesting(
            kEmbedded2, kEmbedder2, ContentSettingsType::TPCD_METADATA_GRANTS),
        ContentSetting::CONTENT_SETTING_BLOCK);

    MockComponentInstallation(metadata);

    ASSERT_EQ(GetCookieSettings()->GetTpcdMetadataGrantsForTesting().size(),
              1u);
    EXPECT_EQ(
        GetCookieSettings()->GetContentSettingForTesting(
            kEmbedded1, kEmbedder1, ContentSettingsType::TPCD_METADATA_GRANTS),
        ContentSetting::CONTENT_SETTING_BLOCK);
    EXPECT_EQ(
        GetCookieSettings()->GetContentSettingForTesting(
            kEmbedded2, kEmbedder2, ContentSettingsType::TPCD_METADATA_GRANTS),
        ContentSetting::CONTENT_SETTING_ALLOW);
  }
}

class UpdaterServiceCookiePrefsBrowserTest
    : public UpdaterServiceBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  UpdaterServiceCookiePrefsBrowserTest() {
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

IN_PROC_BROWSER_TEST_P(UpdaterServiceCookiePrefsBrowserTest,
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
                third_party_url, GURL(), net::CookieSettingOverrides()),
            ContentSetting::CONTENT_SETTING_BLOCK);

  // Simulates a user's preference: Blocks all third parties requests on
  // `first_party_url` to access site data.
  GetCookieSettings()->SetThirdPartyCookieSetting(
      first_party_url, ContentSetting::CONTENT_SETTING_BLOCK);
  EXPECT_EQ(GetCookieSettings()->GetCookieSetting(
                GURL(), first_party_url, net::CookieSettingOverrides()),
            ContentSetting::CONTENT_SETTING_BLOCK);

  const std::string wildcard_spec = "*";
  Metadata metadata =
      MakeMetadataProtoFromVectorOfPair({{wildcard_spec, wildcard_spec}});
  EXPECT_EQ(metadata.metadata_entries_size(), 1);
  MockComponentInstallation(metadata);
  EXPECT_THAT(
      ContentSettingsToString(ContentSettingsType::TPCD_METADATA_GRANTS),
      testing::ElementsAre("[*,*]:1"));

  EXPECT_EQ(GetCookieSettings()->ShouldConsider3pcdMetadataGrantsSettings(),
            GetCookieSettings()->MitigationsEnabledFor3pcd());
  EXPECT_EQ(
      GetCookieSettings()->GetCookieSetting(third_party_url, first_party_url,
                                            net::CookieSettingOverrides()),
      ContentSetting::CONTENT_SETTING_BLOCK);

  NavigateToPageWithFrame(kFirstPartyHost);
  NavigateFrameTo(kThirdPartyHost1, "/browsing_data/site_data.html");
  ExpectCookie(GetFrame(), false);
  ExpectStorage(GetFrame(), false, true,
                ThirdPartyStoragePartitioningEnabled());
}

IN_PROC_BROWSER_TEST_P(UpdaterServiceCookiePrefsBrowserTest,
                       NoSpecificBlockedCookieSpecs) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  SimulateTrackingProtectionSettings();

  {
    const GURL first_party_url = https_server()->GetURL(kFirstPartyHost, "/");
    const GURL third_party_url = https_server()->GetURL(kThirdPartyHost1, "/");

    const std::string primary_pattern_spec =
        ContentSettingsPattern::FromURLNoWildcard(third_party_url).ToString();
    const std::string secondary_pattern_spec =
        ContentSettingsPattern::FromURLNoWildcard(first_party_url).ToString();
    Metadata metadata = MakeMetadataProtoFromVectorOfPair(
        {{primary_pattern_spec, secondary_pattern_spec}});
    EXPECT_EQ(metadata.metadata_entries_size(), 1);
    MockComponentInstallation(metadata);

    EXPECT_THAT(
        ContentSettingsToString(ContentSettingsType::TPCD_METADATA_GRANTS),
        testing::UnorderedElementsAreArray(
            {base::StringPrintf("[%s,%s]:%d", primary_pattern_spec.c_str(),
                                secondary_pattern_spec.c_str(), 1)}));

    EXPECT_EQ(GetCookieSettings()->ShouldConsider3pcdMetadataGrantsSettings(),
              GetCookieSettings()->MitigationsEnabledFor3pcd());
    bool expected =
        GetCookieSettings()->ShouldConsider3pcdMetadataGrantsSettings();
    EXPECT_EQ(
        GetCookieSettings()->GetCookieSetting(third_party_url, first_party_url,
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
    Metadata metadata = MakeMetadataProtoFromVectorOfPair(
        {{primary_pattern_spec, secondary_pattern_spec}});
    EXPECT_EQ(metadata.metadata_entries_size(), 1);
    MockComponentInstallation(metadata);

    EXPECT_THAT(
        ContentSettingsToString(ContentSettingsType::TPCD_METADATA_GRANTS),
        testing::UnorderedElementsAreArray(
            {base::StringPrintf("[%s,%s]:%d", primary_pattern_spec.c_str(),
                                secondary_pattern_spec.c_str(), 1)}));

    bool expected = false;
    EXPECT_EQ(
        GetCookieSettings()->GetCookieSetting(third_party_url, first_party_url,
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
IN_PROC_BROWSER_TEST_P(UpdaterServiceCookiePrefsBrowserTest,
                       NoSpecificBlockedCookieSpecs_AltRegularProfile) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  const GURL first_party_url = https_server()->GetURL(kFirstPartyHost, "/");
  const GURL third_party_url = https_server()->GetURL(kThirdPartyHost1, "/");

  const std::string primary_pattern_spec =
      ContentSettingsPattern::FromURLNoWildcard(third_party_url).ToString();
  const std::string secondary_pattern_spec =
      ContentSettingsPattern::FromURLNoWildcard(first_party_url).ToString();
  std::vector<MetadataPair> metadata_pairs;
  metadata_pairs.emplace_back(primary_pattern_spec, secondary_pattern_spec);
  Metadata metadata = MakeMetadataProtoFromVectorOfPair(metadata_pairs);
  EXPECT_EQ(metadata.metadata_entries_size(), 1);
  MockComponentInstallation(metadata);

  // Regular profile 1:
  {
    SimulateTrackingProtectionSettings();

    // Expected to be updated by the TPCD metadata updater service as soon as
    // the profiles are created.
    EXPECT_THAT(
        ContentSettingsToString(ContentSettingsType::TPCD_METADATA_GRANTS),
        testing::UnorderedElementsAreArray(
            {base::StringPrintf("[%s,%s]:%d", primary_pattern_spec.c_str(),
                                secondary_pattern_spec.c_str(), 1)}));

    EXPECT_EQ(GetCookieSettings()->ShouldConsider3pcdMetadataGrantsSettings(),
              GetCookieSettings()->MitigationsEnabledFor3pcd());
    bool expected =
        GetCookieSettings()->ShouldConsider3pcdMetadataGrantsSettings();
    EXPECT_EQ(
        GetCookieSettings()->GetCookieSetting(third_party_url, first_party_url,
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

    // Expected to be updated by the TPCD metadata updater service as soon as
    // the profiles are created.
    EXPECT_THAT(ContentSettingsToString(
                    ContentSettingsType::TPCD_METADATA_GRANTS, alt_profile),
                testing::UnorderedElementsAreArray({base::StringPrintf(
                    "[%s,%s]:%d", primary_pattern_spec.c_str(),
                    secondary_pattern_spec.c_str(), 1)}));

    EXPECT_EQ(GetCookieSettings(alt_profile)
                  ->ShouldConsider3pcdMetadataGrantsSettings(),
              GetCookieSettings(alt_profile)->MitigationsEnabledFor3pcd());
    bool expected = GetCookieSettings(alt_profile)
                        ->ShouldConsider3pcdMetadataGrantsSettings();
    EXPECT_EQ(GetCookieSettings(alt_profile)
                  ->GetCookieSetting(third_party_url, first_party_url,
                                     net::CookieSettingOverrides()),
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

IN_PROC_BROWSER_TEST_P(UpdaterServiceCookiePrefsBrowserTest,
                       NoSpecificBlockedCookieSpecs_IncognitoProfile) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  SimulateTrackingProtectionSettings();

  const GURL first_party_url = https_server()->GetURL(kFirstPartyHost, "/");
  const GURL third_party_url = https_server()->GetURL(kThirdPartyHost1, "/");

  const std::string primary_pattern_spec =
      ContentSettingsPattern::FromURLNoWildcard(third_party_url).ToString();
  const std::string secondary_pattern_spec =
      ContentSettingsPattern::FromURLNoWildcard(first_party_url).ToString();
  std::vector<MetadataPair> metadata_pairs;
  metadata_pairs.emplace_back(primary_pattern_spec, secondary_pattern_spec);
  Metadata metadata = MakeMetadataProtoFromVectorOfPair(metadata_pairs);
  EXPECT_EQ(metadata.metadata_entries_size(), 1);
  MockComponentInstallation(metadata);

  // Regular profile:
  {
    EXPECT_THAT(
        ContentSettingsToString(ContentSettingsType::TPCD_METADATA_GRANTS),
        testing::UnorderedElementsAreArray(
            {base::StringPrintf("[%s,%s]:%d", primary_pattern_spec.c_str(),
                                secondary_pattern_spec.c_str(), 1)}));

    bool expected =
        GetCookieSettings()->ShouldConsider3pcdMetadataGrantsSettings();
    EXPECT_EQ(
        GetCookieSettings()->GetCookieSetting(third_party_url, first_party_url,
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

    // Expected to be left unaffected by the TPCD metadata updater as no service
    // is spawned for this profile. And, the content setting will only be
    // inherited by incognito if it's less permissive.
    EXPECT_TRUE(
        ContentSettingsToString(ContentSettingsType::TPCD_METADATA_GRANTS,
                                incognito_profile)
            .empty());

    EXPECT_FALSE(GetCookieSettings(incognito_profile)
                     ->ShouldConsider3pcdMetadataGrantsSettings());
    bool expected = false;
    EXPECT_EQ(GetCookieSettings(incognito_profile)
                  ->GetCookieSetting(third_party_url, first_party_url,
                                     net::CookieSettingOverrides()),
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
IN_PROC_BROWSER_TEST_P(UpdaterServiceCookiePrefsBrowserTest,
                       NoSpecificBlockedCookieSpecs_GuestProfile) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  SimulateTrackingProtectionSettings();

  const GURL first_party_url = https_server()->GetURL(kFirstPartyHost, "/");
  const GURL third_party_url = https_server()->GetURL(kThirdPartyHost1, "/");

  const std::string primary_pattern_spec =
      ContentSettingsPattern::FromURLNoWildcard(third_party_url).ToString();
  const std::string secondary_pattern_spec =
      ContentSettingsPattern::FromURLNoWildcard(first_party_url).ToString();
  std::vector<MetadataPair> metadata_pairs;
  metadata_pairs.emplace_back(primary_pattern_spec, secondary_pattern_spec);
  Metadata metadata = MakeMetadataProtoFromVectorOfPair(metadata_pairs);
  EXPECT_EQ(metadata.metadata_entries_size(), 1);
  MockComponentInstallation(metadata);

  // Regular profile:
  {
    EXPECT_THAT(
        ContentSettingsToString(ContentSettingsType::TPCD_METADATA_GRANTS),
        testing::UnorderedElementsAreArray(
            {base::StringPrintf("[%s,%s]:%d", primary_pattern_spec.c_str(),
                                secondary_pattern_spec.c_str(), 1)}));

    bool expected =
        GetCookieSettings()->ShouldConsider3pcdMetadataGrantsSettings();
    EXPECT_EQ(
        GetCookieSettings()->GetCookieSetting(third_party_url, first_party_url,
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

    bool expected = GetCookieSettings(guest_profile)
                        ->ShouldConsider3pcdMetadataGrantsSettings();
    EXPECT_EQ(GetCookieSettings(guest_profile)
                  ->GetCookieSetting(third_party_url, first_party_url,
                                     net::CookieSettingOverrides()),
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

// The first Bool controls the enablement of
// `net::features::kThirdPartyStoragePartitioning`.
//
// The first Bool controls the
// enablement of `prefs::kBlockAll3pcToggleEnabled`.
INSTANTIATE_TEST_SUITE_P(All,
                         UpdaterServiceCookiePrefsBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()));
}  // namespace tpcd::metadata
