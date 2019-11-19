// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_info_host_impl.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using content::PluginService;
using testing::Eq;

namespace {

void PluginsLoaded(const base::Closure& callback,
                   const std::vector<content::WebPluginInfo>& plugins) {
  callback.Run();
}

class FakePluginServiceFilter : public content::PluginServiceFilter {
 public:
  FakePluginServiceFilter() {}
  ~FakePluginServiceFilter() override {}

  bool IsPluginAvailable(int render_process_id,
                         int render_view_id,
                         const GURL& url,
                         const url::Origin& main_frame_origin,
                         content::WebPluginInfo* plugin) override;

  bool CanLoadPlugin(int render_process_id,
                     const base::FilePath& path) override;

  void set_plugin_enabled(const base::FilePath& plugin_path, bool enabled) {
    plugin_state_[plugin_path] = enabled;
  }

 private:
  std::map<base::FilePath, bool> plugin_state_;
};

bool FakePluginServiceFilter::IsPluginAvailable(
    int render_process_id,
    int render_view_id,
    const GURL& url,
    const url::Origin& main_frame_origin,
    content::WebPluginInfo* plugin) {
  auto it = plugin_state_.find(plugin->path);
  if (it == plugin_state_.end()) {
    ADD_FAILURE() << "No plugin state for '" << plugin->path.value() << "'";
    return false;
  }
  return it->second;
}

bool FakePluginServiceFilter::CanLoadPlugin(int render_process_id,
                                            const base::FilePath& path) {
  return true;
}

}  // namespace

class PluginInfoHostImplTest : public ::testing::Test {
 public:
  PluginInfoHostImplTest()
      : foo_plugin_path_(FILE_PATH_LITERAL("/path/to/foo")),
        bar_plugin_path_(FILE_PATH_LITERAL("/path/to/bar")),
        fake_flash_path_(FILE_PATH_LITERAL("/path/to/fake/flash")),
        context_(0, &profile_),
        host_content_settings_map_(
            HostContentSettingsMapFactory::GetForProfile(&profile_)) {}

  void SetUp() override {
    content::WebPluginInfo foo_plugin(base::ASCIIToUTF16("Foo Plugin"),
                                      foo_plugin_path_, base::ASCIIToUTF16("1"),
                                      base::ASCIIToUTF16("The Foo plugin."));
    content::WebPluginMimeType mime_type;
    mime_type.mime_type = "foo/bar";
    foo_plugin.mime_types.push_back(mime_type);
    foo_plugin.type = content::WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;
    PluginService::GetInstance()->Init();
    PluginService::GetInstance()->RegisterInternalPlugin(foo_plugin, false);

    content::WebPluginInfo bar_plugin(base::ASCIIToUTF16("Bar Plugin"),
                                      bar_plugin_path_, base::ASCIIToUTF16("1"),
                                      base::ASCIIToUTF16("The Bar plugin."));
    mime_type.mime_type = "foo/bar";
    bar_plugin.mime_types.push_back(mime_type);
    bar_plugin.type = content::WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;
    PluginService::GetInstance()->RegisterInternalPlugin(bar_plugin, false);

    content::WebPluginInfo fake_flash(
        base::ASCIIToUTF16(content::kFlashPluginName), fake_flash_path_,
        base::ASCIIToUTF16("100.0"),
        base::ASCIIToUTF16("Fake Flash Description."));
    mime_type.mime_type = content::kFlashPluginSwfMimeType;
    fake_flash.mime_types.push_back(mime_type);
    fake_flash.type = content::WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS;
    PluginService::GetInstance()->RegisterInternalPlugin(fake_flash, false);

    PluginService::GetInstance()->SetFilter(&filter_);

#if !defined(OS_WIN)
    // Can't go out of process in unit tests.
    content::RenderProcessHost::SetRunRendererInProcess(true);
#endif
    base::RunLoop run_loop;
    PluginService::GetInstance()->GetPlugins(
        base::BindOnce(&PluginsLoaded, run_loop.QuitClosure()));
    run_loop.Run();
#if !defined(OS_WIN)
    content::RenderProcessHost::SetRunRendererInProcess(false);
#endif
  }

 protected:
  TestingProfile* profile() { return &profile_; }

  PluginInfoHostImpl::Context* context() { return &context_; }

  HostContentSettingsMap* host_content_settings_map() {
    return host_content_settings_map_;
  }

  void VerifyPluginContentSetting(const GURL& url,
                                  const std::string& plugin,
                                  ContentSetting expected_setting,
                                  bool expected_is_default,
                                  bool expected_is_managed) {
    ContentSetting setting = expected_setting == CONTENT_SETTING_DEFAULT
                                 ? CONTENT_SETTING_BLOCK
                                 : CONTENT_SETTING_DEFAULT;
    bool is_default = !expected_is_default;
    bool is_managed = !expected_is_managed;

    // Pass in a fake Flash plugin info.
    content::WebPluginInfo plugin_info(
        base::ASCIIToUTF16(content::kFlashPluginName), base::FilePath(),
        base::ASCIIToUTF16("1"), base::ASCIIToUTF16("Fake Flash"));

    PluginUtils::GetPluginContentSetting(
        host_content_settings_map_, plugin_info, url::Origin::Create(url), url,
        plugin, &setting, &is_default, &is_managed);
    EXPECT_EQ(expected_setting, setting);
    EXPECT_EQ(expected_is_default, is_default);
    EXPECT_EQ(expected_is_managed, is_managed);
  }

