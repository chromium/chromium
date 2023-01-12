// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/chrome_plugin_service_filter.h"

#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/webplugininfo.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ChromePluginServiceFilterTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Ensure that the testing profile is registered for creating a PluginPrefs.
    PluginPrefs::GetForTestingProfile(profile());

    filter_ = ChromePluginServiceFilter::GetInstance();
    filter_->RegisterProfile(profile());
  }

  raw_ptr<ChromePluginServiceFilter> filter_ = nullptr;
};

content::WebPluginInfo GetFakePdfPluginInfo() {
  return content::WebPluginInfo(
      u"", base::FilePath(ChromeContentClient::kPDFExtensionPluginPath), u"",
      u"");
}

}  // namespace

TEST_F(ChromePluginServiceFilterTest, IsPluginAvailable) {
  EXPECT_TRUE(
      filter_->IsPluginAvailable(browser_context(), GetFakePdfPluginInfo()));
}

TEST_F(ChromePluginServiceFilterTest, IsPluginAvailableForInvalidProcess) {
  EXPECT_FALSE(filter_->IsPluginAvailable(nullptr, GetFakePdfPluginInfo()));
}

TEST_F(ChromePluginServiceFilterTest, IsPluginAvailableForDisabledPlugin) {
  profile()->GetPrefs()->SetBoolean(prefs::kPluginsAlwaysOpenPdfExternally,
                                    true);

  EXPECT_FALSE(
      filter_->IsPluginAvailable(browser_context(), GetFakePdfPluginInfo()));
}
