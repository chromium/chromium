// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/chrome_plugin_service_filter.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/plugins/flash_temporary_permission_tracker.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "url/origin.h"

class ChromePluginServiceFilterTest : public ChromeRenderViewHostTestHarness {
 public:
  ChromePluginServiceFilterTest()
      : ChromeRenderViewHostTestHarness(),
        filter_(nullptr),
        flash_plugin_path_(FILE_PATH_LITERAL("/path/to/flash")) {}

  bool IsPluginAvailable(const GURL& plugin_content_url,
                         const url::Origin& main_frame_origin,
                         content::WebPluginInfo plugin_info) {
    return filter_->IsPluginAvailable(
        web_contents()->GetMainFrame()->GetProcess()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(), plugin_content_url,
        main_frame_origin, &plugin_info);
  }

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Ensure that the testing profile is registered for creating a PluginPrefs.
    PluginPrefs::GetForTestingProfile(profile());
    PluginFinder::GetInstance();

    filter_ = ChromePluginServiceFilter::GetInstance();
    filter_->RegisterProfile(profile());
  }

  ChromePluginServiceFilter* filter_;
  base::FilePath flash_plugin_path_;
};

TEST_F(ChromePluginServiceFilterTest, PreferHtmlOverPluginsDefault) {
  content::WebPluginInfo flash_plugin(
      base::ASCIIToUTF16(content::kFlashPluginName), flash_plugin_path_,
      base::ASCIIToUTF16("1"), base::ASCIIToUTF16("The Flash plugin."));

  // The default content setting should block Flash.
  GURL url("http://www.google.com");
  url::Origin main_frame_origin = url::Origin::Create(url);
  EXPECT_FALSE(IsPluginAvailable(url, main_frame_origin, flash_plugin));

  // Block plugins.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(url, url, ContentSettingsType::PLUGINS,
                                     std::string(), CONTENT_SETTING_BLOCK);

  EXPECT_FALSE(IsPluginAvailable(url, main_frame_origin, flash_plugin));

  // Allow plugins.
  map->SetContentSettingDefaultScope(url, url, ContentSettingsType::PLUGINS,
                                     std::string(), CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(IsPluginAvailable(url, main_frame_origin, flash_plugin));

  // Detect important content should block plugins without user gesture.
  map->SetContentSettingDefaultScope(url, url, ContentSettingsType::PLUGINS,
                                     std::string(),
                                     CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);

  EXPECT_FALSE(IsPluginAvailable(url, main_frame_origin, flash_plugin));
}

TEST_F(ChromePluginServiceFilterTest,
       PreferHtmlOverPluginsAllowOrBlockOverrides) {
  content::WebPluginInfo flash_plugin(
      base::ASCIIToUTF16(content::kFlashPluginName), flash_plugin_path_,
      base::ASCIIToUTF16("1"), base::ASCIIToUTF16("The Flash plugin."));

  GURL url("http://www.google.com");
  url::Origin main_frame_origin = url::Origin::Create(url);

  // Allow plugins.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(url, url, ContentSettingsType::PLUGINS,
                                     std::string(), CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(IsPluginAvailable(url, main_frame_origin, flash_plugin));

  // Plugins should be hidden on ASK mode.
  map->SetContentSettingDefaultScope(url, url, ContentSettingsType::PLUGINS,
                                     std::string(),
                                     CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);
  EXPECT_FALSE(IsPluginAvailable(url, main_frame_origin, flash_plugin));

  // Block plugins.
  map->SetContentSettingDefaultScope(url, url, ContentSettingsType::PLUGINS,
                                     std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(IsPluginAvailable(url, main_frame_origin, flash_plugin));
}

TEST_F(ChromePluginServiceFilterTest,
       PreferHtmlOverPluginsIncognitoHasIndependentSetting) {
  Profile* incognito = profile()->GetOffTheRecordProfile();
  filter_->RegisterProfile(incognito);

  content::WebPluginInfo flash_plugin(
      base::ASCIIToUTF16(content::kFlashPluginName), flash_plugin_path_,
      base::ASCIIToUTF16("1"), base::ASCIIToUTF16("The Flash plugin."));

  GURL url("http://www.google.com");

  // Allow plugins for this url in the incognito profile.
  HostContentSettingsMap* incognito_map =
      HostContentSettingsMapFactory::GetForProfile(incognito);
  incognito_map->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::PLUGINS, std::string(),
      CONTENT_SETTING_ALLOW);

  // We pass the availablity check in incognito.
  url::Origin main_frame_origin = url::Origin::Create(url);
  SetContents(
      content::WebContentsTester::CreateTestWebContents(incognito, nullptr));
  EXPECT_TRUE(IsPluginAvailable(url, main_frame_origin, flash_plugin));

  // But the original profile still fails the availability check.
  SetContents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  EXPECT_FALSE(IsPluginAvailable(url, main_frame_origin, flash_plugin));
}

TEST_F(ChromePluginServiceFilterTest, ManagedSetting) {
  content::WebPluginInfo flash_plugin(
      base::ASCIIToUTF16(content::kFlashPluginName), flash_plugin_path_,
      base::ASCIIToUTF16("1"), base::ASCIIToUTF16("The Flash plugin."));

  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultPluginsSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_ASK));

  GURL url("http://www.google.com");
  url::Origin main_frame_origin = url::Origin::Create(url);
  NavigateAndCommit(url);

  // Flash is normally blocked on the ASK managed policy.
  EXPECT_FALSE(IsPluginAvailable(url, main_frame_origin, flash_plugin));

  // Allow flash temporarily.
  FlashTemporaryPermissionTracker::Get(profile())->FlashEnabledForWebContents(
      web_contents());
  EXPECT_TRUE(IsPluginAvailable(url, main_frame_origin, flash_plugin));
}
