// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

using base::test::TestFuture;
using content::ChildFrameAt;
using content::EvalJs;
using content::GetDOMNodeId;
using content::NavigateIframeToURL;

namespace actor {

namespace {

// Test that requesting tool use on a page that's not active fails. In this case
// we use BFCache but a prerendered page would be another example of an inactive
// page with a live RenderFrameHost.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, InvokeToolInInactiveFrame) {
  // This test relies on BFCache so don't run it if it's not available.
  if (!content::BackForwardCache::IsBackForwardCacheFeatureEnabled()) {
    GTEST_SKIP();
  }

  const GURL url_first =
      embedded_test_server()->GetURL("/actor/blank.html?start");
  const GURL url_second =
      embedded_test_server()->GetURL("/actor/blank.html?target");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_first));

  content::WeakDocumentPtr first_rfh = main_frame()->GetWeakDocumentPtr();
  ASSERT_TRUE(first_rfh.AsRenderFrameHostIfValid()->IsActive());

  std::optional<int> body_id = GetDOMNodeId(*main_frame(), "body");
  ASSERT_TRUE(body_id);

  // Create an action that targets the first document.
  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*first_rfh.AsRenderFrameHostIfValid(), body_id.value());

  // Navigate to the second document - we expect this should put the first
  // document into the BFCache rather than destroying the RenderFrameHost.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_second));
  ASSERT_TRUE(first_rfh.AsRenderFrameHostIfValid());
  EXPECT_EQ(first_rfh.AsRenderFrameHostIfValid()->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kFrameWentAway);
}

// Basic test to ensure sending a click to an element in a same-site subframe
// works.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, InvokeToolSameSiteSubframe) {
  const GURL url =
      embedded_https_test_server().GetURL("/actor/positioned_iframe.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const GURL subframe_url = embedded_https_test_server().GetURL(
      "/actor/page_with_clickable_element.html");
  ASSERT_TRUE(NavigateIframeToURL(web_contents(), "iframe", subframe_url));

  content::RenderFrameHost* subframe =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_FALSE(subframe->IsCrossProcessSubframe());
  ASSERT_TRUE(subframe);

  // Send a click to the button in the subframe.
  std::optional<int> button_id =
      GetDOMNodeIdFromSubframe(*subframe, "#iframe", "button#clickable");
  ASSERT_TRUE(button_id);
  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*subframe, button_id.value());

  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  // Ensure the button's event handler was invoked.
  EXPECT_EQ(true, EvalJs(subframe, "button_clicked"));
}

}  // namespace
}  // namespace actor
