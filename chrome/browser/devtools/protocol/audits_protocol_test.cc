// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/public/test/browser_test.h"

namespace content {
// Test fixture for DevTools Audits protocol tests that need an embedded server.
class AuditsProtocolWithServerTest : public DevToolsProtocolTestBase {

public:
  void SetUpOnMainThread() override {
    DevToolsProtocolTestBase::SetUpOnMainThread();
    embedded_test_server()->ServeFilesFromSourceDirectory(
                                                          "chrome/test/data/devtools/protocol");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(AuditsProtocolWithServerTest, OverrideFlashEmbedwithHTML) {
    // Attach to the DevTools agent host for the web contents.
    Attach();

    // Enable the Audits domain to start receiving issue events.
    SendCommandSync("Audits.enable");
    SendCommandSync("DOM.enable");

    GURL url = embedded_test_server()->GetURL("/flash_embed_test.html");
    SendCommandSync("Page.navigate", base::Value::Dict().Set("url", url.spec()));

    base::Value::Dict notification =
      WaitForNotification("Audits.issueAdded", true);
    EXPECT_EQ(*notification.FindStringByDottedPath("issue.code"),
              "DeprecationIssue");
    EXPECT_EQ(*notification.FindStringByDottedPath("issue.details.deprecationIssueDetails.type"),
              "OverrideFlashEmbedwithHTML");
}

}  // namespace content
