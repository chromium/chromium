// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "base/version_info/channel.h"
#include "chrome/browser/ai/ai_data_keyed_service.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {
namespace {

// Matches the key in api_test/experimental_ai_data/manifest.json.
constexpr char kTestExtensionId[] = "kbanhggbnnaciicfpdkheonkpkeakfal";
// The debugging extension's public key gives the positive tests its real ID.
constexpr char kApcDebuggingExtensionKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEArsX3uGAGs3YzPKnM5Q5IWEPY"
    "wyO/Ec5EYPICT+wyZSCFrl8riHe5IoNveDOCol1p/I5tRcJdoOLBWfc8V8KGca67T4ao"
    "QbdJ+TrdkVQZRLjT+XUJzRdHaW2Ifk2U+MKRL/HLssw1n4CfehpXoINBo1M+b2n41zH"
    "9n2X2qf0g8Zp3FvaJeKzTgZh15WpymmF89s+hQ7s5dzIMqsDVS3Pzn3P/uJsHOkiKP5"
    "K8PvDcbuLevhs2vb6/9uR/jBMyQb28RjLV/7Po62+u5h9bTH2ukv0rkRXk3IwhNoFMV"
    "uM5brm/mhCeSCnffNyn+DIMRMb2hoBZ83PF+SqLAW4Jf4upiQIDAQAB";
enum class Access {
  kAllowed,
  kStableChannel,
  kBlocklisted,
  kNotAllowlisted,
  kIncognitoTarget,
  kOtherAllowlistedExtension,
  kOtherAllowlistedStableExtension
};

class ExperimentalAiDataApcApiTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<Access> {
 protected:
  void SetUp() override {
    features_.InitAndEnableFeatureWithParameters(
        AiDataKeyedService::GetAllowlistedAiDataExtensionsFeatureForTesting(),
        {{"allowlisted_extension_ids",
          GetParam() == Access::kNotAllowlisted ? "" : kTestExtensionId},
         {"blocked_extension_ids",
          GetParam() == Access::kBlocklisted ? kTestExtensionId : ""}});
    ExtensionApiTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kExtensionAiDataCollection);
  }

  void RunTest(bool node_access, bool devtools = false);

  ScopedCurrentChannel channel_{
      (GetParam() == Access::kStableChannel ||
       GetParam() == Access::kOtherAllowlistedStableExtension)
          ? version_info::Channel::STABLE
          : version_info::Channel::DEV};
  base::test::ScopedFeatureList features_;
};

