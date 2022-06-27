// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/offscreen_document_host.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

class OffscreenDocumentBrowserTest : public ExtensionApiTest {
 public:
  OffscreenDocumentBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionsOffscreenDocuments);
  }
  ~OffscreenDocumentBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test basic properties of offscreen documents.
IN_PROC_BROWSER_TEST_F(OffscreenDocumentBrowserTest,
                       CreateBasicOffscreenDocument) {
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1"
         })";
  static constexpr char kOffscreenDocumentHtml[] =
      R"(<html>
           <body>
             <div id="signal">Hello, World</div>
           </body>
         </html>)";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("offscreen.html"),
                     kOffscreenDocumentHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("other.html"), "<html>Empty</html>");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  const GURL offscreen_url = extension->GetResourceURL("offscreen.html");
  ProcessManager* const process_manager = ProcessManager::Get(profile());

  scoped_refptr<content::SiteInstance> site_instance =
      process_manager->GetSiteInstanceForURL(offscreen_url);

  // Create a new offscreen document and wait for it to load.
  content::TestNavigationObserver navigation_observer(offscreen_url);
  navigation_observer.StartWatchingNewWebContents();
  auto offscreen_document = std::make_unique<OffscreenDocumentHost>(
      *extension, site_instance.get(), offscreen_url);
  offscreen_document->CreateRendererSoon();
  navigation_observer.Wait();
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());

  // Check basic properties:
  content::WebContents* contents = offscreen_document->host_contents();
  ASSERT_TRUE(contents);
  // - The URL should match the extension's URL.
  EXPECT_EQ(offscreen_url, contents->GetLastCommittedURL());
  // - The offscreen document should be, well, offscreen; it should not be
  //   contained within any Browser window.
  EXPECT_EQ(nullptr, chrome::FindBrowserWithWebContents(contents));
  // - The view type should be correctly set (it should not be considered a
  //   background page, tab, or other type of view).
  EXPECT_EQ(mojom::ViewType::kOffscreenDocument,
            offscreen_document->extension_host_type());
  EXPECT_EQ(mojom::ViewType::kOffscreenDocument, GetViewType(contents));

  {
    // Check the registration in the ProcessManager: the offscreen document
    // should be associated with the extension and have a registered frame.
    ProcessManager::FrameSet frames_for_extension =
        process_manager->GetRenderFrameHostsForExtension(extension->id());
    ASSERT_EQ(1u, frames_for_extension.size());
    content::RenderFrameHost* frame_host = *frames_for_extension.begin();
    EXPECT_EQ(offscreen_url, frame_host->GetLastCommittedURL());
    EXPECT_EQ(contents, content::WebContents::FromRenderFrameHost(frame_host));
    EXPECT_EQ(extension, process_manager->GetExtensionForWebContents(contents));
  }

  {
    // Check the document loaded properly (and, implicitly check that it does,
    // in fact, have a DOM).
    std::string div_text;
    static constexpr char kScript[] =
        R"({
             let div = document.getElementById('signal');
             domAutomationController.send(div ? div.innerText : '<no div>');
           })";
    EXPECT_TRUE(
        content::ExecuteScriptAndExtractString(contents, kScript, &div_text));
    EXPECT_EQ("Hello, World", div_text);
  }

  {
    // Check that the offscreen document runs in the same process as other
    // extension frames. Do this by comparing it to another extension page in
    // a tab.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), extension->GetResourceURL("other.html")));
    content::WebContents* tab_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(tab_contents->GetPrimaryMainFrame()->GetProcess(),
              contents->GetPrimaryMainFrame()->GetProcess());
  }
}

}  // namespace extensions
