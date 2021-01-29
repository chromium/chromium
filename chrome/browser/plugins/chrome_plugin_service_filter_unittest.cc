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
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/plugins/flash_temporary_permission_tracker.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/common/chrome_content_client.h"
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
