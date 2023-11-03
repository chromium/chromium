// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "net/base/schemeful_site.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

namespace extensions {

using ContextType = ExtensionApiTest::ContextType;

class ExtensionContentSettingsApiTest : public ExtensionApiTest {
 public:
  explicit ExtensionContentSettingsApiTest(
      ContextType context_type = ContextType::kNone)
      : ExtensionApiTest(context_type) {}
  ~ExtensionContentSettingsApiTest() override = default;
  ExtensionContentSettingsApiTest(const ExtensionContentSettingsApiTest&) =
      delete;
  ExtensionContentSettingsApiTest& operator=(
      const ExtensionContentSettingsApiTest&) = delete;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    // The browser might get closed later (and therefore be destroyed), so we
    // save the profile.
    profile_ = browser()->profile();

    // Closing the last browser window also releases a KeepAlive. Make
    // sure it's not the last one, so the message loop doesn't quit
    // unexpectedly.
    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::BROWSER, KeepAliveRestartOption::DISABLED);
    profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
        profile_, ProfileKeepAliveOrigin::kBrowserWindow);
  }

  void TearDownOnMainThread() override {
    profile_keep_alive_.reset();
    // BrowserProcess::Shutdown() needs to be called in a message loop, so we
    // post a task to release the keep alive, then run the message loop.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&std::unique_ptr<ScopedKeepAlive>::reset,
                                  base::Unretained(&keep_alive_), nullptr));
    content::RunAllPendingInMessageLoop();

    ExtensionApiTest::TearDownOnMainThread();
  }

 protected:
  void CheckContentSettingsSet() {
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(profile_);
    content_settings::CookieSettings* cookie_settings =
        CookieSettingsFactory::GetForProfile(profile_).get();

    // Check default content settings by using an unknown URL.
    GURL example_url("http://www.example.com");
    EXPECT_TRUE(cookie_settings->IsFullCookieAccessAllowed(
        example_url, net::SiteForCookies::FromUrl(example_url),
        url::Origin::Create(example_url), net::CookieSettingOverrides()));
    EXPECT_TRUE(cookie_settings->IsCookieSessionOnly(example_url));
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              map->GetContentSetting(example_url, example_url,
                                     ContentSettingsType::IMAGES));
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              map->GetContentSetting(example_url, example_url,
                                     ContentSettingsType::JAVASCRIPT));
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              map->GetContentSetting(example_url, example_url,
                                     ContentSettingsType::POPUPS));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(example_url, example_url,
                                     ContentSettingsType::GEOLOCATION));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(example_url, example_url,
                                     ContentSettingsType::NOTIFICATIONS));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(example_url, example_url,
                                     ContentSettingsType::MEDIASTREAM_MIC));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(example_url, example_url,
                                     ContentSettingsType::MEDIASTREAM_CAMERA));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(example_url, example_url,
                                     ContentSettingsType::AUTOMATIC_DOWNLOADS));
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              map->GetContentSetting(example_url, example_url,
                                     ContentSettingsType::AUTOPLAY));
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              map->GetContentSetting(example_url, example_url,
                                     ContentSettingsType::ANTI_ABUSE));

    // Check content settings for www.google.com
    GURL url("http://www.google.com");
    EXPECT_FALSE(cookie_settings->IsFullCookieAccessAllowed(
        url, net::SiteForCookies::FromUrl(url), url::Origin::Create(url),
        net::CookieSettingOverrides()));
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              map->GetContentSetting(url, url, ContentSettingsType::IMAGES));
    EXPECT_EQ(
        CONTENT_SETTING_BLOCK,
        map->GetContentSetting(url, url, ContentSettingsType::JAVASCRIPT));
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              map->GetContentSetting(url, url, ContentSettingsType::POPUPS));
    EXPECT_EQ(
        CONTENT_SETTING_BLOCK,
        map->GetContentSetting(url, url, ContentSettingsType::GEOLOCATION));
    EXPECT_EQ(
        CONTENT_SETTING_BLOCK,
        map->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
    EXPECT_EQ(
        CONTENT_SETTING_BLOCK,
        map->GetContentSetting(url, url, ContentSettingsType::MEDIASTREAM_MIC));
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              map->GetContentSetting(url, url,
                                     ContentSettingsType::MEDIASTREAM_CAMERA));
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              map->GetContentSetting(url, url,
                                     ContentSettingsType::AUTOMATIC_DOWNLOADS));
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              map->GetContentSetting(url, url, ContentSettingsType::AUTOPLAY));
    EXPECT_EQ(
        CONTENT_SETTING_BLOCK,
        map->GetContentSetting(url, url, ContentSettingsType::ANTI_ABUSE));
  }

  void CheckContentSettingsDefault() {
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(profile_);
    content_settings::CookieSettings* cookie_settings =
        CookieSettingsFactory::GetForProfile(profile_).get();

    // Check content settings for www.google.com
    GURL url("http://www.google.com");
    EXPECT_TRUE(cookie_settings->IsFullCookieAccessAllowed(
        url, net::SiteForCookies::FromUrl(url), url::Origin::Create(url),
        net::CookieSettingOverrides()));
    EXPECT_FALSE(cookie_settings->IsCookieSessionOnly(url));
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              map->GetContentSetting(url, url, ContentSettingsType::IMAGES));
    EXPECT_EQ(
        CONTENT_SETTING_ALLOW,
        map->GetContentSetting(url, url, ContentSettingsType::JAVASCRIPT));
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              map->GetContentSetting(url, url, ContentSettingsType::POPUPS));
    EXPECT_EQ(
        CONTENT_SETTING_ASK,
        map->GetContentSetting(url, url, ContentSettingsType::GEOLOCATION));
    EXPECT_EQ(
        CONTENT_SETTING_ASK,
        map->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
    EXPECT_EQ(
        CONTENT_SETTING_ASK,
        map->GetContentSetting(url, url, ContentSettingsType::MEDIASTREAM_MIC));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(url, url,
                                     ContentSettingsType::MEDIASTREAM_CAMERA));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              map->GetContentSetting(url, url,
                                     ContentSettingsType::AUTOMATIC_DOWNLOADS));
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              map->GetContentSetting(url, url, ContentSettingsType::AUTOPLAY));
    EXPECT_EQ(
        CONTENT_SETTING_ALLOW,
        map->GetContentSetting(url, url, ContentSettingsType::ANTI_ABUSE));
  }

  // Returns a snapshot of content settings for a given URL.
  std::vector<int> GetContentSettingsSnapshot(const GURL& url) {
    std::vector<int> content_settings;

    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(profile_);
    content_settings::CookieSettings* cookie_settings =
        CookieSettingsFactory::GetForProfile(profile_).get();

    content_settings.push_back(cookie_settings->IsFullCookieAccessAllowed(
        url, net::SiteForCookies::FromUrl(url), url::Origin::Create(url),
        net::CookieSettingOverrides()));
    content_settings.push_back(cookie_settings->IsCookieSessionOnly(url));
    content_settings.push_back(
        map->GetContentSetting(url, url, ContentSettingsType::IMAGES));
    content_settings.push_back(
        map->GetContentSetting(url, url, ContentSettingsType::JAVASCRIPT));
    content_settings.push_back(
        map->GetContentSetting(url, url, ContentSettingsType::POPUPS));
    content_settings.push_back(
        map->GetContentSetting(url, url, ContentSettingsType::GEOLOCATION));
    content_settings.push_back(
        map->GetContentSetting(url, url, ContentSettingsType::NOTIFICATIONS));
    content_settings.push_back(
        map->GetContentSetting(url, url, ContentSettingsType::MEDIASTREAM_MIC));
    content_settings.push_back(map->GetContentSetting(
        url, url, ContentSettingsType::MEDIASTREAM_CAMERA));
    content_settings.push_back(map->GetContentSetting(
        url, url, ContentSettingsType::AUTOMATIC_DOWNLOADS));
    content_settings.push_back(
        map->GetContentSetting(url, url, ContentSettingsType::AUTOPLAY));
    return content_settings;
  }

 private:
  raw_ptr<Profile, AcrossTasksDanglingUntriaged> profile_ = nullptr;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
};

