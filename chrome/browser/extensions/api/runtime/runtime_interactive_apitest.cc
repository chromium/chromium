// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "components/version_info/channel.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/script_executor.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

// A test suite for `runtime.getContexts()` tests that need to be run as
// interactive UI tests (e.g. due to requiring focus).
class RuntimeGetContextsInteractiveApiTest : public ExtensionApiTest {
 public:
  RuntimeGetContextsInteractiveApiTest() = default;
  RuntimeGetContextsInteractiveApiTest(
      const RuntimeGetContextsInteractiveApiTest&) = delete;
  RuntimeGetContextsInteractiveApiTest& operator=(
      const RuntimeGetContextsInteractiveApiTest&) = delete;
  ~RuntimeGetContextsInteractiveApiTest() override = default;

  // Runs `chrome.runtime.getContexts()` and returns the result as a
  // base::Value.
  base::Value GetContexts(const Extension& extension, std::string_view filter) {
    static constexpr char kScriptTemplate[] =
        R"((async () => {
             chrome.test.sendScriptResult(
                 await chrome.runtime.getContexts(%s));
           })();)";
    std::string script = base::StringPrintf(kScriptTemplate, filter.data());
    return BackgroundScriptExecutor::ExecuteScript(
        profile(), extension.id(), script,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  }
};

// Tests retrieving popup contexts using `chrome.runtime.getContexts()`.
// This needs to run as an interactive ui test because extension popups are
// closed on focus loss.
IN_PROC_BROWSER_TEST_F(RuntimeGetContextsInteractiveApiTest, GetPopupContext) {
  static constexpr char kManifest[] =
      R"({
           "name": "Get Contexts",
           "version": "0.1",
           "manifest_version": 3,
           "background": {
             "service_worker": "background.js"
           },
           "action": { "default_popup": "popup.html" }
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     "// Intentionally blank");
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.html"),
                     "<html>I'm a popup!</html>");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  std::unique_ptr<ExtensionActionTestHelper> toolbar_helper =
      ExtensionActionTestHelper::Create(browser());
  ExtensionHostTestHelper popup_waiter(profile(), extension->id());
  popup_waiter.RestrictToType(mojom::ViewType::kExtensionPopup);
  toolbar_helper->Press(extension->id());
  ExtensionHost* popup_host = popup_waiter.WaitForHostCompletedFirstLoad();

  content::RenderFrameHost* popup_frame =
      popup_host->web_contents()->GetPrimaryMainFrame();
  int expected_frame_id = ExtensionApiFrameIdMap::GetFrameId(popup_frame);
  std::string expected_context_id =
      ExtensionApiFrameIdMap::GetContextId(popup_frame).AsLowercaseString();
  std::string expected_document_id =
      ExtensionApiFrameIdMap::GetDocumentId(popup_frame).ToString();
  std::string expected_frame_url =
      extension->GetResourceURL("popup.html").spec();
  std::string expected_origin = extension->origin().Serialize();

  // Query for popup-based contexts. There should only be one.
  base::Value background_contexts =
      GetContexts(*extension, R"({"contextTypes": ["POPUP"]})");

  // Verify the properties of the returned context.
  // NOTE: Currently, extension popups are considered to have a window ID of -1.
  // This makes sense (they aren't really "in" a window), but there's also a
  // good argument for using the window ID of the Browser they're anchored to.
  // We may want to revisit this in the future.
  static constexpr char kExpectedTemplate[] =
      R"([{
            "contextType": "POPUP",
            "contextId": "%s",
            "tabId": -1,
            "windowId": -1,
            "frameId": %d,
            "documentId": "%s",
            "documentUrl": "%s",
            "documentOrigin": "%s",
            "incognito": false
         }])";
  std::string expected =
      base::StringPrintf(kExpectedTemplate, expected_context_id.c_str(),
                         expected_frame_id, expected_document_id.c_str(),
                         expected_frame_url.c_str(), expected_origin.c_str());
  EXPECT_THAT(background_contexts, base::test::IsJson(expected));
}

}  // namespace extensions