  base::FilePath foo_plugin_path_;
  base::FilePath bar_plugin_path_;
  base::FilePath fake_flash_path_;
  FakePluginServiceFilter filter_;

 private:
  base::ShadowingAtExitManager at_exit_manager_;  // Destroys the PluginService.
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile_;
  PluginInfoHostImpl::Context context_;
  HostContentSettingsMap* host_content_settings_map_;
};

TEST_F(PluginInfoHostImplTest, FindEnabledPlugin) {
  filter_.set_plugin_enabled(foo_plugin_path_, true);
  filter_.set_plugin_enabled(bar_plugin_path_, true);
  {
    chrome::mojom::PluginStatus status;
    content::WebPluginInfo plugin;
    std::string actual_mime_type;
    EXPECT_TRUE(context()->FindEnabledPlugin(0, GURL(), url::Origin(),
                                             "foo/bar", &status, &plugin,
                                             &actual_mime_type, NULL));
    EXPECT_EQ(chrome::mojom::PluginStatus::kAllowed, status);
    EXPECT_EQ(foo_plugin_path_.value(), plugin.path.value());
  }

  filter_.set_plugin_enabled(foo_plugin_path_, false);
  {
    chrome::mojom::PluginStatus status;
    content::WebPluginInfo plugin;
    std::string actual_mime_type;
    EXPECT_TRUE(context()->FindEnabledPlugin(0, GURL(), url::Origin(),
                                             "foo/bar", &status, &plugin,
                                             &actual_mime_type, NULL));
    EXPECT_EQ(chrome::mojom::PluginStatus::kAllowed, status);
    EXPECT_EQ(bar_plugin_path_.value(), plugin.path.value());
  }

  filter_.set_plugin_enabled(bar_plugin_path_, false);
  {
    chrome::mojom::PluginStatus status;
    content::WebPluginInfo plugin;
    std::string actual_mime_type;
    std::string identifier;
    base::string16 plugin_name;
    EXPECT_FALSE(context()->FindEnabledPlugin(0, GURL(), url::Origin(),
                                              "foo/bar", &status, &plugin,
                                              &actual_mime_type, NULL));
    EXPECT_EQ(chrome::mojom::PluginStatus::kDisabled, status);
    EXPECT_EQ(foo_plugin_path_.value(), plugin.path.value());
  }
  {
    chrome::mojom::PluginStatus status;
    content::WebPluginInfo plugin;
    std::string actual_mime_type;
    EXPECT_FALSE(context()->FindEnabledPlugin(0, GURL(), url::Origin(),
                                              "baz/blurp", &status, &plugin,
                                              &actual_mime_type, NULL));
    EXPECT_EQ(chrome::mojom::PluginStatus::kNotFound, status);
    EXPECT_EQ(FILE_PATH_LITERAL(""), plugin.path.value());
  }
}

TEST_F(PluginInfoHostImplTest, PreferHtmlOverPlugins) {
  // The HTML5 By Default feature hides Flash using the plugin filter.
  filter_.set_plugin_enabled(fake_flash_path_, false);

  // Make a real HTTP origin, as all Flash content from non-HTTP and non-FILE
  // origins are blocked.
  url::Origin main_frame_origin =
      url::Origin::Create(GURL("http://example.com"));

  chrome::mojom::PluginStatus status;
  content::WebPluginInfo plugin;
  std::string actual_mime_type;
  EXPECT_TRUE(context()->FindEnabledPlugin(
      0, GURL(), main_frame_origin, content::kFlashPluginSwfMimeType, &status,
      &plugin, &actual_mime_type, NULL));
  EXPECT_EQ(chrome::mojom::PluginStatus::kFlashHiddenPreferHtml, status);

  PluginMetadata::SecurityStatus security_status =
      PluginMetadata::SECURITY_STATUS_UP_TO_DATE;

  context()->DecidePluginStatus(GURL(), main_frame_origin, plugin,
                                security_status, content::kFlashPluginName,
                                &status);
  EXPECT_EQ(chrome::mojom::PluginStatus::kBlockedNoLoading, status);

  // Now enable plugins.
  host_content_settings_map()->SetDefaultContentSetting(
      ContentSettingsType::PLUGINS, CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);

  context()->DecidePluginStatus(GURL(), main_frame_origin, plugin,
                                security_status, content::kFlashPluginName,
                                &status);
  EXPECT_EQ(chrome::mojom::PluginStatus::kPlayImportantContent, status);
}

