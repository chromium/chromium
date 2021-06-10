// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {
namespace {

class CrossOriginIsolationTest : public ExtensionBrowserTest {
 public:
  CrossOriginIsolationTest() = default;
  ~CrossOriginIsolationTest() override = default;
  CrossOriginIsolationTest(const CrossOriginIsolationTest&) = delete;
  CrossOriginIsolationTest& operator=(const CrossOriginIsolationTest&) = delete;

  const Extension* LoadExtension(TestExtensionDir& dir,
                                 const char* coep_value,
                                 const char* coop_value) {
    constexpr char kManifestTemplate[] = R"(
      {
        "background": {
          "scripts": ["background.js"]
        },
        "manifest_version": 2,
        "name": "CrossOriginIsolation",
        "version": "1.1",
        "cross_origin_embedder_policy": {
          "value": "%s"
        },
        "cross_origin_opener_policy": {
          "value": "%s"
        },
        "web_accessible_resources": ["test.html"]
      }
    )";
    dir.WriteManifest(
        base::StringPrintf(kManifestTemplate, coep_value, coop_value));
    dir.WriteFile(FILE_PATH_LITERAL("background.js"), "");
    dir.WriteFile(FILE_PATH_LITERAL("test.html"), "");
    return ExtensionBrowserTest::LoadExtension(dir.UnpackedPath());
  }

  bool IsCrossOriginIsolated(content::RenderFrameHost* host) {
    bool result = false;
    if (!content::ExecuteScriptAndExtractBool(
            host, "window.domAutomationController.send(crossOriginIsolated)",
            &result)) {
      ADD_FAILURE() << "Script execution failed";
      return false;
    }

    return result;
  }

  content::RenderFrameHost* GetBackgroundRenderFrameHost(
      const Extension& extension) {
    ExtensionHost* host =
        ProcessManager::Get(profile())->GetBackgroundHostForExtension(
            extension.id());
    return host ? host->main_frame_host() : nullptr;
  }

 private:
  // TODO(crbug.com/1199491): Remove once the related manifest keys are
  // available on Stable.
  ScopedCurrentChannel scoped_channel_{version_info::Channel::UNKNOWN};
};

// Tests that extensions can opt into cross origin isolation.
IN_PROC_BROWSER_TEST_F(CrossOriginIsolationTest, CrossOriginIsolation) {
  // Set the maximum number of processes to 1.  This is a soft limit that
  // we're allowed to exceed if processes *must* not share, which is the case
  // for cross-origin-isolated origins vs non-cross-origin-isolated
  // origins.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  TestExtensionDir coi_test_dir;
  const Extension* coi_extension =
      LoadExtension(coi_test_dir, "require-corp", "same-origin");
  ASSERT_TRUE(coi_extension);
  content::RenderFrameHost* coi_background_rfh =
      GetBackgroundRenderFrameHost(*coi_extension);
  ASSERT_TRUE(coi_background_rfh);
  EXPECT_TRUE(IsCrossOriginIsolated(coi_background_rfh));

  TestExtensionDir non_coi_test_dir;
  const Extension* non_coi_extension =
      LoadExtension(non_coi_test_dir, "unsafe-none", "same-origin");
  ASSERT_TRUE(non_coi_extension);
  content::RenderFrameHost* non_coi_background_rfh =
      GetBackgroundRenderFrameHost(*non_coi_extension);
  ASSERT_TRUE(non_coi_background_rfh);
  EXPECT_FALSE(IsCrossOriginIsolated(non_coi_background_rfh));

  // A cross-origin-isolated extension should not share a process with a
  // non-cross-origin-isolated one.
  EXPECT_NE(coi_background_rfh->GetProcess(),
            non_coi_background_rfh->GetProcess());
}

// Tests that a web accessible frame from a cross origin isolated extension is
// not cross origin isolated.
IN_PROC_BROWSER_TEST_F(CrossOriginIsolationTest, WebAccessibleFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Set the maximum number of processes to 1.  This is a soft limit that
  // we're allowed to exceed if processes *must* not share, which is the case
  // for cross-origin-isolated origins vs non-cross-origin-isolated
  // origins.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  TestExtensionDir coi_test_dir;
  const Extension* coi_extension =
      LoadExtension(coi_test_dir, "require-corp", "same-origin");
  ASSERT_TRUE(coi_extension);
  content::RenderFrameHost* coi_background_rfh =
      GetBackgroundRenderFrameHost(*coi_extension);
  ASSERT_TRUE(coi_background_rfh);
  EXPECT_TRUE(IsCrossOriginIsolated(coi_background_rfh));

  GURL extension_test_url = coi_extension->GetResourceURL("test.html");
  ui_test_utils::NavigateToURL(browser(), extension_test_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsCrossOriginIsolated(web_contents->GetMainFrame()));
  EXPECT_EQ(web_contents->GetMainFrame()->GetProcess(),
            coi_background_rfh->GetProcess());

  // Load test.html as a web accessible resource inside a web frame.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe_blank.html"));
  ASSERT_TRUE(
      content::NavigateIframeToURL(web_contents, "test", extension_test_url));

  content::RenderFrameHost* extension_iframe =
      content::ChildFrameAt(web_contents->GetMainFrame(), 0);
  ASSERT_TRUE(extension_iframe);
  EXPECT_EQ(extension_test_url, extension_iframe->GetLastCommittedURL());

  // The extension iframe is embedded within a web frame and won't be cross
  // origin isolated. It should also not share a process with the extension's
  // cross origin isolated context, nor with the web frame it's embedded in.
  EXPECT_FALSE(IsCrossOriginIsolated(extension_iframe));
  EXPECT_NE(extension_iframe->GetProcess(), coi_background_rfh->GetProcess());
  EXPECT_NE(extension_iframe->GetProcess(),
            web_contents->GetMainFrame()->GetProcess());

  // Check ProcessManager APIs to ensure they work correctly for the case where
  // an extension has multiple processes for the same profile.
  {
    ProcessManager* process_manager = ProcessManager::Get(profile());
    ASSERT_TRUE(process_manager);
    std::set<content::RenderFrameHost*> extension_hosts =
        process_manager->GetRenderFrameHostsForExtension(coi_extension->id());
    EXPECT_THAT(extension_hosts,
                ::testing::ElementsAre(coi_background_rfh, extension_iframe));

    EXPECT_EQ(coi_extension, process_manager->GetExtensionForRenderFrameHost(
                                 coi_background_rfh));
    EXPECT_EQ(coi_extension, process_manager->GetExtensionForRenderFrameHost(
                                 extension_iframe));
  }

  // Check ProcessMap APIs to ensure they work correctly for the case where an
  // extension has multiple processes for the same profile.
  {
    ProcessMap* process_map = ProcessMap::Get(profile());
    ASSERT_TRUE(process_map);
    EXPECT_TRUE(process_map->Contains(
        coi_extension->id(), coi_background_rfh->GetProcess()->GetID()));
    EXPECT_TRUE(process_map->Contains(coi_extension->id(),
                                      extension_iframe->GetProcess()->GetID()));

    GURL* url = nullptr;
    EXPECT_EQ(
        Feature::BLESSED_EXTENSION_CONTEXT,
        process_map->GetMostLikelyContextType(
            coi_extension, coi_background_rfh->GetProcess()->GetID(), url));
    EXPECT_EQ(Feature::BLESSED_EXTENSION_CONTEXT,
              process_map->GetMostLikelyContextType(
                  coi_extension, extension_iframe->GetProcess()->GetID(), url));
  }
}

}  // namespace
}  // namespace extensions
