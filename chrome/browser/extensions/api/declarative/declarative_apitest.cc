// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_rules_registry.h"
#include "extensions/browser/extension_frame_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/rules_registry_ids.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom.h"

// This file tests declarativeWebRequest, which is not supported on Android.
static_assert(!BUILDFLAG(IS_ANDROID));

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

struct ApiRequestResult {
  bool succeeded = false;
  std::string error;
};

// Simulates `frame`'s renderer sending a raw `mojom::LocalFrameHost::Request`
// IPC directly to the browser's `ExtensionFrameHost` (bypassing renderer JS
// bindings so tests can forge arbitrary `function_name` / `context_type`
// payloads as a compromised renderer would).
ApiRequestResult SendApiRequestFromFrame(content::RenderFrameHost* frame,
                                         const std::string& function_name,
                                         const std::string& extension_id,
                                         mojom::ContextType context_type,
                                         base::ListValue arguments) {
  auto params = mojom::RequestParams::New();
  params->name = function_name;
  params->arguments = std::move(arguments);
  params->extension_id = extension_id;
  params->source_url = frame->GetLastCommittedURL();
  params->context_type = context_type;
  params->request_id = 1;
  params->has_callback = true;
  params->worker_thread_id = kMainThreadId;
  params->service_worker_version_id =
      blink::mojom::kInvalidServiceWorkerVersionId;

  auto* web_contents = content::WebContents::FromRenderFrameHost(frame);
  auto* extension_frame_host =
      ExtensionWebContentsObserver::GetForWebContents(web_contents)
          ->extension_frame_host_for_testing();
  extension_frame_host->receivers_for_testing().SetCurrentTargetFrameForTesting(
      frame);
  base::test::TestFuture<bool, base::ListValue, const std::string&,
                         mojom::ExtraResponseDataPtr>
      response_future;
  extension_frame_host->Request(std::move(params),
                                response_future.GetCallback());
  extension_frame_host->receivers_for_testing().SetCurrentTargetFrameForTesting(
      nullptr);
  auto [success, response_wrapper, error, extra_data] = response_future.Take();
  return {success, error};
}

}  // namespace

class DeclarativeApiTest : public ExtensionApiTest {
 public:
  std::string GetTitle() {
    std::u16string title(GetActiveWebContents()->GetTitle());
    return base::UTF16ToUTF8(title);
  }

  // Reports the number of rules registered for the |extension_id| with the
  // non-webview rules registry.
  size_t NumberOfRegisteredRules(const std::string& extension_id) {
    RulesRegistryService* rules_registry_service =
        extensions::RulesRegistryService::Get(profile());
    scoped_refptr<RulesRegistry> rules_registry =
        rules_registry_service->GetRulesRegistry(
            rules_registry_ids::kDefaultRulesRegistryID,
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
  ScopedAllowHttpForHostnamesForTesting allow_https({"www.example.com"},
                                                    profile()->GetPrefs());

  // Note that we cannot use an extension generated by *GetRedirectToData*
  // helpers in a TestExtensionDir, because we need the extension to persist
  // until the PersistRules test is run.
  ASSERT_TRUE(RunExtensionTest("declarative/redirect_to_data")) << message_;
}

IN_PROC_BROWSER_TEST_F(DeclarativeApiTest, PersistRules) {
  ScopedAllowHttpForHostnamesForTesting allow_https({"www.example.com"},
                                                    profile()->GetPrefs());

  // Wait for declarative rules to be set up from PRE test.
  profile()->GetDefaultStoragePartition()->FlushNetworkInterfaceForTesting();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kArbitraryUrl)));
  EXPECT_EQ(kTestTitle, GetTitle());
}