TEST_F(PluginInfoHostImplTest, RunAllFlashInAllowMode) {
  filter_.set_plugin_enabled(fake_flash_path_, true);

  // Make a real HTTP origin, as all Flash content from non-HTTP and non-FILE
  // origins are blocked.
  url::Origin main_frame_origin =
      url::Origin::Create(GURL("http://example.com"));

  chrome::mojom::PluginStatus status;
  content::WebPluginInfo plugin;
  std::string actual_mime_type;
  ASSERT_TRUE(context()->FindEnabledPlugin(
      0, GURL(), main_frame_origin, content::kFlashPluginSwfMimeType, &status,
      &plugin, &actual_mime_type, nullptr));
  ASSERT_THAT(status, Eq(chrome::mojom::PluginStatus::kAllowed));

  host_content_settings_map()->SetContentSettingDefaultScope(
      main_frame_origin.GetURL(), GURL(), ContentSettingsType::PLUGINS,
      std::string(), CONTENT_SETTING_ALLOW);

  ASSERT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kRunAllFlashInAllowMode));

  PluginMetadata::SecurityStatus security_status =
      PluginMetadata::SECURITY_STATUS_UP_TO_DATE;
  context()->DecidePluginStatus(GURL(), main_frame_origin, plugin,
                                security_status, content::kFlashPluginName,
                                &status);
  EXPECT_THAT(status, Eq(chrome::mojom::PluginStatus::kPlayImportantContent));

  // Reset the status to allowed.
  status = chrome::mojom::PluginStatus::kAllowed;

  profile()->GetPrefs()->SetBoolean(prefs::kRunAllFlashInAllowMode, true);

  context()->DecidePluginStatus(GURL(), main_frame_origin, plugin,
                                security_status, content::kFlashPluginName,
                                &status);
  EXPECT_THAT(status, Eq(chrome::mojom::PluginStatus::kAllowed));
}

TEST_F(PluginInfoHostImplTest, PluginsOnlyAllowedInWhitelistedSchemes) {
  host_content_settings_map()->SetDefaultContentSetting(
      ContentSettingsType::PLUGINS, CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);

  VerifyPluginContentSetting(GURL("http://example.com"), "foo",
                             CONTENT_SETTING_DETECT_IMPORTANT_CONTENT, true,
                             false);
  VerifyPluginContentSetting(GURL("https://example.com"), "foo",
                             CONTENT_SETTING_DETECT_IMPORTANT_CONTENT, true,
                             false);
  VerifyPluginContentSetting(GURL("file://foobar/"), "foo",
                             CONTENT_SETTING_DETECT_IMPORTANT_CONTENT, true,
                             false);
  VerifyPluginContentSetting(GURL("chrome-extension://extension-id"), "foo",
                             CONTENT_SETTING_DETECT_IMPORTANT_CONTENT, true,
                             false);
  VerifyPluginContentSetting(GURL("unknown-scheme://foobar"), "foo",
                             CONTENT_SETTING_BLOCK, true, false);
}

TEST_F(PluginInfoHostImplTest, GetPluginContentSetting) {
  HostContentSettingsMap* map = host_content_settings_map();
  {
    bool is_managed = false;
    EXPECT_EQ(
        CONTENT_SETTING_BLOCK,
        PluginUtils::UnsafeGetRawDefaultFlashContentSetting(map, &is_managed));
    EXPECT_FALSE(is_managed);
  }

  // Set plugins to Plugin Power Saver on example.com and subdomains.
  GURL host("http://example.com/");
  map->SetContentSettingDefaultScope(host, GURL(), ContentSettingsType::PLUGINS,
                                     std::string(),
                                     CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);

  GURL unmatched_host("https://www.google.com");
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      map->GetContentSetting(unmatched_host, unmatched_host,
                             ContentSettingsType::PLUGINS, std::string()));
  EXPECT_EQ(CONTENT_SETTING_DETECT_IMPORTANT_CONTENT,
            map->GetContentSetting(host, host, ContentSettingsType::PLUGINS,
                                   std::string()));

  VerifyPluginContentSetting(host, std::string(),
                             CONTENT_SETTING_DETECT_IMPORTANT_CONTENT, false,
                             false);
  VerifyPluginContentSetting(unmatched_host, std::string(),
                             CONTENT_SETTING_BLOCK, false, false);

  // Block plugins via policy.
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultPluginsSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));

  // All plugins should be blocked now.
  VerifyPluginContentSetting(host, std::string(), CONTENT_SETTING_BLOCK, true,
                             true);
  VerifyPluginContentSetting(unmatched_host, std::string(),
                             CONTENT_SETTING_BLOCK, true, true);
  {
    bool is_managed = false;
    EXPECT_EQ(
        CONTENT_SETTING_BLOCK,
        PluginUtils::UnsafeGetRawDefaultFlashContentSetting(map, &is_managed));
    EXPECT_TRUE(is_managed);
  }
}
