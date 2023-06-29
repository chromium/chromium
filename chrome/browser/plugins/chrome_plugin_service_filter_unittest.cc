// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/chrome_plugin_service_filter.h"

#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/mock_render_process_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class RenderProcessHostObserverAddedWaiter {
 public:
  explicit RenderProcessHostObserverAddedWaiter(
      ChromePluginServiceFilter* filter) {
    filter->NotifyIfObserverAddedForTesting(run_loop_.QuitClosure());
  }
  ~RenderProcessHostObserverAddedWaiter() = default;

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

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

TEST_F(ChromePluginServiceFilterTest, AuthorizePlugin) {
  const int render_process_id = process()->GetID();
  const base::FilePath path(ChromeContentClient::kPDFExtensionPluginPath);
  RenderProcessHostObserverAddedWaiter waiter(filter_);

  // Initially the plugin shouldn't be authorized.
  EXPECT_FALSE(filter_->CanLoadPlugin(render_process_id, path));

  filter_->AuthorizePlugin(render_process_id, path);
  EXPECT_TRUE(filter_->CanLoadPlugin(render_process_id, path));

  // Wait for `filter_` to add itself as an observer to the RenderProcessHost
  // on the UI thread.
  waiter.Wait();
  process()->SimulateRenderProcessExit(
      base::TerminationStatus::TERMINATION_STATUS_NORMAL_TERMINATION,
      /*exit_code=*/0);
  EXPECT_FALSE(filter_->CanLoadPlugin(render_process_id, path));
}