// Test that the rules are correctly persisted and (de)activated during
// changing the "installed" and "enabled" status of an extension.
IN_PROC_BROWSER_TEST_F(DeclarativeApiTest, ExtensionLifetimeRulesHandling) {
  ScopedAllowHttpForHostnamesForTesting allow_https({"www.example.com"},
                                                    profile()->GetPrefs());

  TestExtensionDir ext_dir;

  // 1. Install the extension. Rules should become active.
  ext_dir.WriteManifest(GetRedirectToDataManifestWithVersion(1));
  ext_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                    base::StringPrintf("%s%s%s",
                                       kRedirectToDataConstants,
                                       kRedirectToDataRules,
                                       kRedirectToDataInstallRules));
  ExtensionTestMessageListener ready("ready");
  const Extension* extension = InstallExtensionWithUIAutoConfirm(
      ext_dir.Pack(), 1 /*+1 installed extension*/);
  ASSERT_TRUE(extension);
  // Wait for declarative rules to be set up.
  profile()->GetDefaultStoragePartition()->FlushNetworkInterfaceForTesting();
  std::string extension_id(extension->id());
  ASSERT_TRUE(ready.WaitUntilSatisfied());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kArbitraryUrl)));
  EXPECT_EQ(kTestTitle, GetTitle());
  EXPECT_EQ(1u, NumberOfRegisteredRules(extension_id));

  // 2. Disable the extension. Rules are no longer active, but are still
  // registered.
  DisableExtension(extension_id);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kArbitraryUrl)));
  EXPECT_NE(kTestTitle, GetTitle());
  EXPECT_EQ(1u, NumberOfRegisteredRules(extension_id));

  // 3. Enable the extension again. Rules are active again.
  EnableExtension(extension_id);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kArbitraryUrl)));
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
  ExtensionTestMessageListener ready_after_update("ready");
  EXPECT_TRUE(UpdateExtension(
      extension_id, ext_dir.Pack(), 0 /*no new installed extension*/));
  ASSERT_TRUE(ready_after_update.WaitUntilSatisfied());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kArbitraryUrl)));
  EXPECT_EQ(kTestTitle, GetTitle());
  EXPECT_EQ(1u, NumberOfRegisteredRules(extension_id));

  // 5. Reload the extension. Rules remain active.
  ReloadExtension(extension_id);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kArbitraryUrl)));
  EXPECT_EQ(kTestTitle, GetTitle());
  EXPECT_EQ(1u, NumberOfRegisteredRules(extension_id));

  // 6. Uninstall the extension. Rules are gone.
  UninstallExtension(extension_id);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kArbitraryUrl)));
  EXPECT_NE(kTestTitle, GetTitle());
  EXPECT_EQ(0u, NumberOfRegisteredRules(extension_id));
}

// When an extension is uninstalled, the state store deletes all preferences
// stored for that extension. We need to make sure we don't store anything after
// that deletion occurs.
IN_PROC_BROWSER_TEST_F(DeclarativeApiTest, NoTracesAfterUninstalling) {
  ScopedAllowHttpForHostnamesForTesting allow_https({"www.example.com"},
                                                    profile()->GetPrefs());

  TestExtensionDir ext_dir;

  // 1. Install the extension. Verify that rules become active and some prefs
  // are stored.
  ext_dir.WriteManifest(GetRedirectToDataManifestWithVersion(1));
  ext_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                    base::StringPrintf("%s%s%s",
                                       kRedirectToDataConstants,
                                       kRedirectToDataRules,
                                       kRedirectToDataInstallRules));
  ExtensionTestMessageListener ready("ready");
  const Extension* extension = InstallExtensionWithUIAutoConfirm(
      ext_dir.Pack(), 1 /*+1 installed extension*/);
  ASSERT_TRUE(extension);
  // Wait for declarative rules to be set up.
  profile()->GetDefaultStoragePartition()->FlushNetworkInterfaceForTesting();
  std::string extension_id(extension->id());
  ASSERT_TRUE(ready.WaitUntilSatisfied());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kArbitraryUrl)));
  EXPECT_EQ(kTestTitle, GetTitle());
  EXPECT_EQ(1u, NumberOfRegisteredRules(extension_id));
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(extension_prefs->HasPrefForExtension(extension_id));

  // 2. Uninstall the extension. Rules are gone and preferences should be empty.
  UninstallExtension(extension_id);
  // Wait for declarative rules to be removed.
  profile()->GetDefaultStoragePartition()->FlushNetworkInterfaceForTesting();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kArbitraryUrl)));
  EXPECT_NE(kTestTitle, GetTitle());
  EXPECT_EQ(0u, NumberOfRegisteredRules(extension_id));
  EXPECT_FALSE(extension_prefs->HasPrefForExtension(extension_id));
}

