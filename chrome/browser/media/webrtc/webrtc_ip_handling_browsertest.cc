// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/offscreen/offscreen_document_manager.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/offscreen_document_host.h"
#include "extensions/common/api/offscreen.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// Gathers ICE candidates without STUN or TURN servers and resolves with the
// addresses of the host candidates.
constexpr char kGatherHostCandidateAddresses[] = R"(
  async function gatherHostCandidateAddresses() {
    const pc = new RTCPeerConnection();
    pc.createDataChannel('test');
    const addresses = [];
    const gatheringComplete = new Promise(resolve => {
      pc.onicecandidate = ({candidate}) => {
        if (!candidate) {
          resolve();
        } else if (candidate.type == 'host') {
          addresses.push(candidate.address);
        }
      };
    });
    await pc.setLocalDescription();
    await gatheringComplete;
    pc.close();
    return addresses;
  }
)";

class WebRtcIpHandlingBrowserTest : public extensions::ExtensionBrowserTest {
 protected:
  // Loads an MV3 extension with an empty page.
  const extensions::Extension* LoadTestExtension() {
    test_dir_.WriteManifest(R"({
      "name": "WebRTC IP handling test",
      "version": "1",
      "manifest_version": 3,
      "permissions": ["offscreen"]
    })");
    test_dir_.WriteFile(FILE_PATH_LITERAL("page.html"), "<html></html>");
    return LoadExtension(test_dir_.UnpackedPath());
  }

  // Expects that `web_contents` gathers host candidates and that all of them
  // hide the local IP address behind an mDNS hostname.
  static void ExpectOnlyMdnsHostCandidates(content::WebContents* web_contents) {
    const content::EvalJsResult result = content::EvalJs(
        web_contents, base::StrCat({kGatherHostCandidateAddresses,
                                    "gatherHostCandidateAddresses();"}));
    ASSERT_TRUE(result.is_list()) << result;
    const base::ListValue& addresses = result.ExtractList();
    ASSERT_FALSE(addresses.empty()) << "No host candidates were gathered.";
    for (const base::Value& address : addresses) {
      EXPECT_TRUE(address.GetString().ends_with(".local"))
          << "Host candidate exposes a local IP address: "
          << address.GetString();
    }
  }

 private:
  extensions::TestExtensionDir test_dir_;
};

// Extension pages without camera or microphone permission must hide local IP
// addresses behind mDNS hostnames, the same way web pages do.
// Regression test for https://crbug.com/544139421.
IN_PROC_BROWSER_TEST_F(WebRtcIpHandlingBrowserTest,
                       ExtensionPageHidesLocalIpAddresses) {
  const extensions::Extension* extension = LoadTestExtension();
  ASSERT_TRUE(extension);

  content::WebContents* web_contents = GetActiveWebContents();
  ASSERT_TRUE(
      NavigateToURL(web_contents, extension->GetResourceURL("page.html")));
  ASSERT_NO_FATAL_FAILURE(ExpectOnlyMdnsHostCandidates(web_contents));
}

// Same as above for an offscreen document, which has no visible UI.
IN_PROC_BROWSER_TEST_F(WebRtcIpHandlingBrowserTest,
                       OffscreenDocumentHidesLocalIpAddresses) {
  const extensions::Extension* extension = LoadTestExtension();
  ASSERT_TRUE(extension);

  extensions::ExtensionHostTestHelper host_waiter(profile());
  host_waiter.RestrictToType(extensions::mojom::ViewType::kOffscreenDocument);
  extensions::OffscreenDocumentHost* offscreen_document =
      extensions::OffscreenDocumentManager::Get(profile())
          ->CreateOffscreenDocument(
              *extension, extension->GetResourceURL("page.html"),
              {extensions::api::offscreen::Reason::kWebRtc});
  ASSERT_TRUE(offscreen_document);
  host_waiter.WaitForHostCompletedFirstLoad();
  ASSERT_NO_FATAL_FAILURE(
      ExpectOnlyMdnsHostCandidates(offscreen_document->host_contents()));
}

// Platform apps are exempt and keep getting local IP addresses.
IN_PROC_BROWSER_TEST_F(WebRtcIpHandlingBrowserTest,
                       PlatformAppKeepsLocalIpAddresses) {
  extensions::TestExtensionDir app_dir;
  app_dir.WriteManifest(R"({
    "name": "WebRTC IP handling test app",
    "version": "1",
    "manifest_version": 2,
    "app": {"background": {"scripts": ["background.js"]}}
  })");
  app_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      base::StrCat({kGatherHostCandidateAddresses,
                    "gatherHostCandidateAddresses().then(addresses => "
                    "chrome.test.sendMessage(JSON.stringify(addresses)));"}));
  ExtensionTestMessageListener listener;
  ASSERT_TRUE(LoadExtension(app_dir.UnpackedPath()));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  std::optional<base::ListValue> addresses =
      base::JSONReader::ReadList(listener.message(), base::JSON_PARSE_RFC);
  ASSERT_TRUE(addresses) << listener.message();
  ASSERT_FALSE(addresses->empty()) << "No host candidates were gathered.";
  for (const base::Value& address : *addresses) {
    EXPECT_FALSE(address.GetString().ends_with(".local"))
        << address.GetString();
  }
}

}  // namespace
