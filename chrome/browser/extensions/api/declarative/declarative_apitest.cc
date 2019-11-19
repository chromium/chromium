// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_rules_registry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

namespace {

const char kArbitraryUrl[] = "http://www.example.com";  // Must be http://.

// The extension in "declarative/redirect_to_data" redirects every navigation to
// a page with title |kTestTitle|.
#define TEST_TITLE_STRING ":TEST:"
const char kTestTitle[] = TEST_TITLE_STRING;

// All methods and constands below containing "RedirectToData" in their names
// are parts of a test extension "Redirect to 'data:'".
std::string GetRedirectToDataManifestWithVersion(unsigned version) {
  return base::StringPrintf(
      "{\n"
      "  \"name\": \"Redirect to 'data:' (declarative apitest)\",\n"
      "  \"version\": \"%d\",\n"
      "  \"manifest_version\": 2,\n"
      "  \"description\": \"Redirects all requests to a fixed data: URI.\",\n"
      "  \"background\": {\n"
      "    \"scripts\": [\"background.js\"]\n"
      "  },\n"
      "  \"permissions\": [\n"
      "    \"declarativeWebRequest\", \"<all_urls>\"\n"
      "  ]\n"
      "}\n",
      version);
}

const char kRedirectToDataConstants[] =
    "var redirectDataURI =\n"
    "    'data:text/html;charset=utf-8,<html><head><title>' +\n"
    "    '" TEST_TITLE_STRING "' +\n"
    "    '<%2Ftitle><%2Fhtml>';\n";
#undef TEST_TITLE_STRING

const char kRedirectToDataRules[] =
    "var rules = [{\n"
    "  conditions: [\n"
    "    new chrome.declarativeWebRequest.RequestMatcher({\n"
    "        url: {schemes: ['http']}})\n"
    "  ],\n"
    "  actions: [\n"
    "    new chrome.declarativeWebRequest.RedirectRequest({\n"
    "      redirectUrl: redirectDataURI\n"
    "    })\n"
    "  ]\n"
    "}];\n";

const char kRedirectToDataInstallRules[] =
    "function report(details) {\n"
    "  if (chrome.extension.lastError) {\n"
    "    chrome.test.log(chrome.extension.lastError.message);\n"
    "  } else {\n"
    "    chrome.test.sendMessage(\"ready\", function(reply) {})\n"
    "  }\n"
    "}\n"
    "\n"
    "chrome.runtime.onInstalled.addListener(function(details) {\n"
    "  if (details.reason == 'install')\n"
    "    chrome.declarativeWebRequest.onRequest.addRules(rules, report);\n"
    "});\n";

const char kRedirectToDataNoRules[] =
    "chrome.runtime.onInstalled.addListener(function(details) {\n"
    "  chrome.test.sendMessage(\"ready\", function(reply) {})\n"
    "});\n";

}  // namespace

