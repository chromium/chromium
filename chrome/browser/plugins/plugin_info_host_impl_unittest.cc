// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_info_host_impl.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/common/pref_names.h"
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

void PluginsLoaded(base::OnceClosure callback,
                   const std::vector<content::WebPluginInfo>& plugins) {
  std::move(callback).Run();
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
    content::WebPluginInfo foo_plugin(u"Foo Plugin", foo_plugin_path_, u"1",
                                      u"The Foo plugin.");
    content::WebPluginMimeType mime_type;
    mime_type.mime_type = "foo/bar";
    foo_plugin.mime_types.push_back(mime_type);
    foo_plugin.type = content::WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;
    PluginService::GetInstance()->Init();
    PluginService::GetInstance()->RegisterInternalPlugin(foo_plugin, false);
    PluginService::GetInstance()->RefreshPlugins();

    content::WebPluginInfo bar_plugin(u"Bar Plugin", bar_plugin_path_, u"1",
                                      u"The Bar plugin.");
    mime_type.mime_type = "foo/bar";
    bar_plugin.mime_types.push_back(mime_type);
    bar_plugin.type = content::WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;
    PluginService::GetInstance()->RegisterInternalPlugin(bar_plugin, false);

    content::WebPluginInfo fake_flash(
        base::ASCIIToUTF16(content::kFlashPluginName), fake_flash_path_,
        u"100.0", u"Fake Flash Description.");
    mime_type.mime_type = "application/x-shockwave-flash";
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
    std::u16string plugin_name;
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