class ExtensionContentSettingsApiTestWithContextType
    : public ExtensionContentSettingsApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  ExtensionContentSettingsApiTestWithContextType()
      : ExtensionContentSettingsApiTest(GetParam()) {}
  ~ExtensionContentSettingsApiTestWithContextType() override = default;
  ExtensionContentSettingsApiTestWithContextType(
      const ExtensionContentSettingsApiTestWithContextType&) = delete;
  ExtensionContentSettingsApiTestWithContextType& operator=(
      const ExtensionContentSettingsApiTestWithContextType&) = delete;
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         ExtensionContentSettingsApiTestWithContextType,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         ExtensionContentSettingsApiTestWithContextType,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_F(ExtensionContentSettingsApiTest, Standard) {
  CheckContentSettingsDefault();

  static constexpr char kExtensionPath[] = "content_settings/standard";

  EXPECT_TRUE(RunExtensionTest(kExtensionPath, {.extension_url = "test.html"}))
      << message_;
  CheckContentSettingsSet();

  // The settings should not be reset when the extension is reloaded.
  ReloadExtension(last_loaded_extension_id());
  CheckContentSettingsSet();

  // Uninstalling and installing the extension (without running the test that
  // calls the extension API) should clear the settings.
  TestExtensionRegistryObserver observer(ExtensionRegistry::Get(profile()),
                                         last_loaded_extension_id());
  UninstallExtension(last_loaded_extension_id());
  observer.WaitForExtensionUninstalled();
  CheckContentSettingsDefault();

  LoadExtension(test_data_dir_.AppendASCII(kExtensionPath));
  CheckContentSettingsDefault();
}