void ExperimentalAiDataApcApiTest::RunTest(bool node_access, bool devtools) {
  const bool check_frame_documents =
      node_access && !devtools &&
      (GetParam() == Access::kAllowed || GetParam() == Access::kStableChannel);
  // DOM node IDs are renderer-local. Keep the frame-document checks in one
  // renderer, including when OriginKeyedProcessesByDefault is enabled.
  const char* page_path = check_frame_documents
                              ? "/set-header?Origin-Agent-Cluster: ?0"
                              : "/simple.html";
  ASSERT_TRUE(embedded_test_server()->Start());
  // An incognito-enabled extension must not capture incognito tabs or resolve
  // nodes in their content-script worlds.
  auto* target_browser = GetParam() == Access::kIncognitoTarget
                             ? CreateIncognitoBrowser()
                             : browser();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      target_browser, embedded_test_server()->GetURL(page_path)));
  auto* contents = target_browser->GetActiveTabInterface()->GetContents();
  ASSERT_TRUE(content::ExecJs(
      contents, "document.body.textContent = 'APC snapshot browser test';"));
  content::WaitForCopyableViewInWebContents(contents);

  net::EmbeddedTestServer other_origin;
  if (check_frame_documents) {
    // A different port gives a cross-origin frame in the same site. All three
    // documents opt out of origin-keyed clustering so they share a renderer.
    // IDs from both child documents must remain inaccessible to the parent.
    other_origin.AddDefaultHandlers();
    ASSERT_TRUE(other_origin.Start());
    ASSERT_TRUE(content::ExecJs(
        contents, content::JsReplace(R"(
      Promise.all([location.href, $1].map(url => new Promise(resolve => {
        const frame = document.createElement('iframe');
        frame.onload = resolve;
        frame.src = url;
        document.body.append(frame);
      })))
    )",
                                     other_origin.GetURL(page_path))));
    auto* main_frame = contents->GetPrimaryMainFrame();
    for (int i = 0; i < 2; ++i) {
      auto* child_frame = content::ChildFrameAt(main_frame, i);
      ASSERT_TRUE(child_frame);
      ASSERT_EQ(main_frame->GetProcess(), child_frame->GetProcess());
    }
  }

  base::DictValue config;
  if (check_frame_documents) {
    // DevTools exposes pseudo-element IDs, but extensions must not receive
    // wrappers for these internal nodes, either alone or within a batch.
    ASSERT_TRUE(content::ExecJs(contents, R"(
      const style = document.createElement('style');
      style.textContent = 'body::before { content: "pseudo"; }';
      document.head.append(style);
      void document.body.offsetTop;
    )"));
    content::TestDevToolsProtocolClient client;
    client.AttachToWebContents(contents);
    // The client does not detach on destruction. Also detach after a failed
    // assertion so shutdown cannot call back into a destroyed test client.
    base::ScopedClosureRunner detach_client(base::BindOnce(
        &content::TestDevToolsProtocolClient::DetachProtocolClient,
        base::Unretained(&client)));
    const auto* document = client.SendCommandSync("DOM.getDocument");
    ASSERT_TRUE(document);
    auto root_id = document->FindIntByDottedPath("root.nodeId");
    ASSERT_TRUE(root_id);
    base::DictValue query;
    query.Set("nodeId", *root_id);
    query.Set("selector", "body");
    const auto* body =
        client.SendCommandSync("DOM.querySelector", std::move(query));
    ASSERT_TRUE(body);
    auto body_id = body->FindInt("nodeId");
    ASSERT_TRUE(body_id);
    base::DictValue describe;
    describe.Set("nodeId", *body_id);
    const auto* description =
        client.SendCommandSync("DOM.describeNode", std::move(describe));
    ASSERT_TRUE(description);
    const auto* pseudos =
        description->FindListByDottedPath("node.pseudoElements");
    ASSERT_TRUE(pseudos);
    ASSERT_EQ(1u, pseudos->size());
    auto pseudo_id = (*pseudos)[0].GetDict().FindInt("backendNodeId");
    ASSERT_TRUE(pseudo_id);
    config.Set("pseudoNodeId", *pseudo_id);
  }
  config.Set("nodeAccess", node_access);
  config.Set("devtools", devtools);
  config.Set("tabId", sessions::SessionTabHelper::IdForTab(contents).id());
  switch (GetParam()) {
    case Access::kAllowed:
    case Access::kStableChannel:
      break;
    case Access::kOtherAllowlistedStableExtension:
      // Stable access is granted only to the debugging extension.
      config.Set("expectedError",
                 node_access ? "API access restricted for this extension."
                             : "API access not allowed on this channel.");
      break;
    case Access::kBlocklisted:
    case Access::kNotAllowlisted:
      config.Set("expectedError", "API access restricted for this extension.");
      break;
    case Access::kIncognitoTarget:
      config.Set("expectedError", node_access
                                      ? "Incognito profile not supported."
                                      : "Invalid target tab passed in.");
      break;
    case Access::kOtherAllowlistedExtension:
      // Existing data access must not grant the new node helpers.
      if (node_access) {
        config.Set("expectedError",
                   "API access restricted for this extension.");
      }
      break;
  }
  const std::string custom_arg = base::WriteJson(config).value();

  const base::FilePath source_dir =
      test_data_dir_.AppendASCII("experimental_ai_data");
  std::string manifest_json;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(source_dir.AppendASCII("manifest.json"),
                                       &manifest_json));
  }
  auto manifest =
      base::JSONReader::ReadDict(manifest_json, base::JSON_PARSE_RFC);
  ASSERT_TRUE(manifest);
  if (GetParam() == Access::kAllowed || GetParam() == Access::kStableChannel ||
      GetParam() == Access::kIncognitoTarget) {
    manifest->Set("key", kApcDebuggingExtensionKey);
  }
  TestExtensionDir extension_dir;
  extension_dir.CopyFileTo(source_dir.AppendASCII("test.js"),
                           FILE_PATH_LITERAL("test.js"));
  if (devtools) {
    // Use the same extension key and JS checks as the content-script test.
    // A blank content script establishes the isolated world before DevTools
    // evaluates in it, as the APC Debugging Extension does on real pages.
    manifest->Remove("background");
    manifest->Set("devtools_page", "devtools.html");
    base::DictValue script;
    script.Set("matches", base::ListValue().Append("http://*/*"));
    script.Set("js", base::ListValue().Append("context.js"));
    manifest->Set("content_scripts",
                  base::ListValue().Append(std::move(script)));
    extension_dir.WriteManifest(*manifest);
    extension_dir.WriteFile(FILE_PATH_LITERAL("context.js"), "");
    extension_dir.WriteFile(FILE_PATH_LITERAL("devtools.html"),
                            "<!doctype html><script src='test.js'></script>");
    SetCustomArg(custom_arg);
    ResultCatcher catcher;
    ASSERT_TRUE(LoadExtension(
        extension_dir.UnpackedPath(),
        {.allow_in_incognito = GetParam() == Access::kIncognitoTarget}));
    // Keep a copy: navigation replaces the WebContents-owned committed URL.
    const GURL reload_url = contents->GetLastCommittedURL();
    ASSERT_TRUE(content::NavigateToURL(contents, reload_url));
    auto* window = DevToolsWindowTesting::OpenDevToolsWindowSync(
        contents, /*is_docked=*/false);
    ASSERT_TRUE(window);
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
    DevToolsWindowTesting::CloseDevToolsWindowSync(window);
    return;
  }
  extension_dir.WriteManifest(*manifest);
  ASSERT_TRUE(RunExtensionTest(
      extension_dir.UnpackedPath(), {.custom_arg = custom_arg.c_str()},
      {.allow_in_incognito = GetParam() == Access::kIncognitoTarget}))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExperimentalAiDataApcApiTest, Snapshot) {
  RunTest(false);
}

IN_PROC_BROWSER_TEST_P(ExperimentalAiDataApcApiTest, NodeBindings) {
  RunTest(true);
}

IN_PROC_BROWSER_TEST_P(ExperimentalAiDataApcApiTest, DevToolsNodeBindings) {
  RunTest(true, true);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ExperimentalAiDataApcApiTest,
    testing::Values(Access::kAllowed,
                    Access::kStableChannel,
                    Access::kBlocklisted,
                    Access::kNotAllowlisted,
                    Access::kIncognitoTarget,
                    Access::kOtherAllowlistedExtension,
                    Access::kOtherAllowlistedStableExtension));

}  // namespace
}  // namespace extensions