class DeclarativeApiTest : public ExtensionApiTest {
 public:
  std::string GetTitle() {
    base::string16 title(
        browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
    return base::UTF16ToUTF8(title);
  }

  // Reports the number of rules registered for the |extension_id| with the
  // non-webview rules registry.
  size_t NumberOfRegisteredRules(const std::string& extension_id) {
    RulesRegistryService* rules_registry_service =
        extensions::RulesRegistryService::Get(browser()->profile());
    scoped_refptr<RulesRegistry> rules_registry =
        rules_registry_service->GetRulesRegistry(
            RulesRegistryService::kDefaultRulesRegistryID,
            extensions::declarative_webrequest_constants::kOnRequest);

    std::vector<const api::events::Rule*> rules;
    rules_registry->GetAllRules(extension_id, &rules);
    return rules.size();
  }
};

IN_PROC_BROWSER_TEST_F(DeclarativeApiTest, DeclarativeApi) {
  ASSERT_TRUE(RunExtensionTest("declarative/api")) << message_;

  // Check that uninstalling the extension has removed all rules.
  std::string extension_id = GetSingleLoadedExtension()->id();
  UninstallExtension(extension_id);

  // UnloadExtension posts a task to the owner thread of the extension
  // to process this unloading. The next task to retrive all rules
  // is therefore processed after the UnloadExtension task has been executed.
  EXPECT_EQ(0u, NumberOfRegisteredRules(extension_id));
}

// PersistRules test first installs an extension, which registers some rules.
// Then after browser restart, it checks that the rules are still in effect.
IN_PROC_BROWSER_TEST_F(DeclarativeApiTest, PRE_PersistRules) {
  // Note that we cannot use an extension generated by *GetRedirectToData*
  // helpers in a TestExtensionDir, because we need the extension to persist
  // until the PersistRules test is run.
  ASSERT_TRUE(RunExtensionTest("declarative/redirect_to_data")) << message_;
}

IN_PROC_BROWSER_TEST_F(DeclarativeApiTest, PersistRules) {
  // Wait for declarative rules to be set up from PRE test.
  content::BrowserContext::GetDefaultStoragePartition(profile())
      ->FlushNetworkInterfaceForTesting();
  ui_test_utils::NavigateToURL(browser(), GURL(kArbitraryUrl));
  EXPECT_EQ(kTestTitle, GetTitle());
}

// Disabled for flakiness: http://crbug.com/851854
#if defined(OS_MACOSX) && defined(ADDRESS_SANITIZER)
#define MAYBE_ExtensionLifetimeRulesHandling \
  DISABLED_ExtensionLifetimeRulesHandling
#else
#define MAYBE_ExtensionLifetimeRulesHandling ExtensionLifetimeRulesHandling
#endif

// Test that the rules are correctly persisted and (de)activated during
// changing the "installed" and "enabled" status of an extension.
IN_PROC_BROWSER_TEST_F(DeclarativeApiTest,
                       MAYBE_ExtensionLifetimeRulesHandling) {
  TestExtensionDir ext_dir;

  // 1. Install the extension. Rules should become active.
  ext_dir.WriteManifest(GetRedirectToDataManifestWithVersion(1));
  ext_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                    base::StringPrintf("%s%s%s",
                                       kRedirectToDataConstants,
                                       kRedirectToDataRules,
                                       kRedirectToDataInstallRules));
  ExtensionTestMessageListener ready("ready", /*will_reply=*/false);
  const Extension* extension = InstallExtensionWithUIAutoConfirm(
      ext_dir.Pack(), 1 /*+1 installed extension*/, browser());
  ASSERT_TRUE(extension);
  // Wait for declarative rules to be set up.
  content::BrowserContext::GetDefaultStoragePartition(profile())
      ->FlushNetworkInterfaceForTesting();
  std::string extension_id(extension->id());
  ASSERT_TRUE(ready.WaitUntilSatisfied());
  ui_test_utils::NavigateToURL(browser(), GURL(kArbitraryUrl));
  EXPECT_EQ(kTestTitle, GetTitle());
  EXPECT_EQ(1u, NumberOfRegisteredRules(extension_id));

  // 2. Disable the extension. Rules are no longer active, but are still
  // registered.
  DisableExtension(extension_id);
  ui_test_utils::NavigateToURL(browser(), GURL(kArbitraryUrl));
  EXPECT_NE(kTestTitle, GetTitle());
  EXPECT_EQ(1u, NumberOfRegisteredRules(extension_id));

  // 3. Enable the extension again. Rules are active again.
  EnableExtension(extension_id);
  ui_test_utils::NavigateToURL(browser(), GURL(kArbitraryUrl));
  EXPECT_EQ(kTestTitle, GetTitle());
  EXPECT_EQ(1u, NumberOfRegisteredRules(extension_id));

