// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

namespace {

// A minimal stub extension view.
class TestExtensionView : public ExtensionView {
 public:
  TestExtensionView() = default;
  ~TestExtensionView() override = default;

#if !BUILDFLAG(IS_ANDROID)
  gfx::NativeView GetNativeView() override { return gfx::NativeView(); }
#endif
  void ResizeDueToAutoResize(content::WebContents* web_contents,
                             const gfx::Size& new_size) override {}
  void RenderFrameCreated(
      content::RenderFrameHost* render_frame_host) override {}
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    return false;
  }
  void OnLoaded() override {}
};

}  // namespace

using ExtensionInspectionBrowserTest = ExtensionBrowserTest;

// Tests that the DevTools target type for extension popup WebContents is
// reported as kTypePage ("page"), ensuring DevTools frontend inspects the
// popup page rather than falling back to the service worker.
IN_PROC_BROWSER_TEST_F(ExtensionInspectionBrowserTest,
                       GetTargetTypeForExtensionPopup) {
  static constexpr char kManifest[] =
      R"({
           "name": "Popup Target Type Test Extension",
           "version": "0.1",
           "manifest_version": 3,
           "action": { "default_popup": "popup.html" }
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.html"),
                     "<html><body>Popup</body></html>");

  scoped_refptr<const Extension> extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ExtensionHostTestHelper host_helper(profile(), extension->id());
  TestExtensionView test_view;
  std::unique_ptr<ExtensionViewHost> host =
      ExtensionViewHostFactory::CreatePopupHost(
          *extension, extension->GetResourceURL("popup.html"),
          browser_window_interface());
  ASSERT_TRUE(host);
  host->set_view(&test_view);
  host->CreateRendererSoon();
  ASSERT_TRUE(host_helper.WaitForHostCompletedFirstLoad());
  content::WebContents* popup_contents = host->host_contents();
  ASSERT_TRUE(popup_contents);
  EXPECT_EQ(mojom::ViewType::kExtensionPopup,
            extensions::GetViewType(popup_contents));

  // Verify that the frame DevToolsAgentHost for the popup WebContents returns
  // "page".
  scoped_refptr<content::DevToolsAgentHost> frame_agent =
      content::DevToolsAgentHost::GetOrCreateFor(popup_contents);
  ASSERT_TRUE(frame_agent);
  EXPECT_EQ(content::DevToolsAgentHost::kTypePage, frame_agent->GetType());

  // Mostly here as documentation: if coerced into a tab contents (via
  // `GetOrCreateForTab()`, it will still be considered a tab. It's necessary to
  // create the agent via `GetOrCreateFor()`, as above.
  scoped_refptr<content::DevToolsAgentHost> tab_agent =
      content::DevToolsAgentHost::GetOrCreateForTab(popup_contents);
  ASSERT_TRUE(tab_agent);
  EXPECT_EQ(content::DevToolsAgentHost::kTypeTab, tab_agent->GetType());
}

}  // namespace extensions