IN_PROC_BROWSER_TEST_P(ExtensionContentSettingsApiTestWithContextType,
                       UnsupportedDefaultSettings) {
  const char kExtensionPath[] = "content_settings/unsupporteddefaultsettings";
  EXPECT_TRUE(RunExtensionTest(kExtensionPath)) << message_;
}

// Tests if an extension clearing content settings for one content type leaves
// the others unchanged.
IN_PROC_BROWSER_TEST_P(ExtensionContentSettingsApiTestWithContextType,
                       ClearProperlyGranular) {
  const char kExtensionPath[] = "content_settings/clearproperlygranular";
  EXPECT_TRUE(RunExtensionTest(kExtensionPath)) << message_;
}

// Tests if changing permissions in incognito mode keeps the previous state of
// regular mode.
IN_PROC_BROWSER_TEST_F(ExtensionContentSettingsApiTest, IncognitoIsolation) {
  GURL url("http://www.example.com");

  // Record previous state of content settings.
  std::vector<int> content_settings_before = GetContentSettingsSnapshot(url);

  // Run extension, set all permissions to allow, and check if they are changed.
  ASSERT_TRUE(RunExtensionTest("content_settings/incognitoisolation",
                               {.extension_url = "test.html",
                                .custom_arg = "allow",
                                .open_in_incognito = true},
                               {.allow_in_incognito = true}))
      << message_;

  // Get content settings after running extension to ensure nothing is changed.
  std::vector<int> content_settings_after = GetContentSettingsSnapshot(url);
  EXPECT_EQ(content_settings_before, content_settings_after);

  // Run extension, set all permissions to block, and check if they are changed.
  ASSERT_TRUE(RunExtensionTest("content_settings/incognitoisolation",
                               {.extension_url = "test.html",
                                .custom_arg = "block",
                                .open_in_incognito = true},
                               {.allow_in_incognito = true}))
      << message_;

  // Get content settings after running extension to ensure nothing is changed.
  content_settings_after = GetContentSettingsSnapshot(url);
  EXPECT_EQ(content_settings_before, content_settings_after);
}

// Tests if changing incognito mode permissions in regular profile are rejected.
IN_PROC_BROWSER_TEST_F(ExtensionContentSettingsApiTest,
                       IncognitoNotAllowedInRegular) {
  EXPECT_FALSE(
      RunExtensionTest("content_settings/incognitoisolation",
                       {.extension_url = "test.html", .custom_arg = "allow"}))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionContentSettingsApiTestWithContextType,
                       EmbeddedSettingsMetric) {
  base::HistogramTester histogram_tester;
  const char kExtensionPath[] = "content_settings/embeddedsettingsmetric";
  EXPECT_TRUE(RunExtensionTest(kExtensionPath)) << message_;

  int images_type =
      content_settings_uma_util::ContentSettingTypeToHistogramValue(
          ContentSettingsType::IMAGES);
  int geolocation_type =
      content_settings_uma_util::ContentSettingTypeToHistogramValue(
          ContentSettingsType::GEOLOCATION);
  int cookies_type =
      content_settings_uma_util::ContentSettingTypeToHistogramValue(
          ContentSettingsType::COOKIES);

  histogram_tester.ExpectBucketCount(
      "ContentSettings.ExtensionEmbeddedSettingSet", images_type, 1);
  histogram_tester.ExpectBucketCount(
      "ContentSettings.ExtensionEmbeddedSettingSet", geolocation_type, 1);
  histogram_tester.ExpectTotalCount(
      "ContentSettings.ExtensionEmbeddedSettingSet", 2);

  histogram_tester.ExpectBucketCount(
      "ContentSettings.ExtensionNonEmbeddedSettingSet", images_type, 1);
  histogram_tester.ExpectBucketCount(
      "ContentSettings.ExtensionNonEmbeddedSettingSet", cookies_type, 1);
  histogram_tester.ExpectTotalCount(
      "ContentSettings.ExtensionNonEmbeddedSettingSet", 2);
}

#if BUILDFLAG(ENABLE_PLUGINS)
IN_PROC_BROWSER_TEST_F(ExtensionContentSettingsApiTest, ConsoleErrorTest) {
  constexpr char kExtensionPath[] = "content_settings/disablepluginsapi";
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kExtensionPath));
  ASSERT_TRUE(extension);
  auto* web_contents = extensions::ProcessManager::Get(profile())
                           ->GetBackgroundHostForExtension(extension->id())
                           ->host_contents();
  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetPattern("*contentSettings.plugins is deprecated.*");
  ExecuteScriptInBackgroundPageNoWait(extension->id(), "setPluginsSetting()");
  ASSERT_TRUE(console_observer.Wait());
  EXPECT_EQ(1u, console_observer.messages().size());
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

}  // namespace extensions
