// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_groups/tab_groups_api.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_event_router.h"
#include "chrome/browser/extensions/api/tab_groups/tab_groups_event_router_factory.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_initialized_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension_builder.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

// NOTE: While the tests in this file appear like they could be unit tests, they
// use browser windows and tabs. As such they need to be browser tests. See
// docs/chrome_browser_design_principles.md. This also makes them easier to port
// to desktop Android.

namespace extensions {
namespace {

base::Value::List RunTabGroupsQueryFunction(
    content::BrowserContext* browser_context,
    const Extension* extension,
    const std::string& query_info) {
  auto function = base::MakeRefCounted<TabGroupsQueryFunction>();
  function->set_extension(extension);
  std::optional<base::Value> value =
      api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), query_info, browser_context,
          api_test_utils::FunctionMode::kNone);
  return std::move(*value).TakeList();
}

// Creates an extension with "tabGroups" permission.
scoped_refptr<const Extension> CreateTabGroupsExtension() {
  return ExtensionBuilder("Extension with tabGroups permission")
      .AddAPIPermission("tabGroups")
      .Build();
}

class TabGroupsApiBrowserTest : public ExtensionBrowserTest {
 public:
  TabGroupsApiBrowserTest() = default;
  TabGroupsApiBrowserTest(const TabGroupsApiBrowserTest&) = delete;
  TabGroupsApiBrowserTest& operator=(const TabGroupsApiBrowserTest&) = delete;
  ~TabGroupsApiBrowserTest() override = default;

  // ExtensionBrowserTest:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    // Add several tabs to the browser and get their web contents.
    constexpr int kNumTabs = 6;
    for (int i = 0; i < kNumTabs; ++i) {
      content::RenderFrameHost* render_frame_host =
          NavigateToURLInNewTab(GURL("about:blank"));
      content::WebContents* contents =
          content::WebContents::FromRenderFrameHost(render_frame_host);
      web_contents_.push_back(contents);
    }

    // Wait for the TabGroupSyncService to properly initialize before making any
    // changes to tab groups.
    WaitForTabGroupSyncServiceInitialized();
  }
  void TearDownOnMainThread() override {
    web_contents_.clear();
    browser()->tab_strip_model()->CloseAllTabs();
    ExtensionBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* web_contents(int index) { return web_contents_[index]; }

  void WaitForTabGroupSyncServiceInitialized() {
    auto observer =
        std::make_unique<tab_groups::TabGroupSyncServiceInitializedObserver>(
            tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile()));
    observer->Wait();
  }

 private:
  // The original web contentses in order.
  std::vector<raw_ptr<content::WebContents, VectorExperimental>> web_contents_;
};

// Tests querying on a TabStripModel that doesn't support tab groups.
IN_PROC_BROWSER_TEST_F(TabGroupsApiBrowserTest,
                       TabStripModelWithNoTabGroupFails) {
  // Create a new window that doesn't support groups. App windows don't allow
  // tab groups.
  Browser* browser2 = CreateBrowserForApp("some app", profile());
  BrowserList::SetLastActive(browser2);

  ASSERT_FALSE(browser2->tab_strip_model()->SupportsTabGroups());

  // Add a few tabs.
  TabStripModel* tab_strip_model2 = browser2->tab_strip_model();
  constexpr int kNumTabs2 = 3;
  for (int i = 0; i < kNumTabs2; ++i) {
    ASSERT_TRUE(AddTabAtIndexToBrowser(browser2, 0, GURL("about:blank"),
                                       ui::PAGE_TRANSITION_LINK));
  }

  // Create an extension and test that the tab group query method skips the
  // unsupported tab strip without throwing an error.
  scoped_refptr<const Extension> extension = CreateTabGroupsExtension();

  const char* kTitleQueryInfo = R"([{"title": "Sample title"}])";
  base::Value::List groups_list =
      RunTabGroupsQueryFunction(profile(), extension.get(), kTitleQueryInfo);

  ASSERT_EQ(0u, groups_list.size());

  tab_strip_model2->CloseAllTabs();
}

}  // namespace
}  // namespace extensions
