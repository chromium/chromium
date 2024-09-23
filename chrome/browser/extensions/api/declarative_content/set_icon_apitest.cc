// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "extensions/browser/api/declarative/rules_registry.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/gfx/image/image.h"

namespace extensions {
namespace {

const char kDeclarativeContentManifest[] = R"({
  "name": "Declarative Content apitest",
  "version": "0.1",
  "manifest_version": 2,
  "description": "end-to-end browser test for the declarative Content API",
  "background": {"scripts": ["background.js"]},
  "page_action": {},
  "permissions": ["declarativeContent"]
})";

constexpr char kOneByOneImageData[] =
    "GAAAAAAAAAAQAAAAAAAAADAAAAAAAAAAKAAAAAAAAAACAAAAAQAAAAEAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAwAAAAEAAAAAAAAAAAAAAA=";

class SetIconAPITest : public ExtensionApiTest {
 public:
  SetIconAPITest()
      // Set the channel to "trunk" since declarativeContent is restricted
      // to trunk.
      : current_channel_(version_info::Channel::UNKNOWN) {}
  ~SetIconAPITest() override {}

 protected:
  const Extension* LoadTestExtension() {
    ext_dir_.WriteManifest(kDeclarativeContentManifest);
    ext_dir_.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
  var declarative = chrome.declarative;

  var PageStateMatcher = chrome.declarativeContent.PageStateMatcher;
  var SetIcon = chrome.declarativeContent.SetIcon;

  var canvas = document.createElement("canvas");
  var ctx = canvas.getContext("2d");
  var imageData = ctx.createImageData(1, 1);

  var rule0 = {
    conditions: [new PageStateMatcher({pageUrl: {queryContains: "show"}})],
    actions: [new SetIcon({"imageData": imageData})]
  };
  var testEvent = chrome.declarativeContent.onPageChanged;

  testEvent.removeRules(undefined, function() {
    testEvent.addRules([rule0], function() {
      chrome.test.sendMessage("ready", function(reply) {});
    });
  });
)");
    ExtensionTestMessageListener ready("ready");
    const Extension* extension = LoadExtension(ext_dir_.UnpackedPath());
    if (!extension) {
      return nullptr;
    }

    // Wait for declarative rules to be set up.
    profile()->GetDefaultStoragePartition()->FlushNetworkInterfaceForTesting();
    EXPECT_TRUE(GetExtensionAction(*extension));

    EXPECT_TRUE(ready.WaitUntilSatisfied());

    return extension;
  }

  const ExtensionAction* GetExtensionAction(const Extension& extension) {
    return ExtensionActionManager::Get(profile())->GetExtensionAction(
        extension);
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

 private:
  extensions::ScopedCurrentChannel current_channel_;
  TestExtensionDir ext_dir_;
};

IN_PROC_BROWSER_TEST_F(SetIconAPITest, Overview) {
  const Extension* extension = LoadTestExtension();
  ASSERT_TRUE(extension);

  content::WebContents* const tab = GetActiveWebContents();
  const int tab_id = ExtensionTabUtil::GetTabId(tab);

  // There should be no declarative icon until we navigate to a matched page.
  const ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  EXPECT_TRUE(action->GetDeclarativeIcon(tab_id).IsEmpty());
  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://example.com/?show")));
  EXPECT_FALSE(action->GetDeclarativeIcon(tab_id).IsEmpty());

  // Navigating to an unmatched page should reset the icon.
  EXPECT_FALSE(NavigateInRenderer(tab, GURL("http://example.com/?hide")));
  EXPECT_TRUE(action->GetDeclarativeIcon(tab_id).IsEmpty());
}

// Regression test for crbug.com/1231027.
IN_PROC_BROWSER_TEST_F(SetIconAPITest, Parameter) {
  const Extension* extension = LoadTestExtension();
  ASSERT_TRUE(extension);

  scoped_refptr<RulesRegistry> rules_registry =
      extensions::RulesRegistryService::Get(browser()->profile())
          ->GetRulesRegistry(RulesRegistryService::kDefaultRulesRegistryID,
                             "declarativeContent.onPageChanged");
  ASSERT_TRUE(rules_registry);

  std::vector<const api::events::Rule*> rules;
  rules_registry->GetAllRules(extension->id(), &rules);
  ASSERT_EQ(1u, rules.size());
  ASSERT_EQ(rules[0]->actions.size(), 1u);

  const base::Value::Dict& action_value = rules[0]->actions[0].GetDict();
  const std::string* action_instance_type =
      action_value.FindString("instanceType");
  ASSERT_TRUE(action_instance_type);
  EXPECT_EQ("declarativeContent.SetIcon", *action_instance_type);

  const std::string* image_data_value =
      action_value.FindStringByDottedPath("imageData.1");
  ASSERT_TRUE(image_data_value);
  EXPECT_EQ(kOneByOneImageData, *image_data_value);
}

class SetIconAPIPrerenderingTest : public SetIconAPITest {
 public:
  SetIconAPIPrerenderingTest()
      : prerender_helper_(base::BindRepeating(
            &SetIconAPIPrerenderingTest::GetActiveWebContents,
            base::Unretained(this))) {}
  ~SetIconAPIPrerenderingTest() override = default;

 protected:
  content::FrameTreeNodeId Prerender(const GURL& url) {
    return prerender_helper_.AddPrerender(url);
  }
  void Activate(const GURL& url) { prerender_helper_.NavigatePrimaryPage(url); }

 private:
  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    ExtensionApiTest::SetUp();
  }

  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(SetIconAPIPrerenderingTest, Overview) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const Extension* extension = LoadTestExtension();
  ASSERT_TRUE(extension);

  content::WebContents* const tab = GetActiveWebContents();
  const int tab_id = ExtensionTabUtil::GetTabId(tab);

  // There should be no declarative icon until we navigate to a matched page.
  const ExtensionAction* action = GetExtensionAction(*extension);
  ASSERT_TRUE(action);

  EXPECT_TRUE(action->GetDeclarativeIcon(tab_id).IsEmpty());

  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html?show");
  EXPECT_TRUE(NavigateInRenderer(tab, kInitialUrl));
  EXPECT_FALSE(action->GetDeclarativeIcon(tab_id).IsEmpty());

  // Prerendering an unmatched page should not reset the icon.
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL("/empty.html?hide");
  content::FrameTreeNodeId host_id = Prerender(kPrerenderingUrl);
  ASSERT_TRUE(host_id);
  EXPECT_FALSE(action->GetDeclarativeIcon(tab_id).IsEmpty());

  // Activating the unmatched page should reset the icon.
  Activate(kPrerenderingUrl);
  EXPECT_TRUE(action->GetDeclarativeIcon(tab_id).IsEmpty());
  EXPECT_EQ(tab->GetLastCommittedURL(), kPrerenderingUrl);
}

}  // namespace
}  // namespace extensions