  // 4. Bump the version and update, without the code to add the rules. Rules
  // are still active, because the registry does not drop them unless the
  // extension gets uninstalled.
  ext_dir.WriteManifest(GetRedirectToDataManifestWithVersion(2));
  ext_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      base::StringPrintf(
          "%s%s", kRedirectToDataConstants, kRedirectToDataNoRules));
  ExtensionTestMessageListener ready_after_update("ready",
                                                  /*will_reply=*/false);
  EXPECT_TRUE(UpdateExtension(
      extension_id, ext_dir.Pack(), 0 /*no new installed extension*/));
  ASSERT_TRUE(ready_after_update.WaitUntilSatisfied());
  ui_test_utils::NavigateToURL(browser(), GURL(kArbitraryUrl));
  EXPECT_EQ(kTestTitle, GetTitle());
  EXPECT_EQ(1u, NumberOfRegisteredRules(extension_id));

  // 5. Reload the extension. Rules remain active.
  ReloadExtension(extension_id);
  ui_test_utils::NavigateToURL(browser(), GURL(kArbitraryUrl));
  EXPECT_EQ(kTestTitle, GetTitle());
  EXPECT_EQ(1u, NumberOfRegisteredRules(extension_id));

  // 6. Uninstall the extension. Rules are gone.
  UninstallExtension(extension_id);
  ui_test_utils::NavigateToURL(browser(), GURL(kArbitraryUrl));
  EXPECT_NE(kTestTitle, GetTitle());
  EXPECT_EQ(0u, NumberOfRegisteredRules(extension_id));
}

// Disabled for flakiness: http://crbug.com/851854
#if defined(OS_MACOSX) && defined(ADDRESS_SANITIZER)
#define MAYBE_NoTracesAfterUninstalling DISABLED_NoTracesAfterUninstalling
#else
#define MAYBE_NoTracesAfterUninstalling NoTracesAfterUninstalling
#endif

// When an extension is uninstalled, the state store deletes all preferences
// stored for that extension. We need to make sure we don't store anything after
// that deletion occurs.
IN_PROC_BROWSER_TEST_F(DeclarativeApiTest, MAYBE_NoTracesAfterUninstalling) {
  TestExtensionDir ext_dir;

  // 1. Install the extension. Verify that rules become active and some prefs
  // are stored.
  ext_dir.WriteManifest(GetRedirectToDataManifestWithVersion(1));
  ext_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                    base::StringPrintf("%s%s%s",
                                       kRedirectToDataConstants,
                                       kRedirectToDataRules,
                                       kRedirectToDataInstallRules));
  ExtensionTestMessageListener ready("ready", /*will_reply=*/false);
  const Extension* extension = InstallExtensionWithUIAutoConfirm(
      ext_dir.Pack(), 1 /*+1 installed extension*/, browser());
  ASSERT_TRUE(extension);
  // Wait for declarative rules to be set up.
  content::BrowserContext::GetDefaultStoragePartition(profile())
      ->FlushNetworkInterfaceForTesting();
  std::string extension_id(extension->id());
  ASSERT_TRUE(ready.WaitUntilSatisfied());
  ui_test_utils::NavigateToURL(browser(), GURL(kArbitraryUrl));
  EXPECT_EQ(kTestTitle, GetTitle());
  EXPECT_EQ(1u, NumberOfRegisteredRules(extension_id));
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(browser()->profile());
  EXPECT_TRUE(extension_prefs->HasPrefForExtension(extension_id));

  // 2. Uninstall the extension. Rules are gone and preferences should be empty.
  UninstallExtension(extension_id);
  // Wait for declarative rules to be removed.
  content::BrowserContext::GetDefaultStoragePartition(profile())
      ->FlushNetworkInterfaceForTesting();
  ui_test_utils::NavigateToURL(browser(), GURL(kArbitraryUrl));
  EXPECT_NE(kTestTitle, GetTitle());
  EXPECT_EQ(0u, NumberOfRegisteredRules(extension_id));
  EXPECT_FALSE(extension_prefs->HasPrefForExtension(extension_id));
}

}  // namespace extensions
