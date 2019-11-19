// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/gfx/image/image.h"

namespace extensions {
namespace {

const char kDeclarativeContentManifest[] =
    "{\n"
    "  \"name\": \"Declarative Content apitest\",\n"
    "  \"version\": \"0.1\",\n"
    "  \"manifest_version\": 2,\n"
    "  \"description\": \n"
    "      \"end-to-end browser test for the declarative Content API\",\n"
    "  \"background\": {\n"
    "    \"scripts\": [\"background.js\"]\n"
    "  },\n"
    "  \"page_action\": {},\n"
    "  \"permissions\": [\n"
    "    \"declarativeContent\"\n"
    "  ]\n"
    "}\n";

class SetIconAPITest : public ExtensionApiTest {
 public:
  SetIconAPITest()
      // Set the channel to "trunk" since declarativeContent is restricted
      // to trunk.
      : current_channel_(version_info::Channel::UNKNOWN) {
  }
  ~SetIconAPITest() override {}

  extensions::ScopedCurrentChannel current_channel_;
  TestExtensionDir ext_dir_;
};

IN_PROC_BROWSER_TEST_F(SetIconAPITest, Overview) {
  ext_dir_.WriteManifest(kDeclarativeContentManifest);
  ext_dir_.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      "var declarative = chrome.declarative;\n"
      "\n"
      "var PageStateMatcher = chrome.declarativeContent.PageStateMatcher;\n"
      "var SetIcon = chrome.declarativeContent.SetIcon;\n"
      "\n"
      "var canvas = document.createElement(\'canvas\');\n"
      "var ctx = canvas.getContext(\"2d\");"
      "var imageData = ctx.createImageData(19,19);\n"
      "\n"
      "var rule0 = {\n"
      "  conditions: [new PageStateMatcher({\n"
      "                   pageUrl: {hostPrefix: \"test1\"}})],\n"
      "  actions: [new SetIcon({\"imageData\": imageData})]\n"
      "}\n"
      "\n"
      "var testEvent = chrome.declarativeContent.onPageChanged;\n"
      "\n"
      "testEvent.removeRules(undefined, function() {\n"
      "  testEvent.addRules([rule0], function() {\n"
      "    chrome.test.sendMessage(\"ready\", function(reply) {\n"
      "    })\n"
      "  });\n"
      "});\n");
  ExtensionTestMessageListener ready("ready", false);
  const Extension* extension = LoadExtension(ext_dir_.UnpackedPath());
  ASSERT_TRUE(extension);
  // Wait for declarative rules to be set up.
  content::BrowserContext::GetDefaultStoragePartition(profile())
      ->FlushNetworkInterfaceForTesting();
  const ExtensionAction* action =
      ExtensionActionManager::Get(browser()->profile())
          ->GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  ASSERT_TRUE(ready.WaitUntilSatisfied());
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  const int tab_id = ExtensionTabUtil::GetTabId(tab);

  // There should be no declarative icon until we navigate to a matched page.
  EXPECT_TRUE(action->GetDeclarativeIcon(tab_id).IsEmpty());
  NavigateInRenderer(tab, GURL("http://test1/"));
  EXPECT_FALSE(action->GetDeclarativeIcon(tab_id).IsEmpty());

  // Navigating to an unmatched page should reset the icon.
  NavigateInRenderer(tab, GURL("http://test2/"));
  EXPECT_TRUE(action->GetDeclarativeIcon(tab_id).IsEmpty());
}
}  // namespace
}  // namespace extensions
