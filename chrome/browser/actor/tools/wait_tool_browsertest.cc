// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/wait_tool.h"

#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor.mojom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

using base::test::TestFuture;

namespace actor {

namespace {

class ActorWaitToolBrowserTest : public ActorToolsTest {
 public:
  ActorWaitToolBrowserTest() = default;
  ~ActorWaitToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(ActorWaitToolBrowserTest, WaitTool) {
  WaitTool::SetNoDelayForTesting();

  const GURL url = embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::unique_ptr<ToolRequest> action = MakeWaitRequest();
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
}

// Ensure the wait tool doesn't cause the current tab to be recorded as being
// acted on.
IN_PROC_BROWSER_TEST_F(ActorWaitToolBrowserTest, WaitTool_DontRecordActOnTask) {
  WaitTool::SetNoDelayForTesting();

  ASSERT_TRUE(actor_task().GetTabs().empty());

  std::unique_ptr<ToolRequest> action = MakeWaitRequest();
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_TRUE(actor_task().GetTabs().empty());
}

IN_PROC_BROWSER_TEST_F(ActorWaitToolBrowserTest, WaitTool_BlockedByScheme) {
  WaitTool::SetNoDelayForTesting();

  // The embedded test server serves over HTTP. For a non-localhost domain like
  // "example.com", the resulting URL is "http://example.com". Tab-gating safety
  // checks (`MayActOnTab`) explicitly block remote HTTP sites (only allowing
  // HTTPS), resulting in `kActionsBlockedForScheme`.
  const GURL url =
      embedded_test_server()->GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::unique_ptr<ToolRequest> action = MakeWaitRequest(active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());

  ExpectErrorResult(result, mojom::ActionResultCode::kActionsBlockedForScheme);
}

}  // namespace
}  // namespace actor