// Regression test for crbug.com/574789262: a compromised web renderer that ran
// an extension's content script must not be able to register
// `declarativeWebRequest` rules via `events.addRules` (claiming either
// `kContentScript` or `kPrivilegedExtension`), even when WebRequest proxying is
// active or the extension holds `declarativeWebRequest`.
IN_PROC_BROWSER_TEST_F(DeclarativeApiTest,
                       ContentScriptCannotAccessDeclarativeRules) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  TestExtensionDir mv3_dir;
  mv3_dir.WriteManifest(R"({
    "name": "MV3 Content Script + DNR",
    "version": "1.0",
    "manifest_version": 3,
    "permissions": ["declarativeNetRequest"],
    "content_scripts": [{
      "matches": ["<all_urls>"], "js": ["cs.js"], "run_at": "document_start"
    }]
  })");
  mv3_dir.WriteFile(FILE_PATH_LITERAL("cs.js"),
                    "chrome.test.sendMessage('mv3_cs_ready');");

  TestExtensionDir mv2_dir;
  mv2_dir.WriteManifest(R"({
    "name": "MV2 With DeclarativeWebRequest",
    "version": "1.0",
    "manifest_version": 2,
    "permissions": ["declarativeWebRequest", "<all_urls>"],
    "content_scripts": [{
      "matches": ["<all_urls>"], "js": ["cs.js"], "run_at": "document_start"
    }]
  })");
  mv2_dir.WriteFile(FILE_PATH_LITERAL("cs.js"),
                    "chrome.test.sendMessage('mv2_cs_ready');");

  ExtensionTestMessageListener mv3_ready("mv3_cs_ready");
  ExtensionTestMessageListener mv2_ready("mv2_cs_ready");
  const Extension* mv3_ext = LoadExtension(mv3_dir.UnpackedPath());
  const Extension* mv2_ext = LoadExtension(mv2_dir.UnpackedPath());
  ASSERT_TRUE(mv3_ext);
  ASSERT_TRUE(mv2_ext);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  ASSERT_TRUE(mv3_ready.WaitUntilSatisfied());
  ASSERT_TRUE(mv2_ready.WaitUntilSatisfied());

  content::RenderFrameHost* web_frame =
      GetActiveWebContents()->GetPrimaryMainFrame();
  constexpr char kHijackRuleArgs[] = R"([
    "declarativeWebRequest.onRequest", 0, [{
      "id": "r1",
      "conditions": [{
        "instanceType": "declarativeWebRequest.RequestMatcher",
        "url": {"schemes": ["http"]}
      }],
      "actions": [{
        "instanceType": "declarativeWebRequest.RedirectRequest",
        "redirectUrl": "data:text/html;charset=utf-8,<title>HIJACKED</title>"
      }]
    }]
  ])";

  for (const Extension* ext : {mv3_ext, mv2_ext}) {
    SCOPED_TRACE(ext->name());
    ApiRequestResult cs_res =
        SendApiRequestFromFrame(web_frame, "events.addRules", ext->id(),
                                mojom::ContextType::kContentScript,
                                base::test::ParseJsonList(kHijackRuleArgs));
    EXPECT_FALSE(cs_res.succeeded);
    EXPECT_EQ("Access to extension API denied.", cs_res.error);

    ApiRequestResult spoof_res =
        SendApiRequestFromFrame(web_frame, "events.addRules", ext->id(),
                                mojom::ContextType::kPrivilegedExtension,
                                base::test::ParseJsonList(kHijackRuleArgs));
    EXPECT_FALSE(spoof_res.succeeded);
    EXPECT_EQ("Invalid context type provided.", spoof_res.error);

    EXPECT_EQ(0u, NumberOfRegisteredRules(ext->id()));
  }

  profile()->GetDefaultStoragePartition()->FlushNetworkInterfaceForTesting();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title2.html")));
  EXPECT_EQ("Title Of Awesomeness", GetTitle());
}

}  // namespace extensions
